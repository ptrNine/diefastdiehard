#pragma once

#include <type_traits>
#include <cstdint>
#include <array>
#include <vector>
#include <string>

#ifndef NDEBUG
#define Expects(...) do { if (!(__VA_ARGS__)) std::terminate(); } while(0)
#else
#define Expects(...) void(0)
#endif


namespace dfdh {

using namespace std::string_view_literals;
using namespace std::string_literals;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;


template<typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template <typename T>
struct is_std_array : std::false_type {};

template <typename T, size_t S>
struct is_std_array<std::array<T, S>> : std::true_type {};

template <typename T>
concept StdArray = is_std_array<T>::value;

template <typename T>
struct is_std_vector : std::false_type {};

template <typename T>
struct is_std_vector<std::vector<T>> : std::true_type {};

template <typename T>
concept StdVector = is_std_vector<T>::value;

template <typename T>
concept Integral = std::is_integral_v<T>;

template <typename T>
concept Unsigned = std::is_unsigned_v<T>;

template <typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

template <typename T>
concept Number = Integral<T> || std::is_floating_point_v<T>;

template<typename T, typename... ArgsT>
concept AnyOfType = (std::is_same_v<T, ArgsT> || ...);

template <typename T>
concept Enum = std::is_enum_v<T>;

template <typename... Ts>
struct type_tuple {};

template <typename... Ts>
requires requires(const Ts&... s) {
    ((s.size()), ...);
    (std::string_view(s), ...);
}
std::string build_string(const Ts&... strings) {
    std::string str;
    str.reserve((strings.size() + ...));
    ((str.append(strings)), ...);
    return str;
}

} // namespace dfdh
