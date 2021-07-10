#pragma once

#include <map>
#include <set>
#include <mutex>
#include <queue>
#include <atomic>
#include <memory>
#include <thread>
#include <optional>

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Clock.hpp>

#include "fixed_string.hpp"
#include "types.hpp"
#include "vec_math.hpp"

namespace dfdh {

enum ai_difficulty {
    ai_easy = 0,
    ai_medium,
    ai_hard
};

class ai_mgr_singleton {
public:
    static ai_mgr_singleton& instance() {
        static ai_mgr_singleton inst;
        return inst;
    }

    ai_mgr_singleton(const ai_mgr_singleton&) = delete;
    ai_mgr_singleton& operator=(const ai_mgr_singleton&) = delete;

    void reset_all() {
        std::lock_guard lock{mtx};

        players.clear();
        bullets.clear();
        platforms.clear();
        platform_map.clear();
    }

    template <typename C, typename F>
    void provide_players(const C& c, F get_adapter) {
        std::lock_guard lock{mtx};

        for (auto& [_, p] : c) {
            auto insert_player = get_adapter(p);
            players.insert_or_assign(insert_player.name, insert_player);
        }
    }

    void provide_physic_sim(const sf::Vector2f& gravity, float time_speed, u32 last_rps) {
        std::lock_guard lock{mtx};

        physic_sim.gravity = gravity;
        physic_sim.time_speed = time_speed;
        physic_sim.last_rps = last_rps;
    }

    void provide_level(const sf::Vector2f& level_size) {
        std::lock_guard lock{mtx};

        level.level_size = level_size;
    }

    template <typename C, typename F>
    void provide_bullets(const C& c, F get_adapter) {
        std::lock_guard lock{mtx};

        bullets.resize(c.size());
        size_t i = 0;
        for (auto it = c.begin(); it != c.end(); ++it)
            bullets[i++] = get_adapter(*it);
    }

    template <typename C, typename F>
    void provide_platforms(const C& c, F get_adapter) {
        std::lock_guard lock{mtx};

        platforms.resize(c.size());
        for (size_t i = 0; i < platforms.size(); ++i)
            platforms[i] = get_adapter(c[i]);

        rebuild_platform_map();
    }

    void worker_start() {
        _work = true;
        _stopped = false;

        _thread = std::thread(worker, this);
    }

    void worker_stop() {
        _work = false;
        while (!_stopped);

        _thread.join();
    }

    static void worker(ai_mgr_singleton* ai) {
        while (ai->_work) {
            float step = 1.f / static_cast<float>(ai->physic_sim.last_rps);
            if (ai->_timer.elapsed() > step) {
                ai->_timer.restart();

                std::lock_guard lock{ai->mtx};
                ai->worker_op();
            }

            std::this_thread::yield();
        }
        ai->_stopped = true;
    }

private:
    ai_mgr_singleton() = default;

    ~ai_mgr_singleton() {
        if (_work)
            worker_stop();
    }

    friend class ai_operator;

    void worker_op();

    void add_ai_operator(const player_name_t& name, class ai_operator* oprt) {
        std::lock_guard lock{mtx};

        _operators.emplace(name, oprt);
    }

    void remove_ai_operator(const player_name_t& name) {
        std::lock_guard lock{mtx};

        _operators.erase(name);
    }

public:
    struct player_t {
        sf::Vector2f  pos;
        sf::Vector2f  dir;
        sf::Vector2f  size;
        sf::Vector2f  vel;
        sf::Vector2f  barrel_pos;
        player_name_t name;
        u32           available_jumps;
        float         x_accel;
        float         x_slowdown;
        float         jump_speed;
        float         max_jump_dist;
        float         max_vel_x;
        float         gun_dispersion;
        int           group;
        float         gun_bullet_vel;
        float         gun_fire_rate;
        bool          on_left;
        bool          is_y_locked;
    };

    struct physic_sim_t {
        sf::Vector2f gravity;
        float        time_speed;
        u32          last_rps;
    };

    struct level_t {
        sf::Vector2f level_size;
    };

    struct bullet_t {
        sf::Vector2f pos;
        sf::Vector2f vel;
        float        hit_mass;
        int          group;
    };

    struct platform_t {
        sf::Vector2f pos1;
        sf::Vector2f pos2;
    };

    using plat_dist_t       = sf::Vector2f;
    using plat_neighbours_t = std::vector<plat_dist_t>;
    using ai_plat_map_t     = std::vector<plat_neighbours_t>;

    std::map<player_name_t, player_t> players;
    std::vector<bullet_t>             bullets;
    physic_sim_t                      physic_sim;
    std::vector<platform_t>           platforms;
    ai_plat_map_t                     platform_map;
    level_t                           level;

    float platforms_bound_start_x;
    float platforms_bound_end_x;

    std::map<player_name_t, class ai_operator*> _operators;
    std::atomic<bool>                           _work;
    std::atomic<bool>                           _stopped;
    timer                                       _timer;
    std::thread                                 _thread;

private:
    mutable std::mutex mtx;

private:
    void rebuild_platform_map() {
        platforms_bound_start_x = std::numeric_limits<float>::max();
        platforms_bound_end_x = std::numeric_limits<float>::lowest();

        platform_map.clear();
        platform_map.resize(platforms.size());

        for (size_t i = 0; i != platforms.size(); ++i) {
            if (platforms[i].pos1.x < platforms_bound_start_x)
                platforms_bound_start_x = platforms[i].pos1.x;

            if (platforms[i].pos2.x > platforms_bound_end_x)
                platforms_bound_end_x = platforms[i].pos2.x;

            auto neighbours = plat_neighbours_t(platforms.size());

            for (size_t j = 0; j != platforms.size(); ++j) {
                if (i == j) {
                    neighbours[j] = {0.f, 0.f};
                    continue;
                }

                sf::Vector2f dist{0.f, 0.f};

                auto& current   = platforms[i];
                auto& neighbour = platforms[j];
                float cl = current.pos1.x;
                float cr = current.pos2.x;
                float cy = current.pos1.y;

                float nl = neighbour.pos1.x;
                float nr = neighbour.pos2.x;
                float ny = neighbour.pos1.y;

                if ((cl <= nr && cr > nl) || (cl < nr && cr >= nl))
                    dist.y = neighbour.pos1.y - current.pos1.y;
                else if (cl > nr)
                    dist = sf::Vector2f{nr - cl, ny - cy};
                else
                    dist = sf::Vector2f{nl - cr, ny - cy};

                neighbours[j] = dist;
            }

            platform_map[i] = std::move(neighbours);
        }
    }
};


inline ai_mgr_singleton& ai_mgr() {
    return ai_mgr_singleton::instance();
}


struct platform_res_t {
    const ai_mgr_singleton::platform_t *stand_on = nullptr, *can_stand_on = nullptr,
                                       *nearest = nullptr;
};


class ai_operator : public std::enable_shared_from_this<ai_operator> {
public:
    friend class ai_mgr_singleton;

    enum task_t {
        t_move_left = 0,
        t_move_right,
        t_stop,
        t_shot,
        t_relax,
        t_jump,
        t_jump_down
    };

    static std::shared_ptr<ai_operator> create(ai_difficulty difficulty, const player_name_t& player_name) {
        return std::make_shared<ai_operator>(difficulty, player_name);
    }

    ai_operator(ai_difficulty difficulty, const player_name_t& player_name):
        _difficulty(difficulty), _player_name(player_name) {
        ai_mgr().add_ai_operator(player_name, this);
    }

    ~ai_operator() {
        ai_mgr().remove_ai_operator(_player_name);
    }

    std::optional<task_t> consume_task() {
        std::lock_guard lock{mtx};

        if (_tasks.empty())
            return {};
        else {
            auto t = _tasks.front();
            _tasks.pop();
            return t;
        }
    }

    void produce_task(task_t task) {
        std::lock_guard lock{mtx};
        _tasks.push(task);
    }

    void move_left() {
        produce_task(t_move_left);
        _mov_left = true;
        _mov_right = false;
    }

    void move_right() {
        produce_task(t_move_right);
        _mov_right = true;
        _mov_left = false;
    }

    void stop() {
        produce_task(t_stop);
        _mov_left = false;
        _mov_right = false;
    }

    void jump() {
        if (!_jump_was) {
            _delayed_jump_delay = 0.0f;
            produce_task(t_jump);
            _jump_was = true;
        }
    }

    void delayed_jump(float delay) {
        if (_delayed_jump_delay > 0.001f)
            return;

        _delayed_jump_delay = delay;
        _start_delayed_jump.restart();
    }

    void jump_down() {
        produce_task(t_jump_down);
    }

    void shot() {
        _shot = true;
        produce_task(t_shot);
        _timer.restart();
    }

    void relax() {
        produce_task(t_relax);
        _shot = false;
    }

    [[nodiscard]]
    bool is_on_shot() const {
        return _shot;
    }

    [[nodiscard]]
    ai_difficulty difficulty() const {
        return _difficulty;
    }

    void set_difficulty(ai_difficulty value) {
        _difficulty = value;
    }

    void shot_next_frame(float shot_delay = 0.f) {
        if (!_shot_next_frame) {
            _shot_delay = shot_delay;
            _shot_next_frame = true;
            _start_shot_timer.restart();
        }
    }

    void update_shot_next_frame(const ai_mgr_singleton::physic_sim_t& sim) {
        _jump_was = false;

        if (_delayed_jump_delay > 0.001f &&
            _start_delayed_jump.elapsed(sim.time_speed) > _delayed_jump_delay)
            jump();

        if (_shot_next_frame && _start_shot_timer.elapsed(sim.time_speed) > _shot_delay) {
            shot();
            _shot_next_frame = false;
        }
    }

    [[nodiscard]]
    const player_name_t player_name() const {
        return _player_name;
    }

    [[nodiscard]]
    auto& operated_platforms() const {
        return _operated_platforms;
    }

    [[nodiscard]]
    auto& operated_platforms() {
        return _operated_platforms;
    }

    [[nodiscard]]
    float jump_x_max() const {
        return _jump_x_max;
    }

    [[nodiscard]]
    auto last_stand_on_plat() const {
        return _last_stand_on_plat;
    }

    static constexpr player_name_t no_target = player_name_t{};

private:
    ai_difficulty      _difficulty;
    player_name_t      _player_name;
    std::queue<task_t> _tasks;

    float _shot_delay = 0.f;
    timer _start_shot_timer;
    float _delayed_jump_delay = 0.f;
    timer _start_delayed_jump;
    float _last_plat_path_delay = 0.f;
    timer _start_plat_path;

    mutable std::mutex mtx;

    timer                               _timer;
    player_name_t                       _target_id;
    player_name_t                       _last_target_id;
    float                               _jump_x_max         = 0.f;
    const ai_mgr_singleton::platform_t* _last_stand_on_plat = nullptr;
    platform_res_t                      _operated_platforms;
    bool                                _mov_left        = false;
    bool                                _mov_right       = false;
    bool                                _shot            = false;
    bool                                _shot_next_frame = false;
    bool                                _jump_was        = false;
};

inline bool overlap(float a1, float a2, float b1, float b2) {
    return (a2 > b1 && a1 < b2) || (b2 > a1 && b1 < a2);
}

inline constexpr float falling_dist = 350.f;

inline bool hard_i_see_you(const ai_mgr_singleton::player_t& it,
                             const ai_mgr_singleton::player_t& pl,
                             const ai_mgr_singleton::level_t&  level,
                             float                             gun_dispersion) {
    if (it.group != -1 && it.group == pl.group)
        return false;

    if (gun_dispersion > M_PI)
        return true;

    if (std::fabs(pl.pos.x) - level.level_size.x > falling_dist)
        return false;

    auto disp = gun_dispersion * 0.35f;
    auto x_dist = std::fabs(pl.pos.x - it.pos.x);
    auto y_add = std::sin(disp) * x_dist;

    return overlap(pl.pos.y, pl.pos.y + pl.size.y, it.pos.y - y_add + it.size.y * 0.5f, it.pos.y + y_add + it.size.y * 0.5f);
}

inline bool i_see_you(const ai_mgr_singleton::player_t& it, const ai_mgr_singleton::player_t& pl) {
    if (it.group != -1 && it.group == pl.group)
        return false;
    return overlap(it.pos.y, it.pos.y + it.size.y, pl.pos.y, pl.pos.y + pl.size.y);
}

inline std::optional<std::pair<player_name_t, sf::Vector2f>>
easy_ai_find_nearest(const ai_mgr_singleton::player_t&                          it,
                     const std::map<player_name_t, ai_mgr_singleton::player_t>& players,
                     const ai_mgr_singleton::level_t&                           level,
                     ai_operator&                                               oper) {
    static constexpr auto cmp = [](const std::pair<player_name_t, sf::Vector2f>& lhs,
                                   const std::pair<player_name_t, sf::Vector2f>& rhs) {
        return magnitude2(lhs.second) < magnitude2(rhs.second);
    };

    std::set<std::pair<player_name_t, sf::Vector2f>, decltype(cmp)> nearests;
    for (auto& [_, pl] : players) {
        auto iseeyou = oper.difficulty() == ai_hard ? hard_i_see_you(it, pl, level, it.gun_dispersion)
                                                      : i_see_you(it, pl);
        if (pl.name != it.name && iseeyou)
            nearests.emplace(pl.name, pl.pos - it.pos);
    }

    if (nearests.empty())
        return {};
    else
        return *nearests.begin();
}

inline auto calc_dist_to_platform(const ai_mgr_singleton::platform_t& plat,
                                  const ai_mgr_singleton::player_t&   plr) {
    if (plr.pos.x > plat.pos2.x)
        return plat.pos2 - plr.pos;
    else if (plr.pos.x < plat.pos1.x)
        return plat.pos1 - plr.pos;
    else
        return sf::Vector2f(0.f, plat.pos1.y - plr.pos.y);
}

inline platform_res_t find_platform(const std::vector<ai_mgr_singleton::platform_t>& platforms,
                                    const ai_mgr_singleton::player_t&                player) {
    platform_res_t res;
    sf::Vector2f nearest_dist;
    float nearest_dist_sc = std::numeric_limits<float>::max();

    auto& p = player.pos;
    for (auto& pl : platforms) {
        if (p.x > pl.pos1.x && p.x < pl.pos2.x) {
            if (approx_equal(pl.pos1.y, p.y, 0.0001f)) {
                res.stand_on = &pl;
                res.can_stand_on = &pl;
                res.nearest = &pl;
                break;
            }
            else {
                if (pl.pos1.y > player.pos.y &&
                    (!res.can_stand_on ||
                     res.can_stand_on->pos1.y - player.pos.y > pl.pos1.y - player.pos.y)) {
                    res.can_stand_on = &pl;
                }
            }
        }

        auto dist_to_plat = calc_dist_to_platform(pl, player);
        float dist_to_plat_sc = magnitude(dist_to_plat);
        if (!res.nearest || dist_to_plat_sc < nearest_dist_sc) {
            res.nearest = &pl;
            nearest_dist = dist_to_plat;
            nearest_dist_sc = dist_to_plat_sc;
        }
    }

    return res;
}

enum ai_move_type {
    ai_move_none = 0,
    ai_move_left,
    ai_move_right,
    ai_move_stop
};

struct ai_move_spec {
    void update_move(ai_move_type itype, int ipriority) {
        if (ipriority >= priority) {
            type = itype;
            priority = ipriority;
        }
    }

    void update_apply(ai_operator& oper, ai_move_type itype, int ipriority) {
        if (ipriority >= priority) {
            apply(oper);
            type = itype;
            priority = ipriority;
        }
    }

    void apply(ai_operator& oper) {
        switch (type) {
            case ai_move_left:
                oper.move_left();
                break;
            case ai_move_right:
                oper.move_right();
                break;
            case ai_move_stop:
                oper.stop();
                break;
            default:
                break;
        }
        type = ai_move_none;
        priority = 0;
    }

    ai_move_type type = ai_move_none;
    int priority = 0;
};

inline void easy_ai_platform_actions(const std::vector<ai_mgr_singleton::platform_t>& platforms,
                                     const platform_res_t&             plat,
                                     const ai_mgr_singleton::player_t& player,
                                     ai_operator&                      oper,
                                     ai_move_spec&                     move_spec) {
    auto [stand_on, can_stand_on, nearest_plat] = plat;

    if (!stand_on && !can_stand_on) {
        if (nearest_plat) {
            auto dist = calc_dist_to_platform(*nearest_plat, player);
            if (dist.x < 0.f)
                move_spec.update_move(ai_move_left, 200);
            else
                move_spec.update_move(ai_move_right, 200);
            if (dist.y < 0.f && player.vel.y > -0.001f)
                oper.jump();
        }
    }
    else {
        auto plr = player;
        constexpr auto solve = [](float x, float v0, float a) {
            float t = v0 / a;
            return x + v0 * t + a * t * t * 0.5f;
        };

        if (player.vel.x > 0.f) {
            plr.pos.x = solve(player.pos.x, player.vel.x, player.x_slowdown + player.x_accel * 0.2f);
            if (find_platform(platforms, plr).can_stand_on == nullptr) {
                plr.pos.x = solve(player.pos.x, player.vel.x, player.x_slowdown + player.x_accel);
                if (find_platform(platforms, plr).can_stand_on == nullptr) {
                    if (player.vel.y > -0.001f)
                        oper.jump();
                }

                move_spec.update_move(ai_move_left, 100);
            }
        }
        else {
            plr.pos.x = solve(player.pos.x, player.vel.x, -(player.x_slowdown + player.x_accel * 0.2f));
            if (find_platform(platforms, plr).can_stand_on == nullptr) {
                plr.pos.x = solve(player.pos.x, player.vel.x, -player.x_slowdown - player.x_accel);
                if (find_platform(platforms, plr).can_stand_on == nullptr) {
                    if (player.vel.y > -0.001f)
                        oper.jump();
                }
                move_spec.update_move(ai_move_right, 100);
            }
        }
    }
}

inline std::vector<const ai_mgr_singleton::bullet_t*>
find_dangerous_bullets(const std::vector<ai_mgr_singleton::bullet_t>& bullets,
                       const ai_mgr_singleton::player_t&              player,
                       float                                          bullet_timeshift) {
    std::vector<const ai_mgr_singleton::bullet_t*> dangerous;

    for (auto& bl : bullets) {
        if (bl.pos.y > player.pos.y || bl.pos.y < player.pos.y - player.size.y)
            continue;

        if (bl.group != -1 && bl.group == player.group)
            continue;

        auto dist_x = player.pos.x - bl.pos.x;
        if ((bl.vel.x > 0.f && dist_x > 0.f) || (bl.vel.x < 0.f && dist_x < 0.f)) {
            float time = dist_x / (bl.vel.x - player.vel.x);
            if (time < std::fabs(bl.vel.x) * 0.00013f + bullet_timeshift)
                dangerous.push_back(&bl);
        }
    }

    return dangerous;
}

inline void dodge_ai(const std::vector<ai_mgr_singleton::bullet_t>& bullets,
                     const ai_mgr_singleton::player_t&              player,
                     const ai_mgr_singleton::physic_sim_t&          physic_sim,
                     ai_move_spec&/*                                  move_spec*/,
                     ai_operator&                                   oper) {
    if (oper.difficulty() < ai_difficulty::ai_medium)
        return;

    if (oper.difficulty() == ai_medium) {
        /* 8% chance */
        bool ok = static_cast<u32>(std::fabs(player.pos.x)) % 12 != 0;
        if (ok)
            return;

        for (auto& bl : bullets) {
            if (bl.pos.y > player.pos.y || bl.pos.y < player.pos.y - player.size.y)
                continue;

            if (bl.group != -1 && bl.group == player.group)
                continue;

            auto dist_x = player.pos.x - bl.pos.x;
            if ((bl.vel.x > 0.f && dist_x > 0.f) || (bl.vel.x < 0.f && dist_x < 0.f)) {
                float time = dist_x / (bl.vel.x - player.vel.x);
                if (time < 0.2f) {
                    if (player.vel.y > -0.001f)
                        oper.jump();
                }
            }
        }
    }

    if (oper.difficulty() == ai_hard) {
        constexpr auto calc_hit_mass =
            [](const std::vector<const ai_mgr_singleton::bullet_t*>& bullets) {
                float hit_mass = 0;
                for (auto b : bullets) hit_mass += (b->vel.x > 0.f ? b->hit_mass : -b->hit_mass);
                return hit_mass;
            };

        auto full_mass = calc_hit_mass(find_dangerous_bullets(bullets, player, 0.f));
        if (std::fabs(full_mass) < 0.1f)
            return;

        auto afterjump_pos =
            player.pos + sf::Vector2f(player.vel.x, -player.jump_speed) * 0.5f + physic_sim.gravity * 0.125f;
        auto afterjump_player = player;
        afterjump_player.pos  = afterjump_pos;

        float afterjump_full_mass =
            calc_hit_mass(find_dangerous_bullets(bullets, afterjump_player, 0.5f));

        if (std::fabs(afterjump_full_mass) < std::fabs(full_mass)) {
            if (player.vel.y > -0.001f)
                oper.jump();
        }
    }
}

inline void hard_shooter(const std::map<player_name_t, ai_mgr_singleton::player_t>& players,
                         const ai_mgr_singleton::player_t&                          plr,
                         ai_operator&                                               op,
                         const ai_mgr_singleton::physic_sim_t&                      physic_sim,
                         const ai_mgr_singleton::level_t&                           level,
                         ai_move_spec&) {
    if (op.is_on_shot())
        return;

    std::map<float, const ai_mgr_singleton::player_t*> targets;

    for (auto& [_, trg] : players) {
        if (trg.name == plr.name || trg.is_y_locked)
            continue;

        if (trg.group != -1 && trg.group == plr.group)
            continue;

        if (std::fabs(trg.pos.x) - level.level_size.x > falling_dist)
            continue;

        auto a = 0.5f *  physic_sim.gravity.y;
        auto b = trg.vel.y;
        auto c = trg.pos.y - plr.pos.y;
        auto d = b * b - 4.f * a * c;

        if (d < 0.f)
            continue;

        auto x1 = (-b + std::sqrt(d)) / (2.f * a);
        auto x2 = (-b - std::sqrt(d)) / (2.f * a);

        float t;
        if (x1 < 0.f && x2 < 0.f)
            continue;
        else {
            if (x1 < 0.f)
                t = x2;
            else if (x2 < 0.f)
                t = x1;
            else
                t = std::min(x1, x2);
        }

        if (t < 1.f) {
            targets.emplace(t, &trg);
        }
    }

    for (auto& [t, trg] : targets) {
        /* TODO: read from gun params */

        auto trg_new_pos = trg->pos + trg->vel * t + physic_sim.gravity * t * t * 0.5f;
        auto dist = trg_new_pos - (plr.pos - (plr.barrel_pos - plr.pos));
        auto blt_dist    = plr.gun_bullet_vel * t;
        if (((dist.x < 0.f && dist.x < blt_dist) || (dist.x > 0.f && dist.x > blt_dist)) &&
            std::fabs(std::fabs(dist.x) - blt_dist) < 50.f / std::cos(plr.gun_dispersion * 0.5f)) {
            if (trg_new_pos.x - plr.pos.x < 0.f) {
                if (!plr.on_left)
                    op.move_left();
            }
            else {
                if (plr.on_left)
                    op.move_right();
            }

            op.shot_next_frame();
        }
    }
}

inline auto pathfinder(const std::vector<ai_mgr_singleton::platform_t>& platforms,
                       const ai_mgr_singleton::ai_plat_map_t&           plat_map,
                       const platform_res_t&                            start_plat,
                       const platform_res_t&                            target_plat) {
    std::vector<const ai_mgr_singleton::platform_t*> res;
    auto src_plat = start_plat.stand_on;
    auto trg_plat = target_plat.stand_on ? target_plat.stand_on : target_plat.nearest;

    if (!src_plat) src_plat = start_plat.stand_on;
    if (!src_plat) src_plat = start_plat.nearest;

    if (!src_plat || !trg_plat)
        return res;

    if (src_plat == trg_plat)
        return res;

    auto src_plat_idx = static_cast<size_t>(src_plat - platforms.data());
    auto trg_plat_idx = static_cast<size_t>(trg_plat - platforms.data());

    std::vector<size_t> came_from;
    std::vector<float> cost_so_far;
    came_from.resize(platforms.size(), std::numeric_limits<size_t>::max());
    cost_so_far.resize(platforms.size());

    using pair_t = std::pair<float, size_t>;
    std::priority_queue<pair_t, std::vector<pair_t>, std::greater<>> frontier;
    frontier.push({0.0, src_plat_idx});

    came_from[src_plat_idx] = src_plat_idx;
    cost_so_far[src_plat_idx] = 0.f;

    while (!frontier.empty()) {
        auto current = frontier.top().second;
        frontier.pop();

        if (current == trg_plat_idx)
            break;

        for (size_t i = 0; i < plat_map[current].size(); ++i) {
            auto& next = plat_map[current][i];
            float new_cost = cost_so_far[current] + magnitude2(next);

            if (came_from[i] == std::numeric_limits<size_t>::max() || new_cost < cost_so_far[i]) {
                cost_so_far[i] = new_cost;
                came_from[i] = current;
                frontier.push({new_cost, i});
            }
        }
    }

    if (!frontier.empty()) {
        auto cur = trg_plat_idx;
        while (cur != src_plat_idx) {
            res.push_back(platforms.data() + cur);
            cur = came_from[cur];
        }

        std::reverse(res.begin(), res.end());
    }

    return res;
}

inline void move_to_target(const std::vector<ai_mgr_singleton::platform_t>& platforms,
                           const ai_mgr_singleton::ai_plat_map_t&           plat_map,
                           const ai_mgr_singleton::player_t&                player,
                           ai_operator&                                     oper,
                           ai_move_spec&                                    move_spec,
                           const ai_mgr_singleton::player_t&                target,
                           const platform_res_t&                            target_plat,
                           float                                            plats_bound_start,
                           float                                            plats_bound_end) {
    auto operated_plats = oper.operated_platforms();
    if (!operated_plats.stand_on)
        operated_plats.stand_on = oper.last_stand_on_plat();

    auto path = pathfinder(platforms, plat_map, operated_plats, target_plat);

    if (path.empty())
        return;

    auto& next_plat = path.front();

    bool ready_to_jump = false;

    float plat_center;
    float plat_min_dist;

    if (path.size() == 1 && oper.difficulty() == ai_hard && next_plat->pos1.x < target.pos.x &&
        target.pos.x < next_plat->pos2.x) {
        auto  trg_plat_f    = inverse_lerp(next_plat->pos1.x, next_plat->pos2.x, target.pos.x);
        auto  global_plat_f = inverse_lerp(plats_bound_start, plats_bound_end, target.pos.x);
        float f;
        if (global_plat_f <= 0.5f)
            f = lerp(trg_plat_f, 1.f, 0.5f);
        else
            f = lerp(0.f, trg_plat_f, 0.5f);
        plat_center     = (next_plat->pos2.x - next_plat->pos1.x) * f;
        plat_min_dist = std::min(plat_center - next_plat->pos1.x, next_plat->pos2.x - plat_center) * 0.1f;
    }
    else {
        plat_center     = (next_plat->pos1.x + next_plat->pos2.x) * 0.5f;
        plat_min_dist = (next_plat->pos2.x - next_plat->pos1.x) * 0.1f;
    }

    if (plat_center + plat_min_dist < player.pos.x) {
        move_spec.update_move(ai_move_left, 0);
        ready_to_jump = next_plat->pos2.x > player.pos.x ||
                        player.pos.x - next_plat->pos2.x < oper.jump_x_max();
    }
    else if (plat_center - plat_min_dist > player.pos.x) {
        move_spec.update_move(ai_move_right, 0);
        ready_to_jump = next_plat->pos1.x < player.pos.x ||
                        next_plat->pos1.x - player.pos.x < oper.jump_x_max();
    } else {
        move_spec.update_move(ai_move_stop, 0);
        ready_to_jump = true;
    }

    if (next_plat->pos1.y < player.pos.y) {
        if (ready_to_jump && player.vel.y > -0.001f)
            oper.jump();
    } else {
        if (player.is_y_locked)
            oper.jump_down();
    }

    /*
    std::cout << "Path: ";
    for (auto p : path) {
        std::cout << p - platforms.data() << " ";
    }
    std::cout << std::endl;
    */
}

inline std::optional<player_name_t>
find_closest_target(const std::map<player_name_t, ai_mgr_singleton::player_t>& players,
                    const ai_mgr_singleton::player_t&                          player) {
    std::optional<player_name_t> closest;
    float dist = std::numeric_limits<float>::max();

    for (auto& [_, trg] : players) {
        if (&player != &trg && (trg.group == -1 || trg.group != player.group)) {
            auto cur_dist = magnitude2(trg.pos - player.pos);
            if (cur_dist < dist) {
                closest = trg.name;
                dist = cur_dist;
            }
        }
    }

    return closest;
}

inline void stay_away_from_borders(const ai_mgr_singleton::player_t& player,
                                   ai_move_spec&                     move_spec,
                                   float                             start,
                                   float                             end) {
    if (player.pos.x < start || player.pos.x > end)
        return;

    auto plat_factor = inverse_lerp(start, end, player.pos.x);
    if (plat_factor < 0.2f) {
        move_spec.update_move(ai_move_right, 1);
    }
    else if (plat_factor > 0.8f) {
        move_spec.update_move(ai_move_left, 1);
    }
}

inline void ai_mgr_singleton::worker_op() {
    for (auto& [player_name, oper_ptr] : _operators) {
        auto& player = players[player_name];

        auto last_stand_on = oper_ptr->_operated_platforms.stand_on;
        oper_ptr->_operated_platforms = find_platform(platforms, player);
        if (oper_ptr->_operated_platforms.stand_on != last_stand_on)
            oper_ptr->_last_stand_on_plat = last_stand_on;

        oper_ptr->_jump_x_max = ((player.jump_speed * 2.f) / physic_sim.gravity.y) * player.max_vel_x * 0.7f;
    }

    for (auto& [player_id, oper_ptr] : _operators) {
        auto& oper   = *oper_ptr;
        auto& player = players[player_id];
        auto plat    = oper._operated_platforms;
        ai_move_spec move_spec;

        oper.update_shot_next_frame(physic_sim);

        if (oper._target_id == ai_operator::no_target) {
            if (auto id = easy_ai_find_nearest(player, players, level, oper))
                oper._target_id = id->first;
        }
        else {
            if (oper.difficulty() == ai_hard) {
                if (!hard_i_see_you(player, players[oper._target_id], level, player.gun_dispersion)) {
                    if (auto next_target = find_closest_target(players, player))
                        oper._last_target_id = *next_target;
                    else
                        oper._last_target_id = ai_operator::no_target;

                    oper._target_id = ai_operator::no_target;
                    if (auto id = easy_ai_find_nearest(player, players, level, oper))
                        oper._target_id = id->first;
                }
            }
            else {
                if (!i_see_you(player, players[oper._target_id])) {
                    if (auto next_target = find_closest_target(players, player))
                        oper._last_target_id = *next_target;
                    oper._target_id = ai_operator::no_target;
                }
            }
        }

        easy_ai_platform_actions(platforms, plat, player, oper, move_spec);

        if (oper._target_id != ai_operator::no_target) {
            if (!oper._shot) {
                float delay = 0.f;
                if (oper.difficulty() == ai_easy)
                    delay = 0.35f; //rand_float(0.15f, 0.4f);
                else if (oper.difficulty() == ai_medium)
                    delay = 0.15f; //rand_float(0.05f, 0.15f);
                oper.shot_next_frame(delay);
            }

            auto& trg = players[oper._target_id];
            auto dist = trg.pos - player.pos;
            auto mod_dist_x = fabs(dist.x);

            std::array<float, 3> dists; // NOLINT
            if (oper.difficulty() != ai_hard) {
                dists[0] = std::fabs(player.barrel_pos.x - player.pos.x) * 1.3f;
                dists[1] = dists[0] * 0.8f;
                dists[2] = 500.f;
            } else {
                dists[0] = std::fabs(player.barrel_pos.x - player.pos.x);
                dists[1] = dists[0];
                dists[2] = 40.f;
            }

            if (mod_dist_x < dists[0]) {
                if (oper.difficulty() >= ai_hard && player.vel.y > -0.001f)
                    oper.jump();
                /* Move from target */
                if (dist.x >= 0.f && !oper._mov_left)
                    move_spec.update_move(ai_move_left, 0);
                else if (dist.x < 0.f && !oper._mov_right)
                    move_spec.update_move(ai_move_right, 0);
            }
            else if (mod_dist_x >= dists[1] && mod_dist_x < dists[2]) {
                if (dist.x >= 0.f && player.on_left)
                    move_spec.update_move(ai_move_right, 0);
                else if (dist.x < 0.f && !player.on_left)
                    move_spec.update_move(ai_move_left, 0);

                if (oper._mov_right || oper._mov_left)
                    move_spec.update_apply(oper, ai_move_stop, 0);
            }
            else {
                /* Move to target */
                if (dist.x >= 0.f && !oper._mov_right)
                    move_spec.update_move(ai_move_right, 0);
                else if (dist.x < 0.f && !oper._mov_left)
                    move_spec.update_move(ai_move_left, 0);
            }

            if (oper.difficulty() == ai_hard && trg.vel.y < -trg.jump_speed * 0.99f &&
                player.vel.y > -0.001f/* && roll_the_dice(0.5f)*/) {
                /* For 50% chance */
                if (static_cast<int>(std::fabs(player.pos.x)) % 2 == 0)
                    oper.delayed_jump(0.15f/*rand_float(0.05f, 0.25f)*/);
            }
        } else {
            if (oper._last_target_id != ai_operator::no_target) {
                auto& trg = players[oper._last_target_id];
                auto target_plat = find_platform(platforms, trg);

                if (oper.difficulty() == ai_hard && player.pos.y > trg.pos.y &&
                    (player.pos.y - trg.pos.y) <
                        player.max_jump_dist * float(player.available_jumps) &&
                    player.pos.y > -0.001f) {
                    oper.jump();
                }
                else if (!oper._shot) {
                    move_to_target(platforms,
                                   platform_map,
                                   player,
                                   oper,
                                   move_spec,
                                   players[oper._last_target_id],
                                   target_plat,
                                   platforms_bound_start_x,
                                   platforms_bound_end_x);
                }
            }

            /* Relax with delay */
            float relax_delay = player.gun_fire_rate * 0.001f;
            if (oper._shot && oper._timer.elapsed(physic_sim.time_speed) > relax_delay) {
                if (oper._shot)
                    oper.relax();
            }

            if (oper.difficulty() == ai_hard)
                hard_shooter(players, player, oper, physic_sim, level, move_spec);
        }

        if (std::fabs(player.vel.x) > player.max_vel_x * 3.f && plat.stand_on && player.vel.y > -0.001f)
            oper.jump();

        dodge_ai(bullets, player, physic_sim, move_spec, oper);

        move_spec.apply(oper);
    }
}

}

