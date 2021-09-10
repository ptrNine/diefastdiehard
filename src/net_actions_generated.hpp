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
    default:
        throw unknown_net_action(act);
    }
    return false;
}


inline bool net_action_downcast(const net_spec& action_base, auto&& overloaded) {
    switch (action_base.action) {
    case a_transcontrol_ok::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_transcontrol_ok>) {
            overloaded(static_cast<const a_transcontrol_ok&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_transcontrol_corrupted::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_transcontrol_corrupted>) {
            overloaded(static_cast<const a_transcontrol_corrupted&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_ping::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_ping>) {
            overloaded(static_cast<const a_ping&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_cli_i_wanna_play::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), a_cli_i_wanna_play>) {
            overloaded(static_cast<const a_cli_i_wanna_play&>(action_base)); // NOLINT
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
    case a_transcontrol_ok::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_transcontrol_ok>) {
            overloaded(address, static_cast<const a_transcontrol_ok&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_transcontrol_corrupted::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_transcontrol_corrupted>) {
            overloaded(address, static_cast<const a_transcontrol_corrupted&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_ping::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_ping>) {
            overloaded(address, static_cast<const a_ping&>(action_base)); // NOLINT
            return true;
        }
        break;
    case a_cli_i_wanna_play::ACTION:
        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, a_cli_i_wanna_play>) {
            overloaded(address, static_cast<const a_cli_i_wanna_play&>(action_base)); // NOLINT
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
}
inline void a_cli_i_wanna_play::deserialize(auto& _d) {
    net_spec::deserialize(_d);
}
template <>
struct printer<a_cli_i_wanna_play> {
    void operator()(std::ostream& os, const a_cli_i_wanna_play& act) const {
        os << "action: " << act.action
           << "  transcontrol: " << act.transcontrol
           << "  id: " << act.id
           << "  hash: " << act.hash
           << "";
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
        os << "action: " << act.action
           << "  transcontrol: " << act.transcontrol
           << "  id: " << act.id
           << "  hash: " << act.hash
           << "  ping_id: " << act.ping_id
           << "  ping_ms: " << act.ping_ms
           << "";
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
        os << "action: " << act.action
           << "  transcontrol: " << act.transcontrol
           << "  id: " << act.id
           << "  hash: " << act.hash
           << "  target_id: " << act.target_id
           << "  target_hash: " << act.target_hash
           << "";
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
        os << "action: " << act.action
           << "  transcontrol: " << act.transcontrol
           << "  id: " << act.id
           << "  hash: " << act.hash
           << "  target_id: " << act.target_id
           << "  target_hash: " << act.target_hash
           << "";
    }
};

} // namespace dfdh
