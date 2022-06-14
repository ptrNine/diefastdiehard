#pragma once

#include <deque>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/CircleShape.hpp>

//#include "rand_pool.hpp"
#include "base/types.hpp"
#include "base/cfg.hpp"
#include "base/avg_counter.hpp"
#include "bullet.hpp"
#include "physic/physic_simulation.hpp"
#include "weapon.hpp"
#include "player_configurator.hpp"
#include "player_controller.hpp"
#include "net_actions.hpp"

namespace dfdh {

class player {
public:
    static vec2f default_size() {
        return {50.f, 90.f};
    }

    static vec2f sprite_size_adjust_factors() {
        return {1.25f, 1.08f};

    }

    static vec2f default_sprite_size() {
        auto adj = sprite_size_adjust_factors();
        auto sz = default_size();
        return {sz.x * adj.x, sz.y * adj.y};
    }

    static void default_hand_setup(sf::CircleShape& hand, const vec2f& size) {
        hand.setOutlineColor(sf::Color(0, 0, 0));
        hand.setOutlineThickness(size.x * 0.045f);
        hand.setRadius(size.x * 0.14f);
        hand.setOrigin(size.x * 0.14f, size.x * 0.14f);
    }

    static void default_leg_setup(sf::CircleShape& leg, const vec2f& size) {
        leg.setOutlineColor(sf::Color(0, 0, 0));
        leg.setOutlineThickness(size.x * 0.045f);
        leg.setRadius(size.x * 0.2f);
        leg.setOrigin(size.x * 0.2f, size.x * 0.35f);
    }

    static std::shared_ptr<player> create(const player_name_t& name, physic_simulation& sim, const vec2f& size) {
        return std::make_shared<player>(name, sim, size);
    }

    static std::shared_ptr<player> create(physic_simulation& sim, const vec2f& size) {
        return std::make_shared<player>(player_name_t{}, sim, size);
    }

    player(const player_name_t& name, physic_simulation& sim, const vec2f& size):
        _name(name), _size(size) {
        _collision_box = physic_group::create();
        _collision_box->append(physic_line::create({0.f, 0.f}, {0.f, -size.y}));
        _collision_box->append(physic_line::create({0.f, -size.y}, {size.x, 0.f}));
        _collision_box->append(physic_line::create({size.x, -size.y}, {0.f, size.y}));
        _collision_box->append(physic_line::create({size.x, 0.f}, {-size.x, 0.f}));

        _collision_box->enable_gravity();
        _collision_box->allow_platform(true);
        _collision_box->user_data(user_data_type::player);
        _collision_box->user_any(this);

        sim.add_primitive(_collision_box);

        default_hand_setup(_left_hand, _size);
        default_hand_setup(_right_hand, _size);
        default_leg_setup(_left_leg, size);
        default_leg_setup(_right_leg, size);

        jumps_left = max_jumps;
    }

    player(physic_simulation& sim, const vec2f& size): player({}, sim, size) {}

    ~player() {
        _collision_box->delete_later();
    }

    void set_params_from_client(const player_skin_params_t& params) {
        set_body(params.body_txtr, params.body_color);
        set_face(params.face_txtr);
        tracer_color(params.tracer_color);
    }

    void update_from_client(const player_move_states_t& st) {
        if (st.jump) {
            if (!_jump_pressed) {
                jump();
                _jump_pressed = true;
            }
        }
        else {
            _jump_pressed = false;
        }

        if (st.jump_down) {
            if (!_jump_down_pressed) {
                jump_down();
                _jump_down_pressed = true;
            }
        }
        else {
            _jump_down_pressed = false;
        }

        if (st.mov_left) {
            _cur_x_accel_l = -_x_accel;
            _dir           = dir_left;
        }
        else {
            _cur_x_accel_l = 0.f;
        }

        if (st.mov_right) {
            _cur_x_accel_r = _x_accel;
            _dir           = dir_right;
        }
        else {
            _cur_x_accel_r = 0.f;
        }

        if (st.on_shot)
            shot();
        else {
            relax();
        }

        if (st.lock_y)
            _collision_box->lock_y();
        else
            _collision_box->unlock_y();
    }

    bool update_input(const player_controller& ks, sf::Event evt) {
        bool update_was = false;

        if (evt.type == sf::Event::KeyPressed) {
            auto c = evt.key.code;
            if (c == ks.up && !_jump_pressed) {
                jump();
                _jump_pressed = true;
                update_was = true;
            }
            else if (c == ks.down && !_jump_down_pressed) {
                jump_down();
                _jump_down_pressed = true;
                update_was = true;
            }
            else if (c == ks.left && is_float_zero(_cur_x_accel_l)) {
                _cur_x_accel_l = -_x_accel;
                _dir = dir_left;
                update_was = true;
            }
            else if (c == ks.right && is_float_zero(_cur_x_accel_r)) {
                _cur_x_accel_r = _x_accel;
                _dir = dir_right;
                update_was = true;
            }
            else if (c == ks.shot) {
                update_was = shot();
            }
            else if (c == ks.adjust_up) {
                _long_shot_enabled = true;
            }
        }
        else if (evt.type == sf::Event::KeyReleased) {
            auto c = evt.key.code;
            if (c == ks.left && !is_float_zero(_cur_x_accel_l)) {
                _cur_x_accel_l = 0.f;
                update_was = true;
            }
            else if (c == ks.right && !is_float_zero(_cur_x_accel_r)) {
                _cur_x_accel_r = 0.f;
                update_was = true;
            }
            else if (c == ks.shot) {
                update_was = relax();
            }
            else if (c == ks.up && _jump_pressed) {
                _jump_pressed = false;
                update_was = true;
            }
            else if (c == ks.down && _jump_down_pressed) {
                _jump_down_pressed = false;
                update_was = true;
            }
            else if (c == ks.adjust_up) {
                _long_shot_enabled = false;
            }
        }

        if (update_was)
            ++_evt_counter;

        return update_was;
    }

    void stop() {
        _cur_x_accel_l = 0.f;
        _cur_x_accel_r = 0.f;
    }

    void move_left() {
        _cur_x_accel_r = 0.f;
        _cur_x_accel_l = -_x_accel;
        _dir = dir_left;
    }

    void move_right() {
        _cur_x_accel_l = 0.f;
        _cur_x_accel_r = _x_accel;
        _dir = dir_right;
    }

    bool shot() {
        if (_pistol)
            return _pistol.pull_trigger(_tracer_color);
        return false;
    }

    bool relax() {
        if (_pistol)
            return _pistol.relax_trigger();
        return false;
    }

    void enable_long_shot(bool value = true) {
        _long_shot_enabled = value;
    }

    void disable_long_shot() {
        _long_shot_enabled = false;
    }

    static int player_group_getter(const physic_point* p) {
        return std::any_cast<player*>(p->get_user_any())->get_group();
    }

    template <typename F = void (*)(const vec2f&, const vec2f&, float)>
    void game_update(physic_simulation& sim,
                     bullet_mgr&        bm,
                     const vec2f&       cam_position,
                     bool               gravity_for_bullets   = false,
                     bool               spawn_bullet          = true,
                     F&&                bullet_spawn_callback = nullptr) {
        /* Hit player sounds */
        if (_hit_sound_info) {
            sound_mgr().play(_hit_sound_info->pwned ? "player/headshot0.wav"s
                                                    : ("player/bullethit"s + rand_num('0', '1') + ".wav"),
                             _group,
                             (_hit_sound_info->position - cam_position),
                             sim.last_speed());
            _hit_sound_info.reset();
        }

        /* Steps and fall sounds */
        if (_collision_box->is_lock_y() && std::abs(_collision_box->get_velocity().x) > 20.f) {
            if (!_step_sound_active) {
                _step_sound_timer.restart();
                _step_sound_active = true;
            }
        }
        else if (_step_sound_active) {
            _step_sound_active = false;
        }
        if (_step_sound_active) {
            float period = 65.f / (std::abs(_collision_box->get_velocity().x) * sim.last_speed());
            if (_step_sound_timer.getElapsedTime().asSeconds() > period) {
                sound_mgr().play("player/step"s + rand_num('0', '2') + ".wav",
                                 _group,
                                 get_position() - cam_position,
                                 sim.last_speed());
                _step_sound_timer.restart();
            }
        }
        if (_fall_velocity) {
            auto volume = inverse_lerp(0.f, 1800.f, std::clamp(*_fall_velocity, 0.f, 1800.f));
            volume *= volume * 100.f;
            sound_mgr().play("player/fall"s + rand_num('0', '1') + ".wav",
                             _group,
                             get_position() - cam_position,
                             sim.last_speed(),
                             volume);
            _fall_velocity.reset();
        }

        /* Update weapon */
        if (_pistol) {
            auto dir       = _on_left ? vec2f{-1.f, 0.f} : vec2f{1.f, 0.f};
            auto pos       = collision_box()->get_position();
            auto arm_pos_f = _pistol.arm_position_factors(_on_left, _bobbing);
            pos += vec2f(_size.x * arm_pos_f.x, _size.y * arm_pos_f.y);

            if (auto recoil = _pistol.update(pos,
                                             cam_position,
                                             dir,
                                             gravity_for_bullets && _long_shot_enabled,
                                             bm,
                                             sim,
                                             gravity_for_bullets,
                                             spawn_bullet,
                                             std::forward<F>(bullet_spawn_callback),
                                             _group,
                                             player_group_getter))
                _collision_box->apply_impulse(-dir * *recoil);
        }
    }

    void physic_update(const physic_simulation& sim, float timestep) {
        static constexpr auto accel_f_restore = [](float& accel, float timestep) {
            accel += timestep * 2.f;
            if (accel > 1.f)
                accel = 1.f;
        };
        accel_f_restore(_accel_left_f, timestep);
        accel_f_restore(_accel_right_f, timestep);

        auto  vel          = _collision_box->get_velocity();
        auto  moving_left  = !is_float_zero(_cur_x_accel_l);
        auto  moving_right = !is_float_zero(_cur_x_accel_r);
        bool  moving       = moving_left || moving_right;
        float move_accel_l = _cur_x_accel_l;
        float move_accel_r = _cur_x_accel_r;

        if (moving_left && moving_right) {
            moving_left  = false;
            moving_right = false;
            move_accel_l = 0.f;
            move_accel_r = 0.f;
        }

        float accel = 0.f;
        if (_collision_box->is_lock_y() && !moving) {
            if (vel.x > _max_speed || (vel.x > 0.f && !moving_right))
                accel = -_x_slowdown;
            else if (vel.x < -_max_speed || (vel.x < 0.f && !moving_left))
                accel = _x_slowdown;
        }

        if (vel.x < _max_speed) {
            auto mov_accel = move_accel_r * _accel_left_f;
            auto new_vel   = vel.x + mov_accel * timestep;
            if (new_vel < _max_speed)
                accel += mov_accel;
            else
                vel.x = _max_speed;
        }
        if (vel.x > -_max_speed) {
            auto mov_accel = move_accel_l * _accel_right_f;
            auto new_vel   = vel.x + mov_accel * timestep;
            if (new_vel > -_max_speed)
                accel += mov_accel;
            else
                vel.x = -_max_speed;
        }

        float prev_v = vel.x;
        vel.x += accel * timestep;
        if (!moving && ((prev_v < 0.f && vel.x > 0.f) || (prev_v > 0.f && vel.x < 0.f))) {
            vel.x                 = 0.f;
            _movement_acceleration = {0.f, 0.f};
        }
        else {
            _movement_acceleration = {accel, 0.f};
        }

        _collision_box->velocity(vel);

        physic_jump_update();
        physic_weapon_bobbing_update(sim);
        leg_animation_update(timestep, _collision_box->get_position(), vel);

        if (_position_trace.size() > 300)
            _position_trace.pop_front();
        _position_trace.emplace_back(sim.current_update_time(), get_position());
    }

    void jump() {
        _jump_scheduled = true;
    }

    void jump_down() {
        if (_collision_box->is_lock_y()) {
            _jump_down_scheduled = true;
        }
    }

    static void draw_to_texture_target(sf::RenderTexture&  target,
                                       const vec2f& size,
                                       float               scale,
                                       sf::Sprite&         body,
                                       sf::Sprite&         face,
                                       sf::CircleShape&    hand_or_leg,
                                       weapon*             wpn) {
        float icon_x_size =
            wpn ? size.x * wpn->arm_position_factors(false).x + wpn->barrel_pos().x * scale
                : size.y;

        target.create(u32(icon_x_size), u32(size.y));

        auto body_txtr_size = body.getTexture()->getSize();
        auto face_txtr_size = face.getTexture()->getSize();
        auto body_f_x       = size.x / float(body_txtr_size.x);
        auto body_f_y       = size.y / float(body_txtr_size.y);
        auto face_f_x       = size.x / float(face_txtr_size.x);
        auto face_f_y       = size.y / float(face_txtr_size.y);

        body.setScale(body_f_x, body_f_y);
        face.setScale(face_f_x, face_f_y);
        body.setPosition(0.f, 0.f);
        face.setPosition(0.f, 0.f);

        target.setView(sf::View(sf::FloatRect{0.f, size.y, icon_x_size, -size.y}));
        target.draw(body);
        target.draw(face);

        auto sz = size;
        sz.x /= sprite_size_adjust_factors().x;
        sz.y /= sprite_size_adjust_factors().y;

        if (wpn) {
            auto pos = size;
            auto f = wpn->arm_position_factors(false);
            pos.x *= f.x;
            pos.y *= f.y;
            pos.y += size.y;
            wpn->draw(pos, false, 0.f, target, scale);

            default_hand_setup(hand_or_leg, sz);
            hand_or_leg.setPosition(pos + wpn->arm_idle_pos() * scale);
            target.draw(hand_or_leg);

            hand_or_leg.move(wpn->arm2_idle_pos() * scale);
            target.draw(hand_or_leg);
        }


        default_leg_setup(hand_or_leg, sz);
        hand_or_leg.setPosition(size.x * 0.3f, size.y);
        target.draw(hand_or_leg);

        hand_or_leg.setPosition(size.x * 0.7f, size.y);
        target.draw(hand_or_leg);

        target.display();
    }

    void draw(sf::RenderWindow& wnd, float interpolation_factor, float timestep, bool gravity_for_bullets = false) {
        auto pos      = _collision_box->get_position();
        auto next_pos = pos + _collision_box->get_velocity() * timestep;

        pos = lerp(pos, next_pos, interpolation_factor);
        //glog().info("now: {} new: {} intr: {}", _collision_box->get_position(), next_pos, pos);

        auto adj = sprite_size_adjust_factors();
        auto dif = vec2f((_size.x * adj.x - _size.x) * 0.5f, _size.y * adj.y);

        if (_dir == dir_left && !_on_left) {
            _body.setScale(-_body.getScale().x, _body.getScale().y);
            _face.setScale(-_face.getScale().x, _face.getScale().y);
            _hat.setScale(-_hat.getScale().x, _hat.getScale().y);
            _dir = dir_none;
            _on_left = true;
        }
        if (_dir == dir_right) {
            _body.setScale(std::fabs(_body.getScale().x), _body.getScale().y);
            _face.setScale(std::fabs(_face.getScale().x), _face.getScale().y);
            _hat.setScale(std::fabs(_hat.getScale().x), _hat.getScale().y);
            _dir = dir_none;
            _on_left = false;
        }

        if (_on_left)
            dif.x = -dif.x - _size.x;

        _body.setPosition(pos - dif);
        _face.setPosition(pos - dif);
        _hat.setPosition(pos - dif);

        wnd.draw(_body);
        wnd.draw(_face);
        wnd.draw(_hat);

        static constexpr auto draw_leg_lerp =
            [](auto&& wnd, float timestep, float factor, auto&& leg, const vec2f& vel) {
                vec2f pos      = leg.getPosition();
                auto  next_pos = pos + vel * timestep;
                leg.setPosition(lerp(pos, next_pos, factor));
                wnd.draw(leg);
                leg.setPosition(pos);
            };
        draw_leg_lerp(wnd, timestep, interpolation_factor, _left_leg, _collision_box->get_velocity());
        draw_leg_lerp(wnd, timestep, interpolation_factor, _right_leg, _collision_box->get_velocity());

        if (_pistol) {
            auto arm_pos_f = _pistol.arm_position_factors(_on_left, _bobbing);
            auto [lh, rh]  = _pistol.draw(pos + vec2f(_size.x * arm_pos_f.x, _size.y * arm_pos_f.y),
                                         _on_left,
                                         gravity_for_bullets && _long_shot_enabled,
                                         wnd,
                                         _collision_box->get_velocity());
            _left_hand.setPosition(lh);
            _right_hand.setPosition(rh);
            wnd.draw(_left_hand);
            wnd.draw(_right_hand);
        }
        else {
            _left_hand.setPosition(pos.x, pos.y - _size.y / 2.5f);
            _right_hand.setPosition(pos.x + _size.x, pos.y - _size.y / 2.5f);
            wnd.draw(_left_hand);
            wnd.draw(_right_hand);
        }
    }

    void set_body(const std::string& path, const sf::Color& color = {255, 255, 255}) {
        _body_txtr_path = path;
        _body_color = color;

        set_some(path, _body_txtr, _body, _size, color);
        _left_hand.setFillColor(color);
        _right_hand.setFillColor(color);
        _left_leg.setFillColor(color);
        _right_leg.setFillColor(color);
    }

    void set_face(const std::string& path, const sf::Color& color = {255, 255, 255}) {
        _face_txtr_path = path;
        set_some(path, _face_txtr, _face, _size, color);
    }

    void set_hat(const std::string& path, const sf::Color& color = {255, 255, 255}) {
        set_some(path, _hat_txtr, _hat, _size, color);
    }

    void setup_pistol(const std::string& section) {
        _pistol = weapon_instance(section);
        _pistol_section = section;
    }

    [[nodiscard]]
    const std::string& pistol_section() const {
        return _pistol_section;
    }

    [[nodiscard]]
    const std::string& face_texture_path() const {
        return _face_txtr_path;
    }

    [[nodiscard]]
    const std::string& body_texture_path() const {
        return _body_txtr_path;
    }

    [[nodiscard]]
    const sf::Color& body_color() const {
        return _body_color;
    }

    [[nodiscard]]
    const weapon_instance& get_gun() const {
        return _pistol;
    }

    [[nodiscard]]
    weapon_instance& get_gun() {
        return _pistol;
    }

    [[nodiscard]]
    sf::Color tracer_color() const {
        return _tracer_color;
    }

    void tracer_color(sf::Color value) {
        _tracer_color = value;
    }

    [[nodiscard]]
    const vec2f& movement_acceleration() const {
        return _movement_acceleration;
    }

    [[nodiscard]]
    bool is_walking() const {
        return !is_float_zero(_cur_x_accel_l) ^ !is_float_zero(_cur_x_accel_r);
    }

    [[nodiscard]]
    player_move_states_t client_input_state() const {
        return player_move_states_t{
            _cur_x_accel_l < 0.f,
            _cur_x_accel_r > 0.f,
            _pistol ? _pistol.on_shot() : false,
            _jump_pressed,
            _jump_down_pressed,
            _collision_box->is_lock_y()
        };
    }

    std::optional<player_move_states_t> try_extract_client_input_state() {
        //std::cout << "Movleft: " << (_cur_x_accel_l < 0.f) << " movright: " << (_cur_x_accel_r > 0.f) << std::endl;
        auto states = client_input_state();
        if (_last_states == states)
            return {};
        else {
            _last_states = states;
            return states;
        }
    }

    [[nodiscard]]
    std::optional<vec2f> position_trace_lookup(std::chrono::steady_clock::time_point now, float latency_offset) const {
        auto time_dur = [](auto now, auto last) {
            return std::chrono::duration_cast<std::chrono::duration<float>>(now - last).count();
        };

        auto pos = _position_trace.begin();
        for (; pos != _position_trace.end(); ++pos)
            if (time_dur(now, pos->first) < latency_offset)
                break;

        if (pos == _position_trace.end())
            return {};

        if (pos != _position_trace.begin()) {
            auto before = pos - 1;
            auto dist_before = std::fabs(time_dur(now, before->first));
            auto dist = std::fabs(time_dur(now, pos->first));
            auto f = dist_before / (dist + dist_before);

            return lerp(before->second, pos->second, dist_before / f);
        }

        return pos->second;
    }

    void set_from_config() {
        player_configurator pc{_name};
        set_body(pc.body_texture_path(), pc.body_color);
        set_face(pc.face_texture_path());
        setup_pistol(pc.pistol);
        tracer_color(pc.tracer_color);
    }

    struct hit_sound_info {
        vec2f position;
        bool  pwned;
    };
    void play_hit_sound(const vec2f& hit_position, bool pwned) {
        _hit_sound_info.emplace(hit_sound_info{hit_position, pwned});
    }

private:
    static constexpr float bobbing_coef = 0.1f;
    static constexpr float bobbing_minmax = 0.2f;

    void physic_weapon_bobbing_update(const physic_simulation& sim) {
        auto gforce = _collision_box->g_force(sim.prev_timestep());
        if (_collision_box->is_lock_y())
            gforce += sim.gravity();

        auto  c1       = std::pow(float(sim.last_rps()) / sim.last_speed(), 1.5f) * 0.010758287f;
        auto  c2       = c1 / (c1 + 1.f);
        auto  scalar_g = magnitude(sim.gravity());
        float minmax   = bobbing_minmax * scalar_g / bobbing_coef;

        _bobbing_accel = clamp(_bobbing_accel * c2 + gforce / c1, vec2f::filled(-minmax), vec2f::filled(minmax));

        bool bobbing_x_on = std::abs(_bobbing_accel.x) > 0.5f;
        bool bobbing_y_on = std::abs(_bobbing_accel.y) > 0.5f;

        auto bobbing_f = (1.f / magnitude(sim.gravity())) * -bobbing_coef;
        _bobbing.x = bobbing_x_on ? _bobbing_accel.x * bobbing_f : 0.f;
        _bobbing.y = bobbing_y_on ? _bobbing_accel.y * bobbing_f : 0.f;
    }

    void physic_jump_update() {
        if (_jump_scheduled) {
            if (_collision_box->is_lock_y() || jumps_left) {
                auto vel = _collision_box->get_velocity();
                vel.y    = -_jump_speed;
                _collision_box->velocity(vel);

                if (!_collision_box->is_lock_y())
                    --jumps_left;
                else
                    _collision_box->unlock_y();
            }
            _jump_scheduled = false;
        }

        if (_jump_down_scheduled) {
            if (_collision_box->is_lock_y()) {
                _collision_box->unlock_y();
                auto vel = _collision_box->get_velocity();
                vel.y += 100.f;
                _collision_box->velocity(vel);
            }
            _jump_down_scheduled = false;
        }
    }

    void leg_animation_update(float timestep, const vec2f& pos, const vec2f& vel) {
        auto vx = vel.x;
        float jump_f = 0.2f;
        if (!_collision_box->is_lock_y()) {
            jump_f = std::clamp(jump_f + -vel.y * 0.001f, 0.f, 1.f);
            vx = 0.f;
            if (_leg_timer < 0.5f) {
                _leg_timer -= timestep * 2.f;
                if (_leg_timer < 0.f)
                    _leg_timer = 0.f;
            } else {
                _leg_timer += timestep * 2.f;
                if (_leg_timer > 0.9999f)
                    _leg_timer = 0.9999f;
            }
        }

        auto pos1 = pos;
        auto pos2 = pos;
        pos1.x += _size.x * 0.25f;
        pos2.x += _size.x * 0.75f;

        _leg_timer = std::fmod(_leg_timer + (std::fabs(vx) * timestep) / (_size.x * 2.f), 1.f);
        auto p1 = lerp(pos1.x, pos2.x, _leg_timer);
        auto p2 = lerp(pos1.x, pos2.x, 1.f - _leg_timer);

        pos1.x = p1;
        pos2.x = p2;

        auto jump_y = _size.x * 0.4f * (jump_f - 0.2f);
        pos1.y += jump_y;
        pos2.y += jump_y;

        _left_leg.setPosition(pos1);
        _right_leg.setPosition(pos2);
    }

    void set_some(const std::string&  path,
                  sf::Texture&        txtr,
                  sf::Sprite&         sprite,
                  const vec2f& size,
                  const sf::Color&    color) {
        txtr = texture_mgr().load(path);
        txtr.setSmooth(true);
        sprite.setTexture(txtr);
        sprite.setColor(color);

        auto sz = txtr.getSize();
        auto adj = sprite_size_adjust_factors();
        float xf = (size.x * adj.x) / float(sz.x);
        float yf = (size.y * adj.y) / float(sz.y);
        sprite.setScale(_on_left ? -xf : xf, yf);
    }

private:
    player_name_t _name;
    std::string   _pistol_section;
    std::string   _body_txtr_path;
    std::string   _face_txtr_path;
    sf::Color     _body_color;

    std::shared_ptr<physic_group> _collision_box;
    sf::Sprite  _body;
    sf::Sprite  _face;
    sf::Sprite  _hat;
    sf::Texture _body_txtr;
    sf::Texture _face_txtr;
    sf::Texture _hat_txtr;
    sf::CircleShape _left_hand;
    sf::CircleShape _right_hand;
    sf::CircleShape _left_leg;
    sf::CircleShape _right_leg;

    player_move_states_t _last_states;
    std::deque<std::pair<std::chrono::steady_clock::time_point, vec2f>> _position_trace;

//    rand_float_pool _rand_pool;

    sf::Color _tracer_color = {255, 255, 0};

    float _leg_timer = 0.f;

    vec2f _size;

    float _accel_left_f     = 1.f;
    float _accel_right_f    = 1.f;
    float _max_speed        = 280.f;
    float _x_accel          = 1550.f; // 2250.f;
    float _x_slowdown       = 700.f;
    float _jump_speed       = 620.f;

    float _cur_x_accel_l = 0.f;
    float _cur_x_accel_r = 0.f;

    vec2f _movement_acceleration = {0.f, 0.f};
    vec2f _bobbing_accel         = {0.f, 0.f};
    vec2f _bobbing               = {0.f, 0.f};

    u32 jumps_left = 1;
    u32 max_jumps = 1;

    u32 _deaths = 0;
    u64 _evt_counter = 0;

    weapon_instance _pistol;

    enum player_dir_t { dir_none = 0, dir_left, dir_right } _dir;
    bool _on_left            = false;
    bool _jump_pressed       = false;
    bool _jump_down_pressed  = false;
    bool _long_shot_enabled  = false;

    bool _jump_scheduled      = false;
    bool _jump_down_scheduled = false;

    int _group = 0;

    bool _on_hit_event = false;
    std::optional<hit_sound_info> _hit_sound_info;

    bool                 _step_sound_active = false;
    std::optional<float> _fall_velocity;
    sf::Clock _step_sound_timer;

public:
    u64 last_state_packet_id = 0;

    [[nodiscard]]
    const player_name_t& name() const {
        return _name;
    }

    [[nodiscard]]
    vec2f barrel_pos() const {
        if (_pistol) {
            auto pos       = collision_box()->get_position();
            auto arm_pos_f = _pistol.arm_position_factors(_on_left, _bobbing);
            pos += vec2f(_size.x * arm_pos_f.x, _size.y * arm_pos_f.y);
            return pos + _pistol.shot_displacement(_on_left ? vec2f{-1.f, 0.f}
                                                            : vec2f{1.f, 0.f});
        }
        else
            return _collision_box->get_position();
    }

    void increase_deaths() {
        ++_deaths;
    }

    [[nodiscard]]
    u32 get_deaths() const {
        return _deaths;
    }

    void group(int value) {
        _group = value;
    }

    [[nodiscard]]
    int get_group() const {
        return _group;
    }

    void on_left(bool value) {
        _on_left = value;
    }

    [[nodiscard]]
    bool get_on_left() const {
        return _on_left;
    }

    void reset_accel_f(bool left_only) {
        (left_only ? _accel_left_f : _accel_right_f) = 0.f;
    }

    void smooth_velocity_set(const vec2f& value, float smooth_factor = 0.25f) {
        auto vel = get_velocity();
        velocity(vel + (value - vel) * smooth_factor);
    }

    void smooth_position_set(const vec2f& value, float smooth_factor = 0.25f) {
        auto pos = get_position();
        position(pos + (value - pos) * smooth_factor);
    }

    void position(const vec2f& value) {
        _collision_box->position(value);
    }

    [[nodiscard]]
    const vec2f& get_position() const {
        return _collision_box->get_position();
    }

    void velocity(const vec2f& value) {
        _collision_box->velocity(value);
    }

    [[nodiscard]]
    vec2f get_velocity() const {
        return _collision_box->get_velocity();
    }

    [[nodiscard]]
    const vec2f& get_size() const {
        return _size;
    }

    [[nodiscard]]
    float get_max_speed() const {
        return _max_speed;
    }

    void max_speed(float value) {
        _max_speed = value;
    }

    [[nodiscard]]
    float calc_max_jump_y_dist(float gravity_y) const {
        auto t = _jump_speed / gravity_y;
        return (_jump_speed * t - gravity_y * t * t * 0.5f) * float(get_available_jumps());
    }

    [[nodiscard]]
    float get_jump_speed() const {
        return _jump_speed;
    }

    void jump_speed(float value) {
        _jump_speed = value;
    }

    [[nodiscard]]
    float get_x_accel() const {
        return _x_accel;
    }

    void x_accel(float value) {
        _x_accel = value;
    }

    [[nodiscard]]
    float get_x_slowdown() const {
        return _x_slowdown;
    }

    void x_slowdown(float value) {
        _x_slowdown = value;
    }

    [[nodiscard]]
    const physic_group* collision_box() const {
        return _collision_box.get();
    }

    [[nodiscard]]
    physic_group* collision_box() {
        return _collision_box.get();
    }

    void enable_double_jump() {
        jumps_left = max_jumps;
    }

    void on_ground(float fall_velocity = 0.f) {
        enable_double_jump();
        _fall_velocity = fall_velocity;
    }

    void mass(float value) {
        _collision_box->mass(value);
    }

    [[nodiscard]]
    u32 get_available_jumps() const {
        return _collision_box->is_lock_y() ? jumps_left + 1 : jumps_left;
    }

    [[nodiscard]]
    bool long_shot_enabled() const {
        return _long_shot_enabled;
    }

    /*
    [[nodiscard]]
    const rand_float_pool& rand_pool() const {
        return _rand_pool;
    }

    rand_float_pool& rand_pool() {
        return _rand_pool;
    }

    void rand_pool(rand_float_pool pool) {
        _rand_pool = std::move(pool);
    }
    */

    void increment_evt_counter() {
        ++_evt_counter;
    }

    void evt_counter(u64 value) {
        _evt_counter = value;
    }

    [[nodiscard]]
    u64 evt_counter() const {
        return _evt_counter;
    }

    void set_on_hit_event() {
        _on_hit_event = true;
    }

    bool pop_on_hit_event() {
        auto res = _on_hit_event;
        _on_hit_event = false;
        return res;
    }
};

inline void player_hit_callback(physic_point*            bullet_pnt,
                                physic_group*            player_grp,
                                collision_result) {
    if (bullet_pnt->get_user_data() == user_data_type::bullet &&
        player_grp->get_user_data() == user_data_type::player && !bullet_pnt->ready_delete_later()) {

        auto impulse = bullet_pnt->impulse();
        if (player_grp->is_lock_y())
            if (impulse.y < -180.f) 
                player_grp->unlock_y();

        player_grp->apply_impulse(impulse);
        bullet_pnt->delete_later();
        auto pl = std::any_cast<player*>(player_grp->get_user_any());

        pl->reset_accel_f(bullet_pnt->get_direction().x < 0.f);
        pl->set_on_hit_event();
        pl->play_hit_sound(bullet_pnt->get_position(), magnitude(bullet_pnt->impulse()) > 2200.f);
    }
}
} // namespace dfdh
