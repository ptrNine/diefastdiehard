#pragma once

#include <fstream>
#include <filesystem>

#include "base/log.hpp"

namespace luacpp {
    class luactx;
}

namespace dfdh
{

namespace fs = std::filesystem;
using namespace std::chrono_literals;

template <typename F>
class lua_caller;

class luactx_mgr {
private:
    template <typename RT>
    struct _opt_return;

    template <typename RT, typename... ArgsT>
    struct _opt_return<RT(ArgsT...)> {
        using return_t = RT;
        auto operator()() {
            if constexpr (!std::is_same_v<RT, void>)
                return std::optional<RT>();
            return false;
        }
    };


public:
    static luactx_mgr global(bool deferred_load = false) {
        return {"global", deferred_load};
    }

    static luactx_mgr ai(bool deferred_load = false) {
        return {"ai", deferred_load};
    }

    operator bool() const {
        return loaded;
    }

    luacpp::luactx& ctx() {
        return *_ctx;
    }

    void load();

    void execute_line(const std::string& line);

    template <typename F, typename... ArgsT>
    decltype(_opt_return<F>()()) try_call_function(std::string name, ArgsT&&... args);

    template <typename F, typename... ArgsT>
    typename _opt_return<F>::return_t call_function(std::string name, ArgsT&&... args);

    template <typename F>
    lua_caller<F>
    get_caller(std::string name, std::chrono::microseconds retry_timeout = 0us, bool suppress_error = false);

    template <typename F>
    std::function<F> get_caller_type_erased(std::string               name,
                                            std::chrono::microseconds retry_timeout  = 0us,
                                            bool                      suppress_error = false);

private:
    luactx_mgr(std::string instance_name, bool deferred_load);

private:
    std::unique_ptr<luacpp::luactx> _ctx;
    std::string                     _instance_name;
    bool                            loaded = false;
};

class lua_runner {
public:
    private:

};

} // namespace dfdh
