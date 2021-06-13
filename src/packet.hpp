#pragma once

#include <vector>
#include <cstring>

#include "types.hpp"

namespace dfdh {
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
}
