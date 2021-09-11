#pragma once

#include <SFML/Graphics/Color.hpp>

#include "types.hpp"
#include "serialization.hpp"
#include "print.hpp"

namespace dfdh {

struct rgba_t {
    DFDH_SERIALIZE(r, g, b, a)

    rgba_t() = default;
    rgba_t(const sf::Color& c): r(c.r), g(c.g), b(c.b), a(c.a) {}

    operator sf::Color() const {
        return {r, g, b, a};
    }

    [[nodiscard]]
    std::string to_string() const {
        constexpr auto to_hex = [](u8 n, std::string& str) {
            constexpr auto chars = "0123456789abcdef";
            str.push_back(chars[n >> 4]);
            str.push_back(chars[n & 0x0f]);
        };

        std::string res;
        res.reserve(9);
        res.push_back('#');
        to_hex(r, res);
        to_hex(g, res);
        to_hex(b, res);
        to_hex(a, res);

        return res;
    }

    static rgba_t from(std::string_view str) {
        rgba_t c;
        if (str.empty() || str.front() != '#')
            return c;

        constexpr auto from_hex = [](char c) -> u8 {
            if (c >= 'A' && c <= 'F')
                return u8(10 + (c - 'A'));
            if (c >= 'a' && c <= 'f')
                return u8(10 + (c - 'a'));
            return u8(c - '0');
        };

        if (str.size() < 3) return c;
        c.r = u8(from_hex(str[1]) << 4);
        c.r |= u8(from_hex(str[2]));

        if (str.size() < 5) return c;
        c.g = u8(from_hex(str[3]) << 4);
        c.g |= u8(from_hex(str[4]));

        if (str.size() < 7) return c;
        c.b = u8(from_hex(str[5]) << 4);
        c.b |= u8(from_hex(str[6]));

        if (str.size() < 9) return c;
        c.a = u8(from_hex(str[7]) << 4);
        c.a |= u8(from_hex(str[8]));

        return c;
    }

    u8 r = 255, g = 255, b = 255, a = 255;
};

template <>
struct printer<rgba_t> {
    void operator()(std::ostream& os, const rgba_t& color) {
        os << color.to_string();
    }
};

} // namespace dfdh
