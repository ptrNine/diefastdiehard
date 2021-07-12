#pragma once

#include <chrono>
#include <functional>
#include <SFML/Network/UdpSocket.hpp>

#include "network_actions.hpp"
#include "network_sender.hpp"

namespace dfdh {

using namespace std::chrono_literals;

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
        auto spec = get_spec_if_valid_packet_action(packet);
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

}
