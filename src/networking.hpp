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

struct act_from_client_t {
    sf::IpAddress ip;
    u16           port;
    player_name_t operated_player;
    u64           packet_id;
};

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

        sock.send(found->second.ip(), found->second.port, act, transcontrol_handler);
    }

    template <typename T, typename F = bool>
    void send(const sf::IpAddress& ip, u16 port, const T& act, F transcontrol_handler = false) {
        auto found = _clients.find({ip, port});
        if (found == _clients.end())
            return;

        sock.send(found->second.ip, found->second.port, act, transcontrol_handler);
    }

    template <typename T, typename F = bool>
    void send_to_all(const T& act, F transcontrol_handler = false) {
        for (auto& [addr, client] : _clients)
            sock.send(addr.ip(), addr.port, act, transcontrol_handler);
    }

    template <typename T>
    void send_to_all_except_client_player(const player_name_t& client_player_name, const T& act) {
        for (auto& [addr, client] : _clients)
            if (client.state.operated_player != client_player_name)
                sock.send(addr.ip(), addr.port, act);
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

        _clients.emplace(client_address_t{ip, port}, client_t{ip, port});
    }

    u16 get_ping_packet_id() {
        return _ping_id++;
    }

    template <typename F>
    void act(const sf::IpAddress& ip, u16 port, u64, const a_srv_ping& act, F&&) {
        auto found = _clients.find({ip, port});
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
    void act(const sf::IpAddress& ip, u16 port, u64 packet_id, const PlayerActs auto& act, F&& player_update_callback) {
        auto client_info = get_client_info(ip, port);

        if (client_info)
            player_update_callback(
                act_from_client_t{ip, port, client_info->state.operated_player, packet_id}, act);
    }

    template <typename F>
    void act(const sf::IpAddress&     ip,
             u16                      port,
             u64                      packet_id,
             const a_cli_load_player& act,
             F&&                      player_update_callback) {
        auto found = _clients.find({ip, port});
        if (found == _clients.end()) {
            LOG_WARN("packet dropped: cannot find player with ip {}", ip.toString());
            return;
        }
        auto& cli = found->second;
        cli.state.operated_player = act.player_name;
        _on_game_clients.emplace(act.player_name, client_address_t{ip, port});

        player_update_callback(act_from_client_t{ip, port, cli.state.operated_player, packet_id}, act);
    }

    template <typename T, typename F> requires (!PlayerActs<T>)
    void act(const sf::IpAddress&, u16, u64, const T&, F&&) {
        LOG_WARN("server action: invalid overload");
    }

    struct client_t {
        act_from_client_t make_act_from_client(u64 packed_id) {
            return act_from_client_t{ip, port, state.operated_player, packed_id};
        }

        sf::IpAddress     ip;
        u16               port;
        u16               _last_ping_id = 0;
        avg_counter<long> ping{5, 0, 0};
        client_sync_state state{};

        std::chrono::steady_clock::time_point _last_send = std::chrono::steady_clock::now();
    };

    struct client_address_t {
        client_address_t(u32 iraw_ip, u16 iport): raw_ip(iraw_ip), port(iport) {}
        client_address_t(const sf::IpAddress& i_ip, u16 iport):
            raw_ip(i_ip.toInteger()), port(iport) {}

        u32 raw_ip;
        u16 port;
        u16 _dummy0 = 0;

        [[nodiscard]]
        sf::IpAddress ip() const {
            return sf::IpAddress(raw_ip);
        }

        [[nodiscard]]
        bool operator<(const client_address_t& addr) const {
            u64 lhs, rhs;
            std::memcpy(&lhs, this, sizeof(lhs));
            std::memcpy(&rhs, &addr, sizeof(rhs));

            return lhs < rhs;
        }
    };

    [[nodiscard]]
    std::optional<client_t> get_client_info(const sf::IpAddress& ip, u16 port) const {
        auto find = _clients.find({ip, port});
        if (find == _clients.end()) {
            LOG_WARN("packet dropped: cannot find player with ip {}", ip.toString());
            return {};
        }
        return find->second;
    }

    void work(auto&& player_update_callback) {
        sock.receiver([&, this](const sf::IpAddress& ip, u16 port, u64 id, auto&& act) {
            this->act(ip, port, id, act, player_update_callback);
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
                sock.send(addr.ip(),
                          addr.port,
                          a_srv_ping{client._last_ping_id, static_cast<u16>(client.ping.value())});
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
    act_socket sock;

    std::map<client_address_t, client_t> _clients;
    std::map<player_name_t, client_address_t> _on_game_clients;

    u16 _ping_id = 0;
};

}

