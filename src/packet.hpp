#pragma once

#include <vector>
#include <cstring>

#include "types.hpp"
#include "log.hpp"

namespace dfdh {

inline uint64_t fnv1a64(const u8* ptr, size_t size) {
    uint64_t hash = 0xcbf29ce484222325;

    while (size--)
        hash = (hash ^ *ptr++) * 0x100000001b3;

    return hash;
}

inline u64 next_packet_id() {
    static u64 id = 0;
    return id++;
}

class packet_t {
public:
    template <typename... Ts>
    static constexpr size_t calc_size(const Ts&... v) {
        return 0 + (sizeof(v) + ...);
    }

    template <typename Tpl, size_t... I>
    static constexpr size_t _offset(std::index_sequence<I...>) {
        return (sizeof(std::tuple_element_t<I, Tpl>) + ... + 0);
    }

    template <typename... Ts, size_t... S>
    void _append_impl(const Ts&... args, std::index_sequence<S...>) {
        auto old_size = _data.size();
        _data.resize(_data.size() + calc_size(args...));

        auto tpl = std::tuple<const Ts&...>(args...);

        (std::memcpy(_data.data() + old_size +
                         _offset<std::tuple<Ts...>>(std::make_index_sequence<S>()),
                     &(std::get<S>(tpl)),
                     sizeof(std::tuple_element_t<S, std::tuple<Ts...>>)),
         ...);
    }

    template <typename... Ts>
    void append(const Ts&... args) {
        _append_impl<Ts...>(args..., std::make_index_sequence<sizeof...(Ts)>());
    }

    void append_raw(const u8* data, size_t size) {
        auto old_size = _data.size();
        _data.resize(old_size + size);
        std::memcpy(_data.data() + old_size, data, size);
    }

    [[nodiscard]]
    size_t size() const {
        return _data.size();
    }

    [[nodiscard]]
    const u8* data() const {
        return _data.data();
    }

    [[nodiscard]]
    u8* data() {
        return _data.data();
    }

    void resize(size_t size) {
        _data.resize(size);
    }

private:
    std::vector<u8> _data;
};

struct uniq_packet_info {
    u32 ip;
    u16 port;
    u64 id;
    u64 hash;

    auto operator<=>(const uniq_packet_info&) const = default;
};

struct packet_spec_t {
    /* Part of action data */
    u32 act;

    u32     transcontrol;
    u64     id;
    u64     hash;
};


inline bool validate_packet_size(const packet_t& packet) {
    if (packet.size() < sizeof(packet_spec_t)) {
        LOG_WARN("packet dropped: invalid size {}", packet.size());
        return false;
    }
    return true;
}

inline bool validate_packet_hash(const packet_t& packet, u64 spec_hash) {
    auto real_hash = fnv1a64(packet.data(), packet.size() - sizeof(u64));
    if (spec_hash != real_hash) {
        LOG_WARN("packet dropped: invalid hash {} (must be equal with {})", spec_hash, real_hash);
        return false;
    }
    return true;
}

inline packet_spec_t get_packet_spec(const packet_t& packet) {
    packet_spec_t spec;
    auto spec_pos = packet.size() - sizeof(packet_spec_t);
    std::memcpy(&spec,
                reinterpret_cast<const u8*>(packet.data()) + spec_pos, // NOLINT
                sizeof(packet_spec_t));
    return spec;
}

inline std::optional<packet_spec_t> get_spec_if_valid_packet(const packet_t& packet) {
    if (!validate_packet_size(packet))
        return {};

    auto spec = get_packet_spec(packet);

    if (!validate_packet_hash(packet, spec.hash))
        return {};

    return spec;
}

}
