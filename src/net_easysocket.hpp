#pragma once

#include "net_action_socket.hpp"
#include "net_transcontrol.hpp"

namespace dfdh {

class easysocket {
public:
    easysocket(ip_address ip, port_t port): sock(ip, port, false) {}
    easysocket(const address_t& address): sock(address, false) {}

    [[nodiscard]]
    address_t address() const {
        return sock.address();
    }

    [[nodiscard]]
    send_rc try_send(const address_t& address, const NetAction auto& action) {
        auto packet = net_action_to_packet(action);
        auto rc     = sock.raw_socket().try_send(address.ip, address.port, packet);

        if (action.transcontrol && rc == send_rc::ok)
            ts.on_initial_send(std::move(packet), address);

        return rc;
    }

    [[nodiscard]]
    send_rc try_send(ip_address ip, port_t port, const NetAction auto& action) {
        return try_send(address_t(ip, port), action);
    }

    void send_somehow(const address_t& address, const NetAction auto& action) {
        [[maybe_unused]] auto rc = try_send(address, action);
    }

    void send_somehow(ip_address ip, port_t port, const NetAction auto& action) {
        [[maybe_unused]] auto rc = try_send(ip, port, action);
    }

    auto try_receive() {
        auto scope_exit = scope_guard([&] {
            ts.update_resend([&](ip_address ip, port_t port, const packet_t& packet) {
                sock.raw_socket().send_somehow(ip, port, packet);
            });
            tr.update_cleanup();
        });

        auto res = sock.try_receive();
        if (res.rc == receive_rc::invalid_hash) {
            auto spec        = res.data.cast_to<net_spec>();
            auto act         = a_transcontrol_corrupted();
            act.transcontrol = 1;
            act.target_id    = spec.id;
            act.target_hash  = spec.hash;
            sock.send_somehow(res.address, act);
            return res;
        }

        if (res.rc != receive_rc::ok)
            return res;

        auto spec = res.data.cast_to<net_spec>();
        if (spec.transcontrol) {
            switch (spec.action) {
            case a_transcontrol_ok::ACTION:
                ts.on_response_receive(res.address, res.data.cast_to<a_transcontrol_ok>());
                break;
            case a_transcontrol_corrupted::ACTION:
                ts.on_response_receive(res.address, res.data.cast_to<a_transcontrol_corrupted>());
                break;
            default:
                if (tr.is_already_received(res.address,
                                           spec.id,
                                           spec.hash,
                                           [&](ip_address ip, port_t port, const auto& ok_action) {
                                               sock.send_somehow(ip, port, ok_action);
                                           }))
                    res.rc = receive_rc::already_received;
            }
        }

        return res;
    }

    template <typename F>
    auto try_receive_dispatch(F&& func) {
        auto res = try_receive();
        if (res.rc == receive_rc::ok)
            net_action_dispatch(res.address, res.data, std::forward<F>(func));
        return res;
    }

private:
    action_socket         sock;
    transcontrol_sender   ts;
    transcontrol_receiver tr;
};

} // namespace dfdh
