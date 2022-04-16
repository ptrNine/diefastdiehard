#pragma once

#include <stdexcept>
#include <span>
#include <optional>
#include <bit>
#include <tuple>

#include "types.hpp"

#define DFDH_SERIALIZE(...)                                                                        \
    void serialize(auto& _s) const {                                                               \
        dfdh::serialize_all(_s, __VA_ARGS__);                                                      \
    }                                                                                              \
    void deserialize(std::span<const std::byte>& _d) {                                             \
        dfdh::deserialize_all(_d, __VA_ARGS__);                                                    \
    }

#define DFDH_SERIALIZE_OVERRIDE(...)                                                               \
    void serialize(auto& _s) const override {                                                      \
        dfdh::serialize_all(_s, __VA_ARGS__);                                                      \
    }                                                                                              \
    void deserialize(std::span<const std::byte>& _d) override {                                    \
        dfdh::deserialize_all(_d, __VA_ARGS__);                                                    \
    }

#define DFDH_SERIALIZE_SUPER(SUPER_CLASS, ...)                                                     \
    void serialize(auto& _s) const {                                                               \
        SUPER_CLASS::serialize(_s);                                                                \
        dfdh::serialize_all(_s, __VA_ARGS__);                                                      \
    }                                                                                              \
    void deserialize(std::span<const std::byte>& _d) {                                             \
        SUPER_CLASS::deserialize(_d);                                                              \
        dfdh::deserialize_all(_d, __VA_ARGS__);                                                    \
    }

namespace dfdh
{

class serializable_base {
public:
    virtual ~serializable_base()                             = default;
    virtual void serialize(std::vector<std::byte>& _s) const = 0;
    virtual void deserialize(std::span<const std::byte>& _d) = 0;
};

using std::back_inserter;
using std::begin;
using std::end;

template <typename T>
struct dfdh_serialize;

template <typename T>
struct dfdh_deserialize;

using byte_vector = std::vector<std::byte>;

template <typename T>
concept SerializerVec = requires (T v) {
    {v.data()} -> std::same_as<std::byte*>;
    {v.size()} -> std::convertible_to<size_t>;
    {v.resize(std::declval<size_t>())};
    {v.clear()};
};

template <typename T>
concept SerializableExternalF = requires(T& v, byte_vector& b, std::span<const std::byte>& s) {
    {dfdh_serialize<T>()(v, b)};
    {dfdh_deserialize<T>()(v, s)};
};

template <typename T>
concept SerializableMemberF = requires(T v, byte_vector& b, std::span<const std::byte>& s) {
    {v.serialize(b)};
    {v.deserialize(s)};
}
&&!SerializableExternalF<T>;

// Array with size < 32 only (prevent std::array<T, 10000000> compile-time jokes)
template <typename T>
concept SerializableTupleLike =
    !SerializableExternalF<T> && !SerializableMemberF<T> &&
    std::tuple_size<std::remove_const_t<std::remove_reference_t<T>>>::value < 32 &&
    !std::is_bounded_array_v<T>;

template <typename T>
concept SerializableIterable =
    !SerializableExternalF<T> && !SerializableMemberF<T> && !SerializableTupleLike<T> &&
    !std::is_bounded_array_v<T> && requires(T & t) {
    begin(t);
    end(t);
    back_inserter(t) = *begin(t);
    end(t) - begin(t);
};

template <typename T>
concept SerializableArray =
    !SerializableExternalF<T> && !SerializableMemberF<T> && !SerializableTupleLike<T> &&
    !std::is_bounded_array_v<T> && is_std_array<T>::value;

template <typename T>
concept SerializableMap =
    !SerializableExternalF<T> && !SerializableMemberF<T> && !SerializableTupleLike<T> &&
    !SerializableIterable<T> && !SerializableArray<T> && requires(T & t) {
    begin(t);
    end(t);
    t.size();
    { *begin(t) } -> SerializableTupleLike;
    t.emplace(get<0>(*begin(t)), get<1>(*begin(t)));
};

template <size_t size>
using byte_array = std::array<std::byte, size>;

template <typename... Ts>
auto make_byte_vector(Ts... bytes) {
    return std::vector{static_cast<std::byte>(bytes)...};
}

template <typename T>
struct FloatSizeEqualTypeHelper;

template <>
struct FloatSizeEqualTypeHelper<float> {
    using type = uint32_t;
};

template <>
struct FloatSizeEqualTypeHelper<double> {
    using type = uint64_t;
};

template <typename T>
requires Integral<T> && Unsigned<T>
inline void serialize(T val, SerializerVec auto& s);

template <typename T> requires Integral<T> &&(!Unsigned<T>)
inline void serialize(T signed_val, SerializerVec auto& s);

template <typename T> requires FloatingPoint<T>
inline void serialize(T float_val, SerializerVec auto& out);

template <typename T> requires Enum<T> &&(!Integral<T>)
inline void serialize(T enum_val, SerializerVec auto& out);

template <typename T>
inline void serialize(const std::optional<T>& o, SerializerVec auto& out);

template <SerializableTupleLike T>
inline void serialize(const T& p, SerializerVec auto& out);

template <SerializableArray T>
inline void serialize(const T& vec, SerializerVec auto& out);

template <SerializableIterable T>
inline void serialize(const T& vec, SerializerVec auto& out);

template <SerializableMap T>
inline void serialize(const T& map, SerializerVec auto& out);

template <typename T, size_t S>
inline void serialize(const T (&)[S], SerializerVec auto& out); // NOLINT

template <SerializableMemberF T>
inline void serialize(const T& v, SerializerVec auto& out) {
    v.serialize(out);
}

template <SerializableExternalF T>
inline void serialize(const T& v, SerializerVec auto& out) {
    dfdh_serialize<T>()(v, out);
}

template <typename... Ts>
static inline void serialize_all(SerializerVec auto& out, const Ts&... args) {
    ((serialize(args, out)), ...);
}


class serializer_size_reached : public std::out_of_range {
public:
    serializer_size_reached(): std::out_of_range("size of buffer reached") {}
};

template <typename Storage>
class serializer_tmpl {
public:
    serializer_tmpl() = default;

    template <typename... Ts>
    serializer_tmpl(Ts&&... args): _data(std::forward<Ts>(args)...) {}

    template <typename... Ts>
    void write(const Ts&... values) {
        (serialize(values, _data), ...);
    }

    [[nodiscard]] Storage& data() {
        return _data;
    }

    [[nodiscard]] const Storage& data() const {
        return _data;
    }

    [[nodiscard]]
    auto detach_data() {
        auto result = move(_data);
        _data       = {};
        return result;
    }

    void reset() {
        _data.clear();
    }

private:
    Storage _data;
};

using serializer = serializer_tmpl<byte_vector>;

template <typename T>
requires Integral<T> && Unsigned<T>
inline void deserialize(T& val, std::span<const std::byte>& in);

template <typename T> requires Integral<T> &&(!Unsigned<T>)
inline void deserialize(T& out, std::span<const std::byte>& in);

template <typename T> requires FloatingPoint<T>
inline void deserialize(T& out, std::span<const std::byte>& in);

template <typename T> requires Enum<T> &&(!Integral<T>)
inline void deserialize(T& out, std::span<const std::byte>& in);

template <typename T>
inline void deserialize(std::optional<T>& out, std::span<const std::byte>& in);

template <SerializableTupleLike T>
inline void deserialize(T& p, std::span<const std::byte>& in);

template <SerializableArray T>
inline void deserialize(T& vec, std::span<const std::byte>& in);

template <SerializableIterable T>
inline void deserialize(T& vec, std::span<const std::byte>& in);

template <SerializableMap T>
inline void deserialize(T& map, std::span<const std::byte>& in);

template <typename T, size_t S>
inline void deserialize(T (&)[S], std::span<const std::byte>& in); // NOLINT

template <SerializableMemberF T>
inline void deserialize(T& v, std::span<const std::byte>& in) {
    v.deserialize(in);
}

template <SerializableExternalF T>
inline void deserialize(T& v, std::span<const std::byte>& in) {
    dfdh_deserialize<T>()(v, in);
}

template <typename... Ts>
static inline void deserialize_all(std::span<const std::byte>& in, Ts&... args) {
    ((deserialize(args, in)), ...);
}

class deserializer_view {
public:
    deserializer_view(std::span<const std::byte> range): _range(range) {}

    template <typename... Ts>
    void read(Ts&... values) {
        (deserialize(values, _range), ...);
    }

    template <typename T>
    T read_get() {
        T v;
        read(v);
        return v;
    }

private:
    std::span<const std::byte> _range;
};

//==================== Unsigned Integral

template <typename T> requires Integral<T> && Unsigned<T>
inline void serialize(T val, SerializerVec auto& out) {
    if constexpr (std::endian::native == std::endian::big)
        val = bswap(val);

    out.resize(out.size() + sizeof(T));
    memcpy(out.data() + out.size() - sizeof(T), &val, sizeof(T));
}

template <typename T> requires Integral<T> && Unsigned<T>
inline void deserialize(T& val, std::span<const std::byte>& in) {
    Expects(sizeof(T) <= static_cast<size_t>(in.size()));

    T value;
    memcpy(&value, in.data(), sizeof(T));
    in = in.subspan(sizeof(T));

    if constexpr (std::endian::native == std::endian::big)
        value = bswap(value);

    val = value;
}

//===================== Signed Integral

template <typename T> requires Integral<T> &&(!Unsigned<T>)
inline void serialize(T signed_val, SerializerVec auto& out) {
    std::make_unsigned_t<T> val;
    memcpy(&val, &signed_val, sizeof(T));

    serialize(val, out);
}

template <typename T> requires Integral<T> &&(!Unsigned<T>)
inline void deserialize(T& out, std::span<const std::byte>& in) {
    std::make_unsigned_t<T> val;

    deserialize(val, in);

    T value;
    memcpy(&value, &val, sizeof(T));

    out = value;
}

//===================== FloatingPoint

template <typename T> requires FloatingPoint<T>
inline void serialize(T float_val, SerializerVec auto& out) {
    static_assert(AnyOfType<T, float, double>, "Unsupported floating point type");
    static_assert(std::numeric_limits<T>::is_iec559, "IEEE 754 required");

    typename FloatSizeEqualTypeHelper<T>::type val;
    memcpy(&val, &float_val, sizeof(T));
    serialize(val, out);
}

template <typename T> requires FloatingPoint<T>
inline void deserialize(T& out, std::span<const std::byte>& in) {
    static_assert(AnyOfType<T, float, double>, "Unsupported floating point type");
    static_assert(std::numeric_limits<T>::is_iec559, "IEEE 754 required");

    typename FloatSizeEqualTypeHelper<T>::type size_eq_int;
    deserialize(size_eq_int, in);

    T value;
    memcpy(&value, &size_eq_int, sizeof(T));
    out = value;
}

//===================== Enum

template <typename T> requires Enum<T> &&(!Integral<T>)
inline void serialize(T enum_val, SerializerVec auto& out) {
    serialize(static_cast<std::underlying_type_t<T>>(enum_val), out);
}

template <typename T> requires Enum<T> &&(!Integral<T>)
inline void deserialize(T& out, std::span<const std::byte>& in) {
    using underlying_t = std::underlying_type_t<T>;
    underlying_t res;
    deserialize(res, in);
    out = static_cast<T>(res);
}

//===================== Optional

template <typename T>
inline void serialize(const std::optional<T>& o, SerializerVec auto& out) {
    serialize(o.has_value(), out);
    if (o)
        serialize(*o, out);
}

template <typename T>
inline void deserialize(std::optional<T>& out, std::span<const std::byte>& in) {
    bool has_value;
    deserialize(has_value, in);

    if (has_value) {
        T val;
        deserialize(val, in);
        out = optional<T>(move(val));
    }
}

//====================== Tuple like

template <SerializableTupleLike T, size_t... idxs>
inline void serialize_helper(const T& p, SerializerVec auto& out, std::index_sequence<idxs...>&&) {
    using std::get;
    (serialize(get<idxs>(p), out), ...);
}

template <SerializableTupleLike T>
inline void serialize(const T& p, SerializerVec auto& out) {
    serialize_helper(
        p, out, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<T>>>());
}

template <SerializableTupleLike T, size_t... idxs>
inline void deserialize_helper(T& p, std::span<const std::byte>& in, std::index_sequence<idxs...>&&) {
    using std::get;
    (deserialize(get<idxs>(p), in), ...);
}

template <SerializableTupleLike T>
inline void deserialize(T& p, std::span<const std::byte>& in) {
    deserialize_helper(
        p, in, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<T>>>());
}

//===================== Iterable

// Special for array
template <SerializableArray T>
inline void serialize(const T& vec, SerializerVec auto& out) {
    for (auto p = begin(vec); p != end(vec); ++p) serialize(*p, out);
}

template <SerializableArray T>
inline void deserialize(T& vec, std::span<const std::byte>& in) {
    for (auto& e : vec) deserialize(e, in);
}

template <SerializableIterable T>
inline void serialize(const T& vec, SerializerVec auto& out) {
    serialize(static_cast<u64>(end(vec) - begin(vec)), out);

    for (auto p = begin(vec); p != end(vec); ++p) serialize(*p, out);
}

template <SerializableIterable T>
inline void deserialize(T& vec, std::span<const std::byte>& in) {
    auto inserter = std::back_inserter(vec);

    u64 size;
    deserialize(size, in);

    for (decltype(size) i = 0; i < size; ++i) {
        std::decay_t<decltype(*begin(vec))> v;
        deserialize(v, in);
        inserter = std::move(v);
    }
}

//==================== Map
template <SerializableMap T>
void serialize(const T& map, SerializerVec auto& out) {
    serialize(static_cast<u64>(map.size()), out);

    for (auto p = begin(map); p != end(map); ++p) serialize(*p, out);
}

template <SerializableMap T>
void deserialize(T& map, std::span<const std::byte>& in) {
    u64 size;
    deserialize(size, in);

    for (decltype(size) i = 0; i < size; ++i) {
        std::decay_t<decltype(*begin(map))> v;
        deserialize(v, in);
        map.emplace(move(v));
    }
}

//==================== C-array
template <typename T, size_t S>
void serialize(const T (&arr)[S], SerializerVec auto& out) { // NOLINT
    for (auto& v : arr) serialize(v, out);
}

template <typename T, size_t S>
void deserialize(T (&arr)[S], std::span<const std::byte>& in) { // NOLINT
    for (auto& v : arr) deserialize(v, in);
}

//===================== Span
template <typename T>
void serialize(std::span<const T> data, const T* storage_ptr, SerializerVec auto& out) {
    if (data.empty()) {
        serialize(static_cast<u64>(0), out);
        serialize(static_cast<u64>(0), out);
    }
    else {
        serialize(static_cast<u64>(&(*data.begin()) - storage_ptr), out);
        serialize(static_cast<u64>(data.size()), out);
    }
}

template <typename T>
void deserialize(std::span<T>& data, T* storage_ptr, std::span<const std::byte>& in) {
    u64 start, size;
    deserialize(start, in);
    deserialize(size, in);
    data = std::span<T>(storage_ptr + static_cast<ptrdiff_t>(start), static_cast<ptrdiff_t>(size));
}
} // namespace core
