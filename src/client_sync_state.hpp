#pragma once

#include "types.hpp"
#include "fixed_string.hpp"

namespace dfdh {


enum class sync_state {
    start = 0, send, ok, fail
};

struct client_sync_state {
    [[nodiscard]]
    bool on_game() const {
        return !operated_player.empty()/* && init_sync == sync_state::ok*/;
    }

    bool init_sended = false;
    //sync_state init_sync;

    player_name_t operated_player;
};

}
