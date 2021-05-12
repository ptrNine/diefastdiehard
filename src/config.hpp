#pragma once

#include <map>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <optional>

#include "types.hpp"
#include "vec_math.hpp"
#include "split_view.hpp"

namespace dfdh {

namespace fs = std::filesystem;

class cfg_singleton {
public:
    static cfg_singleton& instance() {
        static auto inst = cfg_singleton();
        return inst;
    }

    cfg_singleton(const cfg_singleton&) = delete;
    cfg_singleton& operator=(const cfg_singleton&) = delete;
    cfg_singleton(cfg_singleton&&) = delete;
    cfg_singleton& operator=(cfg_singleton&&) = delete;

    static std::string_view trim_spaces(std::string_view str) {
        while (!str.empty() && (str.front() == ' ' || str.front() == '\t'))
            str = str.substr(1);
        while (!str.empty() && (str.back() == ' ' || str.back() == '\t'))
            str = str.substr(0, str.size() - 1);
        return str;
    }

    [[nodiscard]]
    decltype(auto) sections() const {
        return _sections;
    }

private:
    cfg_singleton() {
        parse(fs::current_path() / fs::path("fs.cfg"));
    }

    ~cfg_singleton() = default;

public:
    void parse(const fs::path& file) {
        auto ifs = std::ifstream(file.string(), std::ios::in);
        if (!ifs.is_open())
            throw std::runtime_error("Can't open config file: " + file.string());

        std::string cur_sect;

        for (std::string line; std::getline(ifs, line);) {
            auto l = trim_spaces(line);
            if (cur_sect.empty()) {
                if (l.starts_with("include")) {
                    auto found = l.find(':');
                    if (found != std::string_view::npos)
                        parse(file.parent_path() / fs::path(trim_spaces(l.substr(found + 1))));
                } else if (l.starts_with('[')) {
                    auto found = l.find(']');
                    if (found != std::string_view::npos) {
                        cur_sect = trim_spaces(l.substr(1, found - 1));
                        _sections[cur_sect];
                    }
                }
            } else {
                if (l.starts_with('[')) {
                    auto found = l.find(']');
                    if (found != std::string_view::npos) {
                        cur_sect = trim_spaces(l.substr(1, found - 1));
                        _sections[cur_sect];
                    }
                } else {
                    auto found = l.find('=');
                    if (found == std::string_view::npos) {
                        _sections[cur_sect][std::string(l)];
                    } else {
                        auto key = trim_spaces(l.substr(0, found));
                        auto value = trim_spaces(l.substr(found + 1));
                        _sections[cur_sect][std::string(key)] = value;
                    }
                }
            }
        }
    }

    [[nodiscard]]
    std::optional<std::string> get_raw(const std::string& section, const std::string& key) const {
        auto sect_i = _sections.find(section);
        if (sect_i == _sections.end())
            throw std::runtime_error("Section " + section + " not found");

        auto value_i = sect_i->second.find(key);
        if (value_i == sect_i->second.end())
            return std::nullopt;

        return value_i->second;
    }

    template <typename T>
    std::optional<T> get(const std::string& section, const std::string& key) const {
        auto raw = get_raw(section, key);
        if (!raw)
            return std::nullopt;

        return cast<T>(*raw, section, key);
    }

    template <typename T>
    T get_req(const std::string& section, const std::string& key) const {
        if (auto v = get<T>(section, key))
            return v.value();
        else
            throw std::runtime_error("Key " + key + " not found in section " + section);
    }

    template <typename T>
    T get_default(const std::string& section, const std::string& key, const T& default_value) {
        if (auto res = get<T>(section, key))
            return *res;
        else
            return default_value;
    }

private:
    template <typename T>
    static T cast(const std::string& raw, const std::string& section, const std::string& key) {
        if constexpr (std::is_same_v<T, bool>)
            return raw == "true" || raw == "on";
        else if constexpr (std::is_integral_v<T>)
            return static_cast<T>(std::stoll(raw));
        else if constexpr (std::is_floating_point_v<T>)
            return static_cast<T>(std::stod(raw));
        else if constexpr (std::is_same_v<T, sf::Vector2f>) {
            auto found = raw.find(' ');
            if (found == std::string::npos) {
                return sf::Vector2f(std::stof(std::string(trim_spaces(raw))), 0.f);
            }
            else {
                return sf::Vector2f(std::stof(std::string(trim_spaces(raw.substr(0, found)))),
                                    std::stof(std::string(trim_spaces(raw.substr(found + 1)))));
            }
        }
        else if constexpr (StdVector<T>) {
            T res;
            for (auto v : raw / split(' '))
                res.push_back(cast<typename T::value_type>(std::string(v.begin(), v.end()), section, key));
            return res;
        }
        else if constexpr (StdArray<T>) {
            T res;
            std::vector<std::string> splits;
            for (auto v : raw / split(' '))
                splits.push_back(std::string(v.begin(), v.end()));

            if (res.size() > splits.size())
                throw std::runtime_error("Error reading key " + key + " in section " + section + ": " +
                        std::to_string(res.size()) + " args required but there are only " +
                        std::to_string(splits.size()) + " args.");

            for (size_t i = 0; i < res.size(); ++i)
                res[i] = cast<typename T::value_type>(splits[i], section, key);
            return res;
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return raw;
        } else {
            throw std::runtime_error("Unknown type");
        }

    }

private:
    std::map<std::string, std::map<std::string, std::string>> _sections;
};

inline cfg_singleton& cfg() {
    return cfg_singleton::instance();
}

}
