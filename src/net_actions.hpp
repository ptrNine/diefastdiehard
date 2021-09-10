#pragma once

#include "types.hpp"
#include "net_basic.hpp"

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

def_action(cli_i_wanna_play,);


SETUP_ACTIONS_END()


template <typename T>
concept NetAction = std::derived_from<T, net_spec>;

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
#undef action_serialize

