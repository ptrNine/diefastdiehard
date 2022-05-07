#pragma once

#include <fstream>
#include <filesystem>

#include "log.hpp"

namespace luacpp {
    class luactx;
}

namespace dfdh
{

namespace fs = std::filesystem;

class luactx_mgr {
public:
    luactx_mgr(std::string instance_name = "global");

    operator bool() const {
        return loaded;
    }

    luacpp::luactx& ctx() {
        return *_ctx;
    }

    void load();

    void execute_line(const std::string& line);

    template <typename NameT, typename... Ts>
    auto try_call_proc(bool game_loop, NameT name, Ts&&... args);

public:
    luactx_mgr(const luactx_mgr&) = delete;
    luactx_mgr& operator=(const luactx_mgr&) = delete;

    static luactx_mgr& instance() {
        static luactx_mgr inst;
        return inst;
    }

    static luactx_mgr& ai_instance() {
        static luactx_mgr inst{"ai"};
        return inst;
    }

private:
    std::unique_ptr<luacpp::luactx> _ctx;
    std::string                     _instance_name;
    bool                            loaded = false;

    struct failed_info {
        std::chrono::steady_clock::time_point time;
        bool                                  failed = false;
    };

    std::map<std::string_view, failed_info> call_history;
};

luactx_mgr& lua() {
    return luactx_mgr::instance();
}

luactx_mgr& lua_ai() {
    return luactx_mgr::ai_instance();
}

} // namespace dfdh
