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
                LOG_WARN_UPDATE("{}", str);
            else
                LOG_WARN("{}", str);
        }
        else if (level.starts_with("err")) {
            if (level.find("upd") != std::string::npos)
                LOG_ERR_UPDATE("{}", str);
            else
                LOG_ERR("{}", str);
        }
        else {
            if (level.find("upd") != std::string::npos)
                LOG_INFO_UPDATE("{}", str);
            else
                LOG_INFO("{}", str);
        }
    });
}
} // namespace dfdh
