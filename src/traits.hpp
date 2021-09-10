#pragma once

#include <tuple>
#include <functional>

namespace dfdh
{
namespace details
{
    template <typename F>
    struct function_traits_impl;

    template <typename RetT, typename... ArgsT>
    struct function_traits_impl<RetT(ArgsT...)> {
        using return_t                     = RetT;
        static constexpr inline auto arity = sizeof...(ArgsT);
        template <std::size_t N>
        using arg_t = std::tuple_element_t<N, std::tuple<ArgsT...>>;
    };

    template <typename RetT, typename... ArgsT>
    struct function_traits_impl<RetT (*)(ArgsT...)> : function_traits_impl<RetT(ArgsT...)> {};

    template <typename RetT, typename T, typename... ArgsT>
    struct function_traits_impl<RetT (T::*)(ArgsT...)> : function_traits_impl<RetT(ArgsT...)> {};

    template <typename RetT, typename T, typename... ArgsT>
    struct function_traits_impl<RetT (T::*)(ArgsT...) const>
        : function_traits_impl<RetT(ArgsT...)> {};

    template <typename RetT, typename... ArgsT>
    struct function_traits_impl<std::function<RetT(ArgsT...)>>
        : function_traits_impl<RetT(ArgsT...)> {};
} // namespace details

template <typename F, typename = void>
struct function_traits : details::function_traits_impl<F> {};

template <typename F>
struct function_traits<F, std::void_t<decltype(&F::operator())>>
    : details::function_traits_impl<decltype(&F::operator())> {};

} // namespace dfdh
