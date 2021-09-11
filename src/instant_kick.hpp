#pragma once

#include <list>

#include "types.hpp"
#include "physic_simulation.hpp"
#include "bullet.hpp"

namespace dfdh {
class instant_kick {
public:
    instant_kick(physic_simulation&          sim,
                 const vec2f&                position,
                 const vec2f&                end_point,
                 float                       mass,
                 float                       velocity,
                 int                         group,
                 std::weak_ptr<physic_point> related_bullet_pbox):
        _group(group), _related_bullet_pbox(std::move(related_bullet_pbox)) {
        auto tstep  = sim.last_timestep();
        auto dist   = end_point - position;
        auto vel    = dist * (1.f / tstep);
        auto s_vel  = magnitude(vel);
        auto f_mass = velocity / s_vel;
        mass *= f_mass;

        _ph = physic_point::create(position, normalize(vel), s_vel, mass);
        _ph->user_any(group);
        sim.add_primitive(_ph);
    }

    [[nodiscard]]
    auto& physic() {
        return _ph;
    }

    [[nodiscard]]
    auto& physic() const {
        return _ph;
    }

    [[nodiscard]]
    int group() const {
        return _group;
    }

    [[nodiscard]]
    bool expired() const {
        return definitely_greater(_ph->get_distance(), 0.f, 0.0001f);
    }

    [[nodiscard]]
    std::weak_ptr<physic_point>& related_bullet_pbox() {
        return _related_bullet_pbox;
    }

private:
    std::shared_ptr<physic_point> _ph;
    int                           _group;
    std::weak_ptr<physic_point>   _related_bullet_pbox;
};

class instant_kick_mgr {
public:
    template <typename F>
    instant_kick_mgr(const std::string& name, physic_simulation& sim, F hit_callback): _name("ikm_" + name) {
        sim.add_collide_callback(_name, std::move(hit_callback));
    }

    void spawn(physic_simulation&          sim,
               const vec2f&                position,
               const vec2f&                end_point,
               float                       mass,
               float                       velocity,
               int                         group,
               std::weak_ptr<physic_point> related_bullet_pbox = {}) {
        auto& kick = _kicks.emplace_back(
            sim, position, end_point, mass, velocity, group, std::move(related_bullet_pbox));
        kick.physic()->user_data(0xdeadbeef);
        if (group != -1)
            kick.physic()->set_collide_allower([=](const physic_point* p) {
                return p->get_user_data() == 0xdeaddead;
            });
    }

    void update() {
        for (auto i = _kicks.begin(); i != _kicks.end();) {
            if (i->expired() || i->physic()->ready_delete_later()) {
                if (i->physic()->ready_delete_later())
                    if (auto blt = i->related_bullet_pbox().lock())
                        blt->delete_later();
                i->physic()->delete_later();
                _kicks.erase(i++);
            } else
                ++i;
        }
    }

private:
    std::string             _name;
    std::list<instant_kick> _kicks;
};
}
