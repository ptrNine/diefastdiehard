#pragma once

#include "usertypes.hpp"
#include <luaffi/luacpp_ctx.hpp>

namespace dfdh
{
inline void lua_sfml_event_init(luacpp::luactx& ctx) {
    ctx.annotate({.explicit_type = "string"});
    ctx.annotate({.explicit_type = "string"});
    ctx.set_member_table(luacpp::member_table<sf::Event>{
        {"type",
         {[](const sf::Event& e, luacpp::luactx& ctx) { ctx.push(sfml_event_type_to_str(e.type)); },
          [](sf::Event& e, luacpp::luactx& ctx) {
              std::string event_type;
              ctx.get_new(event_type);
              e.type = sfml_str_to_event_type(event_type);
          }}},
        {"keycode",
         {[](const sf::Event& e, luacpp::luactx& ctx) { ctx.push(sfml_key_to_str(e.key.code)); }, // NOLINT
          [](sf::Event& e, luacpp::luactx& ctx) {
              std::string key;
              ctx.get_new(key);
              e.key.code = sfml_str_to_key(key); // NOLINT
          }}},
    });
}
} // namespace dfdh
