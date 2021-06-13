#pragma once

#include <cstring>
#include <map>
#include <iostream>
#include <chrono>
#include <list>
#include <optional>

#include <SFML/Network/UdpSocket.hpp>
#include <SFML/Graphics/Color.hpp>
#include <queue>

#include "types.hpp"
#include "packet.hpp"
#include "vec_math.hpp"
#include "fixed_string.hpp"
#include "rand_pool.hpp"
#include "avg_counter.hpp"
#include "log.hpp"

namespace dfdh {

using namespace std::chrono_literals;

inline constexpr auto CLI_HELLO_MAGICK        = 0xdeadbeeffeedf00d;
inline constexpr u16  SERVER_DEFAULT_PORT     = 27015;
inline constexpr u16  CLIENT_DEFAULT_PORT     = 27020;
inline constexpr bool ENABLE_DEBUG_SEND_DELAY = true;
inline constexpr auto DEBUG_SEND_DELAY        = 40ms;
inline constexpr bool VARIABLE_PING           = true;
inline constexpr auto VARIABLE_PING_MAX_DELTA = 39ms;

template <bool Enable>
struct _debug_packet_queue {
    struct delayed_send_t {
        std::chrono::steady_clock::time_point tp;
        sf::IpAddress                         ip;
        u16                                   port;
        packet_t                              packet;

        [[nodiscard]]
        bool operator<(const delayed_send_t& ds) const {
            return tp.time_since_epoch().count() > ds.tp.time_since_epoch().count();
        }
    };

    static auto& queue() {
        static std::priority_queue<delayed_send_t, std::vector<delayed_send_t>> queue;
        return queue;
    }

    static auto& mt() {
        static std::mt19937 mt;
        return mt;
    }
};

template <>
struct _debug_packet_queue<false> {};

template <bool Debug = ENABLE_DEBUG_SEND_DELAY>
void send_packet(sf::UdpSocket& sock, const sf::IpAddress& ip, u16 port, const packet_t& packet) {
    if constexpr (Debug) {
        auto& mt         = _debug_packet_queue<Debug>::mt();
        auto  delta      = std::uniform_int_distribution<long>(-VARIABLE_PING_MAX_DELTA.count(),
                                                         VARIABLE_PING_MAX_DELTA.count())(mt);
        auto  send_delay = DEBUG_SEND_DELAY + std::chrono::milliseconds(delta);

        _debug_packet_queue<Debug>::queue().push(
            {std::chrono::steady_clock::now() + send_delay, ip, port, packet});
    }
    else {
        sock.send(packet.data(), packet.size(), ip, port);
    }
}

template <bool Debug = ENABLE_DEBUG_SEND_DELAY>
void update_delayed_send(sf::UdpSocket& sock) {
    if constexpr (Debug) {
        auto now = std::chrono::steady_clock::now();

        auto& q = _debug_packet_queue<true>::queue();

        while (!q.empty() && now > q.top().tp) {
            auto& top = q.top();
            sock.send(top.packet.data(), top.packet.size(), top.ip, top.port);
            q.pop();
        }
    }
}

/*====================================== ACTIONS ==========================================*/

enum net_act : u32 {
    act_transcontrol_ok = 0,
    act_transcontrol_corrupted,
    act_cli_i_wanna_play,
    act_cli_player_params,
    act_cli_player_sync,

    act_srv_ping,
    act_srv_player_states,
    act_srv_player_physic_sync,
//    act_srv_player_random_pool_init,

    act_spawn_bullet,

    NET_ACT_COUNT
};

struct a_cli_i_wanna_play {
    u64     magik   = CLI_HELLO_MAGICK;
    u32     _dummy0 = 0;
    net_act _act    = act_cli_i_wanna_play;
};

struct a_cli_player_params {
    fixed_str<23> body_txtr;
    fixed_str<23> face_txtr;
    sf::Color     body_color;

    net_act _act = act_cli_player_params;
};

struct a_transcontrol_ok {
    u64     id;
    u64     hash;
    u32     _dummy0 = 0;
    net_act _act    = act_transcontrol_ok;
};

struct a_transcontrol_corrupted {
    u64     id;
    u64     hash;
    u32     _dummy0 = 0;
    net_act _act    = act_transcontrol_corrupted;
};

struct a_srv_ping {
    u16     _ping_id;
    u16     _ping_ms;
    net_act _act = act_srv_ping;
};

struct player_states_t {
    auto operator<=>(const player_states_t&) const = default;

    bool mov_left;
    bool mov_right;
    bool on_shot;
    bool jump;
    bool jump_down;
    bool lock_y;

    u8 _dummy0[2] = {0};
};

struct a_cli_player_sync {
    player_states_t st;
    u64             evt_counter;
    sf::Vector2f    position;
    sf::Vector2f    velocity;

    u32     _dummy0 = 0;
    net_act _act    = act_cli_player_sync;
};

struct a_srv_player_states {
    player_states_t st;
    u64             player_id;
    u64             evt_counter;
    sf::Vector2f    position;
    sf::Vector2f    velocity;

    u32     _dummy0 = 0;
    net_act _act    = act_srv_player_states;
};

struct a_srv_player_physic_sync {
    u64             player_id;
    sf::Vector2f    position;
    sf::Vector2f    velocity;
    player_states_t st;
    fixed_str<23>   cur_wpn_name;
//    u64             random_pool_pos;
    u64             evt_counter;
    u32             ammo_elapsed;
    bool            on_left;

    u8      _dummy0[3] = {0};
    u32     _dummy1    = 0;

    net_act _act       = act_srv_player_physic_sync;
};

/*
struct a_srv_player_random_pool_init {
    u64             player_id;
    rand_float_pool pool;
    net_act _act = act_srv_player_random_pool_init;
};
*/

struct a_spawn_bullet {
    struct bullet_data_t {
        sf::Vector2f _position;
        sf::Vector2f _velocity;
    };
    u64                        player_id;
    float                      mass;
    std::vector<bullet_data_t> bullets;

    net_act _act    = act_spawn_bullet;
};

inline uint64_t fnv1a64(const u8* ptr, size_t size) {
    uint64_t hash = 0xcbf29ce484222325;

    while (size--)
        hash = (hash ^ *ptr++) * 0x100000001b3;

    return hash;
}

inline u64 next_packet_id() {
    static u64 id = 0;
    return id++;
}

template <typename T>
packet_t create_packet(const T& act, u32 transcontrol = 0, u64* id = nullptr) {
    packet_t packet;
    u64 packet_id = next_packet_id();
    packet.append(act, transcontrol, packet_id);

    auto hash = fnv1a64(packet.data(), packet.size());
    packet.append(hash);

    if (id != nullptr)
        *id = packet_id;

    return packet;
}

struct packet_spec_t {
    /* Part of action data */
    net_act act;

    u32     transcontrol;
    u64     id;
    u64     hash;
};

/*
packet_t create_packet(const a_srv_player_random_pool_init& act, u32 transcontrol = 0, u64* id = nullptr) {
    packet_t packet;
    u64 packet_id = next_packet_id();

    u64 sz = act.pool.size();
    packet.append(act.player_id, sz);

    packet.append_raw(act.pool.data(), act.pool.size());
    packet.append(act._act, transcontrol, packet_id);

    auto hash = fnv1a64(packet.data(), packet.size());
    packet.append(hash);

    if (id != nullptr)
        *id = packet_id;

    return packet;
}
*/

packet_t create_packet(const a_spawn_bullet& act, u32 transcontrol = 0, u64* id = nullptr) {
    packet_t packet;
    u64 packet_id = next_packet_id();

    u64 sz = act.bullets.size();
    packet.append(act.player_id, act.mass);
    packet.append(sz);
    packet.append_raw(reinterpret_cast<const u8*>(act.bullets.data()), // NOLINT
                      sizeof(act.bullets[0]) * act.bullets.size());
    packet.append(act._act, transcontrol, packet_id);

    auto hash = fnv1a64(packet.data(), packet.size());
    packet.append(hash);

    if (id != nullptr)
        *id = packet_id;

    return packet;
}

inline bool validate_packet_size(const packet_t& packet) {
    if (packet.size() < sizeof(packet_spec_t)) {
        LOG_WARN("packet dropped: invalid size {}", packet.size());
        return false;
    }
    return true;
}

inline bool validate_packet_hash(const packet_t& packet, u64 spec_hash) {
    auto real_hash = fnv1a64(packet.data(), packet.size() - sizeof(u64));
    if (spec_hash != real_hash) {
        LOG_WARN("packet dropped: invalid hash {} (must be equal with {})", spec_hash, real_hash);
        return false;
    }
    return true;
}

inline bool validate_spec_act(const packet_spec_t& spec) {
    if (spec.act >= NET_ACT_COUNT) {
        LOG_WARN("packet dropped: invalid action {}", u64(spec.act));
        return false;
    }
    return true;
}

inline packet_spec_t get_packet_spec(const packet_t& packet) {
    packet_spec_t spec;
    auto spec_pos = packet.size() - sizeof(packet_spec_t);
    std::memcpy(&spec,
                reinterpret_cast<const u8*>(packet.data()) + spec_pos, // NOLINT
                sizeof(packet_spec_t));
    return spec;
}

inline std::optional<packet_spec_t> get_spec_if_valid_packet(const packet_t& packet) {
    if (!validate_packet_size(packet))
        return {};

    auto spec = get_packet_spec(packet);

    if (!validate_packet_hash(packet, spec.hash))
        return {};

    if (!validate_spec_act(spec))
        return {};

    return spec;
}

template <typename T>
T action_cast(const packet_t& packet) {
    T action;
    std::memcpy(&action, packet.data(), packet.size() - sizeof(packet_spec_t));
    return action;
}

/*
template <>
a_srv_player_random_pool_init action_cast(const packet_t& packet) {
    a_srv_player_random_pool_init act;
    u64 size;
    std::memcpy(&act.player_id, packet.data(), sizeof(act.player_id));
    std::memcpy(&size, packet.data() + sizeof(act.player_id), sizeof(size));

    act.pool.data(packet.data() + sizeof(size) + sizeof(act.player_id), size);

    return act;
}
*/

template <>
a_spawn_bullet action_cast(const packet_t& packet) {
    a_spawn_bullet act;
    size_t offset = 0;
    std::memcpy(&act.player_id, packet.data() + offset, sizeof(act.player_id));
    offset += sizeof(act.player_id);
    std::memcpy(&act.mass, packet.data() + offset, sizeof(act.mass));
    offset += sizeof(act.mass);

    u64 size;
    std::memcpy(&size, packet.data() + offset, sizeof(size));
    offset += sizeof(size);
    act.bullets.resize(size);

    std::memcpy(act.bullets.data(), packet.data() + offset, size * sizeof(act.bullets[0]));
    return act;
}

#define overload(NAME) \
    case act_##NAME: \
        overloaded(ip, port, id, action_cast<a_##NAME>(packet)); \
        break

template <typename F>
void action_dispatch(const sf::IpAddress& ip, u16 port, net_act act, u64 id, const packet_t& packet, F overloaded) {

    switch (act) {
        overload(cli_i_wanna_play);
        overload(cli_player_params);
        overload(cli_player_sync);
        overload(srv_ping);
        overload(srv_player_states);
        overload(srv_player_physic_sync);
 //       overload(srv_player_random_pool_init);
        overload(spawn_bullet);
    default: break;
    }
}

struct uniq_packet_info {
    u32 ip;
    u16 port;
    u64 id;
    u64 hash;

    auto operator<=>(const uniq_packet_info&) const = default;
};

class transcontrol_sender {
public:
    struct params_t {
        using time_point_t =
            std::chrono::steady_clock::time_point;
        using time_dur_t =
            std::chrono::steady_clock::duration;

        packet_t      packet;
        u16           retries_left = 10;
        time_point_t  send_tp;
        time_dur_t    resend_interval;

        std::function<void(bool)> _handler;
    };

    void send(sf::UdpSocket&            sock,
              packet_t                  packet,
              const sf::IpAddress&      ip,
              u16                       port,
              std::function<void(bool)> handler = {},
              std::chrono::milliseconds resend_interval = 200ms) {
        auto spec = get_spec_if_valid_packet(packet);
        if (!spec.value().transcontrol)
            throw std::runtime_error("transcontrol attribute not set");

        auto id = spec.value().id;

        auto& p = _active
                      .emplace(uniq_packet_info{ip.toInteger(), port, id, spec->hash},
                               params_t{std::move(packet),
                                        10,
                                        std::chrono::steady_clock::now(),
                                        resend_interval,
                                        std::move(handler)})
                      .first->second.packet;
        send_packet(sock, ip, port, p);
    }

    void update_resend(sf::UdpSocket& sock) {
        for (auto i = _active.begin(); i != _active.end();) {
            auto& [info, params] = *i;

            auto now = std::chrono::steady_clock::now();

            if (params.send_tp + params.resend_interval > now) {
                ++i;
                continue;
            }
            else if (params.retries_left == 0) {
                if (i->second._handler)
                    i->second._handler(false);
                _active.erase(i++);
                //std::cout << "Send packet " << info.id << " failed" << std::endl;
                continue;
            }

            //std::cout << "Resend packet " << info.id << ", retry: " << params.retries_left << std::endl;
            params.send_tp = now;
            --params.retries_left;

            send_packet(sock, sf::IpAddress(info.ip), info.port, params.packet);

            ++i;
        }
    }

    void receive_response(const sf::IpAddress& ip, u16 port, const a_transcontrol_ok& ok) {
        auto found = _active.find(uniq_packet_info{ip.toInteger(), port, ok.id, ok.hash});
        if (found == _active.end()) {
            LOG_WARN("packet dropped: unexpected transcontrol_ok packet with id={} hash={}",
                ok.id,
                ok.hash);
            return;
        }

        if (found->second._handler)
            found->second._handler(true);

        _active.erase(found);
    }

    void receive_response(const sf::IpAddress& ip, u16 port, const a_transcontrol_corrupted& corrupt) {
        auto found = _active.find(uniq_packet_info{ip.toInteger(), port, corrupt.id, corrupt.hash});
        if (found == _active.end()) {
            LOG_WARN("packet dropped: unexpected transcontrol_corrupted packet");
            return;
        }

        found->second.retries_left = 10;
        found->second.send_tp += found->second.resend_interval;
    }

    [[nodiscard]]
    std::optional<params_t> get_active(const uniq_packet_info& packet_info) const {
        auto found = _active.find(packet_info);
        if (found != _active.end())
            return found->second;
        else
            return {};
    }

private:
    std::map<uniq_packet_info, params_t> _active;
};


class transcontrol_receiver {
public:
    using time_point_t =
        std::chrono::steady_clock::time_point;

    bool validate_spec(sf::UdpSocket&       sock,
                       const sf::IpAddress& ip,
                       u16                  port,
                       const packet_t&      packet,
                       const packet_spec_t& spec) {
        if (!validate_packet_hash(packet, spec.hash) || !validate_spec_act(spec)) {
            auto resp = create_packet(a_transcontrol_corrupted{spec.id, spec.hash});
            send_packet(sock, ip, port, resp);
            return false;
        }

        auto resp = create_packet(a_transcontrol_ok{spec.id, spec.hash});

        auto [i, insert_was] =
            _already_received.emplace(uniq_packet_info{ip.toInteger(), port, spec.id, spec.hash},
                                      std::chrono::steady_clock::now());
        if (!insert_was) {
            send_packet(sock, ip, port, resp);
            LOG_WARN("packet dropped: already received");
            i->second = std::chrono::steady_clock::now();
            return false;
        }
        send_packet(sock, ip, port, resp);

        return true;
    }

    void update_cleanup() {
        for (auto i = _already_received.begin(); i != _already_received.end();) {
            if (std::chrono::steady_clock::now() - i->second > 5s)
                _already_received.erase(i++);
            else
                ++i;
        }
    }

private:
    std::map<uniq_packet_info, time_point_t> _already_received;
};


inline auto trc_success_set(bool& value) {
    return [&](bool success) { return value = success; };
}


class act_socket {
public:
    act_socket(u16 port): act_socket(sf::IpAddress::Any, port) {}

    act_socket(const sf::IpAddress& ip, u16 port) {
        if (socket.bind(port, ip) != sf::Socket::Done)
            throw std::runtime_error("Cannot bind socket " + ip.toString() + ":" +
                                     std::to_string(int(port)));
        socket.setBlocking(false);
    }

    template <typename T, typename F = bool>
    u64 send(const sf::IpAddress& ip, u16 port, const T& act, F transcontrol_handler = false) {
        u64 id;

        bool trc_enabled = true;
        if constexpr (std::is_same_v<F, bool>)
            trc_enabled = transcontrol_handler;

        auto packet = create_packet(act, trc_enabled, &id);

        //std::cout << "Send packet with id=" << id << std::endl;
        if constexpr (std::is_same_v<F, bool>) {
            if (transcontrol_handler)
                trc_send.send(socket, packet, ip, port);
            else
                send_packet(socket, ip, port, packet);
        } else {
            trc_send.send(socket, packet, ip, port, std::function{transcontrol_handler});
        }
        return id;
    }

    [[nodiscard]]
    auto delivery_params(const uniq_packet_info& info) const {
        return trc_send.get_active(info);
    }

    template <typename F>
    void incoming(const sf::IpAddress& ip, u16 port, const packet_t& packet, F overloaded) {
        if (!validate_packet_size(packet))
            return;

        auto spec = get_packet_spec(packet);

        bool ok = false;
        if (spec.transcontrol)
            ok = trc_recv.validate_spec(socket, ip, port, packet, spec);
        else
            ok = validate_packet_hash(packet, spec.hash) && validate_spec_act(spec);

        if (!ok)
            return;

        switch (spec.act) {
        case act_transcontrol_ok:
            trc_send.receive_response(ip, port, action_cast<a_transcontrol_ok>(packet));
            break;
        case act_transcontrol_corrupted:
            trc_send.receive_response(ip, port, action_cast<a_transcontrol_corrupted>(packet));
            break;
        default:
            action_dispatch(ip, port, spec.act, spec.id, packet, overloaded);
        }
    }

    template <typename F>
    void receiver(F overloaded) {
        packet_t packet;
        packet.resize(sf::UdpSocket::MaxDatagramSize);
        size_t packet_size;

        sf::IpAddress ip;
        u16 port;

        sf::Socket::Status rc;
        while ((rc = socket.receive(packet.data(), packet.size(), packet_size, ip, port)) != sf::Socket::NotReady) {
            packet.resize(packet_size);

            switch (rc) {
                case sf::Socket::Done:
                    incoming(ip, port, packet, overloaded);
                    break;
                default:
                    break;
            }

            packet.resize(sf::UdpSocket::MaxDatagramSize);
        }

        trc_recv.update_cleanup();
        trc_send.update_resend(socket);

        update_delayed_send(socket);
    }

private:
    sf::UdpSocket socket;
    transcontrol_sender trc_send;
    transcontrol_receiver trc_recv;
};

template <typename T>
concept PlayerActs = AnyOfType<T, a_cli_player_params, a_cli_player_sync, a_spawn_bullet>;

class server_singleton {
public:
    static server_singleton& instance() {
        static server_singleton inst;
        return inst;
    }

    template <typename T, typename F = bool>
    void send(u64 player_id, const T& act, F transcontrol_handler = false) {
        auto found = _player_id_to_ip.find(player_id);
        if (found == _player_id_to_ip.end())
            return;

        sock.send(found->second.ip, found->second.port, act, transcontrol_handler);
    }

    template <typename T>
    void send_to_all(const T& act) {
        for (auto& [_, address] : _player_id_to_ip)
            sock.send(address.ip, address.port, act);
    }

    template <typename F>
    void act(const sf::IpAddress& ip, u16 port, u64 packet_id, const a_cli_i_wanna_play& act, F&& player_update_callback) {
        if (act.magik != CLI_HELLO_MAGICK) {
            LOG_WARN("packet dropped: client hello has invalid magik {} (could be {})",
                act.magik,
                CLI_HELLO_MAGICK);
            return;
        }

        /* TODO: create player slot */
        u32 player_id = 1;

        _clients.insert_or_assign(ip.toInteger(), client_t{player_id, port});
        _player_id_to_ip.insert_or_assign(player_id, client_t_ip{ip, port});

        player_update_callback(player_id, packet_id, act);
    }

    u16 get_ping_packet_id() {
        return _ping_id++;
    }

    template <typename F>
    void act(const sf::IpAddress& ip, u16, u64, const a_srv_ping& act, F&&) {
        auto found = _clients.find(ip.toInteger());
        if (found != _clients.end() && found->second._last_ping_id == act._ping_id) {
            auto& client = found->second;
            client._last_ping_id = 0;
            auto now = std::chrono::steady_clock::now();
            auto time = std::chrono::duration_cast<std::chrono::milliseconds>(now - client._last_send).count();
            client.ping.update(time);
            LOG_UPDATE("Ping: {}ms", client.ping.value());
        }
    }

    template <typename F>
    void act(const sf::IpAddress& ip, u16, u64 packet_id, const PlayerActs auto& act, F&& player_update_callback) {
        auto client_info = get_client_info(ip);

        if (client_info)
            player_update_callback(client_info->player_id, packet_id, act);
    }

    template <typename T, typename F> requires (!PlayerActs<T>)
    void act(const sf::IpAddress&, u16, u64, const T&, F&&) {
        LOG_WARN("server action: invalid overload");
    }

    struct client_t {
        u32 player_id;
        u16 port;
        u16 _last_ping_id = 0;
        avg_counter<long> ping{5, 0, 0};

        std::chrono::steady_clock::time_point _last_send = std::chrono::steady_clock::now();
    };

    struct client_t_ip {
        sf::IpAddress ip;
        u16 port;
    };

    [[nodiscard]]
    std::optional<client_t> get_client_info(const sf::IpAddress& ip) const {
        auto find = _clients.find(ip.toInteger());
        if (find == _clients.end()) {
            LOG_WARN("packet dropped: cannot find player with ip {}", ip.toString());
            return {};
        }
        return find->second;
    }

    template <typename F>
    void work(F&& player_update_callback) {
        sock.receiver([&, this](const sf::IpAddress& ip, u16 port, u64 id, auto&& act) {
            this->act(ip, port, id, act, player_update_callback);
        });

        for (auto& [_, client] : _clients) {
            auto now = std::chrono::steady_clock::now();
            if (now - client._last_send > 100ms && client._last_ping_id == 0) {
                client._last_send = now;
                client._last_ping_id = get_ping_packet_id();
                send(client.player_id,
                     a_srv_ping{client._last_ping_id, static_cast<u16>(client.ping.value())});
            }
        }
    }

    auto get_ping(u32 player_id) {
        return _clients.at(_player_id_to_ip.at(player_id).ip.toInteger()).ping.value();
    }

private:
    server_singleton(): sock(SERVER_DEFAULT_PORT) {
        LOG("Server started at {}:{}",
            sf::IpAddress::getLocalAddress().toString(),
            SERVER_DEFAULT_PORT);
    }
    ~server_singleton() = default;

private:
    act_socket sock;

    std::map<u32, client_t>    _clients;
    std::map<u64, client_t_ip> _player_id_to_ip;

    u16 _ping_id = 0;
};


inline server_singleton& server() {
    return server_singleton::instance();
}
}

