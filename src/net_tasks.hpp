#pragma once

#include "net_basic.hpp"
#include "base/tasks.hpp"
#include "net_actions_generated.hpp"

namespace dfdh
{
template <typename... Ts>
bool accept_any_of(const packet_t& packet) {
    return net_action_dispatch({}, packet, overloaded_typecheck<Ts...>{}) == net_action_dispatch_rc::ok;
}

using worker_t = task_worker<address_t, packet_t>;

struct net_task_base {
    net_task_base(worker_t* worker): w(worker) {}

    template <typename T>
    task<T> for_action(const address_t& address) const {
        auto result = co_await w->for_event(address, accept_any_of<T>);
        co_return result.template cast_to<T>();
    }

    template <typename T>
    task<void> for_action_discard(const address_t& address) const {
        auto result = co_await w->for_event(address, accept_any_of<T>);
    }

    worker_t* w;
};

} // namespace dfdh
