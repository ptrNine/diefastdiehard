#pragma once

#include <random>
#include <cstring>

#include "types.hpp"
#include "vec_math.hpp"

namespace dfdh {

class rand_float_pool {
public:
    using gen_t = u32;

    rand_float_pool(size_t size = 1024) {
        pool.resize(size);
        std::generate(
            pool.begin(), pool.end(), []() { return static_cast<u8>(rand_gen_singleton::instance().mt()()); });
    }

    float gen(float min, float max) {
        while (true) {
            gen_t v;
            std::memcpy(&v, pool.data() + pos, sizeof(gen_t));

            ++pos;
            if (pool.size() - pos < sizeof(gen_t))
                pos = 0;

            auto num = static_cast<float>(v);
            auto div = static_cast<float>(std::numeric_limits<gen_t>::max());
            auto res = num / div * (max - min) + min;

            if (res <= max)
                return res;
        }
    }

    bool roll_the_dice(float probability) {
        return gen(0.f, 1.f) < probability;
    }

    [[nodiscard]]
    size_t size() const {
        return pool.size();
    }

    void resize(size_t new_size) {
        pool.resize(new_size);
    }

    [[nodiscard]]
    const u8* data() const {
        return pool.data();
    }

    u8* data() {
        return pool.data();
    }

    [[nodiscard]]
    size_t position() const {
        return pos;
    }

    void position(size_t value) {
        pos = value;
    }

    void data(const u8* data, size_t size) {
        pool.resize(size);
        std::memcpy(pool.data(), data, size);
    }

private:
    std::vector<u8> pool;
    size_t pos = 0;
};

}

