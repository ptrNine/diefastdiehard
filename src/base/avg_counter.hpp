#pragma once

#include "types.hpp"

namespace dfdh {

template <typename T>
class avg_counter {
public:
    avg_counter(size_t max_count, T pre_calculated_value = T(0), size_t pre_calculated_count = 0):
        _value(pre_calculated_value),
        _count(pre_calculated_count),
        _max_count(max_count) {}

    void reset() {
        _count = 0;
    }

    void update(T value) {
        auto c = T(_count);
        _value = (_value * c + value) / (c + 1);
        if (_count + 1 < _max_count)
            _count += 1;
    }

    void max_count(size_t max_count) {
        _max_count = max_count;
        if (_count >= max_count)
            _count = max_count - 1;
    }

    [[nodiscard]]
    size_t max_count() const {
        return _max_count;
    }

    T value() const {
        return _value;
    }

    void value(const T& v) {
        _value = v;
    }

private:
    T      _value;
    size_t _count;
    size_t _max_count;
};
}
