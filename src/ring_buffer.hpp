#pragma once

#include <vector>

namespace dfdh {

template <typename T, typename Allocator = std::allocator<T>>
class ring_buffer {
public:
    template <typename IterT>
    class iterator_t {
    public:
        iterator_t(IterT begin, IterT end, size_t position):
            b(begin), e(end), pos(position), sz(size_t(end - begin)) {}

        [[nodiscard]]
        bool operator==(const iterator_t& iter) const {
            return pos == iter.pos;
        }

        decltype(auto) operator*() {
            return *(b + ssize_t(pos % sz));
        }

        decltype(auto) operator->() {
            return (b + ssize_t(pos % sz)).operator->();
        }

        iterator_t& operator++() {
            ++pos;
            return *this;
        }

        iterator_t operator++(int) {
            auto res = *this;
            ++(*this);
            return res;
        }

        iterator_t& operator+=(std::ptrdiff_t i) {
            if (i < 0)
                return operator-=(-i);
            else
                pos += size_t(i);
            return *this;
        }

        iterator_t& operator-=(std::ptrdiff_t i) {
            if (i < 0)
                return operator+=(-i);
            else {
                if (i > ssize_t(pos))
                    pos = sz - (size_t(i) - pos) % sz;
                else
                    pos -= size_t(i);
            }
            return *this;
        }

        iterator_t operator+(std::ptrdiff_t i) {
            auto res = *this;
            res += i;
            return res;
        }

        iterator_t operator-(std::ptrdiff_t i) {
            auto res = *this;
            res -= i;
            return res;
        }

        iterator_t& operator--() {
            if (pos == 0) {
                pos = sz - 1;
            } else
                --pos;
            return *this;
        }

        iterator_t operator--(int) {
            auto res = *this;
            --(*this);
            return res;
        }

    private:
        IterT b;
        IterT e;
        size_t pos = 0;
        size_t sz;
    };

    using iterator = iterator_t<typename std::vector<T, Allocator>::iterator>;
    using const_iterator = iterator_t<typename std::vector<T, Allocator>::const_iterator>;

    ring_buffer(size_t size): _data(size > 0 ? size : 1) {}

    void push(T value) {
        if (_size == _data.size())
            _start = (_start + 1) % _data.size();
        else
            ++_size;

        _data[_insert] = std::move(value);
        _insert = (_insert + 1) % _data.size();
    }

    void pop() {
        --_size;
        if (_insert == 0)
            _insert = _data.size() - 1;
        else
            --_insert;
        _data[_insert] = {};
    }

    [[nodiscard]]
    bool empty() const {
        return _size == 0;
    }

    void skip() {
        if (_size == _data.size()) {
            _data[_start] = {};
            _start = (_start + 1) % _data.size();
        }
        else {
            ++_size;
        }

        _insert = (_insert + 1) % _data.size();
    }

    [[nodiscard]]
    size_t size() const {
        return _size;
    }

    decltype(auto) back() {
        return _data[_insert == 0 ? _data.size() - 1 : _insert - 1];
    }

    decltype(auto) back() const {
        return _data[_insert == 0 ? _data.size() - 1 : _insert - 1];
    }

    auto begin() {
        return iterator(_data.begin(), _data.end(), _start);
    }

    auto end() {
        return iterator(_data.begin(),
                        _data.end(),
                        _insert + (_start > _insert || _size == _data.size() ? _data.size() : 0));
    }

    auto begin() const {
        return const_iterator(_data.begin(), _data.end(), _start);
    }

    auto end() const {
        return const_iterator(_data.begin(),
                              _data.end(),
                              _insert +
                                  (_start > _insert || _size == _data.size() ? _data.size() : 0));
    }

private:
    std::vector<T, Allocator> _data;
    size_t _start  = 0;
    size_t _insert = 0;
    size_t _size   = 0;
};

}
