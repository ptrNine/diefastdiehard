#pragma once

#include "types.hpp"
#include "vec2.hpp"
#include "physic_simulation.hpp"
#include "player.hpp"

namespace dfdh {

inline void adjustment_box_hit_callback(physic_point* bullet_pnt,
                                        physic_group* adjustment_box_grp,
                                        collision_result);

class adjustment_box_callback_uniq_name {
public:
    adjustment_box_callback_uniq_name(const adjustment_box_callback_uniq_name&) = delete;
    adjustment_box_callback_uniq_name& operator=(const adjustment_box_callback_uniq_name&) = delete;

    static adjustment_box_callback_uniq_name& instance() {
        static adjustment_box_callback_uniq_name inst;
        return inst;
    }

    std::string next_name() {
        return "adjbox" + std::to_string(id++);
    }

private:
    adjustment_box_callback_uniq_name() = default;
    ~adjustment_box_callback_uniq_name() = default;
    u32 id = 0;
};

class adjustment_box {
public:
    adjustment_box(std::weak_ptr<player> player,
                   physic_simulation&    sim,
                   const vec2f&          pos,
                   const vec2f&          size):
        _player(std::move(player)) {
        _box = physic_group::create();
        _box->append(physic_line::create({0.f, 0.f}, {0.f, -size.y}));
        _box->append(physic_line::create({0.f, -size.y}, {size.x, 0.f}));
        _box->append(physic_line::create({size.x, -size.y}, {0.f, size.y}));
        _box->append(physic_line::create({size.x, 0.f}, {-size.x, 0.f}));

        _box->position(pos);
        _box->user_data(0xdeaddead);
        _box->user_any(this);

        /* Only for distance change */
        _box->enable_gravity();

        sim.add_primitive(_box);

        _collide_callback_name = adjustment_box_callback_uniq_name::instance().next_name();
        sim.add_collide_callback(_collide_callback_name, adjustment_box_hit_callback);
    }

    [[nodiscard]]
    std::weak_ptr<player> player_ptr() {
        return _player;
    }

    [[nodiscard]]
    const std::string& collide_callback_name() const {
        return _collide_callback_name;
    }

    [[nodiscard]]
    bool expired() const {
        return definitely_greater(_box->get_distance(), 0.f, 0.0001f);
    }

    [[nodiscard]]
    physic_group* physic() {
        return _box.get();
    }

private:
    std::string                   _collide_callback_name;
    std::weak_ptr<player>         _player;
    std::shared_ptr<physic_group> _box;
};


class adjustment_box_mgr {
public:
    void
    add(std::weak_ptr<player> player, physic_simulation& sim, const vec2f& pos, const vec2f& size) {
        _boxes.emplace_back(std::move(player), sim, pos, size);
    }

    void update(physic_simulation& sim) {
        for (auto i = _boxes.begin(); i != _boxes.end();) {
            if (i->expired() || i->physic()->ready_delete_later()) {
                i->physic()->delete_later();
                sim.remove_collide_callback(i->collide_callback_name());
                _boxes.erase(i++);
            } else {
                ++i;
            }
        }
    }

private:
    std::list<adjustment_box> _boxes;
};

inline void adjustment_box_hit_callback(physic_point* bullet_pnt,
                                        physic_group* adjustment_box_grp,
                                        collision_result) {
    if (bullet_pnt->get_user_data() == 0xdeadbeef &&
        adjustment_box_grp->get_user_data() == 0xdeaddead && !bullet_pnt->ready_delete_later()) {
        auto adj_box = std::any_cast<adjustment_box*>(adjustment_box_grp->get_user_any());

        if (auto pl = adj_box->player_ptr().lock()) {
            auto blt_group = std::any_cast<int>(bullet_pnt->get_user_any());
            if (pl->get_group() != -1 && pl->get_group() == blt_group)
                return;

            bullet_pnt->delete_later();
            adjustment_box_grp->delete_later();

            pl->collision_box()->apply_impulse(bullet_pnt->impulse());
            pl->reset_accel_f(bullet_pnt->get_direction().x < 0.f);
            pl->set_on_hit_event();
        }
    }
}
}
