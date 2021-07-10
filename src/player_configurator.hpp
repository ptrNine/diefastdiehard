#pragma once

#include <SFML/Graphics/Color.hpp>
#include <filesystem>

#include "fixed_string.hpp"
#include "log.hpp"
#include "config.hpp"

namespace dfdh {
class player_configurator {
public:
    player_configurator(const player_name_t& player_name): name(player_name) {
        read();
    }

    ~player_configurator() {
        if (write_on_delete)
            write();
    }

    void read() {
        auto cfgpath = "data/settings/players_settings.cfg"s;
        cfg().try_parse(cfgpath);

        auto sect = "player_settings_" + std::string(name);

        pistol       = cfg().get_or_write_default<std::string>(sect, "pistol", "wpn_glk17", cfgpath);
        face_id      = cfg().get_or_write_default<u32>(sect, "face_id", 0, cfgpath);
        body_id      = cfg().get_or_write_default<u32>(sect, "body_id", 0, cfgpath);
        body_color   = cfg().get_or_write_default<sf::Color>(sect, "body_color", sf::Color(255, 255, 255), cfgpath);
        tracer_color = cfg().get_or_write_default<sf::Color>(sect, "tracer_color", sf::Color(255, 255, 255), cfgpath);

        cfg().try_refresh_file(sect);
    }

    void write() {
        auto  sect = "player_settings_" + std::string(name);
        auto& s    = cfg()
                      .sections()
                      .emplace(sect, cfg_section(sect, "data/settings/players_settings.cfg"))
                      .first->second;

        s.sects["pistol"] = pistol;
        s.sects["face_id"] = std::to_string(face_id);
        s.sects["body_id"] = std::to_string(body_id);
        s.sects["body_color"] = format("{}", body_color);
        s.sects["tracer_color"] = format("{}", tracer_color);

        cfg().try_refresh_file(sect);
    }

    [[nodiscard]]
    std::string face_texture_path() const {
        return "player/face" + std::to_string(face_id) + ".png";
    }

    [[nodiscard]]
    std::string body_texture_path() const {
        return "player/body" + std::to_string(body_id) + ".png";
    }

private:
    static bool number_str(const char* str, size_t sz) {
        for (size_t i = 0; i < sz; ++i)
            if (str[i] < '0' || str[i] > '9')
                return false;
        return true;
    }

public:
    static u32 texture_path_to_index(std::string_view path) {
        if (!path.ends_with(".png"))
            return 0;

        path = std::string_view(path.data(), path.size() - 4);
        if (path.empty())
            return 0;

        size_t idx = path.size();
        while (idx != 0 && isdigit(path[idx - 1])) {
            --idx;
        }

        return u32(std::stoul(std::string(path.substr(idx))));
    }

    static std::vector<std::string> available_face_textures() {
        namespace fs = std::filesystem;
        std::vector<std::string> res;

        for (auto& f : fs::directory_iterator("data/textures/player/")) {
            if (f.is_regular_file()) {
                auto fname = f.path().filename().string();
                if (fname.starts_with("face") && fname.ends_with(".png") &&
                    number_str(fname.data() + 4, fname.size() - 8)) {
                    res.push_back(f.path().string().substr(sizeof("data/textures/") - 1));
                }
            }
        }

        return res;
    }

    static std::vector<std::string> available_body_textures() {
        namespace fs = std::filesystem;
        std::vector<std::string> res;

        for (auto& f : fs::directory_iterator("data/textures/player/")) {
            if (f.is_regular_file()) {
                auto fname = f.path().filename().string();
                if (fname.starts_with("body") && fname.ends_with(".png") &&
                    number_str(fname.data() + 4, fname.size() - 8)) {
                    res.push_back(f.path().string().substr(sizeof("data/textures/") - 1));
                }
            }
        }

        return res;
    }

public:
    player_name_t name         = "";
    std::string   pistol       = "wpn_glk17";
    u32           face_id      = 0;
    u32           body_id      = 0;
    sf::Color     body_color   = {255, 255, 255};
    sf::Color     tracer_color = {255, 255, 255};

    bool write_on_delete = true;
};
}
