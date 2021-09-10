#pragma once

#include "net_basic.hpp"
#include "net_actions.hpp"
#include "net_actions_generated.hpp"
#include "hash_functions.hpp"
#include "log.hpp"

namespace dfdh {

class action_socket {
public:
    action_socket(ip_address ip, port_t port, bool blocking = true): sock(ip, port, blocking) {}
    action_socket(const address_t& address, bool blocking = true): sock(address, blocking) {}

    static u64 gen_id() {
        static u64 id = 0;
        return id++;
    }

    [[nodiscard]]
    address_t address() const {
        return sock.address();
    }

    void set_blocking(bool value) {
        sock.set_blocking(value);
    }

    [[nodiscard]]
    bool is_blocking() const {
        return sock.is_blocking();
    }

    [[nodiscard]]
    send_rc try_send(const ip_address& ip, port_t port, const NetAction auto& action) {
        return sock.try_send(ip, port, net_action_to_packet(action));
    }

    [[nodiscard]]
    send_rc try_send(const address_t& address, const NetAction auto& action) {
        return sock.try_send(address, net_action_to_packet(action));
    }

    void send_somehow(const ip_address& ip, port_t port, const NetAction auto& action) {
        sock.send_somehow(ip, port, net_action_to_packet(action));
    }

    void send_somehow(const address_t& address, const NetAction auto& action) {
        sock.send_somehow(address, net_action_to_packet(action));
    }

    auto try_receive() {
        auto result = sock.try_receive();
        if (result.rc == receive_rc::ok) {
            if (result.data.size() > sizeof(net_spec)) {
                auto hash = fnv1a64(result.data.data() + sizeof(net_spec), result.data.size() - sizeof(net_spec));
                auto spec = result.data.cast_to<net_spec>();
                if (spec.hash != hash) {
                    LOG_WARN("packet dropped: invalid hash (target: {} actual: {})", spec.hash, hash);
                    result.rc = receive_rc::invalid_hash;
                }
            }
        }
        return result;
    }

    template <typename F>
    auto try_receive_dispatch(F&& func) {
        auto res = try_receive();
        if (res.rc == receive_rc::ok)
            net_action_dispatch(res.address, res.data, std::forward<F>(func));
        return res;
    }

    udp_socket& raw_socket() {
        return sock;
    }

private:
    udp_socket sock;
};

} // namespace dfdh
