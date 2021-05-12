#pragma once

#include <SFML/System/Vector2.hpp>

namespace dfdh {

class physic_platform {
public:
    physic_platform(const sf::Vector2f& position, float length):
        _pos(position), _length(length) {}

    [[nodiscard]]
    const sf::Vector2f& get_position() const {
        return _pos;
    }

    [[nodiscard]]
    float length() const {
        return _length;
    }

private:
    sf::Vector2f _pos;
    float        _length;
};

}
