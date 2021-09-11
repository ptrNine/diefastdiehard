#pragma once

#include <SFML/System/Vector2.hpp>

#include "types.hpp"

namespace dfdh
{

template <typename T>
struct vec2 {
public:
    constexpr vec2() = default;

    constexpr vec2(T ix, T iy = T(0)): x(ix), y(iy) {}

    constexpr vec2(const sf::Vector2<T>& vec): x(vec.x), y(vec.y) {}

    operator sf::Vector2<T>() const {
        return {x, y};
    }

    constexpr vec2 xy() const {
        return *this;
    }

    constexpr vec2 yx() const {
        return vec2(y, x);
    }

    constexpr vec2& operator+=(const vec2& vec) {
        x += vec.x;
        y += vec.y;
        return *this;
    }

    constexpr vec2& operator-=(const vec2& vec) {
        x -= vec.x;
        y -= vec.y;
        return *this;
    }

    constexpr vec2 operator+(const vec2& vec) const {
        auto res = *this;
        res += vec;
        return res;
    }

    constexpr vec2 operator-(const vec2& vec) const {
        auto res = *this;
        res -= vec;
        return res;
    }

    constexpr vec2& operator*=(T value) {
        x *= value;
        y *= value;
        return *this;
    }

    constexpr vec2& operator/=(T value) {
        x /= value;
        y /= value;
        return *this;
    }

    constexpr vec2& operator-() {
        x = -x;
        y = -y;
        return *this;
    }

    constexpr vec2& operator+() {
        return *this;
    }

    constexpr vec2 operator*(T value) const {
        auto res = *this;
        res *= value;
        return res;
    }

    constexpr vec2 operator/(T value) const {
        auto res = *this;
        res /= value;
        return res;
    }

    constexpr friend vec2 operator*(T value, const vec2& vec) {
        return vec * value;
    }

    /* SFML vec */
    friend constexpr sf::Vector2<T>& operator+=(const sf::Vector2<T>& l, const vec2& r) {
        l.x += r.x;
        l.y += r.y;
        return l;
    }

    friend constexpr sf::Vector2<T>& operator-=(const sf::Vector2<T>& l, const vec2& r) {
        l.x -= r.x;
        l.y -= r.y;
        return l;
    }

    friend constexpr vec2 operator+(const sf::Vector2<T>& l, const vec2& r) {
        return vec2(l) + r;
    }

    friend constexpr vec2 operator-(const sf::Vector2<T>& l, const vec2& r) {
        return vec2(l) - r;
    }

    T x, y;
};

using vec2f   = vec2<float>;
using vec2d   = vec2<double>;
using vec2i   = vec2<int>;
using vec2u   = vec2<unsigned int>;
using vec2u8  = vec2<u8>;
using vec2i8  = vec2<i8>;
using vec2u16 = vec2<u16>;
using vec2i16 = vec2<i16>;
using vec2u32 = vec2<u32>;
using vec2i32 = vec2<i32>;
using vec2u64 = vec2<u64>;
using vec2i64 = vec2<i64>;

} // namespace dfdh

namespace std
{
template <typename T>
struct tuple_size<dfdh::vec2<T>> {
    static constexpr inline size_t value = 2;
};

template <size_t N, typename T>
struct tuple_element<N, dfdh::vec2<T>> {
    using type = T;
};
} // namespace std

namespace dfdh
{
template <size_t S, typename T>
constexpr T& get(vec2<T>& vec) {
    static_assert(S < 2);
    if constexpr (S == 0)
        return vec.x;
    if constexpr (S == 1)
        return vec.y;
}
template <size_t S, typename T>
constexpr const T& get(const vec2<T>& vec) {
    static_assert(S < 2);
    if constexpr (S == 0)
        return vec.x;
    if constexpr (S == 1)
        return vec.y;
}

} // namespace dfdh
