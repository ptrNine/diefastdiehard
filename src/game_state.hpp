#pragma once

#include <SFML/Graphics/RectangleShape.hpp>

#include "base/types.hpp"
#include "base/cfg_value_control.hpp"
#include "ui/player_configurator_ui.hpp"
#include "bullet.hpp"
#include "physic_simulation.hpp"
#include "instant_kick.hpp"
#include "adjustment_box.hpp"
#include "player_controller.hpp"
#include "level.hpp"
#include "ai.hpp"

namespace dfdh {

enum class game_state_event {
    level_changed = 0,
    shutdown
};

class game_state {
public:
    game_state():
        blt_mgr("blt_mgr", sim, player_hit_callback),
        kick_mgr("kick_mgr", sim, player_hit_callback),
        conf_watcher(&cfg::mutable_global()) {
        sim.add_update_callback("player", [this](const physic_simulation& sim, float timestep) {
            for (auto& [_, p] : players)
                p->physic_update(sim, timestep);
        });

        sim.add_platform_callback("player", [this](physic_point* pnt) {
            for (auto& [_, p] : players)
                if (pnt == p->collision_box())
                    p->on_ground(pnt->prev_scalar_velocity());
        });
    }

    void player_conf_reload(const player_name_t& player_name = {}) {
        if (player_name.empty()) {
            for (auto& [name, p] : players) {
                p->set_from_config();
                LOG_INFO("reload player config {}", name);
            }
        } else {
            auto found = players.find(player_name);
            if (found == players.end()) {
                LOG_ERR("can't reload player conf: player {} not found", player_name);
                return;
            }

            found->second->set_from_config();
            LOG_INFO("reload player config {}", player_name);
        }
    }

    player* player_create(const player_name_t& player_name) {
        if (players.contains(player_name)) {
            LOG_ERR("player {} already exists", player_name);
            return nullptr;
        }

        auto player = player::create(player_name, sim, player::default_size());
        player->set_from_config();
        return players.emplace(player_name, player).first->second.get();
    }

    bool controller_delete(u32 controller_id) {
        auto found_controller = controllers.find(controller_id);
        if (found_controller == controllers.end()) {
            LOG_ERR("player controller{} was not found", controller_id);
            return false;
        }

        controllers.erase(found_controller);
        LOG_INFO("player controller{} deleted", controller_id);

        return true;
    }

    bool controller_bind(u32 controller_id, const player_name_t& player_name) {
        auto& controller =
            controllers
                .emplace(controller_id, std::make_shared<player_controller>(controller_id))
                .first->second;
        auto found_player = players.find(player_name);
        if (found_player == players.end()) {
            LOG_ERR("failed to bind player controller{}: player {} not found",
                    controller_id,
                    player_name);
            return false;
        }

        controller->player_name = player_name;
        LOG_INFO("player controller{} was bind to player {}", controller_id, player_name);

        return true;
    }

    void rebuild_controlled_players() {
        controlled_players.clear();
        std::vector<u32> to_delete;

        for (auto& [id, controller] : controllers) {
            auto found_player = players.find(controller->player_name);
            if (found_player == players.end()) {
                LOG_WARN("controller{} maps to non-existent player and will be deleted", id);
                to_delete.push_back(id);
            }
            else {
                controlled_players.emplace(found_player->second->name(),
                                           controll_player_t{controller, found_player->second});
            }
        }

        for (auto& id : to_delete) controllers.erase(id);
    }

    void level_cache_clear() {
        levels.clear();
        LOG_INFO("level cache was cleared");
    }

    bool level_current(const std::string& level_name = {}) {
        if (level_name.empty()) {
            if (!cur_level) {
                LOG_INFO("current level already reseted");
            } else {
                cur_level.reset();
                LOG_INFO("current level has been reseted");
            }
            return true;
        }

        if (cur_level && cur_level->section_name() == level_name) {
            LOG_INFO("level {} already in use", level_name);
            return true;
        }

        auto found_lvl = levels.find(level_name);
        if (found_lvl != levels.end()) {
            cur_level = found_lvl->second;
            cur_level->setup_to(sim);
            events.push(game_state_event::level_changed);
            LOG_INFO("level {} was loaded", level_name);
            return true;
        }
        else if (!level_cache(level_name))
                return false;

        cur_level = levels.at(level_name);
        cur_level->setup_to(sim);
        events.push(game_state_event::level_changed);
        LOG_INFO("level {} was loaded", level_name);
        return true;
    }

    bool level_cache(const std::string& level_name) {
        try {
            auto [_, was_insert] = levels.emplace(level_name, level::create(level_name));
            if (was_insert)
                LOG_INFO("level {} was cached", level_name);
            else
                LOG_INFO("level {} already cached");
            return true;
        }
        catch (const std::exception& e) {
            LOG_ERR("level cache {} failed: {}", level_name, e.what());
            return false;
        }
    }

    void game_run(bool value = true) {
        if (on_game)
            LOG_INFO(value ? "game already running" : "game stopped");
        else
            LOG_INFO(value ? "game running" : "game already stopped");
        on_game = value;
    }

    void game_stop() {
        game_run(false);
    }

    void shutdown() {
        events.push(game_state_event::shutdown);
    }

    void player_on_dead(player* player) {
        player->increase_deaths();
        player_spawn(player);
    }

    void player_spawn(player* player = nullptr) {
        if (!cur_level) {
            LOG_WARN("Can't spawn players: level was not loaded");
            return;
        }

        if (player) {
            player->position({cur_level->level_size().x / 2.f, 0.f});
            player->velocity({0.f, 0.f});
            player->enable_double_jump();
        } else {
            for (auto& [_, p] : players)
                player_spawn(p.get());
        }
    }

    bool ai_bind(const player_name_t& player_name) {
        if (ai_operators.contains(player_name)) {
            LOG_WARN("player {} is already ai controled", player_name);
            return false;
        }

        if (!players.contains(player_name)) {
            LOG_ERR("player {} not found", player_name);
            return false;
        }

        ai_operators.emplace(player_name, ai_operator::create(player_name, "medium"));

        if (!ai_mgr().running()) {
            ai_provide_level_info();
            ai_provide_player_level_sim_info();
            ai_mgr().worker_start();
        }
        return true;
    }

    /* Game updates */

    void handle_event(const sf::Event& evt) {
        if (!on_game)
            return;

        for (auto& [_, control_data] : controlled_players) {
            auto& [controller, this_player] = control_data;
            this_player->update_input(*controller, evt);
        }

        if (cfgval_ctrl) {
            cfgval_ctrl->handle_event(evt);
            if (auto section_name = cfgval_ctrl->consume_update())
                reload_section(*section_name);
        }
    }

    void reload_section(const std::string& section_name) {
        try {
            if (section_name.starts_with("wpn_")) {
                for (auto& [_, section] : cfg::global().get_sections()) {
                    if (section_name.starts_with(section_name) && section.has_key("class")) {
                        weapon_mgr().reload(section_name);
                        return;
                    }
                }
            }
            else if (section_name.starts_with("lvl_")) {
                auto found_lvl = levels.find(section_name);
                if (found_lvl != levels.end()) {
                    found_lvl->second->cfg_reload();
                    if (cur_level == found_lvl->second) {
                        cur_level->setup_to(sim);
                        events.push(game_state_event::level_changed);
                    }
                }
            }
        }
        catch (const cfg_exception& e) {
            LOG_ERR("reload section [{}] failed: {}", section_name, e.what());
        }
    }

    void render_update(sf::RenderWindow& wnd) {
        if (!cur_level)
            return;

        cur_level->draw(wnd);

        for (auto& [_, player] : players)
            player->draw(wnd, sim.interpolation_factor(), sim.last_timestep(), gravity_for_bullets);

        blt_mgr.draw(wnd, sim.interpolation_factor(), sim.last_timestep());

        if (debug_physics) {
            for (auto& e : sim.point_primitives()) {
                for (auto p : group_tree_view(e.get())) {
                    sf::RectangleShape s;
                    s.setPosition(p->bb().getPosition());
                    s.setSize(p->bb().getSize());
                    if (s.getSize().y < 2.f)
                        s.setSize({s.getSize().x, 2.f});
                    s.setFillColor(sf::Color(0, 255, 0));
                    wnd.draw(s);
                }
            }

            for (auto& e : sim.line_primitives()) {
                for (auto p : group_tree_view(e.get())) {
                    sf::RectangleShape s;
                    s.setPosition(p->bb().getPosition());
                    s.setSize(p->bb().getSize());
                    s.setOutlineColor(sf::Color(0, 255, 0));
                    s.setOutlineThickness(1.f);
                    wnd.draw(s);
                }
            }

            for (auto& p : sim.platforms()) {
                sf::Vertex line[] = {
                    sf::Vertex(p.get_position()),
                    sf::Vertex(p.get_position() + vec2f(p.length(), 0.f)),
                };
                wnd.draw(line, 2, sf::Lines);
            }
        }
    }

    void ui_update(ui_ctx& ui) {
        if (pconf_ui)
            pconf_ui->update(ui);
    }

    struct bullet_spawn_callback {
        void operator()(const vec2f& position, const vec2f& velocity, float imass) {
            *mass = imass;
            bullets->push_back(bullet_data_t{position, velocity});
        }

        constexpr operator bool() const {
            return true;
        }

        std::vector<bullet_data_t>* bullets;
        float* mass;
    };

    void game_update() {
        /* Try reload watched sections */
        for (auto& section_name : cfg::mutable_global().replace_changed_sections())
            reload_section(section_name);

        if (on_game) {
            sim.update(60, game_speed);

            for (auto& [_, pl] : players) {
                if (controlled_players.contains(pl->name()) || ai_operators.contains(pl->name())) {
                    std::vector<bullet_data_t> bullets;
                    float                      mass = 0.f;
                    pl->game_update(sim, blt_mgr, cam_pos, gravity_for_bullets, true, bullet_spawn_callback{&bullets, &mass});
                }
                else {
                    pl->game_update(sim, blt_mgr, cam_pos, gravity_for_bullets, false, {});
                }

                if (pl->collision_box()->get_position().y > 2100.f)
                    player_on_dead(pl.get());
            }

            kick_mgr.update();
            adj_box_mgr.update(sim);

            if (!ai_operators.empty())
                ai_operators_consume();
        }
        else {
            sim.update_pass();
        }
    }

    /* AI operators */

    void ai_operators_consume() {
        for (auto& [name, ai_op] : ai_operators) {
            auto& player = players.at(name);

            while (auto op = ai_op->consume_action()) {
                switch (*op) {
                case ai_action::jump: player->jump(); break;
                case ai_action::jump_down: player->jump_down(); break;
                case ai_action::move_left: player->move_left(); break;
                case ai_action::move_right: player->move_right(); break;
                case ai_action::stop: player->stop(); break;
                case ai_action::shot: player->shot(); break;
                case ai_action::relax: player->relax(); break;
                case ai_action::enable_long_shot: player->enable_long_shot(); break;
                case ai_action::disable_long_shot: player->disable_long_shot(); break;
                default: break;
                }
            }
        }

        ai_provide_player_level_sim_info();
    }

    void ai_provide_player_level_sim_info() {
        ai_mgr().provide_bullets(blt_mgr.bullets(), [](const bullet& bl) {
            return ai_bullet_t{
                bl.physic()->get_position(),
                bl.physic()->get_velocity(),
                bl.physic()->get_mass(),
                bl.group()
            };
        });

        ai_mgr().provide_players(players, [gy = sim.gravity().y](const std::shared_ptr<player>& pl) {
            auto& gun = pl->get_gun();

            return ai_player_t{pl->get_position(),
                               pl->collision_box()->get_direction(),
                               pl->get_size(),
                               pl->get_velocity(),
                               pl->acceleration(),
                               pl->barrel_pos(),
                               pl->name(),
                               pl->get_available_jumps(),
                               pl->get_x_accel(),
                               pl->get_x_slowdown(),
                               pl->get_jump_speed(),
                               pl->calc_max_jump_y_dist(gy),
                               pl->get_max_speed(),
                               gun ? gun.get_weapon()->get_dispersion() : 0.01f,
                               pl->get_group(),
                               gun ? gun.get_bullet_vel() : 1500.f,
                               gun ? gun.wpn()->hit_power() : 0.f,
                               gun ? gun.get_weapon()->get_fire_rate() : 100.f,
                               gun ? gun.get_weapon()->mag_size() : 1,
                               gun ? gun.ammo_elapsed() : 1,
                               pl->get_on_left(),
                               pl->collision_box()->is_lock_y(),
                               pl->long_shot_enabled(),
                               pl->is_walking(),
                               gun ? gun.wpn()->long_shot_dir(vec2f(pl->get_on_left() ? -1.f : 1.f, 0.f))
                                   : vec2f(pl->get_on_left() ? -1.f : 1.f, 0.f)};
        });

        ai_mgr().provide_physic_sim(sim.gravity(), sim.last_speed(), sim.last_rps(), gravity_for_bullets);
    }

    void ai_provide_level_info() {
        if (!cur_level)
            return;

        /* TODO: move platforms into the ai_level_t */
        ai_mgr().provide_level({cur_level->level_size()});
        ai_mgr().provide_platforms(cur_level->get_platforms(), [](const level::platform_t& pl) {
            auto pos = pl.ph.get_position();
            auto len = pl.ph.length();
            return ai_platform_t{pos, vec2f(pos.x + len, pos.y)};
        });
    }

    void lua_schedule_execution(const std::string& code) {
        lua_cmd_queue.push_back(code);
    }

    physic_simulation  sim;
    bullet_mgr         blt_mgr;
    instant_kick_mgr   kick_mgr;
    adjustment_box_mgr adj_box_mgr;
    float              game_speed = 1.f;

    struct controll_player_t {
        std::shared_ptr<player_controller> controller;
        std::shared_ptr<player>            this_player;
    };

    using controlled_players_t = std::map<player_name_t, controll_player_t>;

    controlled_players_t                                  controlled_players;
    std::map<player_name_t, std::shared_ptr<player>>      players;
    std::map<std::string, std::shared_ptr<level>>         levels;
    std::map<u32, std::shared_ptr<player_controller>>     controllers;
    std::map<player_name_t, std::shared_ptr<ai_operator>> ai_operators;
    std::shared_ptr<level>                                cur_level;
    std::queue<game_state_event>                          events;
    std::unique_ptr<player_configurator_ui>               pconf_ui;
    std::optional<cfg_value_control>                      cfgval_ctrl;
    cfg_watcher                                           conf_watcher;
    std::vector<std::string>                              lua_cmd_queue;

    vec2f cam_pos = {0.f, 0.f};

    bool on_game             = false;
    bool debug_physics       = false;
    bool gravity_for_bullets = true;
    bool lua_cmd_enabled     = true;
};

} // namespace dfdh
