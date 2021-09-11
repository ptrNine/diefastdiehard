#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <filesystem>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include "vec_math.hpp"
#include "config.hpp"
#include "physic_simulation.hpp"
#include <vector>

namespace dfdh {

namespace fs = std::filesystem;

class level {
public:
    struct platform_t {
        physic_platform ph;
        sf::Sprite      start_pl;
        sf::Sprite      end_pl;
        sf::Sprite      pl;
    };

    static std::shared_ptr<level> create(const std::string& section) {
        return std::make_shared<level>(section);
    }

    static std::vector<std::string> list_available_levels() {
        std::vector<std::string> res;
        for (auto& [sect, _] : cfg().sections())
            if (sect.starts_with("lvl_"))
                res.push_back(sect);
        return res;
    }

    void cfg_reload() {
        _name        = cfg().get_req<std::string>(_section, "name");
        _gravity     = cfg().get_req<vec2f>(_section, "gravity");
        _platform_sz = cfg().get_req<float>(_section, "platform_height");
        _view_size   = cfg().get_req<vec2f>(_section, "view_size");
        _level_size  = cfg().get_req<vec2f>(_section, "level_size");

        auto txtr_path              = fs::current_path() / "data/textures/" / _name;
        auto end_platform_txtr_path = txtr_path / "end_platform.png";
        auto platform_txtr_path     = txtr_path / "platform.png";
        auto background_txtr_path   = txtr_path / "background.png";

#define load_if_path_changed(txtr) \
        if (_##txtr##_path != txtr##_path) { \
            _##txtr.loadFromFile(txtr##_path); \
            _##txtr##_path = txtr##_path; \
        }

        load_if_path_changed(end_platform_txtr);
        load_if_path_changed(platform_txtr)
        load_if_path_changed(background_txtr)

#undef load_if_path_changed

        _end_platform.setTexture(_end_platform_txtr);
        _platform.setTexture(_platform_txtr);

        _end_platform.setScale({_platform_sz / float(_end_platform_txtr.getSize().x),
                                _platform_sz / float(_end_platform_txtr.getSize().y)});
        _platform.setScale({_platform_sz / float(_platform_txtr.getSize().x),
                            _platform_sz / float(_platform_txtr.getSize().y)});

        _background.setTexture(_background_txtr);
        auto background_size = _background_txtr.getSize();
        _background.setScale(vec2f(_level_size.x / float(background_size.x),
                                          _level_size.y / float(background_size.y)));

        _background.setPosition(0.f, 0.f);

        _platforms.clear();
        u32 pl = 0;
        while (auto pl_data = cfg().get<std::array<float, 3>>(_section, "pl" + std::to_string(pl))) {
            _platforms.push_back(
                platform_t{physic_platform({(*pl_data)[0], (*pl_data)[1]}, (*pl_data)[2]),
                _end_platform, _end_platform, _platform});
            auto& p = _platforms.back();
            //sim.add_platform(p.ph);

            p.start_pl.setPosition(p.ph.get_position());

            p.pl.setPosition(p.ph.get_position() + vec2f{_platform_sz, 0.f});
            float pl_len = p.ph.length() - _platform_sz * 2.f;
            p.pl.scale(pl_len / _platform_sz, 1.f);

            p.end_pl.setPosition(p.pl.getPosition() + sf::Vector2f(pl_len, 0.f) +
                                 sf::Vector2f(_platform_sz, 0.f));
            p.end_pl.setScale(-p.end_pl.getScale().x, p.end_pl.getScale().y);

            ++pl;
        }
    }

    level(std::string section): _section(std::move(section)) {
        cfg_reload();
    }

    void setup_to(physic_simulation& sim) {
        sim.gravity(_gravity);
        sim.remove_all_platforms();
        for (auto& pl : _platforms)
            sim.add_platform(pl.ph);
    }

    void draw(sf::RenderWindow& wnd) {
        wnd.draw(_background);

        for (auto& p : _platforms) {
            wnd.draw(p.end_pl);
            wnd.draw(p.start_pl);
            wnd.draw(p.pl);
        }
    }

    [[nodiscard]]
    const auto& get_platforms() const {
        return _platforms;
    }

private:
    std::string _section;

    sf::Sprite  _end_platform;
    sf::Sprite  _platform;
    sf::Sprite  _background;

    sf::Texture _end_platform_txtr;
    sf::Texture _platform_txtr;
    sf::Texture _background_txtr;

    std::string _end_platform_txtr_path;
    std::string _platform_txtr_path;
    std::string _background_txtr_path;

    float _platform_sz = 10.f;
    vec2f _view_size;
    vec2f _level_size;
    vec2f _gravity;

    std::vector<platform_t> _platforms;

    std::string _name;

public:
    [[nodiscard]]
    const vec2f& level_size() const {
        return _level_size;
    }

    [[nodiscard]]
    const vec2f& view_size() const {
        return _view_size;
    }

    [[nodiscard]]
    const std::string& section_name() const {
        return _section;
    }
};

}
