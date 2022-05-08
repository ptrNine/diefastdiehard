#pragma once

#include <boost/coroutine2/coroutine.hpp>

#include "traits.hpp"

namespace dfdh {

template <typename T>
using boost_coroutine = boost::coroutines2::coroutine<T>;

template <typename T>
struct coroutine_return_wrapper {
    template <typename U>
    void accept(U&& value) {
        holder = std::forward<U>(value);
    }

    auto& get() {
        return holder;
    }

    T holder;
};

template <typename T>
struct coroutine_return_wrapper<T&> {
    coroutine_return_wrapper() = default;
    coroutine_return_wrapper(T& ref): holder(&ref) {}

    void accept(T& value) {
        holder = &value;
    }

    auto& get() {
        return *holder;
    }

    T* holder = nullptr;
};

template <typename T, typename R>
struct coroutine_value_wrapper {
    coroutine_return_wrapper<R>* return_value;
    T  input_value;
};

template <typename T, typename R = void>
class coroutine_ctx {
public:
    using value_wrapper_t = coroutine_value_wrapper<T, R>;
    using value_t = T;
    using return_t = R;

    coroutine_ctx(typename boost_coroutine<value_wrapper_t>::pull_type& pull): pull_v(pull) {}

    template <typename ReturnT>
    void yield(ReturnT&& ret) {
        return_value->accept(std::forward<ReturnT>(ret));
        pull_v();
    }

    T next() {
        auto res = pull_v.get();
        return_value = res.return_value;
        return res.input_value;
    }

private:
    typename boost_coroutine<value_wrapper_t>::pull_type& pull_v;
    coroutine_return_wrapper<R>* return_value;
};

template <typename T>
class coroutine_ctx<T, void> {
public:
    using value_t = T;
    using return_t = void;

    coroutine_ctx(typename boost_coroutine<T>::pull_type& pull): pull_v(pull) {}

    void yield() {
        pull_v();
    }

    T next() {
        return std::move(pull_v.get());
    }

private:
    typename boost_coroutine<T>::pull_type& pull_v;
};

template <typename R>
class coroutine_ctx<void, R> {
public:
    using value_t = void;
    using return_t = R;

    coroutine_ctx(typename boost_coroutine<coroutine_return_wrapper<R>*>::pull_type& pull):
        pull_v(pull), return_value(pull_v.get()) {}

    template <typename ReturnT>
    void yield(ReturnT&& ret) {
        return_value->accept(std::forward<ReturnT>(ret));
        pull_v();
    }

private:
    typename boost_coroutine<coroutine_return_wrapper<R>*>::pull_type& pull_v;
    coroutine_return_wrapper<R>* return_value;
};


template <typename T, typename R = void>
class coroutine {
public:
    using value_wrapper_t = coroutine_value_wrapper<T, R>;
    using ctx = coroutine_ctx<T, R>;

    template <typename F>
    coroutine(F&& func): cr(std::forward<F>(func)) {}

    R operator()(const T& value) {
        cr(value_wrapper_t{&return_value, value});
        return return_value.get();
    }

private:
    coroutine_return_wrapper<R> return_value;
    typename boost_coroutine<value_wrapper_t>::push_type cr;
};

template <typename T>
class coroutine<T, void> {
public:
    using ctx = coroutine_ctx<T, void>;

    template <typename F>
    coroutine(F&& func): cr(std::forward<F>(func)) {}

    void operator()(T value) {
        cr(value);
    }

private:
    typename boost_coroutine<T>::push_type cr;
};

template <typename R>
class coroutine<void, R> {
public:
    using ctx = coroutine_ctx<void, R>;

    template <typename F>
    coroutine(F&& func): cr(std::forward<F>(func)) {}

    R operator()() {
        cr(&return_value);
        return return_value.get();
    }

private:
    coroutine_return_wrapper<R> return_value;
    typename boost_coroutine<coroutine_return_wrapper<R>*>::push_type cr;
};

template <typename F>
coroutine(F&&) -> coroutine<typename function_traits<F>::template arg_t<0>::value_t,
                            typename function_traits<F>::template arg_t<0>::return_t>;

} // namespace dfdh
