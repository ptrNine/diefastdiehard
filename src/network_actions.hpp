#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Network/IpAddress.hpp>

#include "packet.hpp"
#include "fixed_string.hpp"

namespace dfdh {

inline constexpr auto CLI_HELLO_MAGICK = 0xdeadbeeffeedf00d;

enum net_act : u32 {
    act_transcontrol_ok = 0,
    act_transcontrol_corrupted,
    act_cli_i_wanna_play,
    act_cli_player_params,
    act_cli_player_sync,
    act_cli_load_player,

    act_srv_ping,
    act_srv_player_states,
    act_srv_player_physic_sync,

    act_spawn_bullet,
    act_level_sync,
    act_player_game_conf_sync,

    NET_ACT_COUNT
};

inline bool validate_spec_act(const packet_spec_t& spec) {
    if (spec.act >= NET_ACT_COUNT) {
        LOG_WARN("packet dropped: invalid action {}", u64(spec.act));
        return false;
    }
    return true;
}

inline std::optional<packet_spec_t> get_spec_if_valid_packet_action(const packet_t& packet) {
    if (auto spec = get_spec_if_valid_packet(packet)) {
        if (!validate_spec_act(*spec))
            return {};
        else
            return spec;
    }
    return {};
}


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

    bool mov_left  = false;
    bool mov_right = false;
    bool on_shot   = false;
    bool jump      = false;
    bool jump_down = false;
    bool lock_y    = false;

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

struct a_cli_load_player {
    player_name_t player_name;
    i32           group;

    net_act _act    = act_cli_load_player;
};

struct a_srv_player_states {
    player_states_t st;
    player_name_t   name;
    u64             evt_counter;
    sf::Vector2f    position;
    sf::Vector2f    velocity;

    u32     _dummy0 = 0;
    net_act _act    = act_srv_player_states;
};

struct a_srv_player_physic_sync {
    player_name_t   name;
    sf::Vector2f    position;
    sf::Vector2f    velocity;
    player_states_t st;
    fixed_str<23>   cur_wpn_name;
//    u64             random_pool_pos;
    u64             evt_counter;
    u32             ammo_elapsed;
    i32             group;
    bool            on_left;

    u8      _dummy0[3] = {0};
    net_act _act       = act_srv_player_physic_sync;
};

struct a_spawn_bullet {
    struct bullet_data_t {
        sf::Vector2f _position;
        sf::Vector2f _velocity;
    };
    player_name_t              name;
    float                      mass;
    std::vector<bullet_data_t> bullets;

    net_act _act    = act_spawn_bullet;
};

struct a_level_sync {
    fixed_str<23> level_name;
    float         game_speed;
    bool          on_game;
    u8            _dummy0[3] = {0};

    u32     _dummy1 = 0;
    net_act _act = act_level_sync;
};

struct a_player_game_conf_sync {
    player_name_t  player_name;
    fixed_str<23>  pistol;
    fixed_str<23>  body_txtr;
    fixed_str<23>  face_txtr;
    sf::Color      body_color;
    sf::Color      tracer_color;
    i32            group;

    net_act _act   = act_player_game_conf_sync;
};

template <typename T>
concept PlayerActs = AnyOfType<T, a_cli_player_params, a_cli_player_sync, a_spawn_bullet>;

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

template <typename T>
T action_cast(const packet_t& packet) {
    T action;
    std::memcpy(&action, packet.data(), packet.size() - sizeof(packet_spec_t));
    return action;
}


packet_t create_packet(const a_spawn_bullet& act, u32 transcontrol = 0, u64* id = nullptr) {
    packet_t packet;
    u64 packet_id = next_packet_id();

    u64 sz = act.bullets.size();
    packet.append(act.name, act.mass);
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

template <>
a_spawn_bullet action_cast(const packet_t& packet) {
    a_spawn_bullet act;
    size_t offset = 0;
    std::memcpy(&act.name, packet.data() + offset, sizeof(act.name));
    offset += sizeof(act.name);
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
void action_dispatch(const sf::IpAddress& ip, u16 port, u32 act, u64 id, const packet_t& packet, F overloaded) {

    switch (static_cast<net_act>(act)) {
        overload(cli_i_wanna_play);
        overload(cli_player_params);
        overload(cli_player_sync);
        overload(cli_load_player);
        overload(srv_ping);
        overload(srv_player_states);
        overload(srv_player_physic_sync);
        overload(spawn_bullet);
        overload(level_sync);
        overload(player_game_conf_sync);
    default: break;
    }
}

#undef overload
}
