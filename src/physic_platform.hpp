#pragma once

#include "vec2.hpp"

namespace dfdh {

class physic_platform {
public:
    physic_platform(const vec2f& position, float length):
        _pos(position), _length(length) {}

    [[nodiscard]]
    const vec2f& get_position() const {
        return _pos;
    }

    [[nodiscard]]
    float length() const {
        return _length;
    }

private:
    vec2f _pos;
    float _length;
};
}
