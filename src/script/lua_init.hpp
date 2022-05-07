#pragma once

#include "lua.hpp"

#include "typeinit/vec2f_init.hpp"
#include "typeinit/game_state_init.hpp"
#include "typeinit/timer_init.hpp"
#include "typeinit/log_init.hpp"
#include "typeinit/cfg_init.hpp"
#include "typeinit/ai_init.hpp"
#include "typeinit/sfml_event_init.hpp"

namespace dfdh
{
inline void lua_ai_instance_init(luacpp::luactx& ctx) {
    lua_vec2f_init(ctx);
    lua_log_init(ctx);
    lua_timer_init(ctx);
    lua_ai_init(ctx);
}

inline void lua_global_instance_init(luacpp::luactx& ctx) {
    lua_vec2f_init(ctx);
    lua_log_init(ctx);
    lua_timer_init(ctx);
    lua_game_state_init(ctx);
    lua_sfml_event_init(ctx);
    lua_cfg_init(ctx);
}

void luactx_mgr::load() {
    std::string file_name = "dfdh";
    std::string assist_file_name = "_assist";
    if (_instance_name == "ai") {
        file_name += "_ai";
        assist_file_name += "_ai";
    }

    try {
        auto dir = fs::current_path() / "data/scripts";
        auto fullpath = (dir / (file_name + ".lua")).string();
        //LOG_INFO("Load lua script \"{}\"", fullpath);

        _ctx->load_and_call(fullpath.data());
        loaded = true;

        auto ofd = std::ofstream(dir / (assist_file_name + ".lua"));
        ofd << _ctx->generate_assist();
    }
    catch (const std::exception& e) {
        LOG_ERR("lua: {}", e.what());
        loaded = false;
    }
}

luactx_mgr::luactx_mgr(std::string instance_name):
    _ctx(std::make_unique<luacpp::luactx>(true)), _instance_name(std::move(instance_name)) {
    //LOG_INFO("Init lua instance \"{}\"", _instance_name);

    auto package_path = fs::current_path() / "data/scripts/?.lua";
    _ctx->enable_implicit_assist(false);
    _ctx->provide(LUA_TNAME("package.path"), package_path.string());
    _ctx->enable_implicit_assist();

    if (_instance_name == "global")
        lua_global_instance_init(*_ctx);
    else if (_instance_name == "ai")
        lua_ai_instance_init(*_ctx);
    else
        throw std::runtime_error("Invalid lua instance name: " + _instance_name);
}

template <typename NameT, typename... Ts>
auto luactx_mgr::try_call_proc(bool game_loop, NameT name, Ts&&... args) {
    if (game_loop) {
        auto now               = std::chrono::steady_clock::now();
        auto [pos, was_insert] = call_history.emplace(std::string_view(NameT{}),
                                                      failed_info{std::chrono::steady_clock::time_point{}, false});
        if (!was_insert && now - pos->second.time < std::chrono::seconds(1))
            return;
    }

    try {
        _ctx->extract<void(Ts...)>(name)(std::forward<Ts>(args)...);
        if (game_loop)
            call_history.at(std::string_view(NameT{})).failed = false;
    }
    catch (const std::exception& e) {
        if (game_loop) {
            auto& info = call_history.at(std::string_view(NameT{}));
            info.time = std::chrono::steady_clock::now();
            if (!info.failed)
                LOG_ERR("lua error: {}", e.what());
            info.failed = true;
        }
    }
}

void luactx_mgr::execute_line(const std::string& line) {
    try {
        _ctx->load_and_call(luacpp::lua_code{line});
    }
    catch (const std::exception& e) {
        LOG_ERR("lua error: {}", e.what());
    }
}

ai_lua_operator_update_func_t get_ai_lua_operator_update(const std::string& name) {
    return lua_ai().ctx().extract<void(class ai_operator_base*, const ai_data_t*)>(
        luacpp::lua_name{"AI"}.dot(name).dot({"update"}));
}

ai_lua_operator_init_func_t get_ai_lua_operator_init(const std::string& name) {
    return lua_ai().ctx().extract<void(class ai_operator_base*)>(luacpp::lua_name{"AI"}.dot(name).dot({"init"}));
}

} // namespace dfdh
