#pragma once

#include "usertypes.hpp"
#include <luaffi/luacpp_ctx.hpp>

namespace dfdh
{
inline void lua_game_state_init(luacpp::luactx& ctx) {
    using game_state_p = game_state*;

    ctx.annotate({.explicit_type = "vec2f"});
    ctx.annotate({.explicit_type = "boolean"});
    ctx.annotate({.explicit_type = "number"});
    ctx.annotate({.explicit_type = "boolean"});
    ctx.annotate({.explicit_type = "boolean"});
    ctx.set_member_table(luacpp::member_table<game_state*>{
        lua_p_getez(cam_pos),
        lua_p_getsetez(debug_physics),
        lua_p_getsetez(game_speed),
        lua_p_getsetez(gravity_for_bullets),
        {"pause",
         {[](const game_state_p& g, luacpp::luactx& ctx) { ctx.push(!g->on_game); },
          [](game_state_p& g, luacpp::luactx& ctx) {
              bool pause;
              ctx.get_new(pause);
              g->game_run(!pause);
          }}},
    });
}
} // namespace dfdh
