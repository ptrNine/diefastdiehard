#pragma once

#include "usertypes.hpp"
#include <luaffi/luacpp_ctx.hpp>

namespace dfdh
{
inline void lua_log_init(luacpp::luactx& ctx) {
    ctx.annotate({.argument_names = {"level", "message"}});
    ctx.provide(LUA_TNAME("log_impl"), [](const std::string& level, const std::string& str) {
        if (level.starts_with("warn")) {
            if (level.find("upd") != std::string::npos)
                glog().warn_update(__COUNTER__, "{}", str);
            else
                glog().warn("{}", str);
        }
        else if (level.starts_with("err")) {
            if (level.find("upd") != std::string::npos)
                glog().error_update(__COUNTER__, "{}", str);
            else
                glog().error("{}", str);
        }
        else {
            if (level.find("upd") != std::string::npos)
                glog().info_update(__COUNTER__, "{}", str);
            else
                glog().info("{}", str);
        }
    });
}
} // namespace dfdh
