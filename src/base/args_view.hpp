#pragma once

#include <list>
#include <exception>
#include <optional>
#include <iostream>

#include "types.hpp"
#include "ston.hpp"

namespace dfdh
{
using namespace std::string_literals;

class args_view {
public:
    args_view() = delete;

    args_view(int argc, char** argv, std::vector<std::string_view> default_args = {}) {
        _name = argv[0];
        std::copy(default_args.begin(), default_args.end(), std::back_inserter(_data));
        std::copy(argv + 1, argv + argc, std::back_inserter(_data));
    }

    [[nodiscard]]
    std::string_view program_name() const {
        return _name;
    }

    bool get(std::string_view name) {
        auto found = std::find(_data.begin(), _data.end(), name);
        return found != _data.end() ? _data.erase(found), true : false;
    }

    template <typename T>
    static std::string_view type_to_str() {
        if constexpr (std::is_floating_point_v<T>)
            return "float";
        else if constexpr (std::is_signed_v<T>)
            return "integer";
        else if constexpr (std::is_unsigned_v<T>)
            return "unsigned_integer";
        else if constexpr (std::is_same_v<T, std::string>)
            return "string";
        else
            return "unknown";
    }

    template <typename T>
    static T opt_cast(std::string_view value) {
        if constexpr (Number<T>)
            return ston<T>(value);
        else
            return T(value);
    }

    template <typename T = std::string>
    std::optional<T> by_key_opt(std::string_view name) {
        auto starts_with = [&](std::string_view v) { return v.starts_with(name); };
        auto found = std::find_if(_data.begin(), _data.end(), starts_with);

        if (found != _data.end()) {
            try {
                if (*found != name && found->substr(name.length()).front() == '=') {
                    auto str = static_cast<std::string>(found->substr(name.length() + 1));
                    _data.erase(found);

                    return opt_cast<T>(str);
                }
                else if (std::next(found) != _data.end()) {
                    auto str = static_cast<std::string>(*std::next(found));
                    _data.erase(found, std::next(found, 2));

                    return opt_cast<T>(str);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Argument of '" << *found << "'"
                          << " must be a " << type_to_str<T>() << std::endl;
            }
        }

        return std::nullopt;
    }

    template <typename T = std::string>
    std::optional<T> by_key_opt(std::initializer_list<std::string_view> name_variants) {
        for (auto& name : name_variants)
            if (auto val = by_key_opt<T>(name))
                return *val;

        return std::nullopt;
    }

    template <typename T = std::string>
    T by_key_require(std::string_view name, std::string error_message = {}) {
        if (auto arg = by_key_opt<T>(name))
            return *arg;
        else {
            if (error_message.empty())
                error_message = "Missing option " + std::string(name) + "=" + std::string(type_to_str<T>());
            throw std::invalid_argument(error_message);
        }
    }

    template <typename T = std::string>
    T by_key_require(std::initializer_list<std::string_view> name_variants, std::string error_message = {}) {
        for (auto& name : name_variants)
            if (auto num = by_key_opt<T>(name))
                return *num;

        if (error_message.empty()) {
            error_message = "Missing option ";
            for (auto& name : name_variants) {
                error_message += std::string(name) + "=" + std::string(type_to_str<T>());
                break;
            }
        }
        throw std::invalid_argument(error_message);
    }

    template <typename T = std::string>
    T by_key_default(std::string_view name, T default_val) {
        if (auto arg = by_key_opt<T>(name))
            return *arg;
        else
            return default_val;
    }

    template <typename T = std::string>
    T by_key_default(std::initializer_list<std::string_view> name_variants, T default_val) {
        for (auto& name : name_variants)
            if (auto num = by_key_opt<T>(name))
                return *num;

        return default_val;
    }

    [[nodiscard]]
    size_t size() const {
        return _data.size();
    }

    [[nodiscard]]
    bool empty() const {
        return _data.empty();
    }

    std::optional<std::string> try_next() {
        if (!empty()) {
            auto res = _data.front();
            _data.pop_front();
            return std::string(res);
        }
        return {};
    }

    std::string next(std::string_view error_message) {
        if (auto arg = try_next())
            return *arg;
        else
            throw std::invalid_argument(std::string(error_message));
    }

    std::string next_default(std::string_view default_val) {
        if (auto arg = try_next())
            return *arg;
        else
            return std::string(default_val);
    }

    void require_end(std::string_view error_message = {}) {
        if (!_data.empty()) {
            if (error_message.empty())
                throw std::runtime_error("Unknown option "s + std::string(_data.front()));
            else
                throw std::runtime_error(std::string(error_message));
        }
    }

private:
    std::string_view            _name;
    std::list<std::string_view> _data;
};

} // namespace core

