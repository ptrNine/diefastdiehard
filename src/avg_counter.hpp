#pragma once

#include "types.hpp"

namespace dfdh {

template <typename T>
class avg_counter {
public:
    avg_counter(size_t max_count, T pre_calculated_value = T(0), T pre_calculated_count = T(0)):
        _value(pre_calculated_value),
        _count(pre_calculated_count),
        _max_count(static_cast<T>(max_count)) {}

    void update(T value) {
        _value = (_value * _count + value) / (_count + 1);
        if (_count + 1 < _max_count)
            _count += 1;
    }

    T value() const {
        return _value;
    }

private:
    T _value;
    T _count;
    T _max_count;
};

}
