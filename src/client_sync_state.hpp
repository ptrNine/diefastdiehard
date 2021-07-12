#pragma once

#include "types.hpp"
#include "fixed_string.hpp"

namespace dfdh {

struct client_sync_state {
    [[nodiscard]]
    bool on_game() const {
        return !operated_player.empty() && init_sync_ok;
    }

    bool          init_sync_ok = false;
    player_name_t operated_player;
};

}
