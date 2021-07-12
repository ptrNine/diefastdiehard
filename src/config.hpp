#pragma once

#include <map>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <optional>
#include <SFML/Graphics/Color.hpp>

#include "types.hpp"
#include "vec_math.hpp"
#include "split_view.hpp"

namespace dfdh {

namespace fs = std::filesystem;

class cfg_section {
public:
    cfg_section(std::string isection_name, std::string ifile_path):
        section_name(std::move(isection_name)), file_path(std::move(ifile_path)) {}

    [[nodiscard]]
    std::string content() const {
        size_t indent = 0;
        for (auto& v : sects)
            indent = std::max(indent, v.first.size());

        std::string sect_content;
        for (auto& v : sects) {
            sect_content += v.first;
            sect_content.resize(sect_content.size() + (indent - v.first.size()), ' ');
            sect_content += " = ";
            sect_content += v.second;
            sect_content += '\n';
        }
        if (!sect_content.empty() && sect_content.back() == '\n')
            sect_content.pop_back();
        return sect_content;
    }

    std::string section_name;
    std::string file_path;
    std::map<std::string, std::string> sects;
};

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
    auto& sections() const {
        return _sections;
    }

    [[nodiscard]]
    auto& sections() {
        return _sections;
    }

private:
    cfg_singleton() {
        parse(fs::current_path() / fs::path("fs.cfg"));
    }

    ~cfg_singleton() = default;

public:
    bool try_parse(const fs::path& file) {
        try {
            parse(file);
        } catch (...) {
            return false;
        }
        return true;
    }

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
                        _sections.insert_or_assign(cur_sect, cfg_section(cur_sect, file));
                    }
                }
            } else {
                if (l.starts_with('[')) {
                    auto found = l.find(']');
                    if (found != std::string_view::npos) {
                        cur_sect = trim_spaces(l.substr(1, found - 1));
                        _sections.insert_or_assign(cur_sect, cfg_section(cur_sect, file));
                    }
                } else {
                    auto found = l.find('=');
                    if (found == std::string_view::npos) {
                        if (!l.empty()) {
                            auto& sect = _sections.at(cur_sect);
                            sect.section_name = cur_sect;
                            sect.file_path = file;
                            sect.sects[std::string(l)];
                        }
                    } else {
                        auto key = trim_spaces(l.substr(0, found));
                        auto value = trim_spaces(l.substr(found + 1));
                        auto& sect = _sections.at(cur_sect);
                        sect.section_name = cur_sect;
                        sect.file_path = file;
                        sect.sects[std::string(key)] = value;
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

        auto value_i = sect_i->second.sects.find(key);
        if (value_i == sect_i->second.sects.end())
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

    template <typename T>
    T get_or_write_default(const std::string& section, const std::string& key, const T& default_value, const std::string& config_path, bool refresh_file = false) {
        try {
            return get_req<T>(section, key);
        }
        catch (...) {
            auto& sect = _sections.emplace(section, cfg_section(section, config_path)).first->second;
            sect.sects[key] = str_cast(default_value);
            if (refresh_file)
                try_refresh_file(section);
            return default_value;
        }
    }

    /*
    template <typename F>
    void parse_or_write(const std::string& file_path, F default_writer) {
        if (!try_parse(file_path)) {
            {
                auto ofs = std::ofstream(file_path, std::ios_base::out);
                if (!ofs.is_open())
                    throw std::runtime_error("Can't write config file " + file_path);

                default_writer(ofs);
            }
            parse(file_path);
        }
    }
    */

    template <typename T>
    bool try_write(const std::string& section, const std::string& key, const T& value, bool create = false, bool file_refresh = true) {
        auto found_sect = _sections.find(section);
        if (found_sect == _sections.end())
            return false;

        auto& sect = found_sect->second;
        std::string* val;
        if (create)
            val = &sect.sects[key];
        else {
            auto found_value = sect.sects.find(key);
            if (found_value == sect.sects.end())
                return false;
            val = &found_value->second;
        }

        *val = str_cast(value, *val);

        if (file_refresh)
            try_refresh_file(section);

        return true;
    }

    bool try_refresh_file(const std::string& section) {
        static constexpr auto space = [](char c) {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r';
        };

        auto found_sect = _sections.find(section);
        if (found_sect == _sections.end())
            return false;
        auto& sect = found_sect->second;

        std::string file;
        do {
            auto ifs = std::ifstream(sect.file_path, std::ios_base::binary | std::ios_base::in);
            if (!ifs.is_open())
                break;

            ifs.seekg(0, std::ios_base::end);
            auto pos = ifs.tellg();
            ifs.seekg(0, std::ios_base::beg);
            auto size = static_cast<size_t>(pos - ifs.tellg());

            file.resize(size);
            ifs.read(file.data(), static_cast<std::streamsize>(size));
        } while (false);

        auto sect_name = "[" + sect.section_name + "]";
        auto sect_pos = file.find(sect_name);
        if (sect_pos == std::string::npos) {
            /* Write new section */
            auto ofs = std::ofstream(sect.file_path, std::ios_base::binary | std::ios_base::out | std::ios_base::app);

            ofs << "\n\n";

            auto sect_content = sect_name + "\n" + sect.content();
            ofs.write(sect_content.data(), static_cast<std::streamsize>(sect_content.size()));

            return true;
        }

        sect_pos += sect_name.size();
        while (sect_pos != file.size() && (file[sect_pos] == '\r' || file[sect_pos] == '\n'))
            ++sect_pos;

        auto sect_end = file.find('[', sect_pos);
        if (sect_end == std::string::npos)
            sect_end = file.size();

        auto non_space_end = sect_end - 1;
        while (non_space_end > sect_pos && space(file[non_space_end]))
            --non_space_end;

        sect_end = non_space_end + 1;

        file.replace(sect_pos, sect_end - sect_pos, sect.content());

        auto ofs = std::ofstream(sect.file_path, std::ios_base::binary | std::ios_base::out);
        if (!ofs.is_open())
            return false;

        ofs.write(file.data(), static_cast<std::streamsize>(file.size()));
        return true;
    }

private:
    template <typename T>
    static std::string str_cast(const T& value, const std::string& prev_value = "") {
        if constexpr (std::is_same_v<T, bool>) {
            if (prev_value == "on" || prev_value == "off")
                return value ? "on" : "off";
            else
                return value ? "true" : "false";
        }
        else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>)
            return std::to_string(value);
        else if constexpr (std::is_same_v<T, sf::Vector2f>) {
            return std::to_string(value.x) + " " + std::to_string(value.y);
        }
        else if constexpr (StdVector<T> || StdArray<T>) {
            std::string result;
            for (auto& v : value) {
                result += str_cast(v);
                result.push_back(' ');
            }
            if (!result.empty() && result.back() == ' ')
                result.pop_back();
        }
        else if constexpr (std::is_same_v<T, sf::Color>) {
            std::stringstream ss;
            ss << value;
            return ss.str();
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return value;
        } else {
            throw std::runtime_error("Unknown type");
        }
    }

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
        else if constexpr (std::is_same_v<T, sf::Color>) {
            sf::Color res;
            std::stringstream ss{raw};
            ss >> res;
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
    std::map<std::string, cfg_section> _sections;
};

inline cfg_singleton& cfg() {
    return cfg_singleton::instance();
}

}
