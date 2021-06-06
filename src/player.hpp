#pragma once

#include <deque>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/CircleShape.hpp>

#include "networking.hpp"
//#include "rand_pool.hpp"
#include "bullet.hpp"
#include "types.hpp"
#include "physic_simulation.hpp"
#include "config.hpp"
#include "weapon.hpp"
#include "avg_counter.hpp"

namespace dfdh {

namespace details {
    class player_id_getter {
    public:
        static player_id_getter& instance() {
            static player_id_getter inst;
            return inst;
        }

        [[nodiscard]]
        u32 get() {
            return counter++;
        }

    private:
        u32 counter = 0;
    };

    player_id_getter& player_id() {
        return player_id_getter::instance();
    }
}

struct control_keys {
    static std::string path(int id) {
        return fs::current_path() / ("data/player_controls" + std::to_string(id) + ".cfg");
    }

    control_keys(int id = 0): _id(id) {
        std::string section = "player_controls";
        if (_id != 0)
            section += std::to_string(_id);

        if (!cfg().sections().contains(section)) {
            try {
                cfg().parse(path(_id));
            } catch (...) {
                *this = control_keys(true, _id);
                save();
                cfg().parse(path(_id));
            }
        }

        up      = cfg().get_req<int>(section, "up");
        down    = cfg().get_req<int>(section, "down");
        left    = cfg().get_req<int>(section, "left");
        right   = cfg().get_req<int>(section, "right");
        shot    = cfg().get_req<int>(section, "shot");
        grenade = cfg().get_req<int>(section, "grenade");
    }

    control_keys(bool, int id): _id(id) {
        if (_id == 0) {
            up      = sf::Keyboard::Up;
            down    = sf::Keyboard::Down;
            left    = sf::Keyboard::Left;
            right   = sf::Keyboard::Right;
            shot    = sf::Keyboard::Comma;
            grenade = sf::Keyboard::Dash;
        } else if (_id == 1) {
            up = sf::Keyboard::W;
            down = sf::Keyboard::S;
            left = sf::Keyboard::A;
            right = sf::Keyboard::D;
            shot = sf::Keyboard::Y;
            grenade = sf::Keyboard::U;
        }
    }

    void save() {
        auto ofs = std::ofstream(path(_id));
        if (!ofs.is_open())
            throw std::runtime_error("Can't write " + path(_id));

        ofs << "[player_controls" << (_id == 0 ? std::string("]\n") : std::to_string(_id) + "]\n") <<
               "up      = " << up << "\n"
               "down    = " << down << "\n"
               "left    = " << left << "\n"
               "right   = " << right << "\n"
               "shot    = " << shot << "\n"
               "grenade = " << grenade << std::endl;
    }

    int up, down, left, right, shot, grenade;
    int _id;
};

class player {
public:
    static std::shared_ptr<player> create(u32 id, physic_simulation& sim, const sf::Vector2f& size) {
        return std::make_shared<player>(id, sim, size);
    }

    static std::shared_ptr<player> create(physic_simulation& sim, const sf::Vector2f& size) {
        return std::make_shared<player>(details::player_id().get(), sim, size);
    }

    player(u32 id, physic_simulation& sim, const sf::Vector2f& size): _size(size), _id(id) {
        _collision_box = physic_group::create();
        _collision_box->append(physic_line::create({0.f, 0.f}, {0.f, -size.y}));
        _collision_box->append(physic_line::create({0.f, -size.y}, {size.x, 0.f}));
        _collision_box->append(physic_line::create({size.x, -size.y}, {0.f, size.y}));
        _collision_box->append(physic_line::create({size.x, 0.f}, {-size.x, 0.f}));

        _collision_box->enable_gravity();
        _collision_box->allow_platform(true);
        _collision_box->user_data(0xdeadf00d);
        _collision_box->user_any(this);

        sim.add_primitive(_collision_box);

        _left_hand.setOutlineColor(sf::Color(0, 0, 0));
        _left_hand.setOutlineThickness(_size.x * 0.045f);
        _left_hand.setRadius(size.x * 0.14f);
        _left_hand.setOrigin(size.x * 0.14f, size.x * 0.14f);

        _right_hand.setOutlineColor(sf::Color(0, 0, 0));
        _right_hand.setOutlineThickness(_size.x * 0.045f);
        _right_hand.setRadius(size.x * 0.14f);
        _right_hand.setOrigin(size.x * 0.14f, size.x * 0.14f);

        _left_leg = _left_hand;
        _left_leg.setRadius(size.x * 0.2f);
        _left_leg.setOrigin(size.x * 0.2f, size.x * 0.35f);
        _right_leg = _left_leg;
        _right_leg.setOrigin(size.x * 0.2f, size.x * 0.35f);

        jumps_left = max_jumps;
    }

    player(physic_simulation& sim, const sf::Vector2f& size): player(details::player_id().get(), sim, size) {}

    ~player() {
        _collision_box->delete_later();
    }

    void set_params_from_client(const a_cli_player_params& act) {
        set_body(act.body_txtr, act.body_color);
        set_face(act.face_txtr);
    }

    void update_from_client(const player_states_t& st) {
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

    void update_input(const control_keys& ks, sf::Event evt) {
        if (evt.type == sf::Event::KeyPressed) {
            auto c = evt.key.code;
            if (c == ks.up && !_jump_pressed) {
                jump();
                _jump_pressed = true;
            }
            else if (c == ks.down && !_jump_down_pressed) {
                jump_down();
                _jump_down_pressed = true;
            }
            else if (c == ks.left) {
                _cur_x_accel_l = -_x_accel;
                _dir = dir_left;
            }
            else if (c == ks.right) {
                _cur_x_accel_r = _x_accel;
                _dir = dir_right;
            }
            else if (c == ks.shot) {
                shot();
            }
        }
        else if (evt.type == sf::Event::KeyReleased) {
            auto c = evt.key.code;
            if (c == ks.left) {
                _cur_x_accel_l = 0.f;
            }
            else if (c == ks.right) {
                _cur_x_accel_r = 0.f;
            }
            else if (c == ks.shot) {
                relax();
            }
            else if (c == ks.up) {
                _jump_pressed = false;
            }
            else if (c == ks.down) {
                _jump_down_pressed = false;
            }
        }
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

    void shot() {
        if (_pistol)
            _pistol.pull_trigger(_tracer_color);
    }

    void relax() {
        if (_pistol)
            _pistol.relax_trigger();
    }

    static int player_group_getter(const physic_point* p) {
        return std::any_cast<player*>(p->get_user_any())->get_group();
    }

    template <typename F = void(*)(const sf::Vector2f&, const sf::Vector2f&, float)>
    void game_update(physic_simulation& sim, bullet_mgr& bm, bool spawn_bullet = true, F&& bullet_spawn_callback = nullptr) {
        if (_pistol) {
            auto dir       = _on_left ? sf::Vector2f{-1.f, 0.f} : sf::Vector2f{1.f, 0.f};
            auto pos       = collision_box()->get_position();
            auto arm_pos_f = _pistol.arm_position_factors(_on_left);
            pos += sf::Vector2f(_size.x * arm_pos_f.x, _size.y * arm_pos_f.y);

            if (auto recoil = _pistol.update(pos,
                                             dir,
                                             bm,
                                             sim,
                                             spawn_bullet,
                                             std::forward<F>(bullet_spawn_callback),
                                             _group,
                                             player_group_getter))
                _collision_box->apply_impulse(-dir * *recoil);
        }
    }

    void physic_update(const physic_simulation& sim, float timestep) {
        _accel_f += timestep * 2.f;
        if (_accel_f > 1.f)
            _accel_f = 1.f;

        auto vel = _collision_box->get_velocity();

        float accel = 0.f;
        if (_collision_box->is_lock_y()) {
            if (vel.x > 0.f)
                accel = -_x_slowdown;
            else if (vel.x < 0.f)
                accel = _x_slowdown;
        }

        if (vel.x < _max_speed)
            accel += _cur_x_accel_r * (_accel_left_reset ? _accel_f : 1.f);
        if (vel.x > -_max_speed)
            accel += _cur_x_accel_l * (!_accel_left_reset ? _accel_f : 1.f);

        bool moving = !is_float_zero(_cur_x_accel_r) || !is_float_zero(_cur_x_accel_r);
        float prev_v = vel.x;
        vel.x += accel * timestep;
        if (!moving && ((prev_v < 0.f && vel.x > 0.f) || (prev_v > 0.f && vel.x < 0.f)))
            vel.x = 0.f;

        _collision_box->velocity(vel);

        leg_animation_update(timestep, _collision_box->get_position(), vel);

        if (_position_trace.size() > 300)
            _position_trace.pop_front();
        _position_trace.emplace_back(sim.current_update_time(), get_position());
    }

    void jump() {
        if (_collision_box->is_lock_y() || jumps_left) {
            auto vel = _collision_box->get_velocity();
            vel.y = -_jump_speed;
            _collision_box->velocity(vel);

            if (!_collision_box->is_lock_y())
                --jumps_left;
            else
                _collision_box->unlock_y();
        }
    }

    void jump_down() {
        if (_collision_box->is_lock_y()) {
            _collision_box->unlock_y();
            auto vel = _collision_box->get_velocity();
            vel.y += 100.f;
            _collision_box->velocity(vel);
        }
    }

    void draw(sf::RenderWindow& wnd) {
        auto pos = _collision_box->get_position();
        auto dif = sf::Vector2f((_size.x * _txtr_adjust.x - _size.x) * 0.5f, _size.y * _txtr_adjust.y);

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

        wnd.draw(_left_leg);
        wnd.draw(_right_leg);

        if (_pistol) {
            auto arm_pos_f = _pistol.arm_position_factors(_on_left);
            auto [lh, rh] =
                _pistol.draw(pos + sf::Vector2f(_size.x * arm_pos_f.x, _size.y * arm_pos_f.y),
                             _on_left,
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

    static std::string append_dir(const std::string& path) {
        auto p = fs::path(path);
        if (p == p.filename())
            return fs::current_path() / "data/textures/player" / path;
        else
            return path;
    }

    void set_body(const std::string& path, const sf::Color& color = {255, 255, 255}) {
        set_some(path, _body_txtr, _body, _size, color);
        _left_hand.setFillColor(color);
        _right_hand.setFillColor(color);
        _left_leg.setFillColor(color);
        _right_leg.setFillColor(color);
    }

    void set_face(const std::string& path, const sf::Color& color = {255, 255, 255}) {
        set_some(path, _face_txtr, _face, _size, color);
    }

    void set_hat(const std::string& path, const sf::Color& color = {255, 255, 255}) {
        set_some(path, _hat_txtr, _hat, _size, color);
    }

    void setup_pistol(const std::string& section) {
        _pistol = weapon_instance(section);
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

    player_states_t extract_client_input_state() const {
        //std::cout << "Movleft: " << (_cur_x_accel_l < 0.f) << " movright: " << (_cur_x_accel_r > 0.f) << std::endl;
        return player_states_t{
            _cur_x_accel_l < 0.f,
            _cur_x_accel_r > 0.f,
            _pistol ? _pistol.on_shot() : false,
            _jump_pressed,
            _jump_down_pressed,
            _collision_box->is_lock_y()
        };
    }

    [[nodiscard]]
    std::optional<sf::Vector2f> position_trace_lookup(std::chrono::steady_clock::time_point now, float latency_offset) const {
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

private:
    void leg_animation_update(float timestep, const sf::Vector2f& pos, const sf::Vector2f& vel) {
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
                  const sf::Vector2f& size,
                  const sf::Color&    color) {
        auto p = append_dir(path);
        txtr.loadFromFile(p);
        txtr.setSmooth(true);
        sprite.setTexture(txtr);
        sprite.setColor(color);

        auto sz = txtr.getSize();
        float xf = (size.x * _txtr_adjust.x) / float(sz.x);
        float yf = (size.y * _txtr_adjust.y) / float(sz.y);
        sprite.setScale(xf, yf);
    }

private:
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

    std::deque<std::pair<std::chrono::steady_clock::time_point, sf::Vector2f>> _position_trace;

//    rand_float_pool _rand_pool;

    sf::Color _tracer_color = {255, 255, 0};

    float _leg_timer = 0.f;

    sf::Vector2f _size;
    sf::Vector2f _txtr_adjust = {1.25f, 1.08f};

    bool  _accel_left_reset = false;
    float _accel_f    = 1.f;
    float _max_speed  = 280.f;
    float _x_accel    = 2250.f;
    float _x_slowdown = 700.f;
    float _jump_speed = 620.f;

    float _cur_x_accel_l = 0.f;
    float _cur_x_accel_r = 0.f;

    u32 jumps_left = 1;
    u32 max_jumps = 1;

    u32  _id;

    u32 _deaths = 0;
    u64 _evt_counter = 0;

    weapon_instance _pistol;

    enum player_dir_t {
        dir_none = 0,
        dir_left,
        dir_right
    } _dir;
    bool _on_left = false;
    bool _jump_pressed = false;
    bool _jump_down_pressed = false;

    int _group = 0;

    bool _on_hit_event = false;

public:
    u64 last_state_packet_id = 0;

    [[nodiscard]]
    sf::Vector2f barrel_pos() const {
        if (_pistol) {
            auto pos       = collision_box()->get_position();
            auto arm_pos_f = _pistol.arm_position_factors(_on_left);
            pos += sf::Vector2f(_size.x * arm_pos_f.x, _size.y * arm_pos_f.y);
            return pos + _pistol.shot_displacement(_on_left ? sf::Vector2f{-1.f, 0.f}
                                                            : sf::Vector2f{1.f, 0.f});
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
        _accel_f = 0.f;
        _accel_left_reset = left_only;
    }

    [[nodiscard]]
    u32 id() const {
        return _id;
    }

    void smooth_velocity_set(const sf::Vector2f& value, float smooth_factor = 0.25f) {
        auto vel = get_velocity();
        velocity(vel + (value - vel) * smooth_factor);
    }

    void smooth_position_set(const sf::Vector2f& value, float smooth_factor = 0.25f) {
        auto pos = get_position();
        position(pos + (value - pos) * smooth_factor);
    }

    void position(const sf::Vector2f& value) {
        _collision_box->position(value);
    }

    [[nodiscard]]
    const sf::Vector2f& get_position() const {
        return _collision_box->get_position();
    }

    void velocity(const sf::Vector2f& value) {
        _collision_box->velocity(value);
    }

    [[nodiscard]]
    sf::Vector2f get_velocity() const {
        return _collision_box->get_velocity();
    }

    [[nodiscard]]
    const sf::Vector2f& get_size() const {
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

    void mass(float value) {
        _collision_box->mass(value);
    }

    [[nodiscard]]
    u32 get_available_jumps() const {
        return _collision_box->is_lock_y() ? jumps_left + 1 : jumps_left;
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

inline void
player_hit_callback(physic_point* bullet_pnt, physic_group* player_grp, collision_result) {
    if (bullet_pnt->get_user_data() == 0xdeadbeef && player_grp->get_user_data() == 0xdeadf00d &&
        !bullet_pnt->ready_delete_later()) {
        player_grp->apply_impulse(bullet_pnt->impulse());
        bullet_pnt->delete_later();
        auto pl = std::any_cast<player*>(player_grp->get_user_any());
        pl->reset_accel_f(bullet_pnt->get_direction().x < 0.f);
        pl->set_on_hit_event();
    }
}
}
