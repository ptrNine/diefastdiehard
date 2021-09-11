/* Generated with net_actions_generator.cpp
 * Command: g++ -E src/net_actions.hpp | net_actions_generator > src/net_action_generated.hpp
 */

#pragma once

#include <stdexcept>
#include "net_actions.hpp"
#include "print.hpp"
#include "hash_functions.hpp"

namespace dfdh {

class unknown_net_action : public std::invalid_argument {
public:
    unknown_net_action(u32 act): std::invalid_argument("Unknown action with id " + std::to_string(act)) {}
};

inline bool net_action_dispatch(const address_t& address, const packet_t& packet, auto&& overloaded) {
    auto act = packet.cast_to<u32>();
    switch (act) {
    case a_cli_i_wanna_play::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_cli_i_wanna_play>) {
            overloaded(packet.cast_to<a_cli_i_wanna_play>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_cli_i_wanna_play>) {
            overloaded(address, packet.cast_to<a_cli_i_wanna_play>());
            return true;
        }
        break;
    case a_cli_player_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_cli_player_sync>) {
            overloaded(packet.cast_to<a_cli_player_sync>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_cli_player_sync>) {
            overloaded(address, packet.cast_to<a_cli_player_sync>());
            return true;
        }
        break;
    case a_level_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_level_sync>) {
            overloaded(packet.cast_to<a_level_sync>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_level_sync>) {
            overloaded(address, packet.cast_to<a_level_sync>());
            return true;
        }
        break;
    case a_ping::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_ping>) {
            overloaded(packet.cast_to<a_ping>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_ping>) {
            overloaded(address, packet.cast_to<a_ping>());
            return true;
        }
        break;
    case a_player_conf::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_player_conf>) {
            overloaded(packet.cast_to<a_player_conf>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_player_conf>) {
            overloaded(address, packet.cast_to<a_player_conf>());
            return true;
        }
        break;
    case a_player_game_params::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_player_game_params>) {
            overloaded(packet.cast_to<a_player_game_params>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_player_game_params>) {
            overloaded(address, packet.cast_to<a_player_game_params>());
            return true;
        }
        break;
    case a_player_move_states::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_player_move_states>) {
            overloaded(packet.cast_to<a_player_move_states>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_player_move_states>) {
            overloaded(address, packet.cast_to<a_player_move_states>());
            return true;
        }
        break;
    case a_player_skin_params::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_player_skin_params>) {
            overloaded(packet.cast_to<a_player_skin_params>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_player_skin_params>) {
            overloaded(address, packet.cast_to<a_player_skin_params>());
            return true;
        }
        break;
    case a_spawn_bullet::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_spawn_bullet>) {
            overloaded(packet.cast_to<a_spawn_bullet>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_spawn_bullet>) {
            overloaded(address, packet.cast_to<a_spawn_bullet>());
            return true;
        }
        break;
    case a_srv_player_game_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_srv_player_game_sync>) {
            overloaded(packet.cast_to<a_srv_player_game_sync>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_srv_player_game_sync>) {
            overloaded(address, packet.cast_to<a_srv_player_game_sync>());
            return true;
        }
        break;
    case a_srv_player_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_srv_player_sync>) {
            overloaded(packet.cast_to<a_srv_player_sync>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_srv_player_sync>) {
            overloaded(address, packet.cast_to<a_srv_player_sync>());
            return true;
        }
        break;
    case a_transcontrol_corrupted::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_transcontrol_corrupted>) {
            overloaded(packet.cast_to<a_transcontrol_corrupted>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_transcontrol_corrupted>) {
            overloaded(address, packet.cast_to<a_transcontrol_corrupted>());
            return true;
        }
        break;
    case a_transcontrol_ok::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_transcontrol_ok>) {
            overloaded(packet.cast_to<a_transcontrol_ok>());
            return true;
        }
        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_transcontrol_ok>) {
            overloaded(address, packet.cast_to<a_transcontrol_ok>());
            return true;
        }
        break;
    default:
        throw unknown_net_action(act);
    }
    return false;
}


inline bool net_action_downcast(const net_spec& action_base, auto&& overloaded) {
    switch (action_base.action) {
    case a_cli_i_wanna_play::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_cli_i_wanna_play>) {
            overloaded(static_cast<const a_cli_i_wanna_play&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_cli_player_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_cli_player_sync>) {
            overloaded(static_cast<const a_cli_player_sync&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_level_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_level_sync>) {
            overloaded(static_cast<const a_level_sync&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_ping::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_ping>) {
            overloaded(static_cast<const a_ping&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_player_conf::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_player_conf>) {
            overloaded(static_cast<const a_player_conf&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_player_game_params::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_player_game_params>) {
            overloaded(static_cast<const a_player_game_params&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_player_move_states::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_player_move_states>) {
            overloaded(static_cast<const a_player_move_states&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_player_skin_params::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_player_skin_params>) {
            overloaded(static_cast<const a_player_skin_params&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_spawn_bullet::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_spawn_bullet>) {
            overloaded(static_cast<const a_spawn_bullet&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_srv_player_game_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_srv_player_game_sync>) {
            overloaded(static_cast<const a_srv_player_game_sync&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_srv_player_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_srv_player_sync>) {
            overloaded(static_cast<const a_srv_player_sync&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_transcontrol_corrupted::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_transcontrol_corrupted>) {
            overloaded(static_cast<const a_transcontrol_corrupted&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_transcontrol_ok::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_transcontrol_ok>) {
            overloaded(static_cast<const a_transcontrol_ok&>(action_base)); // NOLINT
            return true;
        }
        break;
    default:
        throw unknown_net_action(action_base.action);
    }
    return false;
}


inline bool net_action_downcast(const address_t& address, const net_spec& action_base, auto&& overloaded) {
    switch (action_base.action) {
    case a_cli_i_wanna_play::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_cli_i_wanna_play>) {
            overloaded(address, static_cast<const a_cli_i_wanna_play&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_cli_player_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_cli_player_sync>) {
            overloaded(address, static_cast<const a_cli_player_sync&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_level_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_level_sync>) {
            overloaded(address, static_cast<const a_level_sync&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_ping::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_ping>) {
            overloaded(address, static_cast<const a_ping&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_player_conf::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_player_conf>) {
            overloaded(address, static_cast<const a_player_conf&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_player_game_params::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_player_game_params>) {
            overloaded(address, static_cast<const a_player_game_params&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_player_move_states::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_player_move_states>) {
            overloaded(address, static_cast<const a_player_move_states&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_player_skin_params::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_player_skin_params>) {
            overloaded(address, static_cast<const a_player_skin_params&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_spawn_bullet::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_spawn_bullet>) {
            overloaded(address, static_cast<const a_spawn_bullet&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_srv_player_game_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_srv_player_game_sync>) {
            overloaded(address, static_cast<const a_srv_player_game_sync&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_srv_player_sync::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_srv_player_sync>) {
            overloaded(address, static_cast<const a_srv_player_sync&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_transcontrol_corrupted::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_transcontrol_corrupted>) {
            overloaded(address, static_cast<const a_transcontrol_corrupted&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_transcontrol_ok::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_transcontrol_ok>) {
            overloaded(address, static_cast<const a_transcontrol_ok&>(action_base)); // NOLINT
            return true;
        }
        break;
    default:
        throw unknown_net_action(action_base.action);
    }
    return false;
}


#define net_action_object_dispatch(OBJECT, member_function, ADDRESS, PACKET) \
    [](auto& object, const address_t& address, packet_t& packet) { \
        auto act = packet.cast_to<u32>(); \
        switch (act) { \
        case a_cli_i_wanna_play::ACTION: \
            if constexpr (requires{object.member_function(a_cli_i_wanna_play());}) { \
                object.member_function(packet.cast_to<a_cli_i_wanna_play>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_cli_i_wanna_play());}) { \
                object.member_function(address, packet.cast_to<a_cli_i_wanna_play>()); \
                return true; \
            } \
            break; \
        case a_cli_player_sync::ACTION: \
            if constexpr (requires{object.member_function(a_cli_player_sync());}) { \
                object.member_function(packet.cast_to<a_cli_player_sync>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_cli_player_sync());}) { \
                object.member_function(address, packet.cast_to<a_cli_player_sync>()); \
                return true; \
            } \
            break; \
        case a_level_sync::ACTION: \
            if constexpr (requires{object.member_function(a_level_sync());}) { \
                object.member_function(packet.cast_to<a_level_sync>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_level_sync());}) { \
                object.member_function(address, packet.cast_to<a_level_sync>()); \
                return true; \
            } \
            break; \
        case a_ping::ACTION: \
            if constexpr (requires{object.member_function(a_ping());}) { \
                object.member_function(packet.cast_to<a_ping>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_ping());}) { \
                object.member_function(address, packet.cast_to<a_ping>()); \
                return true; \
            } \
            break; \
        case a_player_conf::ACTION: \
            if constexpr (requires{object.member_function(a_player_conf());}) { \
                object.member_function(packet.cast_to<a_player_conf>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_player_conf());}) { \
                object.member_function(address, packet.cast_to<a_player_conf>()); \
                return true; \
            } \
            break; \
        case a_player_game_params::ACTION: \
            if constexpr (requires{object.member_function(a_player_game_params());}) { \
                object.member_function(packet.cast_to<a_player_game_params>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_player_game_params());}) { \
                object.member_function(address, packet.cast_to<a_player_game_params>()); \
                return true; \
            } \
            break; \
        case a_player_move_states::ACTION: \
            if constexpr (requires{object.member_function(a_player_move_states());}) { \
                object.member_function(packet.cast_to<a_player_move_states>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_player_move_states());}) { \
                object.member_function(address, packet.cast_to<a_player_move_states>()); \
                return true; \
            } \
            break; \
        case a_player_skin_params::ACTION: \
            if constexpr (requires{object.member_function(a_player_skin_params());}) { \
                object.member_function(packet.cast_to<a_player_skin_params>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_player_skin_params());}) { \
                object.member_function(address, packet.cast_to<a_player_skin_params>()); \
                return true; \
            } \
            break; \
        case a_spawn_bullet::ACTION: \
            if constexpr (requires{object.member_function(a_spawn_bullet());}) { \
                object.member_function(packet.cast_to<a_spawn_bullet>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_spawn_bullet());}) { \
                object.member_function(address, packet.cast_to<a_spawn_bullet>()); \
                return true; \
            } \
            break; \
        case a_srv_player_game_sync::ACTION: \
            if constexpr (requires{object.member_function(a_srv_player_game_sync());}) { \
                object.member_function(packet.cast_to<a_srv_player_game_sync>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_srv_player_game_sync());}) { \
                object.member_function(address, packet.cast_to<a_srv_player_game_sync>()); \
                return true; \
            } \
            break; \
        case a_srv_player_sync::ACTION: \
            if constexpr (requires{object.member_function(a_srv_player_sync());}) { \
                object.member_function(packet.cast_to<a_srv_player_sync>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_srv_player_sync());}) { \
                object.member_function(address, packet.cast_to<a_srv_player_sync>()); \
                return true; \
            } \
            break; \
        case a_transcontrol_corrupted::ACTION: \
            if constexpr (requires{object.member_function(a_transcontrol_corrupted());}) { \
                object.member_function(packet.cast_to<a_transcontrol_corrupted>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_transcontrol_corrupted());}) { \
                object.member_function(address, packet.cast_to<a_transcontrol_corrupted>()); \
                return true; \
            } \
            break; \
        case a_transcontrol_ok::ACTION: \
            if constexpr (requires{object.member_function(a_transcontrol_ok());}) { \
                object.member_function(packet.cast_to<a_transcontrol_ok>()); \
                return true; \
            } \
            else if constexpr (requires{object.member_function(address_t(), a_transcontrol_ok());}) { \
                object.member_function(address, packet.cast_to<a_transcontrol_ok>()); \
                return true; \
            } \
            break; \
        default: \
            throw unknown_net_action(act); \
        } \
        return false; \
    }(OBJECT, ADDRESS, PACKET)


inline void action_serialize_setup_hash(auto& _s) {
    auto hash = fnv1a64(_s.data() + sizeof(net_spec), _s.size() - sizeof(net_spec));
    if constexpr (std::endian::native == std::endian::big)
        hash = bswap(hash);
    constexpr auto hash_pos = sizeof(net_spec) - sizeof(u64);
    ::memcpy(_s.data() + hash_pos, &hash, sizeof(hash));
}

inline void a_cli_i_wanna_play::serialize(auto& _s) const {
    net_spec::serialize(_s);
    serialize_all(_s, magick);
    action_serialize_setup_hash(_s);
}
inline void a_cli_i_wanna_play::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    deserialize_all(_d, magick);
}
template <>
struct printer<a_cli_i_wanna_play> {
    void operator()(std::ostream& os, const a_cli_i_wanna_play& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os,
               "  magick: ", act.magick,
               "");
    }
};

inline void a_cli_player_sync::serialize(auto& _s) const {
    net_spec::serialize(_s);
    cli_player_sync_t::serialize(_s);
    action_serialize_setup_hash(_s);
}
inline void a_cli_player_sync::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    cli_player_sync_t::deserialize(_d);
}
template <>
struct printer<a_cli_player_sync> {
    void operator()(std::ostream& os, const a_cli_player_sync& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os, static_cast<const cli_player_sync_t&>(act));
    }
};

inline void a_level_sync::serialize(auto& _s) const {
    net_spec::serialize(_s);
    serialize_all(_s, level_name, game_speed, on_game);
    action_serialize_setup_hash(_s);
}
inline void a_level_sync::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    deserialize_all(_d, level_name, game_speed, on_game);
}
template <>
struct printer<a_level_sync> {
    void operator()(std::ostream& os, const a_level_sync& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os,
               "  level_name: ", act.level_name,
               "  game_speed: ", act.game_speed,
               "  on_game: ", act.on_game,
               "");
    }
};

inline void a_ping::serialize(auto& _s) const {
    net_spec::serialize(_s);
    serialize_all(_s, ping_id, ping_ms);
    action_serialize_setup_hash(_s);
}
inline void a_ping::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    deserialize_all(_d, ping_id, ping_ms);
}
template <>
struct printer<a_ping> {
    void operator()(std::ostream& os, const a_ping& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os,
               "  ping_id: ", act.ping_id,
               "  ping_ms: ", act.ping_ms,
               "");
    }
};

inline void a_player_conf::serialize(auto& _s) const {
    net_spec::serialize(_s);
    serialize_all(_s, skin, game, pistol);
    action_serialize_setup_hash(_s);
}
inline void a_player_conf::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    deserialize_all(_d, skin, game, pistol);
}
template <>
struct printer<a_player_conf> {
    void operator()(std::ostream& os, const a_player_conf& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os,
               "  skin: ", act.skin,
               "  game: ", act.game,
               "  pistol: ", act.pistol,
               "");
    }
};

inline void a_player_game_params::serialize(auto& _s) const {
    net_spec::serialize(_s);
    player_game_params_t::serialize(_s);
    action_serialize_setup_hash(_s);
}
inline void a_player_game_params::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    player_game_params_t::deserialize(_d);
}
template <>
struct printer<a_player_game_params> {
    void operator()(std::ostream& os, const a_player_game_params& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os, static_cast<const player_game_params_t&>(act));
    }
};

inline void a_player_move_states::serialize(auto& _s) const {
    net_spec::serialize(_s);
    player_move_states_t::serialize(_s);
    action_serialize_setup_hash(_s);
}
inline void a_player_move_states::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    player_move_states_t::deserialize(_d);
}
template <>
struct printer<a_player_move_states> {
    void operator()(std::ostream& os, const a_player_move_states& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os, static_cast<const player_move_states_t&>(act));
    }
};

inline void a_player_skin_params::serialize(auto& _s) const {
    net_spec::serialize(_s);
    player_skin_params_t::serialize(_s);
    action_serialize_setup_hash(_s);
}
inline void a_player_skin_params::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    player_skin_params_t::deserialize(_d);
}
template <>
struct printer<a_player_skin_params> {
    void operator()(std::ostream& os, const a_player_skin_params& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os, static_cast<const player_skin_params_t&>(act));
    }
};

inline void a_spawn_bullet::serialize(auto& _s) const {
    net_spec::serialize(_s);
    serialize_all(_s, shooter, mass, bullets);
    action_serialize_setup_hash(_s);
}
inline void a_spawn_bullet::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    deserialize_all(_d, shooter, mass, bullets);
}
template <>
struct printer<a_spawn_bullet> {
    void operator()(std::ostream& os, const a_spawn_bullet& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os,
               "  shooter: ", act.shooter,
               "  mass: ", act.mass,
               "  bullets: ", act.bullets,
               "");
    }
};

inline void a_srv_player_game_sync::serialize(auto& _s) const {
    net_spec::serialize(_s);
    srv_player_sync_t::serialize(_s);
    serialize_all(_s, wpn_name, ammo_elapsed, group, on_left);
    action_serialize_setup_hash(_s);
}
inline void a_srv_player_game_sync::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    srv_player_sync_t::deserialize(_d);
    deserialize_all(_d, wpn_name, ammo_elapsed, group, on_left);
}
template <>
struct printer<a_srv_player_game_sync> {
    void operator()(std::ostream& os, const a_srv_player_game_sync& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os, static_cast<const srv_player_sync_t&>(act));
        fprint(os,
               "  wpn_name: ", act.wpn_name,
               "  ammo_elapsed: ", act.ammo_elapsed,
               "  group: ", act.group,
               "  on_left: ", act.on_left,
               "");
    }
};

inline void a_srv_player_sync::serialize(auto& _s) const {
    net_spec::serialize(_s);
    srv_player_sync_t::serialize(_s);
    action_serialize_setup_hash(_s);
}
inline void a_srv_player_sync::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    srv_player_sync_t::deserialize(_d);
}
template <>
struct printer<a_srv_player_sync> {
    void operator()(std::ostream& os, const a_srv_player_sync& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os, static_cast<const srv_player_sync_t&>(act));
    }
};

inline void a_transcontrol_corrupted::serialize(auto& _s) const {
    net_spec::serialize(_s);
    serialize_all(_s, target_id, target_hash);
    action_serialize_setup_hash(_s);
}
inline void a_transcontrol_corrupted::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    deserialize_all(_d, target_id, target_hash);
}
template <>
struct printer<a_transcontrol_corrupted> {
    void operator()(std::ostream& os, const a_transcontrol_corrupted& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os,
               "  target_id: ", act.target_id,
               "  target_hash: ", act.target_hash,
               "");
    }
};

inline void a_transcontrol_ok::serialize(auto& _s) const {
    net_spec::serialize(_s);
    serialize_all(_s, target_id, target_hash);
    action_serialize_setup_hash(_s);
}
inline void a_transcontrol_ok::deserialize(auto& _d) {
    net_spec::deserialize(_d);
    deserialize_all(_d, target_id, target_hash);
}
template <>
struct printer<a_transcontrol_ok> {
    void operator()(std::ostream& os, const a_transcontrol_ok& act) const {
        fprint(os, static_cast<const net_spec&>(act));
        fprint(os,
               "  target_id: ", act.target_id,
               "  target_hash: ", act.target_hash,
               "");
    }
};

inline void bullet_data_t::serialize(auto& _s) const {
    serialize_all(_s, position, velocity);
}
inline void bullet_data_t::deserialize(auto& _d) {
    deserialize_all(_d, position, velocity);
}
template <>
struct printer<bullet_data_t> {
    void operator()(std::ostream& os, const bullet_data_t& act) const {
        fprint(os,
               "  position: ", act.position,
               "  velocity: ", act.velocity,
               "");
    }
};

inline void cli_player_sync_t::serialize(auto& _s) const {
    player_move_states_t::serialize(_s);
    serialize_all(_s, evt_counter, position, velocity);
}
inline void cli_player_sync_t::deserialize(auto& _d) {
    player_move_states_t::deserialize(_d);
    deserialize_all(_d, evt_counter, position, velocity);
}
template <>
struct printer<cli_player_sync_t> {
    void operator()(std::ostream& os, const cli_player_sync_t& act) const {
        fprint(os, static_cast<const player_move_states_t&>(act));
        fprint(os,
               "  evt_counter: ", act.evt_counter,
               "  position: ", act.position,
               "  velocity: ", act.velocity,
               "");
    }
};

inline void player_game_params_t::serialize(auto& _s) const {
    serialize_all(_s, name, group);
}
inline void player_game_params_t::deserialize(auto& _d) {
    deserialize_all(_d, name, group);
}
template <>
struct printer<player_game_params_t> {
    void operator()(std::ostream& os, const player_game_params_t& act) const {
        fprint(os,
               "  name: ", act.name,
               "  group: ", act.group,
               "");
    }
};

inline void player_move_states_t::serialize(auto& _s) const {
    serialize_all(_s, mov_left, mov_right, on_shot, jump, jump_down, lock_y);
}
inline void player_move_states_t::deserialize(auto& _d) {
    deserialize_all(_d, mov_left, mov_right, on_shot, jump, jump_down, lock_y);
}
template <>
struct printer<player_move_states_t> {
    void operator()(std::ostream& os, const player_move_states_t& act) const {
        fprint(os,
               "  mov_left: ", act.mov_left,
               "  mov_right: ", act.mov_right,
               "  on_shot: ", act.on_shot,
               "  jump: ", act.jump,
               "  jump_down: ", act.jump_down,
               "  lock_y: ", act.lock_y,
               "");
    }
};

inline void player_skin_params_t::serialize(auto& _s) const {
    serialize_all(_s, body_txtr, face_txtr, body_color, tracer_color);
}
inline void player_skin_params_t::deserialize(auto& _d) {
    deserialize_all(_d, body_txtr, face_txtr, body_color, tracer_color);
}
template <>
struct printer<player_skin_params_t> {
    void operator()(std::ostream& os, const player_skin_params_t& act) const {
        fprint(os,
               "  body_txtr: ", act.body_txtr,
               "  face_txtr: ", act.face_txtr,
               "  body_color: ", act.body_color,
               "  tracer_color: ", act.tracer_color,
               "");
    }
};

inline void srv_player_sync_t::serialize(auto& _s) const {
    cli_player_sync_t::serialize(_s);
    serialize_all(_s, name);
}
inline void srv_player_sync_t::deserialize(auto& _d) {
    cli_player_sync_t::deserialize(_d);
    deserialize_all(_d, name);
}
template <>
struct printer<srv_player_sync_t> {
    void operator()(std::ostream& os, const srv_player_sync_t& act) const {
        fprint(os, static_cast<const cli_player_sync_t&>(act));
        fprint(os,
               "  name: ", act.name,
               "");
    }
};

} // namespace dfdh
