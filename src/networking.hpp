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
#include "client_sync_state.hpp"
#include "network_act_socket.hpp"

namespace dfdh {

using namespace std::chrono_literals;

inline constexpr u16  SERVER_DEFAULT_PORT     = 27015;
inline constexpr u16  CLIENT_DEFAULT_PORT     = 27020;

class server_t {
public:
    static std::unique_ptr<server_t> create(u16 port = SERVER_DEFAULT_PORT) {
        return std::make_unique<server_t>(port);
    }

    server_t(u16 port = SERVER_DEFAULT_PORT): sock(port) {
        LOG("Server started at {}:{}",
            sf::IpAddress::getLocalAddress().toString(),
            SERVER_DEFAULT_PORT);
    }

    ~server_t() = default;

    template <typename T, typename F = bool>
    void send(const player_name_t& player_name, const T& act, F transcontrol_handler = false) {
        auto found = _on_game_clients.find(player_name);
        if (found == _on_game_clients.end())
            return;

        sock.send(found->second.ip, found->second.port, act, transcontrol_handler);
    }

    template <typename T>
    void send_to_all(const T& act) {
        for (auto& [ip, client] : _clients)
            sock.send(sf::IpAddress(ip), client.port, act);
    }

    /* Subscribe client for server updates */
    template <typename F>
    void act(const sf::IpAddress& ip, u16 port, u64, const a_cli_i_wanna_play& act, F&&) {
        if (act.magik != CLI_HELLO_MAGICK) {
            LOG_WARN("packet dropped: client hello has invalid magik {} (could be {})",
                act.magik,
                CLI_HELLO_MAGICK);
            return;
        }

        _clients.emplace(ip.toInteger(), client_t{port});
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
            player_update_callback(client_info->state.operated_player, packet_id, act);
    }

    template <typename T, typename F> requires (!PlayerActs<T>)
    void act(const sf::IpAddress&, u16, u64, const T&, F&&) {
        LOG_WARN("server action: invalid overload");
    }

    struct client_t {
        u16               port;
        u16               _last_ping_id = 0;
        avg_counter<long> ping{5, 0, 0};
        client_sync_state state{};

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

        persistent_update();
    }

    auto get_ping(const player_name_t& player_name) {
        return _clients.at(_on_game_clients.at(player_name).ip.toInteger()).ping.value();
    }

private:
    void persistent_update() {
        ping_sender();
        client_state_sender();
    }

    void ping_sender() {
        for (auto& [client_ip, client] : _clients) {
            auto now = std::chrono::steady_clock::now();
            if (now - client._last_send > 100ms && client._last_ping_id == 0) {
                client._last_send = now;
                client._last_ping_id = get_ping_packet_id();
                sock.send(sf::IpAddress(client_ip),
                          client.port,
                          a_srv_ping{client._last_ping_id, static_cast<u16>(client.ping.value())});
            }
        }
    }

    void client_state_sender() {
        for (auto& [_, client] : _clients) {
            if (!client.state.init_sync_ok)
                client_initial_sync(client);
        }
    }

    void client_initial_sync(client_t&/* cli*/) {
    }

private:
    act_socket sock;

    std::map<u32, client_t>              _clients;
    std::map<player_name_t, client_t_ip> _on_game_clients;

    u16 _ping_id = 0;
};

}

