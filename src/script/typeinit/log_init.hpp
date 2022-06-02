#pragma once

#include "usertypes.hpp"
#include <luaffi/luacpp_ctx.hpp>
#include "base/profiler.hpp"
#include "base/hash_functions.hpp"

namespace dfdh
{
inline void lua_log_init(luacpp::luactx& ctx) {
    ctx.annotate({.argument_names = {"level", "message"}});
    ctx.provide(LUA_TNAME("log_impl"), [](const std::string& level, const std::string& str) {
        if (level.starts_with("debug"))
            glog().debug("{}", str);
        else if (level.starts_with("info"))
            glog().info("{}", str);
        else if (level.starts_with("warn"))
            glog().warn("{}", str);
        else if (level.starts_with("err"))
            glog().error("{}", str);
        else
            glog().detail("{}", str);
    });
    ctx.annotate({.argument_names = {"update_id", "level", "message"}});
    ctx.provide(LUA_TNAME("log_update_impl"),
                [](uint16_t update_id, const std::string& level, const std::string& str) {
                    if (level.starts_with("debug"))
                        glog().debug_update(update_id, "{}", str);
                    else if (level.starts_with("info"))
                        glog().info_update(update_id, "{}", str);
                    else if (level.starts_with("warn"))
                        glog().warn_update(update_id, "{}", str);
                    else if (level.starts_with("err"))
                        glog().error_update(update_id, "{}", str);
                    else
                        glog().detail_update(update_id, "{}", str);
                });
}
} // namespace dfdh
