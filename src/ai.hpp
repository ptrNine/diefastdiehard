#pragma once

#include <SFML/System/Vector2.hpp>

#include "vec_math.hpp"

namespace dfdh {

enum ai_difficulty {
    ai_easy = 0,
    ai_medium,
    ai_hard,
    ai_insane
};

class ai_mgr_singleton {
public:
    static ai_mgr_singleton& instance() {
        static ai_mgr_singleton inst;
        return inst;
    }

    ai_mgr_singleton(const ai_mgr_singleton&) = delete;
    ai_mgr_singleton& operator=(const ai_mgr_singleton&) = delete;

private:
    ai_mgr_singleton() = default;
    ~ai_mgr_singleton() = default;

public:
    struct player_t {
        sf::Vector2f pos;
    };
};


inline ai_mgr_singleton& ai_mgr() {
    return ai_mgr_singleton::instance();
}


class ai_operator {
public:


private:
    ai_difficulty _difficulty;
};

};

