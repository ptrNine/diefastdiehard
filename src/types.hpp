#pragma once

#include <type_traits>
#include <cstdint>
#include <array>
#include <vector>

namespace dfdh {

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

} // namespace dfdh
