#pragma once

#include <SFML/Window/Keyboard.hpp>

#include "types.hpp"
#include "config.hpp"
#include "fixed_string.hpp"

namespace dfdh {

struct player_controller {
    player_controller(u32 id): _id(id) {
        auto sect = "player_controller_" + std::to_string(_id);
        auto path = "data/settings/players_settings.cfg"s;

        up    = cfg().get_or_write_default<int>(sect, "key_up", sf::Keyboard::Up, path);
        down  = cfg().get_or_write_default<int>(sect, "key_down", sf::Keyboard::Down, path);
        left  = cfg().get_or_write_default<int>(sect, "key_left", sf::Keyboard::Left, path);
        right = cfg().get_or_write_default<int>(sect, "key_right", sf::Keyboard::Right, path);
        shot    = cfg().get_or_write_default<int>(sect, "key_shot", sf::Keyboard::Comma, path);
        grenade = cfg().get_or_write_default<int>(sect, "key_grenade", sf::Keyboard::Dash, path);
        cfg().try_refresh_file(sect);
    }

    /*
    control_keys(bool, int id): _id(id) {
        up      = sf::Keyboard::Up;
        down    = sf::Keyboard::Down;
        left    = sf::Keyboard::Left;
        right   = sf::Keyboard::Right;
        shot    = sf::Keyboard::Comma;
        grenade = sf::Keyboard::Dash;

        up      = sf::Keyboard::W;
        down    = sf::Keyboard::S;
        left    = sf::Keyboard::A;
        right   = sf::Keyboard::D;
        shot    = sf::Keyboard::Y;
        grenade = sf::Keyboard::U;
    }
    */

    void save() {
        cfg().try_refresh_file("player_controller_" + std::to_string(_id));
    }

    int up, down, left, right, shot, grenade;
    u32 _id;
    player_name_t player_name;
};

}
