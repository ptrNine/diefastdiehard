#pragma once

#include <cmath>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <iostream>
#include <ostream>
#include <random>
#include <cstring>

namespace dfdh {

inline float magnitude2(const sf::Vector2f& v) {
    return v.x * v.x + v.y * v.y;
}

inline float magnitude(const sf::Vector2f& v) {
    return std::sqrt(magnitude2(v));
}

inline sf::Vector2f normalize(const sf::Vector2f& v) {
    return v / magnitude(v);
}

inline float lerp(float v0, float v1, float t) {
    return v0 * (1.f - t) + v1 * t;
}

inline sf::Vector2f lerp(const sf::Vector2f& v0, const sf::Vector2f& v1, float t) {
    return v0 * (1.f - t) + v1 * t;
}

inline float inverse_lerp(float x1, float x2, float value) {
    return (value - x1) / (x2 - x1);
}

inline std::ostream& operator<<(std::ostream& o, const sf::Vector2f& v) {
    o << "{" << v.x << ", " << v.y << "}";
    return o;
}

inline std::ostream& operator<<(std::ostream& o, const sf::FloatRect& r) {
    o << "{" << r.left << ", " << r.top << ", " << r.width << ", " << r.height << "}";
    return o;
}

inline bool approx_equal(float a, float b, float epsilon) {
    return fabsf(a - b) <= ((fabsf(a) < fabsf(b) ? fabsf(b) : fabsf(a)) * epsilon);
}

inline bool approx_equal(double a, double b, double epsilon) {
    return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

inline bool essentially_equal(float a, float b, float epsilon) {
    return fabsf(a - b) <= ((fabsf(a) > fabsf(b) ? fabsf(b) : fabsf(a)) * epsilon);
}

inline bool essentially_equal(double a, double b, double epsilon) {
    return fabs(a - b) <= ((fabs(a) > fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

inline bool definitely_greater(float a, float b, float epsilon) {
    return (a - b) > ((fabsf(a) < fabsf(b) ? fabsf(b) : fabsf(a)) * epsilon);
}

inline bool definitely_greater(double a, double b, double epsilon) {
    return (a - b) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

inline bool definitely_less(float a, float b, float epsilon) {
    return (b - a) > ((fabsf(a) < fabsf(b) ? fabsf(b) : fabsf(a)) * epsilon);
}

inline bool definitely_less(double a, double b, double epsilon) {
    return (b - a) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

inline bool is_float_zero(float v) {
    static constexpr float zero = 0.f;
    return std::memcmp(&v, &zero, 4) == 0;
}

struct bounding_box {
    static bounding_box maximized() {
        bounding_box b;
        b.max = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        b.min = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        return b;
    }

    bounding_box() = default;

    bounding_box(const sf::FloatRect& r) {
        min.x = r.left;
        min.y = r.top;
        max.x = r.left + r.width;
        max.y = r.top + r.height;
    }

    [[nodiscard]]
    sf::FloatRect rect() const {
        sf::FloatRect r;
        r.left   = min.x;
        r.top    = min.y;
        r.width  = max.x - min.x;
        r.height = max.y - min.y;
        return r;
    }

    void merge(const bounding_box& bb) {
        min.x = std::min(min.x, bb.min.x);
        min.y = std::min(min.y, bb.min.y);
        max.x = std::max(max.x, bb.max.x);
        max.y = std::max(max.y, bb.max.y);
    }

    sf::Vector2f min;
    sf::Vector2f max;
};

inline bool is_power_of_two(auto v) {
    return (v & (v - 1)) == 0;
}


class rand_gen_singleton {
public:
    static rand_gen_singleton& instance() {
        static rand_gen_singleton inst;
        return inst;
    }

    rand_gen_singleton(const rand_gen_singleton&) = delete;
    rand_gen_singleton& operator=(const rand_gen_singleton&) = delete;

    auto& mt() {
        return _mt;
    }

private:
    rand_gen_singleton() = default;
    ~rand_gen_singleton() = default;

    std::mt19937 _mt;
};


inline sf::Vector2f randomize_dir(const sf::Vector2f& dir, float angle) {
    auto angl = std::atan2(dir.y, dir.x);
    auto urd = std::uniform_real_distribution<float>(-angle/2.f, angle/2.f);
    auto new_angl = angl + urd(rand_gen_singleton::instance().mt());
    return {std::cos(new_angl), std::sin(new_angl)};
}

inline sf::Vector2f rotate_vec(const sf::Vector2f& vec, float angle) {
    auto norm = normalize(vec);
    auto magn = magnitude(vec);
    auto angl = std::atan2(norm.y, norm.x);
    auto new_angl = angl + angle;
    auto new_dir = sf::Vector2f(std::cos(new_angl), std::sin(new_angl));
    return new_dir * magn;
}

inline float rand_float(float min, float max) {
    return std::uniform_real_distribution<float>(min, max)(rand_gen_singleton::instance().mt());
}

inline bool roll_the_dice(float probability) {
    return rand_float(0.f, 1.f) < probability;
}

} // namespace dfdh
