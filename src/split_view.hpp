#pragma once

#include <array>
#include "types.hpp"
#include "range.hpp"
#include <algorithm>

namespace dfdh {

namespace details {
    template <typename I>
    concept StringViewConstruct = requires(I b) {
        string_view(b, b);
    };
}

struct skip_whitespace_outside_quotes {
    enum { no_quote = 0, on_single, on_double } state = no_quote;

    bool operator()(char c) {
        switch (state) {
        case no_quote:
            switch (c) {
            case '"': state = on_double; return true;
            case '\'': state = on_single; return true;
            case ' ':
            case '\t': return true;
            default: break;
            }
            break;
        case on_single:
            if (c == '\'') {
                state = no_quote;
                return true;
            }
            break;
        case on_double:
            if (c == '"') {
                state = no_quote;
                return true;
            }
            break;
        }
        return false;
    }
};

template <typename I, size_t Ndelims>
class split_iterator {
public:
    using non_const_t = std::decay_t<decltype(*std::declval<I>())>;

    template <typename TT>
    static auto make_range(TT ibegin, TT iend) {
        return range(ibegin, iend);
    }

    using value_type = decltype(make_range(std::declval<I>(), std::declval<I>()));
    using reference = value_type;

    split_iterator(I                                       start,
                   I                                       iend,
                   const std::array<non_const_t, Ndelims>& idelims,
                   bool                                    iallow_empty = false):
        b(start), e(iend), end(iend), delims(idelims), allow_empty(iallow_empty) {
        init();
    }

    split_iterator& operator++() {
        next();
        return *this;
    }

    split_iterator operator++(int) {
        auto result = *this;
        ++(*this);
        return result;
    }

    auto operator*() {
        return make_range(b, e);
    }

    [[nodiscard]]
    friend bool operator==(const split_iterator& rhs, const split_iterator& lhs) {
        //assert(rhs.delims == lhs.delims && rhs.allow_empty == lhs.allow_empty);
        return rhs.b == lhs.b;
    }

private:
    static bool has_delim(const std::array<non_const_t, Ndelims>& delims, const non_const_t& c) {
        return std::any_of(
            delims.begin(), delims.end(), [&c](const non_const_t& d) { return d == c; });
    }

    void init() {
        if (!allow_empty)
            while (b != end && has_delim(delims, *b))
                ++b;
        e = b;
        while (e != end && !has_delim(delims, *e))
            ++e;
    }

    void next() {
        bool repeat = true;

        while (repeat) {
            b = e;
            if (b == end)
                return;

            e = ++b;
            while (e != end && !has_delim(delims, *e))
                ++e;

            repeat = !allow_empty && b == e;
        }
    }

private:
    I b, e, end;
    std::array<non_const_t, Ndelims> delims;
    bool allow_empty = false;
};


template <typename I, size_t Ndelims>
class split_viewer {
public:
    using value_type = std::remove_reference_t<decltype(*std::declval<I>())>;
    using T = std::remove_const_t<value_type>;

    split_viewer(Range auto&&                  container,
                 const std::array<T, Ndelims>& delims,
                 bool                          allow_empty = false):
        b(std::begin(container), std::end(container), delims, allow_empty),
        e(std::end(container), std::end(container), delims, allow_empty) {}

    split_iterator<I, Ndelims> begin() {
        return b;
    }

    split_iterator<I, Ndelims> end() {
        return e;
    }

private:
    split_iterator<I, Ndelims> b, e;
};

enum class split_mode : u8 {
    skip_empty,
    allow_empty
};

template <typename ContainerT, typename T, size_t Ndelims>
split_viewer(ContainerT&&,
             const std::array<T, Ndelims>&,
             bool) -> split_viewer<decltype(begin(std::declval<ContainerT>())), Ndelims>;

template <typename T, size_t Ndelims>
struct split {
    template <typename C, typename... Cs>
    split(C delim, Cs... delims):
        _delims{std::move(delim), std::move(delims)...}, _mode(split_mode::skip_empty) {}

    template <typename C, typename... Cs>
    split(split_mode mode, C delim, Cs... delims):
        _delims{std::move(delim), std::move(delims)...}, _mode(mode) {}

    template <Range Rng>
    friend auto operator/(Rng&& container, const split& sv) {
        return split_viewer(std::forward<Rng>(container), sv._delims, sv._mode == split_mode::allow_empty);
    }

    std::array<T, Ndelims> _delims;
    split_mode _mode;
};

template <typename C, typename... Cs>
split(C, Cs...) -> split<C, sizeof...(Cs) + 1>;

template <typename C, typename... Cs>
split(split_mode, C, Cs...) -> split<C, sizeof...(Cs) + 1>;


template <typename I, typename F>
class split_when_iterator {
public:
    using non_const_t = std::decay_t<decltype(*std::declval<I>())>;

    template <typename TT>
    static auto make_range(TT ibegin, TT iend) {
        return range(ibegin, iend);
    }

    using value_type = decltype(make_range(std::declval<I>(), std::declval<I>()));
    using reference = value_type;

    split_when_iterator(I start, I iend, F binary_op, bool iallow_empty = false):
        b(start), e(iend), end(iend), _op(std::move(binary_op)), allow_empty(iallow_empty) {
        init();
    }

    split_when_iterator& operator++() {
        next();
        return *this;
    }

    split_when_iterator operator++(int) {
        auto result = *this;
        ++(*this);
        return result;
    }

    auto operator*() {
        return make_range(b, e);
    }

    [[nodiscard]]
    friend bool operator==(const split_when_iterator& rhs, const split_when_iterator& lhs) {
        //assert(rhs.allow_empty == lhs.allow_empty);
        return rhs.b == lhs.b;
    }

private:
    void init() {
        if (!allow_empty)
            while (b != end && _op(*b))
                ++b;
        e = b;
        while (e != end && !_op(*e))
            ++e;
    }

    void next() {
        bool repeat = true;

        while (repeat) {
            b = e;
            if (b == end)
                return;

            e = ++b;
            while (e != end && !_op(*e))
                ++e;

            repeat = !allow_empty && b == e;
        }
    }

private:
    I b, e, end;
    F _op;
    bool allow_empty = false;
};


template <typename I, typename F>
class split_when_viewer {
public:
    using value_type = std::remove_reference_t<decltype(*std::declval<I>())>;
    using T = std::remove_const_t<value_type>;

    split_when_viewer(Range auto&&  container,
                      F             binary_op,
                      bool          allow_empty = false):
        b(std::begin(container), std::end(container), std::move(binary_op), allow_empty),
        e(std::end(container), std::end(container), std::move(binary_op), allow_empty) {}

    split_when_iterator<I, F> begin() {
        return b;
    }

    split_when_iterator<I, F> end() {
        return e;
    }

private:
    split_when_iterator<I, F> b, e;
};

template <typename ContainerT, typename F>
split_when_viewer(ContainerT&&,
                  F,
                  bool) -> split_when_viewer<decltype(begin(std::declval<ContainerT>())), F>;

template <typename F>
struct split_when {
    split_when(F binary_op):
        _op(std::move(binary_op)), _mode(split_mode::skip_empty) {}

    split_when(split_mode mode, F binary_op):
        _op(std::move(binary_op)), _mode(mode) {}

    template <Range Rng>
    friend auto operator/(Rng&& container, const split_when& sv) {
        return split_when_viewer(std::forward<Rng>(container), sv._op, sv._mode == split_mode::allow_empty);
    }

    F _op;
    split_mode _mode;
};

} // namespace core::views

template <typename T, size_t Ndelims>
struct std::iterator_traits<dfdh::split_iterator<T, Ndelims>> {
    using value_type = typename dfdh::split_iterator<T, Ndelims>::value_type;
    using reference = value_type;
    using difference_type = ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
};

template <typename T, typename F>
struct std::iterator_traits<dfdh::split_when_iterator<T, F>> {
    using value_type = typename dfdh::split_when_iterator<T, F>::value_type;
    using reference = value_type;
    using difference_type = ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
};
