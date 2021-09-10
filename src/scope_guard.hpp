#pragma once

#include <functional>
#include <exception>

namespace dfdh
{
template <typename F, typename FF = decltype(std::function{std::declval<F>()})>
class scope_guard {
public:
    scope_guard(F&& function): func(std::forward<F>(function)) {}
    ~scope_guard() noexcept(noexcept(std::declval<F>()())) {
        if (func) {
            if constexpr (noexcept(std::declval<F>()()))
                func();
            else {
                if (std::uncaught_exceptions()) {
                    try {
                        func();
                    }
                    catch (...) {
                        std::terminate();
                    }
                }
                else {
                    func();
                }
            }
        }
    }
    scope_guard(scope_guard&&) noexcept = default;
    scope_guard& operator=(scope_guard&&) noexcept = default;
    scope_guard(const scope_guard&)       = delete;
    scope_guard& operator=(const scope_guard&) = delete;

    void dismiss() {
        func = {};
    }

private:
    FF func;
};

} // namespace cr
