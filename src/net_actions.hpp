#pragma once

#include "types.hpp"
#include "net_basic.hpp"
#include "fixed_string.hpp"
#include "color.hpp"
#include "vec2.hpp"

#define SETUP_COUNTER() \
    namespace details { \
        static constexpr inline u32 _DFDH_START_COUNTER = __COUNTER__; \
        template <u32 ActionId> \
        struct action_dispatcher; \
    }

#define SETUP_ACTIONS_END() \
    namespace details { static constexpr inline u32 _DFDH_END_COUNTER = __COUNTER__; } \
    static constexpr inline u32 ACTIONS_COUNT = details::_DFDH_END_COUNTER - details::_DFDH_START_COUNTER - 1;

#define def_action(action_name, ...) \
    struct a_##action_name : net_spec { \
        static constexpr inline u32 ACTION = __COUNTER__ - details::_DFDH_START_COUNTER - 1; \
        a_##action_name(): net_spec(ACTION) {} \
        void serialize(auto& _s) const; void deserialize(auto& _d); \
        __VA_ARGS__ \
    }; \
    [[maybe_unused]] static constexpr inline u32 act_##action_name = a_##action_name::ACTION

#define def_action_derived(action_name, BASE, ...) \
    struct a_##action_name : net_spec, BASE { \
        static constexpr inline u32 ACTION = __COUNTER__ - details::_DFDH_START_COUNTER - 1; \
        a_##action_name(): net_spec(ACTION) {} \
        using BASE::operator=; \
        using net_spec::operator=; \
        void serialize(auto& _s) const; void deserialize(auto& _d); \
        __VA_ARGS__ \
    }; \
    [[maybe_unused]] static constexpr inline u32 act_##action_name = a_##action_name::ACTION


#define def_type(type_name, ...) \
    struct type_name##_t { \
        void serialize(auto& _s) const; void deserialize(auto& _d); \
        __VA_ARGS__ \
    } \

#define def_type_derived(type_name, BASE, ...) \
    struct type_name##_t : BASE { \
        using BASE::operator=; \
        void serialize(auto& _s) const; void deserialize(auto& _d); \
        __VA_ARGS__ \
    } \

#define action_serialize(...) \
    DFDH_SERIALIZE_SUPER(net_spec, __VA_ARGS__)

namespace dfdh {

struct net_spec {
    static u64 gen_id() {
        static u64 idgen = 0;
        return idgen++;
    }

    DFDH_SERIALIZE(action, transcontrol, id, hash)

    net_spec(u32 net_action): action(net_action) {}
    net_spec() = default;

    u32 action;
    u32 transcontrol = 0;
    u64 id = gen_id();
    u64 hash = 0;

    [[nodiscard]]
    bool is_valid_action() const;
};

template <>
struct printer<net_spec> {
    void operator()(std::ostream& os, const net_spec& spec) {
        os << "action: " << spec.action
           << "  transcontrol: " << spec.transcontrol
           << "  id: " << spec.id
           << "  hash: " << spec.hash;
    }
};

inline constexpr auto CLI_HELLO_MAGICK = 0xdeadbeeffeedf00d;

SETUP_COUNTER()

def_action(transcontrol_ok,
    u64 target_id;
    u64 target_hash;
);

def_action(transcontrol_corrupted,
    u64 target_id;
    u64 target_hash;
);

def_action(ping,
    u16 ping_id;
    u16 ping_ms;
);

def_action(cli_i_wanna_play,
    u64 magick = CLI_HELLO_MAGICK;
);

def_type(player_skin_params,
    fixed_str<23> body_txtr;
    fixed_str<23> face_txtr;
    rgba_t        body_color;
    rgba_t        tracer_color;
);
def_action_derived(player_skin_params, player_skin_params_t,);

def_type(player_move_states,
    bool mov_left  = false;
    bool mov_right = false;
    bool on_shot   = false;
    bool jump      = false;
    bool jump_down = false;
    bool lock_y    = false;
);
def_action_derived(player_move_states, player_move_states_t,);

[[nodiscard]] inline constexpr bool operator==(const player_move_states_t& lhs,
                                               const player_move_states_t& rhs) {
    return lhs.mov_left == rhs.mov_left && lhs.mov_right == rhs.mov_right &&
           lhs.on_shot == rhs.on_shot && lhs.jump == rhs.jump && lhs.jump_down == rhs.jump_down &&
           lhs.lock_y == rhs.lock_y;
}

def_type(player_game_params,
    player_name_t name;
    i32           group = 0;
);
def_action_derived(player_game_params, player_game_params_t,);

def_type_derived(cli_player_sync, player_move_states_t,
    u64   evt_counter;
    vec2f position;
    vec2f velocity;
);
def_action_derived(cli_player_sync, cli_player_sync_t,);

def_type_derived(srv_player_sync, cli_player_sync_t,
    player_name_t name;
);
def_action_derived(srv_player_sync, srv_player_sync_t,);

def_action_derived(srv_player_game_sync, srv_player_sync_t,
    fixed_str<23>  wpn_name;
    u32            ammo_elapsed;
    i32            group;
    bool           on_left;
);

def_type(bullet_data,
    vec2f position;
    vec2f velocity;
);
def_action(spawn_bullet,
    player_name_t              shooter;
    float                      mass;
    std::vector<bullet_data_t> bullets;
);

def_action(level_sync,
    fixed_str<23> level_name;
    float         game_speed;
    bool          on_game;
);

def_action(player_conf,
    player_skin_params_t skin;
    player_game_params_t game;
    fixed_str<23> pistol;
);

SETUP_ACTIONS_END()


template <typename T>
concept NetAction = std::derived_from<T, net_spec>;

template <typename T>
concept NetCliActions = AnyOfType<T, a_player_skin_params, a_cli_player_sync, a_spawn_bullet>;

packet_t net_action_to_packet(const NetAction auto& action) {
    packet_t packet;
    packet.append(action);
    return packet;
}

inline bool net_spec::is_valid_action() const {
    return action < ACTIONS_COUNT;
}

} // namespace dfdh

#undef SETUP_COUNTER
#undef SETUP_ACTIONS_END
#undef def_action
#undef def_action_derived
#undef def_type
#undef def_type_derived
#undef action_serialize

