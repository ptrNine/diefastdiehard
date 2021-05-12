#include <SFML/System/Vector2.hpp>

#include "vec_math.hpp"

namespace dfdh {
using u32 = unsigned int;

struct anim_instance {
    float duration;
    float time;
};

class anim_point {
public:
    enum interpolation {
        linear = 0,
        quadratic = 1,
        square = 2
    };

    struct key_data {
        sf::Vector2f pos;
        sf::Vector2f scale;
        float        rot;
    };

    struct anim_key {
        interpolation i;
        key_data      data;
        u32           num;
    };

    template <typename T>
    static T interpolate(T v1, T v2, float f, interpolation i) {
        switch (i) {
        case linear:
            return lerp(v1, v2, f);
        case quadratic:
            return lerp(v1, v2, f * f);
        case square:
            return lerp(v1, v2, std::sqrt(f));
        }
    }

    [[nodiscard]]
    key_data lookup(const anim_instance& inst) const {
        if (_keys.empty())
            throw std::runtime_error("No keys in animation");
        if (_keys.size() == 1)
            return _keys.front().data;

        auto fpt   = float(_keys.back().num) / inst.duration;
        auto frame = std::fmod(inst.time, inst.duration) * fpt;

        auto prev  = _keys.end() - 1;
        auto cur = _keys.begin();
        for (auto it = _keys.begin(); it + 1 != _keys.end(); ++it) {
            if (frame <= float(it->num)) {
                cur = it;
                if (cur != _keys.begin())
                    prev = cur - 1;
                break;
            }
        }

        auto f = lerp(float(prev->num), float(cur->num), frame);
        return key_data{
            interpolate(prev->data.pos, cur->data.pos, f, cur->i),
            interpolate(prev->data.scale, cur->data.scale, f, cur->i),
            interpolate(prev->data.rot, cur->data.rot, f, cur->i),
        };
    }

private:
    std::vector<anim_key> _keys;
};
}
