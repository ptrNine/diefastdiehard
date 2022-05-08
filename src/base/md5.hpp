#pragma once

#include <iomanip>
#include <bit>
#include <span>
#include <cstring>

#include "types.hpp"

#define MD5_ROUND0_15(r, N, K)                                                                                         \
    F     = (b & c) | ((~b) & d);                                                                                      \
    dTemp = d;                                                                                                         \
    d     = c;                                                                                                         \
    c     = b;                                                                                                         \
    b += md5_dtls::md5_rotl(a + F + K + words[r], N);                                                                  \
    a = dTemp

#define MD5_ROUND16_31(W, N, K)                                                                                        \
    F     = (d & b) | ((~d) & c);                                                                                      \
    dTemp = d;                                                                                                         \
    d     = c;                                                                                                         \
    c     = b;                                                                                                         \
    b += md5_dtls::md5_rotl(a + F + K + words[W], N);                                                                  \
    a = dTemp

#define MD5_ROUND32_47(W, N, K)                                                                                        \
    F     = b ^ c ^ d;                                                                                                 \
    dTemp = d;                                                                                                         \
    d     = c;                                                                                                         \
    c     = b;                                                                                                         \
    b += md5_dtls::md5_rotl(a + F + K + words[W], N);                                                                  \
    a = dTemp

#define MD5_ROUND48_63(W, N, K)                                                                                        \
    F     = c ^ (b | (~d));                                                                                            \
    dTemp = d;                                                                                                         \
    d     = c;                                                                                                         \
    c     = b;                                                                                                         \
    b += md5_dtls::md5_rotl(a + F + K + words[W], N);                                                                  \
    a = dTemp

#define MD5_ROUND1()                                                                                                   \
    MD5_ROUND0_15(0, 7,   0xd76aa478);                                                                                 \
    MD5_ROUND0_15(1, 12,  0xe8c7b756);                                                                                 \
    MD5_ROUND0_15(2, 17,  0x242070db);                                                                                 \
    MD5_ROUND0_15(3, 22,  0xc1bdceee);                                                                                 \
    MD5_ROUND0_15(4, 7,   0xf57c0faf);                                                                                 \
    MD5_ROUND0_15(5, 12,  0x4787c62a);                                                                                 \
    MD5_ROUND0_15(6, 17,  0xa8304613);                                                                                 \
    MD5_ROUND0_15(7, 22,  0xfd469501);                                                                                 \
    MD5_ROUND0_15(8, 7,   0x698098d8);                                                                                 \
    MD5_ROUND0_15(9, 12,  0x8b44f7af);                                                                                 \
    MD5_ROUND0_15(10, 17, 0xffff5bb1);                                                                                 \
    MD5_ROUND0_15(11, 22, 0x895cd7be);                                                                                 \
    MD5_ROUND0_15(12, 7,  0x6b901122);                                                                                 \
    MD5_ROUND0_15(13, 12, 0xfd987193);                                                                                 \
    MD5_ROUND0_15(14, 17, 0xa679438e);                                                                                 \
    MD5_ROUND0_15(15, 22, 0x49b40821)

#define MD5_ROUND2()                                                                                                   \
    MD5_ROUND16_31(1, 5,   0xf61e2562);                                                                                \
    MD5_ROUND16_31(6, 9,   0xc040b340);                                                                                \
    MD5_ROUND16_31(11, 14, 0x265e5a51);                                                                                \
    MD5_ROUND16_31(0, 20,  0xe9b6c7aa);                                                                                \
    MD5_ROUND16_31(5, 5,   0xd62f105d);                                                                                \
    MD5_ROUND16_31(10, 9,  0x02441453);                                                                                \
    MD5_ROUND16_31(15, 14, 0xd8a1e681);                                                                                \
    MD5_ROUND16_31(4, 20,  0xe7d3fbc8);                                                                                \
    MD5_ROUND16_31(9, 5,   0x21e1cde6);                                                                                \
    MD5_ROUND16_31(14, 9,  0xc33707d6);                                                                                \
    MD5_ROUND16_31(3, 14,  0xf4d50d87);                                                                                \
    MD5_ROUND16_31(8, 20,  0x455a14ed);                                                                                \
    MD5_ROUND16_31(13, 5,  0xa9e3e905);                                                                                \
    MD5_ROUND16_31(2, 9,   0xfcefa3f8);                                                                                \
    MD5_ROUND16_31(7, 14,  0x676f02d9);                                                                                \
    MD5_ROUND16_31(12, 20, 0x8d2a4c8a)

#define MD5_ROUND3()                                                                                                   \
    MD5_ROUND32_47(5, 4,   0xfffa3942);                                                                                \
    MD5_ROUND32_47(8, 11,  0x8771f681);                                                                                \
    MD5_ROUND32_47(11, 16, 0x6d9d6122);                                                                                \
    MD5_ROUND32_47(14, 23, 0xfde5380c);                                                                                \
    MD5_ROUND32_47(1, 4,   0xa4beea44);                                                                                \
    MD5_ROUND32_47(4, 11,  0x4bdecfa9);                                                                                \
    MD5_ROUND32_47(7, 16,  0xf6bb4b60);                                                                                \
    MD5_ROUND32_47(10, 23, 0xbebfbc70);                                                                                \
    MD5_ROUND32_47(13, 4,  0x289b7ec6);                                                                                \
    MD5_ROUND32_47(0, 11,  0xeaa127fa);                                                                                \
    MD5_ROUND32_47(3, 16,  0xd4ef3085);                                                                                \
    MD5_ROUND32_47(6, 23,  0x04881d05);                                                                                \
    MD5_ROUND32_47(9, 4,   0xd9d4d039);                                                                                \
    MD5_ROUND32_47(12, 11, 0xe6db99e5);                                                                                \
    MD5_ROUND32_47(15, 16, 0x1fa27cf8);                                                                                \
    MD5_ROUND32_47(2, 23,  0xc4ac5665)

#define MD5_ROUND4()                                                                                                   \
    MD5_ROUND48_63(0, 6,   0xf4292244);                                                                                \
    MD5_ROUND48_63(7, 10,  0x432aff97);                                                                                \
    MD5_ROUND48_63(14, 15, 0xab9423a7);                                                                                \
    MD5_ROUND48_63(5, 21,  0xfc93a039);                                                                                \
    MD5_ROUND48_63(12, 6,  0x655b59c3);                                                                                \
    MD5_ROUND48_63(3, 10,  0x8f0ccc92);                                                                                \
    MD5_ROUND48_63(10, 15, 0xffeff47d);                                                                                \
    MD5_ROUND48_63(1, 21,  0x85845dd1);                                                                                \
    MD5_ROUND48_63(8, 6,   0x6fa87e4f);                                                                                \
    MD5_ROUND48_63(15, 10, 0xfe2ce6e0);                                                                                \
    MD5_ROUND48_63(6, 15,  0xa3014314);                                                                                \
    MD5_ROUND48_63(13, 21, 0x4e0811a1);                                                                                \
    MD5_ROUND48_63(4, 6,   0xf7537e82);                                                                                \
    MD5_ROUND48_63(11, 10, 0xbd3af235);                                                                                \
    MD5_ROUND48_63(2, 15,  0x2ad7d2bb);                                                                                \
    MD5_ROUND48_63(9, 21,  0xeb86d391)

namespace dfdh
{

namespace md5_dtls
{
    inline u32 md5_rotl(u32 x, u32 n) {
        return (x << n) | (x >> (32U - n)); // NOLINT
    }
} // namespace md5_dtls

struct md5_hash {
    friend std::ostream& operator<<(std::ostream& os, const md5_hash& b) {
        if constexpr (std::endian::native == std::endian::little) {
            auto lo = bswap(b.lo);
            auto hi = bswap(b.hi);
            os << std::hex << std::setfill('0') << std::setw(sizeof(b.lo) * 2) << lo;
            os << std::hex << std::setfill('0') << std::setw(sizeof(b.hi) * 2) << hi;
        } else {
            os << std::hex << std::setfill('0') << std::setw(sizeof(b.lo) * 2) << b.lo;
            os << std::hex << std::setfill('0') << std::setw(sizeof(b.hi) * 2) << b.hi;
        }

        return os;
    }

    friend std::istream& operator>>(std::istream& is, md5_hash& b) {
        std::string str;
        is >> str;
        auto start = str.begin();
        auto end   = str.end();

        if (str.starts_with("0x") || str.starts_with("0X"))
            start += 2;

        if (end - start != sizeof(md5_hash) * 2)
            throw std::runtime_error("Invalid hash length");

        b.lo = 0;
        b.hi = 0;

        auto to_byte = [](auto c) -> uint {
            if ('0' <= c && c <= '9')
                return static_cast<uint>(c - '0');
            else if ('A' <= c && c <= 'F')
                return static_cast<uint>(c - 'A' + 10); // NOLINT
            else if ('a' <= c && c <= 'f')
                return static_cast<uint>(c - 'a' + 10); // NOLINT
            else
                throw std::runtime_error("Invalid hex digit");
        };

        constexpr auto size = sizeof(b.lo);
        for (size_t i = 0; i < size; ++i) {
            b.lo <<= 8; // NOLINT
            b.lo = b.lo | static_cast<uint8_t>(to_byte(start[0]) << 4) | (to_byte(start[1]));
            start += 2;
        }
        for (size_t i = 0; i < size; ++i) {
            b.hi <<= 8; // NOLINT
            b.hi = b.hi | static_cast<uint8_t>(to_byte(start[0]) << 4) | (to_byte(start[1]));
            start += 2;
        }
        if (std::endian::native == std::endian::little) {
            b.lo = bswap(b.lo);
            b.hi = bswap(b.hi);
        }

        return is;
    }

    bool operator<(const md5_hash& b) const {
        if (hi < b.hi)
            return true;
        else if (hi == b.hi)
            return lo < b.lo;
        else
            return false;
    }

    bool operator>(const md5_hash& b) const {
        if (hi < b.hi)
            return false;
        else if (hi == b.hi)
            return lo >= b.lo;
        else
            return true;
    }

    bool operator==(const md5_hash& b) const {
        return lo == b.lo && hi == b.hi;
    }

    bool operator!=(const md5_hash& b) const {
        return !(*this == b);
    }

    bool operator<=(const md5_hash& b) const {
        return !(*this > b);
    }

    bool operator>=(const md5_hash& b) const {
        return !(*this < b);
    }

    u64 lo = 0;
    u64 hi = 0;
};

inline md5_hash md5(const u8* str, size_t size) {
    constexpr size_t bits_in_byte     = 8;
    constexpr size_t block_size       = 64;
    constexpr size_t block_size_words = block_size / sizeof(u32);
    constexpr size_t append_size      = 8;
    constexpr u8     one_bit          = 0x80;

    size_t blocks            = ((size + append_size) / block_size) + 1U;
    size_t size_with_padding = blocks * block_size;

    auto data = std::vector<u8>(size_with_padding, 0);
    std::memcpy(data.data(), str, size);

    data[size] = one_bit;

    auto input_bits = static_cast<u64>(size) * bits_in_byte;

    if constexpr (std::endian::native == std::endian::big)
        input_bits = bswap(input_bits);

    memcpy(&data[(blocks * block_size) - append_size], &input_bits, sizeof(input_bits));

    struct {
        u32 a0 = 0x67452301; // NOLINT
        u32 b0 = 0xEFCDAB89; // NOLINT
        u32 c0 = 0x98BADCFE; // NOLINT
        u32 d0 = 0x10325476; // NOLINT
    } x;

    std::array<u32, block_size_words> words; // NOLINT

    for (size_t cb = 0; cb < blocks; ++cb) {
        memcpy(words.data(), &data[cb * block_size], block_size);

        u32 a = x.a0;
        u32 b = x.b0;
        u32 c = x.c0;
        u32 d = x.d0;
        u32 F;        // NOLINT
        u32 dTemp;    // NOLINT

        MD5_ROUND1();
        MD5_ROUND2();
        MD5_ROUND3();
        MD5_ROUND4();

        x.a0 += a;
        x.b0 += b;
        x.c0 += c;
        x.d0 += d;
    }

    md5_hash hash;
    memcpy(static_cast<void*>(&hash), &x, sizeof(hash));

    return hash;
}

template <typename T>
constexpr const u8* safe_u8_cast(const T* p) {
    static_assert(std::is_same_v<u8, char> || std::is_same_v<u8, unsigned char>,
            "Unnable safe cast to const uin8_t* on this platform.");
    return reinterpret_cast<const u8*>(p); // NOLINT
}

inline md5_hash md5(const char* str, size_t size) {
    return md5(safe_u8_cast(str), size);
}

inline md5_hash md5(std::string_view str) {
    return md5(str.data(), str.size());
}

inline md5_hash md5(std::span<std::byte> bytes) {
    return md5(safe_u8_cast(bytes.data()), static_cast<size_t>(bytes.size()));
}

} // namespace core

namespace std
{
template <>
struct hash<dfdh::md5_hash> {
    size_t operator()(const dfdh::md5_hash& h) {
        return static_cast<size_t>(h.lo ^ h.hi);
    }
};
} // namespace std

#undef MD5_ROUND0_15
#undef MD5_ROUND16_31
#undef MD5_ROUND32_47
#undef MD5_ROUND48_63

#undef MD5_ROUND1
#undef MD5_ROUND2
#undef MD5_ROUND3
#undef MD5_ROUND4

