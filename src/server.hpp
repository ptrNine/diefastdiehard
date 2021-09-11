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
#include "vec_math.hpp"
#include "fixed_string.hpp"
#include "rand_pool.hpp"
#include "avg_counter.hpp"
#include "log.hpp"
#include "client_sync_state.hpp"

#include "net_actions.hpp"
#include "net_easysocket.hpp"
#include "net_actor.hpp"

namespace dfdh {

using namespace std::chrono_literals;

inline constexpr port_t SERVER_DEFAULT_PORT = 27015;
inline constexpr port_t CLIENT_DEFAULT_PORT = 27020;

struct act_from_client_t {
    address_t     address;
    player_name_t operated_player;
    u64           packet_id;
};

class server_t {
public:
    static std::unique_ptr<server_t> create(port_t port = SERVER_DEFAULT_PORT) {
        return std::make_unique<server_t>(port);
    }

    server_t(port_t port = SERVER_DEFAULT_PORT): sock(ip_address::localhost(), port) {
        LOG("Server started at {}", sock.address());
        sock.debug_output();
    }

    ~server_t() = default;

    template <typename T>
    void send(const player_name_t&             player_name,
              const T&                         act,
              const std::function<void(bool)>& transcontrol_handler = {},
              std::chrono::milliseconds        resend_interval      = 200ms,
              u16                              max_retries          = 10) {
        auto found = _on_game_clients.find(player_name);
        if (found == _on_game_clients.end())
            return;

        sock.send_somehow(found->second, act, transcontrol_handler, resend_interval, max_retries);
    }

    template <typename T>
    void send(const address_t&                 addr,
              const T&                         act,
              const std::function<void(bool)>& transcontrol_handler = {},
              std::chrono::milliseconds        resend_interval      = 200ms,
              u16                              max_retries          = 10) {
        auto found = _clients.find(addr);
        if (found == _clients.end())
            return;

        sock.send_somehow(addr, act, transcontrol_handler, resend_interval, max_retries);
    }

    template <typename T>
    void send_to_all(const T& act) {
        for (auto& [addr, client] : _clients)
            sock.send_somehow(addr, act);
    }

    template <typename T>
    void send_to_all_except_client_player(const player_name_t& client_player_name, const T& act) {
        for (auto& [addr, client] : _clients)
            if (client.state.operated_player != client_player_name)
                sock.send_somehow(addr, act);
    }

    /* Subscribe client for server updates */
    void act(const address_t& address, const a_cli_i_wanna_play& act, auto&&) {
        if (act.magick != CLI_HELLO_MAGICK) {
            LOG_WARN("packet dropped: client hello has invalid magik {} (could be {})",
                     act.magick,
                     CLI_HELLO_MAGICK);
            return;
        }

        _clients.emplace(address, client_t{address});
    }

    u16 get_ping_packet_id() {
        return _ping_id++;
    }

    void act(const address_t& address, const a_ping& act, auto&&) {
        auto found = _clients.find(address);
        if (found != _clients.end() && found->second._last_ping_id == act.ping_id) {
            auto& client = found->second;
            client._last_ping_id = 0;
            auto now = std::chrono::steady_clock::now();
            auto time = std::chrono::duration_cast<std::chrono::milliseconds>(now - client._last_send).count();
            client.ping.update(time);
            LOG_UPDATE("Ping: {}ms", client.ping.value());
        }
    }

    void act(const address_t& address, const NetCliActions auto& act, auto&& player_update_callback) {
        auto client_info = get_client_info(address);

        if (client_info)
            player_update_callback(
                act_from_client_t{address, client_info->state.operated_player, act.id}, act);
    }

    void act(const address_t& address, const auto& act, auto&&) {
        LOG_WARN("Unknown action from {}: {}", address, act);
    }

    void act(const address_t& address, const a_player_game_params& act, auto&& player_update_callback) {
        auto found = _clients.find(address);
        if (found == _clients.end()) {
            LOG_WARN("packet dropped: cannot find player with address {}", address);
            return;
        }
        auto& cli = found->second;
        cli.state.operated_player = act.name;
        _on_game_clients.emplace(act.name, address);

        player_update_callback(act_from_client_t{address, cli.state.operated_player, act.id}, act);
    }

    struct client_t {
        act_from_client_t make_act_from_client(u64 packed_id) {
            return act_from_client_t{address, state.operated_player, packed_id};
        }

        address_t         address;
        u16               _last_ping_id = 0;
        avg_counter<long> ping{5, 0, 0};
        client_sync_state state{};

        std::chrono::steady_clock::time_point _last_send = std::chrono::steady_clock::now();
    };

    [[nodiscard]]
    std::optional<client_t> get_client_info(const address_t& address) const {
        auto find = _clients.find(address);
        if (find == _clients.end()) {
            LOG_WARN("packet dropped: cannot find player with address {}", address);
            return {};
        }
        return find->second;
    }

    void work(auto&& player_update_callback) {
        actor_processor.receive(sock, [&](const address_t& address, const packet_t& packet) {
            net_action_dispatch(address, packet, [&](const address_t& address, const auto& act) {
                this->act(address, act, player_update_callback);
            });
        });

        persistent_update(player_update_callback);
    }

    auto get_ping(const player_name_t& player_name) {
        auto& addr = _on_game_clients.at(player_name);
        return _clients.at(addr).ping.value();
    }

    /*
    void notify_game_state_sender(const sf::IpAddress& ip, bool ok) {
        auto found = _clients.find(ip.toInteger());
        if (found == _clients.end()) {
            LOG_ERR("notify_game_state_sender: client with ip {} was not found", ip.toString());
            return;
        }

        auto& state = found->second.state;
        if (state.init_sync != sync_state::send) {
            LOG_ERR("notify_game_state_sender: client init_sync must be in 'send' state");
            return;
        }
        state.init_sync = ok ? sync_state::ok : sync_state::fail;
        if (!ok)
            LOG_ERR("notify_game_state_sender: fail to send init game state to client '{}'",
                    ip.toString());
    }
    */

    struct send_game_state_to_client_t{};

private:
    void persistent_update(auto&& player_update_callback) {
        ping_sender();
        client_state_sender(player_update_callback);
    }

    void ping_sender() {
        for (auto& [addr, client] : _clients) {
            auto now = std::chrono::steady_clock::now();
            if (now - client._last_send > 100ms && client._last_ping_id == 0) {
                client._last_send = now;
                client._last_ping_id = get_ping_packet_id();
                a_ping ping;
                ping.ping_id = client._last_ping_id;
                ping.ping_ms = u16(client.ping.value());
                sock.send_somehow(addr, ping);
            }
        }
    }

    void client_state_sender(auto&& player_update_callback) {
        for (auto& [_, client] : _clients) {
            if (!client.state.init_sended) {
                player_update_callback(client.make_act_from_client(0),
                                       send_game_state_to_client_t{});
                client.state.init_sended = true;
            }
        }
    }

private:
    easysocket sock;

    std::map<address_t, client_t> _clients;
    std::map<player_name_t, address_t> _on_game_clients;
    net_actor_processor                actor_processor;

    u16 _ping_id = 0;
};

}

