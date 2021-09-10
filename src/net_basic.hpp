#pragma once

#include <string>
#include <memory>
#include <limits>
#include <cstring>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include "log.hpp"

#include "scope_guard.hpp"
#include "serialization.hpp"

namespace dfdh {

class invalid_ip_address : public std::invalid_argument {
public:
    invalid_ip_address(const std::string& s): std::invalid_argument("Ip address " + s + " is invalid") {}
};

class invalid_address : public std::invalid_argument {
public:
    invalid_address(const std::string& s): std::invalid_argument(s) {}
};

class invalid_port : public std::invalid_argument {
public:
    invalid_port(const std::string& s): std::invalid_argument("Port " + s + " is invalid") {}
};

class socket_error : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

class socket_already_in_use : public socket_error {
public:
    socket_already_in_use(const std::string& address): socket_error("Socket " + address + " already in use") {}
};

static inline constexpr size_t MAX_PACKET_SIZE = 1472;

enum class send_rc { ok = 0, not_ready, too_big_msg, system };
enum class receive_rc { ok = 0, empty, not_connected, system, invalid_hash, already_received };

inline send_rc send_error_cast(int sys_error) {
    if (sys_error == EWOULDBLOCK)
        return send_rc::not_ready;

    switch (sys_error) {
    case EAGAIN: return send_rc::not_ready;
    case EMSGSIZE: return send_rc::too_big_msg;
    default: return send_rc::system;
    }
}

inline receive_rc receive_error_cast(int sys_error) {
    switch (sys_error) {
    case EAGAIN: return receive_rc::empty;
    case ENOTCONN: return receive_rc::not_connected;
    default: return receive_rc::system;
    }
}

class ip_address {
public:
    ip_address(in_addr_t address): addr(address) {}

    ip_address(const std::string& address) {
        setup(address);
    }

    static ip_address localhost() {
        return ip_address("localhost");
    }

    static ip_address broadcast() {
        return ip_address("255.255.255.255");
    }

    static ip_address any() {
        return ip_address("0.0.0.0");
    }

    template <size_t S>
    ip_address(const char (&ip)[S]): ip_address(std::string(ip, S - 1)) {}

    ip_address& operator=(const std::string& address) {
        setup(address);
        return *this;
    }

    [[nodiscard]]
    std::string to_string() const {
        auto res = std::string(INET_ADDRSTRLEN, '\0');
        inet_ntop(AF_INET, &addr, res.data(), INET_ADDRSTRLEN);
        res.resize(size_t(::strchr(res.data(), '\0') - res.data()));
        return res;
    }

    [[nodiscard]]
    auto as_system_type() const {
        return addr;
    }

    auto operator<=>(const ip_address& ip) const noexcept = default;

private:
    void setup(const std::string& address) {
        if (address == "0.0.0.0")
            addr = INADDR_ANY;
        else if (address == "255.255.255.255")
            addr = INADDR_BROADCAST;
        else {
            addr = inet_addr(address.data());
            if (addr == INADDR_NONE) {
                addrinfo hints{};
                hints.ai_family = AF_INET;
                addrinfo* result = nullptr;

                if (getaddrinfo(address.data(), nullptr, &hints, &result) != 0 || !result)
                    throw invalid_ip_address(address);

                addr = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr.s_addr; // NOLINT
                freeaddrinfo(result);
            }
        }
    }

private:
    in_addr_t addr = INADDR_ANY;
};

template <>
struct printer<ip_address> {
    void operator()(std::ostream& os, const ip_address& ip) const {
        os << ip.to_string();
    }
};

using port_t = uint16_t;

inline std::string throw_socket_bind_exception(const ip_address& ip, port_t port, const int sys_error) {
    auto addr = ip.to_string();
    addr += ':';
    addr += std::to_string(u32(port));

    switch (sys_error) {
        case EADDRINUSE:
            throw socket_already_in_use(addr);
        default:
            throw socket_error("Cannot bind socket " + addr);
    }
}

struct address_t {
    static address_t str(const std::string& address) {
        auto delim_pos = address.find(':');
        if (delim_pos == std::string::npos)
            throw invalid_address("Delimiter not found in address " + address);

        auto ip = ip_address(address.substr(0, delim_pos));

        auto port_start = address.data() + delim_pos + 1;
        auto port_end   = address.data() + address.size();

        constexpr auto throw_invalid_port = [](auto port_start, auto port_end) {
            throw invalid_port(std::string(port_start, size_t(port_end - port_start)));
        };

        if (port_end - port_start > 5)
            throw_invalid_port(port_start, port_end);

        uint32_t port = 0;
        auto p = port_start;
        while (p != port_end) {
            if (*p < '0' || *p > '9')
                throw_invalid_port(port_start, port_end);
            port = port * 10 + uint32_t(*p++ - '0');
        }

        if (port > std::numeric_limits<port_t>::max())
            throw_invalid_port(port_start, port_end);

        return {ip, port_t(port)};
    }

    address_t(ip_address ip_addr = ip_address::localhost(), port_t iport = 27015):
        ip(ip_addr), port(iport) {}

    auto operator<=>(const address_t& ip) const noexcept = default;

    ip_address ip;
    port_t     port;
};

template <>
struct printer<address_t> {
    void operator()(std::ostream& os, const address_t& addr) {
        os << addr.ip.to_string() << ':' << addr.port;
    }
};

template <typename T>
struct receive_result {
    [[nodiscard]]
    operator bool() const  {
        return rc == receive_rc::ok;
    }

    T* operator->() {
        return &data;
    }

    const T* operator->() const {
        return &data;
    }

    receive_rc   rc;
    T            data;
    address_t    address;
};

template <size_t S, size_t C>
class block_pool {
public:
    static constexpr auto block_size() {
        return S;
    }

    struct node_t {
        std::array<char, S> data;
        node_t* next = nullptr;
    };

    block_pool() {
        storage.reset(new node_t[C]); // NOLINT
        auto count = C;
        head = &storage[--count]; // NOLINT

        while (count--) {
            auto node = &storage[count];
            node->next = head;
            head = node;
        }
    }

    void* alloc() {
        if (!head)
            return ::malloc(block_size()); // NOLINT

        auto result = head;
        head = head->next;

        return result;
    }

    void free(void* ptr) {
        if (ptr < storage.get() || ptr >= storage.get() + C) {
            ::free(ptr); // NOLINT
            return;
        }

        auto node = reinterpret_cast<node_t*>(ptr); // NOLINT
        node->next = head;
        head = node;
    }

private:
    std::unique_ptr<node_t[]> storage;
    node_t*                   head;
};

namespace details {
    static inline constexpr size_t PACKET_POOL_SIZE = 32;
    static inline auto& packet_pool() {
        thread_local static block_pool<MAX_PACKET_SIZE, PACKET_POOL_SIZE> pool;
        return pool;
    }
}

struct packet_storage_vec {
    packet_storage_vec() = default;

    ~packet_storage_vec() {
        if (storage)
            details::packet_pool().free(storage);
    }

    packet_storage_vec(const packet_storage_vec& vec): cur_size(vec.cur_size) {
        ::memcpy(storage, vec.storage, cur_size);
    }

    packet_storage_vec& operator=(const packet_storage_vec& vec) {
        cur_size = vec.cur_size;
        ::memcpy(storage, vec.storage, cur_size);
        return *this;
    }

    packet_storage_vec(packet_storage_vec&& vec) noexcept: storage(vec.storage), cur_size(vec.cur_size) {
        vec.storage = nullptr;
        vec.cur_size = 0;
    }

    packet_storage_vec& operator=(packet_storage_vec&& vec) noexcept {
        storage = vec.storage;
        cur_size = vec.cur_size;
        vec.storage = nullptr;
        vec.cur_size = 0;
        return *this;
    }

    void resize(size_t new_size) {
        if (cur_size + new_size > MAX_PACKET_SIZE)
            throw serializer_size_reached();

        cur_size = new_size;
    }

    [[nodiscard]]
    std::byte* data() {
        return storage;
    }

    [[nodiscard]]
    const std::byte* data() const {
        return storage;
    }

    [[nodiscard]] auto size() const {
        return cur_size;
    }

    void clear() {
        cur_size = 0;
    }

    std::byte* storage  = reinterpret_cast<std::byte*>(details::packet_pool().alloc()); // NOLINT
    size_t     cur_size = 0;
};

using packet_serializer = serializer_tmpl<packet_storage_vec>;

class packet_t {
public:
    packet_t(): ds({s.data().data(), s.data().size()}) {}

    static constexpr auto max_size() {
        return MAX_PACKET_SIZE;
    }

    void append(const auto&... args) {
        s.write(args...);
        reset_take_pos();
    }

    template <typename T>
    T take() {
        return ds.read_get<T>();
    }

    template <typename T>
    T cast_to() const {
        auto res = ds.read_get<T>();
        ds = deserializer_view({s.data().data(), s.data().size()});
        return res;
    }

    void reset_take_pos() {
        ds = deserializer_view({s.data().data(), s.data().size()});
    }

    void reset() {
        s.reset();
        reset_take_pos();
    }

    packet_t& operator+=(const auto& value) {
        append(value);
        return *this;
    }

    [[nodiscard]]
    auto size() const {
        return s.data().size();
    }

    [[nodiscard]]
    auto data() const {
        return s.data().data();
    }

    auto data() {
        return s.data().data();
    }

public:
    packet_serializer s;
    mutable deserializer_view ds;
};

class udp_socket {
public:
    udp_socket(const ip_address& ip, port_t port, bool blocking = true) {
        if (fd == -1)
            throw socket_error("Can't create socket");
        auto scope_exit = scope_guard([this]{ ::close(fd); });

        set_blocking(blocking);

        int broadcast = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(int)) == -1)
            throw socket_error("Can't setup broadcast for UDP socket");

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = ip.as_system_type();

        if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) // NOLINT
            throw_socket_bind_exception(ip, port, errno);

        scope_exit.dismiss();
    }

    udp_socket(const address_t& address, bool blocking = true): udp_socket(address.ip, address.port, blocking) {}

    udp_socket(const udp_socket&)             = delete;
    udp_socket& operator==(const udp_socket&) = delete;

    udp_socket(udp_socket&& socket) noexcept: addr(socket.addr), fd(socket.fd) {
        socket.fd = -1;
    }

    udp_socket& operator==(udp_socket&& socket) noexcept {
        addr      = socket.addr;
        fd        = socket.fd;
        socket.fd = -1;
        return *this;
    }

    ~udp_socket() {
        if (fd != -1)
            ::close(fd);
    }

    [[nodiscard]]
    address_t address() const {
        return {ip_address(addr.sin_addr.s_addr), htons(addr.sin_port)};
    }

    void set_blocking(bool block) {
        auto flags = fcntl(fd, F_GETFL);
        if (block) {
            if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1)
                throw socket_error("Can't setup blocking status for socket");
        }
        else {
            if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
                throw socket_error("Can't setup non-blocking status for socket");
        }
    }

    [[nodiscard]]
    bool is_blocking() const {
        return fcntl(fd, F_GETFL) & O_NONBLOCK;
    }

    [[nodiscard]]
    send_rc try_send_raw(const ip_address& ip, port_t port, const void* data, size_t size) {
        struct sockaddr_in other = {
            .sin_family = AF_INET,
            .sin_port   = htons(port),
            .sin_addr   = {ip.as_system_type()},
            .sin_zero   = {0}
        };

        if (::sendto(fd, data, size, 0, reinterpret_cast<struct sockaddr*>(&other), sizeof(struct sockaddr)) < 0) // NOLINT
            return send_error_cast(errno);
        return send_rc::ok;
    }

    [[nodiscard]]
    send_rc try_send_raw(const address_t& address, const void* data, size_t size) {
        return try_send_raw(address.ip, address.port, data, size);
    }

    [[nodiscard]]
    send_rc try_send(const ip_address& ip, port_t port, const packet_t& packet) {
        return try_send_raw(ip, port, packet.data(), packet.size());
    }

    [[nodiscard]]
    send_rc try_send(const address_t& address, const packet_t& packet) {
        return try_send_raw(address, packet.data(), packet.size());
    }

    void send_somehow(const ip_address& ip, port_t port, const packet_t& packet) {
        [[maybe_unused]] auto _ = try_send(ip, port, packet);
    }

    void send_somehow(const address_t& address, const packet_t& packet) {
        [[maybe_unused]] auto _ = try_send(address, packet);
    }

    [[nodiscard]]
    receive_result<size_t> try_receive_raw(void* data, size_t max_size) {
        sockaddr_in addr;
        socklen_t   addr_len     = sizeof(in_addr);
        auto        received_len = recvfrom(
            fd, data, max_size, 0, reinterpret_cast<sockaddr*>(&addr), &addr_len); // NOLINT

        return {.rc      = received_len >= 0 ? receive_rc::ok : receive_error_cast(errno),
                .data    = static_cast<size_t>(received_len >= 0 ? received_len : 0),
                .address = {ip_address(addr.sin_addr.s_addr), ntohs(addr.sin_port)}};
    }

    [[nodiscard]]
    receive_result<packet_t> try_receive() {
        packet_t    result;
        sockaddr_in addr         = {};
        socklen_t   addr_len     = sizeof(addr);
        auto        received_len = recvfrom(
            fd, result.data(), result.max_size(), 0, reinterpret_cast<sockaddr*>(&addr), &addr_len); // NOLINT

        if (received_len > 0) {
            result.s.data().resize(size_t(received_len));
            result.reset_take_pos();
        }

        return {.rc      = received_len >= 0 ? receive_rc::ok : receive_error_cast(errno),
                .data    = result,
                .address = {ip_address(addr.sin_addr.s_addr), ntohs(addr.sin_port)}};
    }

private:
    struct sockaddr_in addr = {};
    int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
};
}

namespace std {
template <>
struct hash<dfdh::ip_address> {
    size_t operator()(const dfdh::ip_address& addr) {
        return std::hash<decltype(addr.as_system_type())>()(addr.as_system_type());
    }
};
}
