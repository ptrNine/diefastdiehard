#pragma once

#include <map>
#include <set>
#include <vector>
#include <variant>
#include <chrono>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Clock.hpp>

#include "types.hpp"
#include "physic_point.hpp"
#include "physic_line.hpp"
#include "physic_group.hpp"
#include "physic_platform.hpp"

namespace dfdh {

template <typename T>
concept CollideCallbackArg = std::same_as<std::remove_const_t<T>, physic_point*> ||
                             std::same_as<std::remove_const_t<T>, physic_line*> ||
                             std::same_as<std::remove_const_t<T>, physic_group*>;

struct collision_result {
    physic_point* p1;
    physic_point* p2;
    float         frame_time;

};

struct ricochet {
public:
    void operator()(physic_point* p1, physic_point* p2, collision_result cr) {
        auto l = dynamic_cast<physic_line*>(cr.p2);
        if (!l)
            return;

        auto el = (p1->elasticity() + p2->elasticity()) * 0.5f;

        auto mass_sum = p1->get_mass() + p2->get_mass();
        auto inel_vel = (p1->impulse() + p2->impulse()) / mass_sum;

        auto el_vel1 =
            (2.f * p2->impulse() + p1->get_velocity() * (p1->get_mass() - p2->get_mass())) / mass_sum;
        auto el_vel2 =
            (2.f * p1->impulse() + p2->get_velocity() * (p2->get_mass() - p1->get_mass())) / mass_sum;

        auto inel_imp1 = inel_vel * p1->get_mass();
        auto inel_imp2 = inel_vel * p2->get_mass();
        auto el_imp1   = el_vel1 * p1->get_mass();
        auto el_imp2   = el_vel2 * p2->get_mass();

        auto imp1 = inel_imp1 * (1.f - el) + el_imp1 * el;
        auto imp2 = inel_imp2 * (1.f - el) + el_imp2 * el;

        auto n       = normalize(l->displacement);
        auto dir     = p1->get_velocity(); //normalize(p1->get_velocity() - p2->get_velocity());
        auto new_dir = normalize(2.f * n * (dir.x * n.x + dir.y * n.y) - dir);

        p2->velocity(imp2 / p2->get_mass());
        p1->velocity(imp1 / p1->get_mass());
        p1->velocity(new_dir * p1->scalar_velocity());
    }
};

class physic_simulation {
public:
    template <typename T>
    struct bb_data {
        sf::FloatRect bb;
        T primitive;
    };

    void update(uint rps = 60, float speed = 1.f) {
        float min_timestep = 1.f / float(rps);
        _last_speed = speed;
        _last_rps   = rps;
        _update_accum += _timer.getElapsedTime().asSeconds();
        _timer.restart();
        if (_update_accum > min_timestep) {
            _update_accum -= min_timestep;
            update_immediate(min_timestep * speed);
        }

        _interpolation_factor = _update_accum / min_timestep;
    }

    void update_immediate(float timestep) {
        _current_update_time = std::chrono::steady_clock::now();
        _last_timestep = timestep;

        constexpr auto update_move =
            [](auto& primitives, auto& i, float timestep, const vec2f& _gravity) {
                auto& prim = *i;
                if (prim->ready_delete_later()) {
                    primitives.erase(i++);
                }
                else {
                    if (prim->is_gravity_enabled())
                        prim->velocity(prim->get_velocity() + _gravity * timestep);
                    prim->update_bb(timestep);
                    ++i;
                }
            };

        for (auto i = _pointonly.begin(); i != _pointonly.end();)
            update_move(_pointonly, i, timestep, _gravity);
        for (auto i = _lineonly.begin(); i != _lineonly.end();)
            update_move(_lineonly, i, timestep, _gravity);

        std::set<std::pair<physic_point*, physic_point*>> collisions;

        for (auto& line : _lineonly) {
            for (auto ni : group_tree_view(line.get())) {
                for (auto& point : _pointonly) {
                    if (collisions.contains(std::pair{line.get(), point.get()}))
                        continue;

                    bool collide = false;
                    for (auto nj : group_tree_view(point.get())) {
                        if (!ni->allow_test_with(nj))
                            continue;

                        if (ni->bb().intersects(nj->bb())) {
                            if (analyze(timestep, ni, nj)) {
                                collide = true;
                                break;
                            }
                        }
                    }

                    if (collide)
                        collisions.insert(std::pair{line.get(), point.get()});
                }
            }
        }

        constexpr auto update_platform = [](physic_point* prim, float timestep,
                                            auto& platforms_callbacks, auto& platforms) {
            if (prim->allow_platform()) {
                auto bb1 = prim->pos_bb().rect();
                auto pos_y = prim->get_position().y;
                prim->move(timestep);
                auto bb2 = prim->pos_bb().rect();

                auto low1 = bb1.top + bb1.height;
                auto y_diff = low1 - pos_y;
                auto low2 = bb2.top + bb2.height;
                auto l = bb2.left;
                auto r = l + bb2.width;
                bool stay_on = false;

                for (auto& p : platforms) {
                    auto p_l = p.get_position().x;
                    auto p_r = p.get_position().x + p.length();

                    if (low1 <= p.get_position().y && low2 >= p.get_position().y) {
                        if ((r > p_l && l < p_r) ||
                            (p_r > l && p_l < r)) {
                            auto vel = prim->get_velocity();
                            vel.y = 0.f;
                            prim->velocity(vel);

                            auto pos = prim->get_position();
                            pos.y = (p.get_position().y - y_diff) + 0.001f;
                            prim->position(pos);
                            prim->_lock_y = true;
                            stay_on = true;

                            for (auto& [_, c] : platforms_callbacks)
                                c(prim);
                        }
                    }
                    else if (prim->_lock_y &&
                             essentially_equal(prim->get_position().y,
                                                (p.get_position().y - y_diff) + 0.001f,
                                                0.0001f)) {
                        if (!((r > p_l && l < p_r) || (p_r > l && p_l < r))) {
                            stay_on = stay_on || false;
                        }
                        else {
                            stay_on = true;
                        }
                    }
                }

                if (!stay_on)
                    prim->_lock_y = false;
            } else {
                prim->move(timestep);
            }
        };

        for (auto& prim : _pointonly)
            update_platform(prim.get(), timestep, _platforms_callbacks, _platforms);
        for (auto& prim : _lineonly)
            update_platform(prim.get(), timestep, _platforms_callbacks, _platforms);


        for (auto& [_, c] : _update_callbacks)
            c(*this, timestep);
    }

    static float distance(const sf::Vector3f& line, const vec2f& point) {
        return line.x * point.x + line.y * point.y + line.z;
    }

    bool analyze(float timestep, physic_point* p1, physic_point* p2) {
        physic_point* pnt1 = (dynamic_cast<physic_line*>(p1) == nullptr) ? p1 : nullptr;
        physic_point* pnt2 = (dynamic_cast<physic_line*>(p2) == nullptr) ? p2 : nullptr;
        auto* ln1 = dynamic_cast<physic_line*>(p1);
        auto* ln2 = dynamic_cast<physic_line*>(p2);

        if (pnt2 && !pnt1) {
            pnt1 = pnt2;
            pnt2 = nullptr;
        }

        if (ln1 && !ln2)
            ln2 = ln1;

        if (pnt1 && pnt2) {
            return false;
        }
        if (pnt1 && ln2) {
            auto f_low = 0.f;
            auto f_up = 1.f;

            auto eq_low   = ln2->equation(timestep, f_low);
            auto eq_up    = ln2->equation(timestep, f_up);
            auto p_low    = pnt1->get_position();
            auto p_up     = pnt1->interpolated_pos(timestep, f_up);
            auto dist_low = distance(eq_low, p_low);
            auto dist_up  = distance(eq_up, p_up);

            constexpr auto diff_sign = [](float a, float b) {
              return (a >= 0.f && b < 0.f) ||
                     (a < 0.f && b >= 0.f);
            };

            if (!diff_sign(dist_low, dist_up))
                return false;

            bool attempt = false;

            float dist_mid = std::numeric_limits<float>::max();
            float f_mid;
            for (u32 i = 0; i < _steps; ++i) {
                f_mid = (f_up - f_low) * 0.5f + f_low;
                auto eq_mid    = ln2->equation(timestep, f_mid);
                auto p_mid     = pnt1->interpolated_pos(timestep, f_mid);
                dist_mid = distance(eq_mid, p_mid);

                if (std::fabs(dist_mid) < _collide_dist) {
                    attempt = true;
                    break;
                }

                if (diff_sign(dist_low, dist_mid)) {
                    eq_up   = eq_mid;
                    p_up    = p_mid;
                    dist_up = dist_mid;
                    f_up    = f_mid;
                } else if (diff_sign(dist_mid, dist_up)) {
                    eq_low   = eq_mid;
                    p_low    = p_mid;
                    dist_low = dist_mid;
                    f_low    = f_mid;
                } else {
                    throw std::runtime_error("Sosi");
                }
            }

            if (attempt) {
                resolve(pnt1, ln2, f_mid * timestep);
                return true;
            }
        }
        if (ln1 && ln2) {
            /* Not implemented */
        }
        return false;
    }

    void resolve(physic_point* p1, physic_point* p2, float frame_time) {
        constexpr auto make_arg = [](physic_point* p) -> collide_callback_arg_generic_t {
            if (auto group = p->_group.lock()) {
                while (auto g = group->_group.lock())
                    group = g;
                return group.get();
            }
            if (auto l = dynamic_cast<physic_line*>(p))
                return l;
            return p;
        };

        auto a = make_arg(p1);
        auto b = make_arg(p2);

        for (auto& [_, f] : _collide_callbacks)
            f(a, b, collision_result{p1, p2, frame_time});
    }

    void add_primitive(std::shared_ptr<physic_point> primitive) {
        if (primitive->line_only())
            _lineonly.insert(std::move(primitive));
        else
            _pointonly.insert(std::move(primitive));
    }

    void remove_primitive(const std::shared_ptr<physic_point>& primitive) {
        if (primitive->line_only())
            _lineonly.erase(primitive);
        else
            _pointonly.erase(primitive);
    }

    void add_platform(const physic_platform& platform) {
        _platforms.push_back(platform);
    }

    void remove_all_platforms() {
        _platforms.clear();
    }

    template <typename F>
    void add_collide_callback(const std::string& name, F&& callback) {
        add_collide_callback_internal(name, std::function{callback});
    }

    bool remove_collide_callback(const std::string& name) {
        return _collide_callbacks.erase(name) > 0;
    }

    template <typename F>
    void add_update_callback(const std::string& name, F&& callback) {
        _update_callbacks[name] = std::function{callback};
    }

    bool remove_update_callback(const std::string& name) {
        return _update_callbacks.erase(name) > 0;
    }

    template <typename F>
    void add_platform_callback(const std::string& name, F&& callback) {
        _platforms_callbacks[name] = std::function{callback};
    }

    bool remove_platform_callback(const std::string& name) {
        return _platforms_callbacks.erase(name) > 0;
    }


    [[nodiscard]]
    decltype(auto) line_primitives() const {
        return _lineonly;
    }

    [[nodiscard]]
    decltype(auto) point_primitives() const {
        return _pointonly;
    }

    [[nodiscard]]
    const vec2f& gravity() const {
        return _gravity;
    }

    [[nodiscard]]
    decltype(auto) platforms() const {
        return _platforms;
    }

    void gravity(const vec2f& value) {
        _gravity = value;
    }

    [[nodiscard]]
    float last_timestep() const {
        return _last_timestep;
    }

    [[nodiscard]]
    u32 last_rps() const {
        return _last_rps;
    }

    [[nodiscard]]
    float last_speed() const {
        return _last_speed;
    }

    [[nodiscard]]
    auto current_update_time() const {
        return _current_update_time;
    }

    [[nodiscard]]
    float interpolation_factor() const {
        return _interpolation_factor;
    }

private:
    template <CollideCallbackArg T1, CollideCallbackArg T2>
    void add_collide_callback_internal(const std::string&                            name,
                                       std::function<void(T1, T2, collision_result)> callback) {
        _collide_callbacks[name] = [callback](collide_callback_arg_generic_t a,
                                              collide_callback_arg_generic_t b,
                                              collision_result               cr) {
            std::visit(
                [b, callback, cr](auto a) {
                    std::visit(
                        [a, callback, cr](auto b) {
                            if constexpr (std::is_invocable_v<decltype(callback),
                                                              decltype(a),
                                                              decltype(b),
                                                              collision_result>)
                                callback(a, b, cr);
                        },
                        b);
                },
                a);
        };
    }

private:
    std::vector<physic_platform>            _platforms;
    std::set<std::shared_ptr<physic_point>> _pointonly;
    std::set<std::shared_ptr<physic_point>> _lineonly;
    u32                                     _steps        = 20;
    float                                   _collide_dist = 0.001f;
    vec2f                                   _gravity      = {0.f, 9.8f};
    sf::Clock                               _timer;
    float                                   _last_timestep        = 1.f / 60.f;
    u32                                     _last_rps             = 60;
    float                                   _last_speed           = 1.f;
    float                                   _update_accum         = 0.f;
    float                                   _interpolation_factor = 0.f;

    std::chrono::steady_clock::time_point _current_update_time = std::chrono::steady_clock::now();

    using collide_callback_arg_generic_t =
        std::variant<physic_line*, physic_point*, physic_group*>;
    using collide_callback_generic_t =
        std::function<void(collide_callback_arg_generic_t, collide_callback_arg_generic_t, collision_result)>;

    std::map<std::string, collide_callback_generic_t> _collide_callbacks;
    std::map<std::string, std::function<void(const physic_simulation&, float)>> _update_callbacks;
    std::map<std::string, std::function<void(physic_point*)>> _platforms_callbacks;
};

}
