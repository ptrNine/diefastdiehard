#pragma once

#include <SFML/System/Clock.hpp>
#include <cmath>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Color.hpp>
#include <iostream>
#include <ostream>
#include <random>
#include <cstring>

#include "vec2.hpp"

namespace dfdh {

template <typename T>
float magnitude2(const vec2<T>& v) {
    return v.x * v.x + v.y * v.y;
}

template <typename T>
float magnitude(const vec2<T>& v) {
    return std::sqrt(magnitude2(v));
}

template <typename T>
vec2<T> normalize(const vec2<T>& v) {
    return v / magnitude(v);
}

inline float lerp(float v0, float v1, float t) {
    return v0 * (1.f - t) + v1 * t;
}

template <typename T>
vec2<T> lerp(const vec2<T>& v0, const vec2<T>& v1, float t) {
    return v0 * (1.f - t) + v1 * t;
}

template <typename T>
sf::Vector2<T> lerp(const sf::Vector2<T>& v0, const sf::Vector2<T>& v1, float t) {
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

inline std::ostream& operator<<(std::ostream& o, const sf::Color& c) {
    static constexpr auto hexmap = "0123456789abcdef";
    std::array<uint8_t, 4> color{c.r, c.g, c.b, c.a};

    o << '#';
    for (auto v : color)
        o << hexmap[v >> 4] << hexmap[v & 0x0f];

    return o;
}

inline std::istream& operator>>(std::istream& i, sf::Color& c) {
    std::string str;
    i >> str;

    static constexpr int hexmap[] = {0, 1, 2, 3, 4, 5,  6,  7,  8,  9,  0, 0,
                                     0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15};

    auto p = str.data();
    std::array<uint8_t, 4> color{255, 255, 255, 255};

    if (*p == '#') {
        ++p;

        for (int i = 0, v = toupper(*p); v && i < 8 && ((v >= '0' && v <= '9') || (v >= 'A' && v <= 'F')); ++i) {
            auto digit = uint8_t(hexmap[v - '0']);
            color[size_t(i) / 2] &= uint8_t(i % 2 ? (0xf0 | digit) : (0x0f | (digit << 4)));
            ++p;
            v = toupper(*p);
        }
    }

    c = sf::Color(color[0], color[1], color[2], color[3]);
    return i;
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

    vec2f min;
    vec2f max;
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


template <typename T>
vec2<T> randomize_dir(const vec2<T>& dir, float angle) {
    auto angl = std::atan2(dir.y, dir.x);
    auto urd = std::uniform_real_distribution<float>(-angle * 0.5f, angle * 0.5f);
    auto new_angl = angl + urd(rand_gen_singleton::instance().mt());
    return {std::cos(new_angl), std::sin(new_angl)};
}

template <typename T, typename F>
vec2<T> randomize_dir(const vec2<T>& dir, float angle, F&& rand_gen) {
    auto angl = std::atan2(dir.y, dir.x);
    auto new_angl = angl + rand_gen(-angle * 0.5f, angle * 0.5f);
    return {std::cos(new_angl), std::sin(new_angl)};
}

template <typename T>
vec2<T> rotate_vec(const vec2<T>& vec, float angle) {
    auto norm = normalize(vec);
    auto magn = magnitude(vec);
    auto angl = std::atan2(norm.y, norm.x);
    auto new_angl = angl + angle;
    auto new_dir = vec2(std::cos(new_angl), std::sin(new_angl));
    return new_dir * magn;
}

inline float rand_float(float min, float max) {
    return std::uniform_real_distribution<float>(min, max)(rand_gen_singleton::instance().mt());
}

template <std::integral T>
inline T rand_num(T min, T max) {
    return std::uniform_int_distribution<T>(min, max)(rand_gen_singleton::instance().mt());
}

inline bool roll_the_dice(float probability) {
    return rand_float(0.f, 1.f) < probability;
}

class timer {
public:
    void restart() {
        _clock.restart();
    }

    [[nodiscard]]
    float elapsed(float speed = 1.f) const {
        return _clock.getElapsedTime().asSeconds() * speed;
    }

private:
    sf::Clock _clock;
};

} // namespace dfdh
