#pragma once

#include <chrono>
#include <vector>
#include <queue>
#include <random>
#include <SFML/Network/UdpSocket.hpp>

#include "packet.hpp"


namespace dfdh {

using namespace std::chrono_literals;

inline constexpr bool ENABLE_DEBUG_SEND_DELAY = true;
inline constexpr auto DEBUG_SEND_DELAY        = 200ms;
inline constexpr bool VARIABLE_PING           = true;
inline constexpr auto VARIABLE_PING_MAX_DELTA = 50ms;


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
}
