#pragma once

#include <chrono>
#include <map>

#include "net_basic.hpp"
#include "net_actions.hpp"
#include "log.hpp"

namespace dfdh {
using namespace std::chrono_literals;

struct transcontrol_packet_info {
    ip_address ip;
    port_t     port;
    u64        id;
    u64        hash;

    auto operator<=>(const transcontrol_packet_info&) const = default;
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

    void on_initial_send(packet_t                  packet,
                         const address_t&          address,
                         std::function<void(bool)> handler         = {},
                         std::chrono::milliseconds resend_interval = 200ms,
                         u16                       max_retries     = 10) {
        auto spec = packet.cast_to<net_spec>();
        auto receive_ip = address.ip == ip_address::any() ? ip_address::localhost() : address.ip;

        active_connections.insert_or_assign(
            transcontrol_packet_info{receive_ip, address.port, spec.id, spec.hash},
            params_t{std::move(packet),
                     max_retries,
                     std::chrono::steady_clock::now(),
                     resend_interval,
                     std::move(handler)});
    }

    void update_resend(auto&& resend_func) {
        for (auto i = active_connections.begin(); i != active_connections.end();) {
            auto& [info, params] = *i;
            auto now = std::chrono::steady_clock::now();

            if (params.send_tp + params.resend_interval > now) {
                ++i;
                continue;
            }
            else if (params.retries_left == 0) {
                if (i->second._handler)
                    i->second._handler(false);
                active_connections.erase(i++);
                continue;
            }

            params.send_tp = now;
            --params.retries_left;

            LOG_INFO("packet resend to {}:{} id: {} hash: {}",
                     info.ip.to_string(),
                     info.port,
                     info.id,
                     info.hash);
            resend_func(info.ip, info.port, params.packet);

            ++i;
        }
    }

    void on_response_receive(const address_t& from, const a_transcontrol_ok& ok) {
        auto found = active_connections.find(
            transcontrol_packet_info{from.ip, from.port, ok.target_id, ok.target_hash});
        if (found == active_connections.end()) {
            LOG_WARN("packet dropped: unexpected transcontrol_ok packet from {}:{} with id={} hash={}",
                     from.ip.to_string(),
                     from.port,
                     ok.target_id,
                     ok.target_hash);
            return;
        }

        if (found->second._handler)
            found->second._handler(true);

        active_connections.erase(found);
    }

    void on_response_receive(const address_t& from, const a_transcontrol_corrupted& corrupt) {
        auto found = active_connections.find(
            transcontrol_packet_info{from.ip, from.port, corrupt.target_id, corrupt.target_hash});
        if (found == active_connections.end()) {
            LOG_WARN("packet dropped: unexpected transcontrol_corrupted packet");
            return;
        }

        found->second.retries_left = 10;
        found->second.send_tp += found->second.resend_interval;
    }

    [[nodiscard]] std::optional<params_t>
    get_active_connection(const transcontrol_packet_info& packet_info) const {
        auto found = active_connections.find(packet_info);
        if (found != active_connections.end())
            return found->second;
        else
            return {};
    }

private:
    std::map<transcontrol_packet_info, params_t> active_connections;
};

class transcontrol_receiver {
public:
    using time_point_t =
        std::chrono::steady_clock::time_point;

    bool
    is_already_received(const address_t& from, u64 packet_id, u64 packet_hash, auto&& send_func) {
        auto [i, insert_ok] = already_received.emplace(
            transcontrol_packet_info{from.ip, from.port, packet_id, packet_hash},
            std::chrono::steady_clock::now());

        auto act         = a_transcontrol_ok();
        act.transcontrol = 1;
        act.target_id    = packet_id;
        act.target_hash  = packet_hash;
        send_func(from.ip, from.port, act);

        if (!insert_ok) {
            LOG_WARN("packet dropped: already received");
            i->second = std::chrono::steady_clock::now();
            return true;
        }

        return false;
    }

    void update_cleanup() {
        for (auto i = already_received.begin(); i != already_received.end();) {
            if (std::chrono::steady_clock::now() - i->second > 5s)
                already_received.erase(i++);
            else
                ++i;
        }
    }

private:
    std::map<transcontrol_packet_info, time_point_t> already_received;
};

}
