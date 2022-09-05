#pragma once

#include <span>
#include "types.hpp"

namespace dfdh
{

template <typename T>
concept Range = requires(T&& v) {
    std::begin(v);
    std::end(v);
};

template <typename T>
concept InputIter = requires(T v) {
    {v++};
    {++v};
    {*v};
    { v == v } -> std::convertible_to<bool>;
    { v != v } -> std::convertible_to<bool>;
};

template <typename T>
concept InputRange = Range<T> && requires(T&& v) {
    requires InputIter<decltype(std::begin(v))>;
};

template <typename T>
concept BidirectIter = InputIter<T> && requires(T v) {
    {v--};
    {--v};
};

template <typename T>
concept BiderectRange = InputRange<T> && requires(T&& v) {
    requires BidirectIter<decltype(std::begin(v))>;
};

template <typename T>
concept RandomAccessRange = BiderectRange<T> && requires(T&& v) {
    { std::end(v) - std::begin(v) } -> std::convertible_to<ptrdiff_t>;
};

namespace detail {
    template <typename T>
    concept AllowSpan = requires (const T& v) {
        { std::span{v, v - v} };
    };

    template <typename F, typename I>
    concept RangeFunc = requires (F func, I iter) {
        func(iter, iter);
    };
}

template <typename I>
struct range {
    range(I ibegin, I iend): _begin(ibegin), _end(iend) {}
    I               _begin, _end;
    [[nodiscard]] I begin() const {
        return _begin;
    }
    [[nodiscard]] I end() const {
        return _end;
    }
};
} // namespace core
