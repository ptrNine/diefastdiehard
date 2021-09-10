#pragma once

#include "types.hpp"

namespace dfdh
{

inline uint64_t fnv1a64(const void* ptr, size_t size) {
    auto     p    = static_cast<const u8*>(ptr);
    uint64_t hash = 0xcbf29ce484222325;

    while (size--) hash = (hash ^ *p++) * 0x100000001b3;

    return hash;
}
} // namespace dfdh
