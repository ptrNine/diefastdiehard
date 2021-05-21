#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include "bullet.hpp"
#include "config.hpp"
#include "texture_mgr.hpp"

namespace dfdh
{
struct weapon_anim_frame {
    enum interpl_type {
        linear = 0,
        quadratic,
        square
    };

    static interpl_type interpl_from_str(const std::string& str) {
        if (str == "linear")
            return linear;
        if (str == "quadratic")
            return quadratic;
        if (str == "square")
            return square;
        throw std::runtime_error("Unknown interpolation type: " + str);
    }

    struct key_t {
        sf::Vector2f position = {0.f, 0.f};
        sf::Vector2f scale = {1.f, 1.f};
        float        angle = 0.f;
    };

    std::vector<key_t>   _layers;
    interpl_type         _intrpl;
    float                _time;
};

struct weapon_anim {
    std::vector<weapon_anim_frame> _frames;
    float _duration;
};

class weapon {
public:
    friend class weapon_instance;

    enum weapon_class {
        pistol = 0,
        rifle,
        shotgun
    };

    static weapon_class weapon_class_from_str(const std::string& str) {
        if (str == "pistol")
            return pistol;
        else if (str == "rifle")
            return rifle;
        else if (str == "shotgun")
            return shotgun;
        throw std::runtime_error("Invalid weapon class: " + str);
    }

    static float tier_to_bullet_vel(u32 tier) {
        switch (tier) {
            case 0: return 1200.f;
            case 1: return 1500.f;
            case 2: return 1800.f;
            case 3: return 2100.f;
            default: return 1500.f;
        }
    }

    weapon(const std::string& section) {
        _hit_mass      = cfg().get_req<float>(section, "hit_mass");
        _dispersion    = cfg().get_req<float>(section, "dispersion");
        _fire_rate     = cfg().get_req<float>(section, "fire_rate");
        _recoil        = cfg().get_req<float>(section, "recoil");
        _buckshot      = cfg().get_req<u32>(section, "buckshot");
        _mag_size      = cfg().get_req<u32>(section, "mag_size");
        _size          = cfg().get_req<sf::Vector2f>(section, "size");
        _arm_joint     = cfg().get_req<sf::Vector2f>(section, "arm_joint");
        _arm_bone      = cfg().get_req<u32>(section, "arm_bone");
        _arm2_bone     = cfg().get_req<u32>(section, "arm2_bone");
        _arm_idle_pos  = cfg().get_req<sf::Vector2f>(section, "arm_idle_pos");
        _arm2_idle_pos = cfg().get_req<sf::Vector2f>(section, "arm2_idle_pos");
        _arm_pos_f     = cfg().get_req<sf::Vector2f>(section, "arm_pos_f");
        _barrel        = cfg().get_req<sf::Vector2f>(section, "barrel");
        _shot_flash    = cfg().get_default<bool>(section, "shot_flash", false);
        _eject_shell   = cfg().get_default<bool>(section, "eject_shell", false);
        _wpn_class     = weapon_class_from_str(cfg().get_req<std::string>(section, "class"));
        _bullet_vel_tier = cfg().get_req<u32>(section, "bullet_velocity_tier");

        if (_eject_shell) {
            _shell_pos   = cfg().get_req<sf::Vector2f>(section, "shell_pos");
            _shell_dir   = normalize(cfg().get_req<sf::Vector2f>(section, "shell_dir"));
            _shell_frame = cfg().get_req<u32>(section, "shell_frame");
            auto& txtr = texture_mgr().load(cfg().get_req<std::string>(section, "shell_txtr"));
            _shell_sprite.setTexture(txtr);
            _shell_sprite.setOrigin(float(txtr.getSize().x) * 0.5f, float(txtr.getSize().y) * 0.5f);
            auto sz = cfg().get_req<sf::Vector2f>(section, "shell_size");
            _shell_sprite.setScale(sz.x / float(txtr.getSize().x), sz.y / float(txtr.getSize().y));
            _shell_vel = cfg().get_req<float>(section, "shell_vel");
        }

        int i = 0;
        while (auto layer_txtr = cfg().get<std::string>(section, "layer" + std::to_string(i))) {
            if (layer_txtr->empty()) {
                _layers.push_back(sf::Sprite());
                ++i;
                continue;
            }

            auto& txtr = texture_mgr().load(*layer_txtr);
            sf::Sprite sprite;
            sprite.setTexture(txtr);
            auto txtr_sz = txtr.getSize();
            _xf = _size.x / float(txtr_sz.x);
            _yf = _size.y / float(txtr_sz.y);

            sprite.setOrigin(_arm_joint.x / _xf, _arm_joint.y / _yf);
            sprite.setScale(_xf, _yf);

            _layers.push_back(std::move(sprite));

            ++i;
        }

        if (_layers.empty())
            throw std::runtime_error("Weapon " + section + " has not layers");

        load_animations(section);
    }

public:
    void draw(const sf::Vector2f& start_pos, bool left_dir, sf::RenderWindow& wnd) {
        float invert = left_dir ? -1.f : 1.f;
        for (auto& sprite : _layers) {
            sprite.setPosition(start_pos);
            sprite.setScale(_xf * invert, _yf);
            sprite.setRotation(0.f);
            wnd.draw(sprite);
        }
    }

private:
    void load_animations(const std::string& section) {
        auto anims = cfg().get_req<std::vector<std::string>>(section, "animations");

        if (anims.empty())
            throw std::runtime_error("Weapon " + section + " must have at least one animation");

        for (auto& anim : anims)
            _animations.emplace(anim, load_anim(section + "_" += anim));
    }

    weapon_anim load_anim(const std::string& section) {
        auto frames = cfg().get_req<u32>(section, "frames");
        auto layers = _layers.size();

        std::vector<weapon_anim_frame> _animation;

        auto duration = cfg().get_req<float>(section, "duration");

        for (u32 frame = 0; frame < frames; ++frame) {
            auto frame_str = std::to_string(frame);
            weapon_anim_frame fr;
            fr._time   = cfg().get_req<float>(section, frame_str + "_time");
            fr._intrpl = weapon_anim_frame::interpl_from_str(
                cfg().get_req<std::string>(section, frame_str + "_intrpl"));

            for (size_t layer = 0; layer < layers; ++layer) {
                auto layer_str = "_layer" + std::to_string(layer) + "_";
                auto pos_key   = cfg().get_default<sf::Vector2f>(section, frame_str + layer_str + "pos", {0.f, 0.0001f});
                auto scale_key = cfg().get_default<sf::Vector2f>(section, frame_str + layer_str + "scale", {1.f, 1.f});
                auto rot_key   = cfg().get_default<float>(section, frame_str + layer_str + "rot", 0.f);

                fr._layers.push_back(weapon_anim_frame::key_t{pos_key, scale_key, rot_key});
            }

            _animation.push_back(std::move(fr));
        }
        return {std::move(_animation), duration};
    }

private:
    float _hit_mass;
    float _dispersion;
    float _fire_rate;
    float _recoil;
    u32   _buckshot;
    u32   _mag_size;
    u32   _arm_bone;
    u32   _arm2_bone;
    u32   _bullet_vel_tier;

    float _xf, _yf;

    sf::Vector2f _arm_joint;
    sf::Vector2f _arm_pos_f;
    sf::Vector2f _size;
    sf::Vector2f _arm_idle_pos;
    sf::Vector2f _arm2_idle_pos;
    sf::Vector2f _barrel;
    sf::Vector2f _shell_pos;
    sf::Vector2f _shell_dir;
    float        _shell_vel;
    u32          _shell_frame;
    sf::Sprite   _shell_sprite;

    weapon_class _wpn_class;
    bool         _shot_flash;
    bool         _eject_shell;

    std::vector<sf::Sprite> _layers;
    std::map<std::string, weapon_anim> _animations;

public:
    [[nodiscard]]
    weapon_class wpn_class() const {
        return _wpn_class;
    }

    [[nodiscard]]
    u32 mag_size() const {
        return _mag_size;
    }

    [[nodiscard]]
    float get_dispersion() const {
        return _dispersion;
    }
};


class weapon_storage_singleton {
public:
    static weapon_storage_singleton& instance() {
        static weapon_storage_singleton inst;
        return inst;
    }

    weapon& load(const std::string& section) {
        auto found = _wpns.find(section);
        if (found != _wpns.end())
            return found->second;
        else
            return _wpns.emplace(section, weapon(section)).first->second;
    }

    weapon_storage_singleton(const weapon_storage_singleton&) = delete;
    weapon_storage_singleton& operator=(const weapon_storage_singleton&) = delete;

private:
    weapon_storage_singleton() = default;
    ~weapon_storage_singleton() = default;

    std::map<std::string, weapon> _wpns;
};

inline weapon_storage_singleton& weapon_storage() {
    return weapon_storage_singleton::instance();
}


class weapon_instance {
public:
    struct anim_spec_t {
        std::string _name;
        sf::Clock   _timer = {};
        bool        _stop_at_end = true;
    };

    weapon_instance() = default;
    weapon_instance(weapon* wpn): _ammo_elapsed(wpn->mag_size()), _wpn(wpn) {
        if (_wpn->_shot_flash) {
            auto& txtr = texture_mgr().load("wpn/shot.png");
            _shot_flash.setTexture(txtr);
            _shot_flash.setOrigin({float(txtr.getSize().x) * 0.5f, float(txtr.getSize().y) * 0.5f});
            _shot_flash.setScale(0.55f, 0.55f);
        }
    }

    weapon_instance(const std::string& section): weapon_instance(&weapon_storage().load(section)) {}

    [[nodiscard]]
    weapon* wpn() {
        return _wpn;
    }

    [[nodiscard]]
    bool empty() const {
        return _wpn == nullptr;
    }

    [[nodiscard]]
    sf::Vector2f arm_position_factors(bool lefty) const {
        if (_wpn)
            return lefty ? sf::Vector2f{1.f - _wpn->_arm_pos_f.x, _wpn->_arm_pos_f.y} : _wpn->_arm_pos_f;
        else
            return lefty ? sf::Vector2f(0.f, -0.4f) : sf::Vector2f(1.f, -0.4f);
    }

    operator bool() const {
        return !empty();
    }

    void play_animation(const std::string& name) {
        if (!_wpn)
            return;

        auto found_anim = _wpn->_animations.find(name);
        if (found_anim == _wpn->_animations.end()) {
            std::cerr << "Animation " << name << " not found" << std::endl;
            return;
        }

        _current_anim = anim_spec_t{name, {}};
    }

    template <typename F = int>
    std::optional<float> update(const sf::Vector2f& position,
                                const sf::Vector2f& direction,
                                bullet_mgr&         bm,
                                physic_simulation&  sim,
                                int                 group               = -1,
                                F                   player_group_getter = -1) {
        if (!_wpn)
            return {};

        if (_ammo_elapsed == 0 && !_current_anim && !_on_reload) {
            _on_reload = true;
            if (_wpn->_wpn_class == weapon::shotgun)
                play_animation("load_start");
            else
                play_animation("reload");
            return {};
        }

        if (_on_shot && _ammo_elapsed &&
            _shot_timer.getElapsedTime().asSeconds() > 60.f / _wpn->_fire_rate) {
            if constexpr (std::is_same_v<F, int>)
                shot(position, direction, bm, sim);
            else
                shot(position, direction, bm, sim, group, std::move(player_group_getter));
            _on_reload = false;
            _shot_timer.restart();
            return _wpn->_recoil;
        }

        if (_on_reload && !_current_anim) {
            _ammo_elapsed = _wpn->mag_size();
            _on_reload = false;
        }
        return {};
    }

    void pull_trigger() {
        _on_shot = true;
    }

    void relax_trigger() {
        _on_shot = false;
    }

    std::array<sf::Vector2f, 2> draw(const sf::Vector2f& position,
                                     bool                left_dir,
                                     sf::RenderWindow&   wnd,
                                     const sf::Vector2f& shell_additional_vel = {0.f, 0.f}) {
        float                 LF       = left_dir ? -1.f : 1.f;
        static constexpr auto leftyfix = [](const sf::Vector2f& v, float invert) {
            return sf::Vector2f(v.x * invert, v.y);
        };

        static constexpr auto make_return = [](const sf::Vector2f& pos, weapon* wpn, float LF) {
            return std::array<sf::Vector2f, 2>{pos + leftyfix(wpn->_arm_idle_pos, LF),
                                               pos + leftyfix(wpn->_arm_idle_pos, LF) +
                                                   leftyfix(wpn->_arm2_idle_pos, LF)};
        };

        if (auto c = _shot_flash.getColor(); _wpn->_shot_flash && c.a != 0) {
            _shot_flash.setPosition(position + shot_displacement(sf::Vector2f(left_dir ? -1.f : 1.f, 0.f)));
            wnd.draw(_shot_flash);
            c.a /= 2;
            _shot_flash.setColor(c);
        }

        if (!_current_anim) {
            _wpn->draw(position, left_dir, wnd);
            draw_shells(wnd);
            return make_return(position, _wpn, LF);
        }

        weapon_anim* anim;
        std::vector<weapon_anim_frame>* frames;
        float max_frame;
        float cur_frame_time;

        for (bool repeat = true; repeat;) {
            repeat = false;

            anim   = &_wpn->_animations.at(_current_anim->_name);
            frames = &anim->_frames;

            if (frames->empty()) {
                _current_anim.reset();
                draw(position, left_dir, wnd);
                return make_return(position, _wpn, LF);
            }

            auto  dur      = anim->_duration;
            float time     = _current_anim->_timer.getElapsedTime().asSeconds();
            max_frame      = frames->back()._time;
            cur_frame_time = max_frame / dur * time;

            if (cur_frame_time > max_frame && _current_anim->_stop_at_end) {
                if (_current_anim->_name == "load_start") {
                    _current_anim = anim_spec_t{"shell"};
                    repeat = true;
                }
                else if (_current_anim->_name == "shell") {
                    ++_ammo_elapsed;
                    if (_ammo_elapsed == _wpn->_mag_size)
                        _current_anim = anim_spec_t{"load_end"};
                    else
                        _current_anim = anim_spec_t{"shell"};
                    repeat = true;
                }
                else {
                    _current_anim.reset();
                    draw(position, left_dir, wnd);
                    return make_return(position, _wpn, LF);
                }
            }
        }

        cur_frame_time = std::fmod(cur_frame_time, max_frame);

        auto beg = frames->begin();
        auto end = frames->begin() + 1;
        for (auto i = frames->begin(); i != frames->end() - 1; ++i) {
            if (i->_time <= cur_frame_time && std::next(i)->_time >= cur_frame_time) {
                beg = i;
                end = i + 1;
                break;
            }
        }

        if (_wpn->_eject_shell && !_shell_ejected && _current_anim->_name == "shot" &&
            beg - frames->begin() == _wpn->_shell_frame) {
            _shell_ejected = true;

            auto vel = _wpn->_shell_vel + rand_float(-_wpn->_shell_vel * 0.1f, _wpn->_shell_vel * 0.1f);
            auto dir = left_dir ? sf::Vector2f{-_wpn->_shell_dir.x, _wpn->_shell_dir.y} : _wpn->_shell_dir;

            _active_shells.push_back(shell_data{
                vel * dir + shell_additional_vel,
                position + shell_displacement(left_dir),
                rand_float(-40.f, 40.f),
                rand_float(-360.f, 360.f),
                left_dir});
        }

        constexpr auto interpl = [](auto v1, auto v2, float f, weapon_anim_frame::interpl_type it) {
            switch (it) {
                case weapon_anim_frame::linear:
                    return lerp(v1, v2, f);
                case weapon_anim_frame::quadratic:
                    return lerp(v1, v2, f * f);
                case weapon_anim_frame::square:
                    return lerp(v1, v2, std::sqrt(f));
            }
            throw std::runtime_error("WTF");
        };

        float factor = inverse_lerp(beg->_time, end->_time, cur_frame_time);
        std::vector<weapon_anim_frame::key_t> keys;
        for (u32 i = 0; i < beg->_layers.size(); ++i) {
            weapon_anim_frame::key_t key;
            key.position = interpl(beg->_layers[i].position, end->_layers[i].position, factor, end->_intrpl);
            key.scale    = interpl(beg->_layers[i].scale, end->_layers[i].scale, factor, end->_intrpl);
            key.angle    = interpl(beg->_layers[i].angle, end->_layers[i].angle, factor, end->_intrpl);
            keys.push_back(key);
        }

        auto& layers = _wpn->_layers;
        auto& main_layer_keys = keys[_wpn->_arm_bone];
        auto xf = _wpn->_xf;
        auto yf = _wpn->_yf;
        for (u32 i = 0; i < layers.size(); ++i) {
            if (i == _wpn->_arm_bone) {
                layers[i].setPosition(position + leftyfix(main_layer_keys.position, LF));
                layers[i].setScale(
                    {LF * xf * main_layer_keys.scale.x, yf * main_layer_keys.scale.y});
                layers[i].setRotation(main_layer_keys.angle * LF);
            }
            else {
                auto addition =
                    i == _wpn->_arm2_bone ? _wpn->_arm2_idle_pos : sf::Vector2f(0.f, 0.f);
                layers[i].setPosition(position + leftyfix(main_layer_keys.position, LF) +
                                      rotate_vec(leftyfix(keys[i].position, LF) + leftyfix(addition, LF),
                                                 M_PIf32 * main_layer_keys.angle * LF / 180.f));
                /* TODO: fix scale with rotations */
                layers[i].setScale({LF * xf * main_layer_keys.scale.x * keys[i].scale.x,
                                    yf * main_layer_keys.scale.y * keys[i].scale.y});
                layers[i].setRotation(main_layer_keys.angle * LF + keys[i].angle * LF);
            }
            wnd.draw(layers[i]);
        }

        draw_shells(wnd);

        return {layers[_wpn->_arm_bone].getPosition(), layers[_wpn->_arm2_bone].getPosition()};
    }

    [[nodiscard]]
    sf::Vector2f shot_displacement(const sf::Vector2f& direction) const {
        return direction.x < 0.f ? sf::Vector2f(-_wpn->_barrel.x, _wpn->_barrel.y) : _wpn->_barrel;
    }

private:
    void draw_shells(sf::RenderWindow& wnd) {
        auto& sprite = _wpn->_shell_sprite;
        for (auto i = _active_shells.begin(); i != _active_shells.end();) {
            auto elapsed_t = i->_timer.getElapsedTime().asSeconds();
            if (elapsed_t > 5.f) {
                _active_shells.erase(i++);
                continue;
            }

            sprite.setPosition(i->_pos);
            sprite.setRotation(i->_angle + (i->_left ? 180.f : 0.f));
            wnd.draw(sprite);

            float timestep = elapsed_t - i->_last_time;
            i->_last_time = elapsed_t;
            i->_vel += _last_gravity * timestep;
            i->_pos += i->_vel * timestep;
            i->_angle = std::fmod(i->_angle + i->_angle_vel * timestep, 360.f);

            ++i;
        }
    }

    [[nodiscard]]
    sf::Vector2f shell_displacement(bool on_left) {
        return on_left ? sf::Vector2f(-_wpn->_shell_pos.x, _wpn->_shell_pos.y) : _wpn->_shell_pos;
    }

    template <typename F = int>
    void shot(const sf::Vector2f& position,
              const sf::Vector2f& direction,
              bullet_mgr&         bm,
              physic_simulation&  sim,
              int                 group = -1,
              F                   player_group_getter = -1) {
        auto pos = position + shot_displacement(direction);
        auto dir = randomize_dir(direction, _wpn->_buckshot == 1 ? _wpn->_dispersion : _wpn->_dispersion * 0.5f);

        auto bullet_vel = weapon::tier_to_bullet_vel(_wpn->_bullet_vel_tier);
        if (_wpn->_buckshot == 1) {
            if constexpr (std::is_same_v<F, int>)
                bm.shot(sim, pos, _wpn->_hit_mass, dir * bullet_vel);
            else
                bm.shot(sim, pos, _wpn->_hit_mass, dir * bullet_vel, group, std::move(player_group_getter));
        } else {
            for (u32 i = 0; i < _wpn->_buckshot; ++i) {
                auto newdir = randomize_dir(dir, _wpn->_dispersion);
                if constexpr (std::is_same_v<F, int>)
                    bm.shot(sim, pos, _wpn->_hit_mass, newdir * bullet_vel);
                else
                    bm.shot(sim, pos, _wpn->_hit_mass, newdir * bullet_vel, group, std::move(player_group_getter));
            }
        }

        play_animation("shot");

        if (_wpn->_shot_flash) {
            _shot_flash.setRotation(rand_float(0.f, 360.f));
            _shot_flash.setColor({255, 255, 255, 255});
            _shot_flash.setPosition(pos);
        }

        _last_gravity = sim.gravity();
        _shell_ejected = false;

        --_ammo_elapsed;
    }

    struct shell_data {
        sf::Vector2f _vel;
        sf::Vector2f _pos;
        float        _angle;
        float        _angle_vel;
        bool         _left;
        float        _last_time = 0.f;
        sf::Clock    _timer = {};
    };

private:
    u32                        _ammo_elapsed = 0;
    weapon*                    _wpn          = nullptr;
    std::optional<anim_spec_t> _current_anim;

    sf::Sprite                 _shot_flash;

    std::list<shell_data>      _active_shells;
    bool                       _shell_ejected = false;
    sf::Vector2f               _last_gravity = {0.f, 0.f};

    bool                       _on_shot = false;
    bool                       _on_reload = false;
    sf::Clock                  _shot_timer;

public:
    [[nodiscard]]
    const weapon* get_weapon() const {
        return _wpn;
    }

    [[nodiscard]]
    float get_bullet_vel() const {
        return weapon::tier_to_bullet_vel(_wpn->_bullet_vel_tier);
    }
};

} // namespace dfdh
