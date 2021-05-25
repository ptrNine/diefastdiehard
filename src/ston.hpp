#pragma once

#include <string>
#include "types.hpp"

namespace dfdh
{
template <typename T>
inline T ston(const std::string& str) {
    static_assert(Integral<T> || AnyOfType<T, double, float>, "T is not a number");

    if constexpr (Integral<T>) {
        if constexpr (Unsigned<T>)
            return static_cast<T>(std::stoull(str));
        else
            return static_cast<T>(std::stoll(str));
    }
    else if constexpr (std::is_same_v<float, T>) {
        return std::stof(str);
    }
    else if constexpr (std::is_same_v<double, T>) {
        return std::stod(str);
    }
}
} // namespace dfdh
