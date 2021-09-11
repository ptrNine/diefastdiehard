#pragma once

#include <SFML/Graphics/RectangleShape.hpp>

#include "types.hpp"
#include "bullet.hpp"
#include "physic_simulation.hpp"
#include "instant_kick.hpp"
#include "adjustment_box.hpp"
#include "player_controller.hpp"
#include "player_configurator_ui.hpp"
#include "level.hpp"
#include "ai.hpp"
#include "cfg_value_control.hpp"

#include "net_actions.hpp"
#include "net_actor.hpp"
#include "server.hpp"

namespace dfdh {

inline constexpr bool MP_MOVE_ADJUSTMENT = true;
inline constexpr float MP_MOVE_ADJUSTMENT_VEL_THRESHOLD = 200.f;
inline constexpr float MP_MOVE_ADJUSTMENT_POS_THRESHOLD = 200.f;

enum class game_state_event {
    level_changed = 0,
    shutdown
};

struct client_state {
    static std::unique_ptr<client_state> create(ip_address server_ip   = ip_address::localhost(),
                                                port_t     server_port = SERVER_DEFAULT_PORT,
                                                port_t     client_port = CLIENT_DEFAULT_PORT) {
        return std::make_unique<client_state>(server_ip, server_port, client_port);
    }

    client_state(ip_address server_ip   = "127.0.0.1",
                 port_t     server_port = SERVER_DEFAULT_PORT,
                 port_t     client_port = CLIENT_DEFAULT_PORT):
        server_address(server_ip, server_port), sock(ip_address::localhost(), client_port) {
        sock.debug_output();
        send(a_cli_i_wanna_play{}, [this](bool ok) {
            if (ok)
                hello = true;
            else
                fprintln(std::cerr, "Cannot connect to server ", server_address);
        });
    }

    void send(const NetAction auto&            act,
              const std::function<void(bool)>& transcontrol_handler = {},
              std::chrono::milliseconds        resend_interval      = 200ms,
              u16                              max_retries          = 10) {
        sock.send_somehow(server_address, act, transcontrol_handler, resend_interval, max_retries);
    }

    template <typename F>
    void receiver(F&& overloaded) {
        actor_processor.receive(sock, overloaded);
    }

    bool hello = false;

    address_t           server_address;
    easysocket          sock;
    net_actor_processor actor_processor;
};

class game_state {
public:
    game_state(): blt_mgr("blt_mgr", sim, player_hit_callback), kick_mgr("kick_mgr", sim, player_hit_callback) {
        sim.add_update_callback("player", [this](const physic_simulation& sim, float timestep) {
            for (auto& [_, p] : players)
                p->physic_update(sim, timestep);
        });

        sim.add_platform_callback("player", [this](physic_point* pnt) {
            for (auto& [_, p] : players)
                if (pnt == p->collision_box())
                    p->enable_double_jump();
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

    void player_create_from_client(const player_name_t& player_name, int group = 0) {
        if (players.contains(player_name)) {
            LOG_ERR("player {} already exists", player_name);
            return;
        }

        if (client) {
            a_player_game_params act;
            act.name = player_name;
            act.group = group;
            client->send(act);
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

        auto was_empty = ai_operators.empty();
        ai_operators.emplace(player_name, ai_operator::create(ai_medium, player_name));

        if (was_empty) {
            ai_provide_level_info();
            ai_provide_player_level_sim_info();
            ai_mgr().worker_start();
        }
        return true;
    }

    void connect_to_server(const address_t& server_address) {
        if (server) {
            LOG_ERR("can't connect to server: server running on this instance (destroy it?)");
            return;
        }

        if (client) {
            LOG_ERR("can't connect to server: already connected");
            return;
        }

        cur_level = nullptr;
        controlled_players.clear();
        controllers.clear();
        if (ai_mgr()._work)
            ai_mgr().worker_stop();
        ai_operators.clear();
        players.clear();

        auto cli_port = CLIENT_DEFAULT_PORT;

        while (!client && cli_port < CLIENT_DEFAULT_PORT + 20) {
            try {
                client = client_state::create(server_address.ip, server_address.port, cli_port);
            } catch (...) {
                ++cli_port;
            }
        }
        if (!client)
            client = client_state::create(server_address.ip, server_address.port, cli_port);
    }

    void server_init() {
        if (server) {
            LOG_ERR("can't init server: already running");
            return;
        }

        if (client) {
            LOG_ERR("can't init server: client already running");
            return;
        }

        server = std::make_unique<server_t>();
    }

    //void ai_difficulty(const player_name_t& player_name, )

    /* Game updates */

    void handle_event(const sf::Event& evt) {
        if (!on_game)
            return;

        for (auto& [_, control_data] : controlled_players) {
            auto& [controller, this_player] = control_data;
            bool update_was = this_player->update_input(*controller, evt);

            if (update_was) {
                if (server) {
                    a_srv_player_sync act;
                    act             = this_player->client_input_state();
                    act.name        = this_player->name();
                    act.evt_counter = this_player->evt_counter();
                    act.position    = this_player->get_position();
                    act.velocity    = this_player->get_velocity();

                    server->send_to_all_except_client_player(this_player->name(), act);
                }
                if (client) {
                    a_cli_player_sync act;
                    act = this_player->client_input_state();
                    act.evt_counter = this_player->evt_counter();
                    act.position    = this_player->get_position();
                    act.velocity    = this_player->get_velocity();

                    client->send(act);
                }
            }

            /*
            if (evt.type == sf::Event::KeyPressed) {
                if (evt.key.code == sf::Keyboard::Space) {
                    ++cur_wpn;
                    if (cur_wpn == wpns.size())
                        cur_wpn = 0;

                    for (auto& [_, p] : gs.players)
                        p->setup_pistol(wpns[cur_wpn]);
                }
            }
            */
        }

        if (cfgval_ctrl) {
            cfgval_ctrl->handle_event(evt);
            cfgval_ctrl_update();
        }
    }

    void cfgval_ctrl_update() {
        if (auto sect = cfgval_ctrl->consume_update()) {
            if (sect->starts_with("wpn_")) {
                for (auto& [sect_name, section] : cfg().sections()) {
                    if (sect->starts_with(sect_name) && section.sects.contains("class")) {
                        weapon_mgr().reload(sect_name);
                        return;
                    }
                }
            }

            if (sect->starts_with("lvl_")) {
                auto found_lvl = levels.find(*sect);
                if (found_lvl != levels.end()) {
                    found_lvl->second->cfg_reload();
                    if (cur_level == found_lvl->second) {
                        cur_level->setup_to(sim);
                        events.push(game_state_event::level_changed);
                    }
                }
            }
        }
    }

    void render_update(sf::RenderWindow& wnd) {
        if (!cur_level)
            return;

        cur_level->draw(wnd);

        for (auto& [_, player] : players)
            player->draw(wnd, sim.interpolation_factor(), sim.last_timestep());

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
        if (on_game) {
            if (!ai_operators.empty())
                ai_operators_consume();

            if (client) {
                client->receiver(act_from_server_overloaded{this});

                if (server_sync_timer.getElapsedTime().asSeconds() > server_sync_step) {
                    server_sync_timer.restart();
                    for (auto& [_, control_data] : controlled_players) {
                        auto& this_player = control_data.this_player;

                        a_cli_player_sync act;
                        act = this_player->client_input_state();
                        act.evt_counter = this_player->evt_counter();
                        act.position    = this_player->get_position();
                        act.velocity    = this_player->get_velocity();

                        client->send(act);
                    }
                }
            }
            if (server) {
                server->work(act_from_client_overloaded{this});

                if (server_sync_timer.getElapsedTime().asSeconds() > server_sync_step) {
                    server_sync_timer.restart();

                    for (auto& [_, player] : players) {
                        auto& gun = player->get_gun();

                        a_srv_player_game_sync act;
                        act              = player->client_input_state();
                        act.evt_counter  = player->evt_counter();
                        act.position     = player->get_position();
                        act.velocity     = player->get_velocity();
                        act.name         = player->name();
                        act.group        = player->get_group();
                        act.wpn_name     = gun ? gun.get_weapon()->section() : "";
                        act.ammo_elapsed = gun ? gun.ammo_elapsed() : 0;
                        act.on_left      = player->get_on_left();

                        server->send_to_all_except_client_player(player->name(), act);
                    }
                }
            }

            sim.update(60, game_speed);

            for (auto& [_, pl] : players) {
                if (controlled_players.contains(pl->name()) || ai_operators.contains(pl->name())) {
                    std::vector<bullet_data_t> bullets;
                    float                                      mass = 0.f;
                    pl->game_update(sim, blt_mgr, true, bullet_spawn_callback{&bullets, &mass});

                    if (!bullets.empty()) {
                        if (client) {
                            a_spawn_bullet act;
                            act.shooter = pl->name();
                            act.mass    = mass;
                            act.bullets = std::move(bullets);
                            client->send(act);
                        }
                        if (server) {
                            a_spawn_bullet act;
                            act.shooter = pl->name();
                            act.mass    = mass;
                            act.bullets = std::move(bullets);
                            server->send_to_all(act);
                        }
                    }
                }
                else {
                    pl->game_update(sim, blt_mgr, false, {});
                }

                if (pl->collision_box()->get_position().y > 2100.f)
                    player_on_dead(pl.get());
            }

            kick_mgr.update();
            adj_box_mgr.update(sim);
        } else {
            if (client) {
                if (client->hello) {
                    on_game = true;
                }
                client->receiver(act_from_server_overloaded{this});
            }
            if (server)
                server->work(act_from_client_overloaded{this});
        }
    }

    void ai_operators_consume() {
        for (auto& [name, ai_op] : ai_operators) {
            auto& player = players.at(name);

            while (auto op = ai_op->consume_task()) {
                switch (*op) {
                case ai_operator::t_jump: player->jump(); break;
                case ai_operator::t_jump_down: player->jump_down(); break;
                case ai_operator::t_move_left: player->move_left(); break;
                case ai_operator::t_move_right: player->move_right(); break;
                case ai_operator::t_stop: player->stop(); break;
                case ai_operator::t_shot: player->shot(); break;
                case ai_operator::t_relax: player->relax(); break;
                default: break;
                }
            }

            /* TODO: investigate this */
            if (server) {
                if (auto states = player->try_extract_client_input_state()) {
                    player->increment_evt_counter();

                    a_srv_player_sync act;
                    act             = *states;
                    act.name        = player->name();
                    act.evt_counter = player->evt_counter();
                    act.position    = player->get_position();
                    act.velocity    = player->get_velocity();

                    server->send_to_all(act);
                }
            }
        }

        ai_provide_player_level_sim_info();
    }

    void ai_provide_player_level_sim_info() {
        ai_mgr().provide_bullets(blt_mgr.bullets(), [](const bullet& bl) {
            return ai_mgr_singleton::bullet_t{
                bl.physic()->get_position(),
                bl.physic()->get_velocity(),
                bl.physic()->get_mass(),
                bl.group()
            };
        });

        ai_mgr().provide_players(players, [gy = sim.gravity().y](const std::shared_ptr<player>& pl) {
            auto& gun = pl->get_gun();

            return ai_mgr_singleton::player_t{
                pl->get_position(),
                pl->collision_box()->get_direction(),
                pl->get_size(),
                pl->get_velocity(),
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
                gun ? gun.get_weapon()->get_fire_rate() : 100.f,
                pl->get_on_left(),
                pl->collision_box()->is_lock_y()
            };
        });

        ai_mgr().provide_physic_sim(sim.gravity(), sim.last_speed(), sim.last_rps());
    }

    void ai_provide_level_info() {
        if (!cur_level)
            return;

        ai_mgr().provide_level(cur_level->level_size());
        ai_mgr().provide_platforms(cur_level->get_platforms(), [](const level::platform_t& pl) {
            auto pos = pl.ph.get_position();
            auto len = pl.ph.length();
            return ai_mgr_singleton::platform_t{pos, vec2f(pos.x + len, pos.y)};
        });
    }


    /*========================== server side actions ============================*/

    struct act_from_client_overloaded {
        void operator()(const act_from_client_t& info, const auto& act) {
            it->act_from_client(info, act);
        }

        game_state* it;
    };

    void act_from_client(const act_from_client_t&, const a_cli_i_wanna_play&) {
        /* TODO: */
    }

    void act_from_client(const act_from_client_t& info, const a_player_skin_params& params) {
        auto found_plr = players.find(info.operated_player);
        if (found_plr == players.end()) {
            LOG_ERR("client player with name '{}' not found on server", info.operated_player);
            return;
        }
        auto& plr = found_plr->second;

        plr->set_params_from_client(params);
    }

    void act_from_client(const act_from_client_t& info, const a_cli_player_sync& states) {
        auto found_plr = players.find(info.operated_player);
        if (found_plr == players.end()) {
            LOG_ERR("client player with name '{}' not found on server", info.operated_player);
            return;
        }
        auto& pl = found_plr->second;

        if constexpr (MP_MOVE_ADJUSTMENT) {
            if (pl->last_state_packet_id > info.packet_id)
                return;
            pl->last_state_packet_id = info.packet_id;

            bool y_now_locked = !pl->collision_box()->is_lock_y() && states.lock_y;
            /* TODO: cheaters! */
            if (y_now_locked || magnitude(states.position - pl->get_position()) > MP_MOVE_ADJUSTMENT_POS_THRESHOLD)
                pl->position(states.position);
            else
                pl->smooth_position_set(states.position);

            if (magnitude(states.velocity - pl->get_velocity()) > MP_MOVE_ADJUSTMENT_VEL_THRESHOLD)
                pl->velocity(states.velocity);
            else
                pl->smooth_velocity_set(states.velocity);
        }
        else {
            pl->position(states.position);
            pl->velocity(states.velocity);
        }

        pl->update_from_client(states);
        pl->evt_counter(states.evt_counter);
    }

    void act_from_client(const act_from_client_t& info, const a_spawn_bullet& act) {
        client_server_side_shot_update(info.operated_player, act);
    }

    void act_from_client(const act_from_client_t& info, server_t::send_game_state_to_client_t) {
        a_level_sync act;
        act.level_name = cur_level ? cur_level->section_name() : std::string();
        act.game_speed = game_speed;
        act.on_game    = on_game;

        server->send(
            info.address,
            act,
            [addr = info.address](bool ok) {
                if (!ok)
                    LOG_ERR("Failed to send level init to client {}", addr);
            });

        for (auto& [_, player] : players)
            if (info.operated_player != player->name()) {
                a_player_conf act;
                act.pistol            = player->pistol_section();
                act.skin.body_txtr    = player->body_texture_path();
                act.skin.face_txtr    = player->face_texture_path();
                act.skin.body_color   = player->body_color();
                act.skin.tracer_color = player->tracer_color();
                act.game.name         = player->name();
                act.game.group        = player->get_group();

                server->send(
                    info.address, act, [pl_name = player->name(), addr = info.address](bool ok) {
                        if (!ok)
                            LOG_ERR("Failed to send player {} init to client {}", pl_name, addr);
                    });
            }
    }

    void act_from_client(const act_from_client_t&, const a_player_game_params& act) {
        auto player = player_create(act.name);
        player->group(act.group);
        if (player) {
            a_player_conf act;
            act.pistol            = player->pistol_section();
            act.skin.body_txtr    = player->body_texture_path();
            act.skin.face_txtr    = player->face_texture_path();
            act.skin.body_color   = player->body_color();
            act.skin.tracer_color = player->tracer_color();
            act.game.name         = player->name();
            act.game.group        = player->get_group();

            server->send_to_all(act);
        }
    }

    /*========================== client side actions ============================*/

    struct act_from_server_overloaded {
        void operator()(const address_t&, const auto& act) {
            if constexpr (requires {it->act_from_server(act);})
                it->act_from_server(act);
        }

        game_state* it;
    };

    void act_from_server(const a_ping& act) {
        this_ping = static_cast<float>(act.ping_ms) * 0.001f;
        client->send(act);
    }

    void act_from_server(const a_srv_player_sync& player_states) {
        auto found = players.find(player_states.name);
        if (found == players.end()) {
            LOG_ERR("client: can't update player '{}': player not found", player_states.name);
            return;
        }

        auto& player = found->second;

        if (player_states.evt_counter < player->evt_counter())
            return;

        if constexpr (MP_MOVE_ADJUSTMENT) {
            if (player->last_state_packet_id > player_states.id)
                return;
            player->last_state_packet_id = player_states.id;

            bool y_now_locked = !player->collision_box()->is_lock_y() && player_states.lock_y;
            if (y_now_locked || magnitude(player_states.position - player->get_position()) >
                                    MP_MOVE_ADJUSTMENT_POS_THRESHOLD)
                player->position(player_states.position);
            else
                player->smooth_position_set(player_states.position);

            if (magnitude(player_states.velocity - player->get_velocity()) >
                MP_MOVE_ADJUSTMENT_VEL_THRESHOLD)
                player->velocity(player_states.velocity);
            else
                player->smooth_velocity_set(player_states.velocity);
        }
        else {
            player->position(player_states.position);
            player->velocity(player_states.velocity);
        }

        player->update_from_client(player_states);
        player->evt_counter(player_states.evt_counter);
    }

    void act_from_server(const a_srv_player_game_sync& sync_state) {
        auto found = players.find(sync_state.name);
        if (found == players.end()) {
            LOG_ERR("client: can't physic sync player '{}': player not found", sync_state.name);
            return;
        }

        auto& player = found->second;

        if (sync_state.evt_counter < player->evt_counter())
            return;

        if constexpr (MP_MOVE_ADJUSTMENT) {
            if (player->last_state_packet_id > sync_state.id)
                return;
            player->last_state_packet_id = sync_state.id;

            bool y_now_locked = !player->collision_box()->is_lock_y() && sync_state.lock_y;
            if (y_now_locked || magnitude(sync_state.position - player->get_position()) >
                                    MP_MOVE_ADJUSTMENT_POS_THRESHOLD)
                player->position(sync_state.position);
            else
                player->smooth_position_set(sync_state.position);

            if (magnitude(sync_state.velocity - player->get_velocity()) > MP_MOVE_ADJUSTMENT_VEL_THRESHOLD)
                player->velocity(sync_state.velocity);
            else
                player->smooth_velocity_set(sync_state.velocity);
        }
        else {
            player->position(sync_state.position);
            player->velocity(sync_state.velocity);
        }

        player->update_from_client(sync_state);
        player->group(sync_state.group);
        //LOG_UPDATE("{} {}", sync_state.evt_counter, player->evt_counter());

        auto old_gun_name = player->get_gun() ? player->get_gun().get_weapon()->section() : "";
        if (old_gun_name != sync_state.wpn_name) {
            player->setup_pistol(sync_state.wpn_name);
        }
    }

    void act_from_server(const a_spawn_bullet& act) {
        client_server_side_shot_update(act.shooter, act);
    }

    void act_from_server(const a_level_sync& act) {
        level_current(act.level_name);

        game_speed = act.game_speed;
        on_game    = act.on_game;
    }

    void act_from_server(const a_player_conf& act) {
        auto pl = player_create(act.game.name);
        if (!pl) {
            auto found = players.find(act.game.name);
            if (found == players.end()) {
                LOG_ERR("client: can't sync player {}", act.game.name);
                return;
            }
            pl = found->second.get();
        }

        pl->set_body(act.skin.body_txtr, act.skin.body_color);
        pl->set_face(act.skin.face_txtr, act.skin.body_color);
        pl->tracer_color(act.skin.tracer_color);
        pl->setup_pistol(act.pistol);
        pl->group(act.game.group);
    }

    //void act_from_server(const auto&) {} /* dummy */



    /*====================== client/server side actions =========================*/

    void client_server_side_shot_update(const player_name_t&  player_name,
                                        const a_spawn_bullet& act) {
        auto found_player = players.find(player_name);
        if (found_player == players.end())
            return;

        auto& pl      = found_player->second;
        auto  latency = !server ? 0.f : static_cast<float>(server->get_ping(player_name)) * 0.001f;

        if (server) {
            auto new_act = act;
            for (auto& b : new_act.bullets)
                b.position += b.velocity * latency;
            server->send_to_all_except_client_player(player_name, new_act);
        }

        for (auto& blt : act.bullets) {
            auto& b = blt_mgr.shot(sim,
                                   blt.position + blt.velocity * latency,
                                   act.mass,
                                   blt.velocity,
                                   pl->tracer_color(),
                                   pl->get_group(),
                                   player::player_group_getter);

            if (client)
                continue;

            kick_mgr.spawn(sim,
                           blt.position,
                           blt.position + blt.velocity * latency,
                           act.mass,
                           magnitude(blt.velocity),
                           pl->get_group(),
                           b.physic());

            for (auto& [_, plr] : players) {
                vec2f plr_point = plr->get_position();
                auto  plr_size  = plr->get_size();
                if (plr_point.x > blt.position.x)
                    plr_point.x += plr_size.x;
                plr_point.y -= plr_size.y * 0.5f;
                auto dist        = magnitude(plr_point - blt.position);
                auto bullet_time = dist / magnitude(blt.velocity);

                if (auto prev_pos = plr->position_trace_lookup(sim.current_update_time(),
                                                               latency - bullet_time)) {
                    adj_box_mgr.add(plr, sim, *prev_pos, plr->get_size());
                }
            }
        }
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
    bool                                                  on_game;

    std::unique_ptr<client_state> client;
    std::unique_ptr<server_t>     server;
    sf::Clock                     server_sync_timer;
    float                         server_sync_step = 0.06f;

    bool                          debug_physics = false;
    float                         this_ping     = 0.f;

    std::optional<cfg_value_control> cfgval_ctrl;

    std::queue<game_state_event> events;

    std::unique_ptr<player_configurator_ui> pconf_ui;



public:
    [[nodiscard]]
    bool is_client() const {
        return client != nullptr;
    }

    [[nodiscard]]
    bool is_server() const {
        return server != nullptr;
    }
};

} // namespace dfdh
