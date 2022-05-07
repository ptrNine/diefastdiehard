#pragma once

#include "usertypes.hpp"
#include <luaffi/luacpp_ctx.hpp>

namespace dfdh
{
inline void lua_timer_init(luacpp::luactx& ctx) {
    ctx.annotate({.comment = "creates timer"});
    ctx.annotate({.comment = "makes timer copy", .argument_names = {"tm"}});
    ctx.provide(
        LUA_TNAME("timer.new"),
        []() { return lua_timer_t(std::chrono::steady_clock::now()); },
        [](const lua_timer_t& t) { return t; });

    ctx.annotate({.comment = "get elapsed time in seconds"});
    ctx.provide_member<lua_timer_t>(LUA_TNAME("elapsed"), [](const lua_timer_t& t) {
        auto tp = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double>>(tp - t).count();
    });

    ctx.annotate({.comment = "resets the timer"});
    ctx.provide_member<lua_timer_t>(LUA_TNAME("restart"), [](lua_timer_t& t) {
        t = std::chrono::steady_clock::now();
    });

    ctx.annotate({.comment = "get elapsed time in seconds and resets the timer"});
    ctx.provide_member<lua_timer_t>(LUA_TNAME("tick"), [](lua_timer_t& t) {
        auto tp  = std::chrono::steady_clock::now();
        auto res = std::chrono::duration_cast<std::chrono::duration<double>>(tp - t).count();
        t = tp;
        return res;
    });
}
} // namespace dfdh
