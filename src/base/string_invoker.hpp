#pragma once

#include <string>
#include <set>
#include <functional>
#include <optional>

namespace dfdh
{

template <typename F>
class named_function {
public:
    named_function(std::string_view name): _name(name) {}

    template <typename F1>
    named_function(std::string_view name, F1 function): _name(name), _func(std::move(function)) {}

    template <typename F2>
    bool operator<(const named_function<F2>& func) const {
        return _name < func._name;
    }

    template <typename... Ts>
    decltype(auto) operator()(Ts&&... args) const {
        return _func(std::forward<Ts>(args)...);
    }

private:
    std::string _name;
    F           _func;
};

template <typename F>
named_function(std::string_view, F) -> named_function<decltype(std::function{F{}})>;

template <typename F>
class string_invoker {
public:
    template <typename... Ts>
    string_invoker(std::string arg, F function, Ts&&... args) {
        init(std::move(arg), std::move(function), std::forward<Ts>(args)...);
    }

    template <typename FT, typename... Ts>
    void init(std::string arg, FT function, Ts&&... args) {
        _fs.emplace(std::move(arg), std::move(function));
        if constexpr (sizeof...(Ts) > 1)
            init(std::forward<Ts>(args)...);
    }

    template <typename... Ts>
    auto operator()(std::string_view arg, Ts&&... args) const {
        auto found = _fs.find(arg);
        if constexpr (std::is_same_v<void, decltype((*found)(std::forward<Ts>(args)...))>) {
            if (found != _fs.end())
                (*found)(std::forward<Ts>(args)...);
        }
        else {
            using RT = decltype(std::optional{(*found)(std::forward<Ts>(args)...)});
            if (found != _fs.end())
                return std::optional{(*found)(std::forward<Ts>(args)...)};
            else
                return RT{};
        }
    }

private:
    std::set<named_function<decltype(std::function{F{}})>> _fs;
};

template <typename F, typename... Ts>
string_invoker(std::string, F, Ts...) -> string_invoker<decltype(std::function{F{}})>;

/*
template <typename F, typename... Fs>
    requires (!std::is_same_v<F, Fs> || ...)
string_invoker(string_invoker_function<F>, string_invoker_function<Fs>...)
    -> string_invoker<std::function<std::remove_pointer_t<F>>>;
*/

} // namespace dfdh
