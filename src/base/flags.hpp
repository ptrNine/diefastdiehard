#pragma once

#include <type_traits>
#include <string>

#include "types.hpp"
#include "vec_math.hpp"
#include "macro_argname.hpp"
//#include "serialization.hpp"


/**
 * Flag type implementation
 *
 * example:
 *
 * // Define flag type with A,B,C,D,E,F,G,H states
 * DEF_FLAG_TYPE(Flagger, flag8_t,
        A = Flagger::def<0>,
        B = Flagger::def<1>,
        C = Flagger::def<2>,
        D = Flagger::def<3>,
        E = Flagger::def<4>,
        F = Flagger::def<5>,
        G = Flagger::def<6>,
        H = Flagger::def<7>,
        // I = Flagger::def<8>   <- compilation error, because Flag8 is 8-bit type
    );

    Flagger flag;

    flag.set(Flagger::A); // Set flag to A   (A)
    flag.set(flag.B);     // Set flag to B   (A|B)

    // Test
    if (flag.test(Flagger::A | Flagger::B)) {
        std::cout << "Success!" << std::endl;
    }
 */

namespace dfdh
{
template <typename Type>
class flag_tmpl {
    static_assert(std::is_integral_v<Type> && std::is_unsigned_v<Type>,
                  "Flag type must be unsigned int!");
public:
    using value_type = Type;

    /*
    void serialize(vector<byte>& out) const {
        dfdh::serialize(_data, out);
    }

    void deserialize(span<const byte>& in) {
        dfdh::deserialize(_data, in);
    }
    */

    template <Type flag, auto check_value>
    struct _define_helper {
        static_assert((Type(1) << flag) == static_cast<Type>(check_value));
        static_assert(flag < sizeof(Type) * 8, "Can't define that bit!"); // NOLINT
        inline static constexpr Type def = Type(1) << flag;
    };

    template <Type flag, auto check_value = (Type(1) << flag)>
    inline static constexpr Type def = _define_helper<flag, check_value>::def;

    flag_tmpl() = default;
    flag_tmpl(Type flags): _data(flags) {}

public:
    constexpr void set     (Type flags) noexcept            { _data |= flags; }
    constexpr void set_if  (Type flags, bool expr) noexcept { if (expr) set(flags); }
    constexpr bool test_or (Type flags) const noexcept      { return _data & flags; }
    constexpr bool test    (Type flags) const noexcept      { return _data == (_data | flags); }
    constexpr void reset   () noexcept                      { _data = 0; }
    constexpr Type data    () const noexcept                { return _data; }

private:
    Type _data = 0;
};

using flag8_t  = flag_tmpl<uint8_t>;
using flag16_t = flag_tmpl<uint16_t>;
using flag32_t = flag_tmpl<uint32_t>;
using flag64_t = flag_tmpl<uint64_t>;
using flag_t   = flag_tmpl<unsigned int>;

inline std::pair<std::string_view, u32> flag_definition_to_name_bitnum(std::string_view flag_def) {
    std::string_view name;
    u32 num = 65; // NOLINT

    auto name_end = flag_def.find(' ');
    if (name_end != std::string_view::npos)
        name = flag_def.substr(0, name_end);
    else {
        name = flag_def;
        return std::pair{name, num};
    }

    auto def_start = flag_def.find('=', name_end);
    if (def_start == std::string_view::npos)
        return std::pair{name, num};

    ++def_start;
    if (def_start >= flag_def.size())
        return std::pair{name, num};

    while (def_start < flag_def.size() && (flag_def[def_start] == ' ' || flag_def[def_start] == '\t'))
        ++def_start;

    if (def_start >= flag_def.size())
        return std::pair{name, num};

    std::string_view def = flag_def.substr(def_start);
    if (!def.empty() && std::isdigit(def.front())) {
        u64 bit = 3;
        try {
            bit = def.starts_with("0x") ? std::stoull(std::string(def.substr(2)), nullptr, 16) // NOLINT
                                        : std::stoull(std::string(def));
        } catch (...) {
            bit = 3;
        }

        if (is_power_of_two(bit)) {
            decltype(bit) mask = 1;
            num = 0;
            while (!(bit & mask)) {
                mask = mask << 1;
                ++num;
            }
        }
    } else {
        auto start = def.find("::def<");
        if (start != std::string_view::npos) {
            start += sizeof("::def<") - 1;
            auto end = def.find('>', start);
            if (end == std::string_view::npos)
                end = def.size();

            if (end != start) {
                auto strnum = def.substr(start, end - start);
                try {
                    auto bitnum = std::stoull(std::string(strnum));
                    num = static_cast<u32>(bitnum);
                } catch (...) {}
            }
        }
    }

    return std::pair{name, num};
}
} // namespace core

#define DEF_FLAG_TYPE(NAME, TYPE, ...) /*NOLINT*/                                                                      \
    struct NAME : TYPE {                                                                                               \
        using t____ = TYPE;                                                                                            \
        using t____::t____;                                                                                            \
        NAME(): t____() {}                                                                                             \
        enum EnumFlag : t____::value_type { __VA_ARGS__ };                                                             \
        static constexpr std::array _introspect = {PE_ARGNAMES(__VA_ARGS__)};                                          \
        struct _introspect_details {                                                                                   \
            static constexpr size_t bitsize = 8;                                                                       \
            _introspect_details() {                                                                                    \
                for (auto& def_str : _introspect) {                                                                    \
                    auto name_bitnum = dfdh::flag_definition_to_name_bitnum(def_str);                                  \
                    if (name_bitnum.second < sizeof(t____::value_type) * bitsize)                                      \
                        _numbit_to_name.at(name_bitnum.second) = name_bitnum.first;                                    \
                }                                                                                                      \
            }                                                                                                          \
            std::array<std::string_view, sizeof(t____::value_type) * bitsize> _numbit_to_name;                         \
                                                                                                                       \
            static _introspect_details& instance() {                                                                   \
                static _introspect_details i;                                                                          \
                return i;                                                                                              \
            }                                                                                                          \
        };                                                                                                             \
        std::vector<std::string_view> enabled_info() const {                                                           \
            dfdh::u32                     bitnum = 0;                                                                  \
            t____::value_type             mask   = 1;                                                                  \
            std::vector<std::string_view> result;                                                                      \
            while (mask) {                                                                                             \
                if (this->data() & mask)                                                                               \
                    result.push_back(_introspect_details::instance()._numbit_to_name.at(bitnum));                      \
                ++bitnum;                                                                                              \
                mask = mask << 1;                                                                                      \
            }                                                                                                          \
            return result;                                                                                             \
        }                                                                                                              \
    }
