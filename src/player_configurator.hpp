#pragma once

#include <SFML/Graphics/Color.hpp>
#include <filesystem>

#include "log.hpp"
#include "config.hpp"

namespace dfdh {
class player_configurator {
public:
    player_configurator(u32 iplayer_id): player_id(iplayer_id) {
        read();
    }

    ~player_configurator() {
        if (write_on_delete)
            write();
    }

    static bool write_default_config(const std::string& cfg_path, u32 player_id) {
        auto os = std::ofstream(cfg_path, std::ios_base::out);
        if (!os.is_open()) {
            LOG_ERR("Can't write config {}", cfg_path);
            return false;
        }
        os << "[player_settings" << std::to_string(player_id) << "]\n";
        return true;
    }

    void read() {
        auto player_id_str = std::to_string(player_id);
        auto cfg_path      = "data/settings/player_settings" + player_id_str + ".cfg";
        auto sect          = "player_settings" + player_id_str;

        if (!cfg().try_parse(cfg_path) || !cfg().sections().contains(sect)) {
            LOG_WARN("Can't parse config {} (will be created)", cfg_path);
            if (!write_default_config(cfg_path, player_id))
                return;
            else
                cfg().try_parse(cfg_path);
        }

        pistol       = cfg().get_default<std::string>(sect, "pistol", "wpn_glk17");
        face_id      = cfg().get_default<u32>(sect, "face_id", 0);
        body_id      = cfg().get_default<u32>(sect, "body_id", 0);
        body_color   = cfg().get_default<sf::Color>(sect, "body_color", sf::Color(255, 255, 255));
        tracer_color = cfg().get_default<sf::Color>(sect, "tracer_color", sf::Color(255, 255, 255));
    }

    void write() {
        auto player_id_str = std::to_string(player_id);
        auto cfg_path      = "data/settings/player_settings" + player_id_str + ".cfg";
        auto os            = std::ofstream(cfg_path, std::ios_base::out);

        if (!os.is_open()) {
            LOG_ERR("Can't write config {}", cfg_path);
            return;
        }

        os << "[player_settings" << player_id_str << "]\n";
        os << "pistol       = " << pistol << '\n';
        os << "face_id      = " << face_id << '\n';
        os << "body_id      = " << body_id << '\n';
        os << "body_color   = " << body_color << '\n';
        os << "tracer_color = " << tracer_color << '\n';
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
    u32         player_id    = 0;
    std::string pistol       = "wpn_glk17";
    u32         face_id      = 0;
    u32         body_id      = 0;
    sf::Color   body_color   = {255, 255, 255};
    sf::Color   tracer_color = {255, 255, 255};

    bool write_on_delete = true;
};
}
