#include <iostream>
#include "src/engine.hpp"
#include "src/physic_simulation.hpp"
#include "src/config.hpp"
#include "src/player.hpp"
#include "src/level.hpp"
#include "src/bullet.hpp"
#include "src/instant_kick.hpp"
#include "src/weapon.hpp"
#include "src/ai.hpp"
#include "src/rand_pool.hpp"
#include "src/networking.hpp"
#include "src/adjustment_box.hpp"
#include "src/log.hpp"
#include <ranges>

#include <SFML/Graphics/RectangleShape.hpp>

auto wpns = std::array{
    "wpn_glk17",
    "wpn_brn10",
    "wpn_de",
    "wpn_1887",
    "wpn_ak9",
    "wpn_416",
    "wpn_ss50"
};

size_t cur_wpn = 0;

inline constexpr bool MP_MOVE_ADJUSTMENT = true;

namespace dfdh {

using st_clock = std::chrono::steady_clock;

struct client_state {
    client_state() {
        send(a_cli_i_wanna_play{}, [this](bool ok) {
            if (ok)
                hello = true;
            else
                std::cerr << "Cannot connect to server " << serv_ip.toString() << ":" << serv_port << std::endl;
        });
    }

    template <typename T, typename F = bool>
    void send(const T& act, F transcontrol_handler = false) {
        sock.send(serv_ip, serv_port, act, transcontrol_handler);
    }

    template <typename F>
    void receiver(F overloaded) {
        sock.receiver(overloaded);
    }

    bool hello = false;

    player_states_t input_states{false, false, false, false, false, false};

    sf::IpAddress serv_ip = "127.0.0.1";
    u16 serv_port = SERVER_DEFAULT_PORT;
    act_socket sock = act_socket(CLIENT_DEFAULT_PORT);
};


class diefastdiehard : public dfdh::engine {
public:
    diefastdiehard(): _bm("bm1", sim, player_hit_callback), _kick_mgr("kick_mgr", sim, player_hit_callback) {}

    void on_init(args_view args) final {
        if (args.get("--client")) {
            _client = std::make_shared<client_state>();
            player_idx = 1;
        }

        if (args.get("--physic-debug")) {
            _physic_debug = true;
        }

        window().setSize({800, 600});
        if (!_client) {
            server();
            window().setPosition({0, 0});
        } else {
            window().setPosition({800, 0});
        }

        levels.push_back(level::create("lvl_aes", sim));

        /*
        auto pl = player::create(sim, {50.f, 90.f});
        pl->set_body("body.png", {247, 198, 70});
        pl->set_face("face3.png");
        pl->setup_pistol("wpn_brn10");
        players.push_back(pl);
        */

        for (int i = 0; i < 2; ++i) {
            auto pl2 = player::create(sim, {50.f, 90.f});
            pl2->setup_pistol(wpns[cur_wpn]);
            pl2->position({600.f + float(i) * 50.f, 500});
            players.push_back(pl2);
            if (i >= 1) {
                pl2->set_face("face1.png");
                pl2->group(0);
                pl2->set_body("body.png", {132, 10, 153});
                pl2->tracer_color({255, 170, 100});
            } else if (i == 0) {
                pl2->set_face("face3.png");
                pl2->group(1);
                pl2->set_body("body.png", {247, 203, 72});
            } else {
                pl2->set_face("face0.png");
                pl2->group(1);
                pl2->set_body("body.png", {255, 255, 255});
            }
        }

        sim.add_update_callback("player", [this](const physic_simulation& sim, float timestep) {
            for (auto& p : players)
                p->physic_update(sim, timestep);
        });

        sim.add_platform_callback("player", [this](physic_point* pnt) {
            for (auto& p : players)
                if (pnt == p->collision_box())
                    p->enable_double_jump();
        });

        if (!_client)
            ai_operators.push_back(ai_operator::create(ai_difficulty::ai_hard, 0));
        //ai_operators.push_back(ai_operator::create(ai_difficulty::ai_hard, 1));
        //ai_operators.push_back(ai_operator::create(ai_difficulty::ai_easy, 2));
        //ai_operators.push_back(ai_operator::create(ai_difficulty::ai_easy, 3));


        if (!ai_operators.empty()) {
            setup_ai();
            ai_mgr().worker_start();
        }
    }

    void on_destroy() final {
        if (!ai_operators.empty())
            ai_mgr().worker_stop();
    }

    /* TODO: remove later */
    player_states_t server_player_states{false, false, false, false, false, false};

    void handle_event(const sf::Event& evt) final {
        if (!_on_game)
            return;

        auto& player = players[player_idx];
        player->update_input(control_ks, evt);

        if (!_client) {
            auto states = player->extract_client_input_state();
            if (states != server_player_states) {
                player->increment_evt_counter();
                server_player_states = states;
                server().send_to_all(a_srv_player_states{server_player_states,
                                                         player_idx,
                                                         player->evt_counter(),
                                                         player->get_position(),
                                                         player->get_velocity()});
            }
        } else {
            auto states = player->extract_client_input_state();
            if (_client->input_states != states) {
                player->increment_evt_counter();
                _client->input_states = states;
                _client->send(a_cli_player_sync{
                    states, player->evt_counter(), player->get_position(), player->get_velocity()});
            }
        }

        if (evt.type == sf::Event::KeyPressed) {
            if (evt.key.code == sf::Keyboard::Space) {
                ++cur_wpn;
                if (cur_wpn == wpns.size())
                    cur_wpn = 0;

                for (auto& plr : players) plr->setup_pistol(wpns[cur_wpn]);
            }
        }
    }

    void render_update(sf::RenderWindow& wnd) final {
        levels[level_idx]->draw(wnd);

        for (auto& player : players)
            player->draw(wnd);

        _bm.draw(wnd);

        if (_physic_debug) {
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
        }

        if (_physic_debug) {
            for (auto& p : sim.platforms()) {
                sf::Vertex line[] = {
                    sf::Vertex(p.get_position()),
                    sf::Vertex(p.get_position() + sf::Vector2f(p.length(), 0.f)),
                };
                wnd.draw(line, 2, sf::Lines);
            }
        }
    }

    void ai_operators_consume() {
        for (auto& ai_op : ai_operators) {
            auto id = ai_op->player_id();
            while (auto op = ai_op->consume_task()) {
                switch (*op) {
                case ai_operator::t_jump: players[id]->jump(); break;
                case ai_operator::t_jump_down: players[id]->jump_down(); break;
                case ai_operator::t_move_left: players[id]->move_left(); break;
                case ai_operator::t_move_right: players[id]->move_right(); break;
                case ai_operator::t_stop: players[id]->stop(); break;
                case ai_operator::t_shot: players[id]->shot(); break;
                case ai_operator::t_relax: players[id]->relax(); break;
                default: break;
                }
            }

            if (!_client) {
                auto states = players[id]->extract_client_input_state();
                if (states != server_player_states) {
                    players[id]->increment_evt_counter();
                    server_player_states = states;
                    server().send_to_all(a_srv_player_states{server_player_states,
                                                             player_idx,
                                                             players[id]->evt_counter(),
                                                             players[id]->get_position(),
                                                             players[id]->get_velocity()});
                }
            }
        }
        update_ai();
    }

    struct client_player_update_overloaded {
        void operator()(u32 player_id, u64 packet_id, const auto& act) {
            it->client_player_update(player_id, packet_id, act);
        }

        diefastdiehard* it;
    };

    struct act_from_server_overloaded {
        void operator()(const sf::IpAddress&, u16, u64 packet_id, const auto& act) {
            it->act_from_server(packet_id, act);
        }

        diefastdiehard* it;
    };

    void act_from_server(u64, const a_srv_ping& act) {
        _self_ping = static_cast<float>(act._ping_ms) * 0.001f;
        _client->send(act);
    }

    void act_from_server(u64 packet_id, const a_srv_player_states& player_states) {
        auto& player = players[player_states.player_id];

        if constexpr (MP_MOVE_ADJUSTMENT) {
            if (player->last_state_packet_id > packet_id)
                return;
            player->last_state_packet_id = packet_id;

            bool y_now_locked = !player->collision_box()->is_lock_y() && player_states.st.lock_y;
            if (y_now_locked)
                player->position(player_states.position);
            else
                player->smooth_position_set(player_states.position);
            player->smooth_velocity_set(player_states.velocity);
        }
        else {
            player->position(player_states.position);
            player->velocity(player_states.velocity);
        }

        player->update_from_client(player_states.st);
        player->evt_counter(player_states.evt_counter);
    }

    void act_from_server(u64 packet_id, const a_srv_player_physic_sync& sync_state) {
        auto& player = players[sync_state.player_id];

        if constexpr (MP_MOVE_ADJUSTMENT) {
            if (player->last_state_packet_id > packet_id)
                return;
            player->last_state_packet_id = packet_id;
        }

        if (sync_state.player_id != player_idx) {
            if constexpr (MP_MOVE_ADJUSTMENT) {
                bool y_now_locked = !player->collision_box()->is_lock_y() && sync_state.st.lock_y;

                if (y_now_locked) {
                    player->position(sync_state.position);
                }
                else {
                    player->smooth_position_set(sync_state.position);
                }
                player->smooth_velocity_set(sync_state.velocity);
            }
            else {
                player->position(sync_state.position);
                player->velocity(sync_state.velocity);
            }

            player->update_from_client(sync_state.st);
            LOG_UPDATE("{} {}", sync_state.evt_counter, player->evt_counter());
            if (sync_state.evt_counter < player->evt_counter())
                return;
        }
        else {
            if (sync_state.evt_counter < player->evt_counter()) {
                return;
            }
        }

        auto old_gun_name = player->get_gun() ? player->get_gun().get_weapon()->section() : "";
        if (old_gun_name != sync_state.cur_wpn_name) {
            player->setup_pistol(sync_state.cur_wpn_name);
        }
    }

    void act_from_server(u64, const a_spawn_bullet& act) {
        server_client_shot_update(static_cast<u32>(act.player_id), act);
    }

    void act_from_server(u64, const auto&) {} /* dummy */

    void client_player_update(u32, u64, const a_cli_i_wanna_play&) {
        /* TODO: handle this */
        _on_game = true;
    }

    void client_player_update(u32 player_id, u64, const a_cli_player_params& params) {
        players[player_id]->set_params_from_client(params);
    }

    void client_player_update(u32 player_id, u64 packet_id, const a_cli_player_sync& states) {
        if (player_id >= players.size())
            return;

        LOG_UPDATE("packet_id: {}", packet_id);

        auto& pl = players[player_id];

        if constexpr (MP_MOVE_ADJUSTMENT) {
            if (pl->last_state_packet_id > packet_id)
                return;
            pl->last_state_packet_id = packet_id;

            bool y_now_locked = !pl->collision_box()->is_lock_y() && states.st.lock_y;
            /* TODO: cheaters! */
            if (y_now_locked)
                pl->position(states.position);
            else
                pl->smooth_position_set(states.position);
            pl->smooth_velocity_set(states.velocity);
        }
        else {
            pl->position(states.position);
            pl->velocity(states.velocity);
        }

        pl->update_from_client(states.st);
        pl->evt_counter(states.evt_counter);
    }

    void client_player_update(u32 player_id, u64, const a_spawn_bullet& act) {
        server_client_shot_update(player_id, act);
    }

    void server_client_shot_update(u32 player_id, const a_spawn_bullet& act) {
        if (player_id >= players.size())
            return;

        auto& pl = players[player_id];
        auto  latency =
            _client ? _self_ping : static_cast<float>(server().get_ping(player_id)) * 0.001f;

        for (auto& blt : act.bullets) {
            auto& b = _bm.shot(sim,
                                 blt._position + blt._velocity * latency,
                                 act.mass,
                                 blt._velocity,
                                 pl->tracer_color(),
                                 pl->get_group(),
                                 player::player_group_getter);

            _kick_mgr.spawn(sim,
                            blt._position,
                            blt._position + blt._velocity * latency,
                            act.mass,
                            magnitude(blt._velocity),
                            pl->get_group(),
                            b.physic());


            for (size_t plr_id = 0; plr_id != players.size(); ++plr_id) {
                if (plr_id == player_id)
                    continue;

                auto plr_point  = players[plr_id]->get_position();
                auto plr_size   = players[plr_id]->get_size();
                if (plr_point.x > blt._position.x)
                    plr_point.x += plr_size.x;
                plr_point.y -= plr_size.y * 0.5f;
                auto dist = magnitude(plr_point - blt._position);
                auto bullet_time = dist / magnitude(blt._velocity);

                if (auto prev_pos = players[plr_id]->position_trace_lookup(
                        sim.current_update_time(), latency - bullet_time)) {
                    _adj_box_mgr.add(players[plr_id], sim, *prev_pos, players[plr_id]->get_size());
                }
            }
        }
    }

    struct bullet_spawn_callback {
        void operator()(const sf::Vector2f& position, const sf::Vector2f& velocity, float imass) {
            *mass = imass;
            bullets->push_back(a_spawn_bullet::bullet_data_t{position, velocity});
        }

        constexpr operator bool() const {
            return true;
        }

        std::vector<a_spawn_bullet::bullet_data_t>* bullets;
        float* mass;
    };

    void game_update() final {
        if (_on_game) {
            if (!ai_operators.empty())
                ai_operators_consume();

            if (_client) {
                _client->receiver(act_from_server_overloaded{this});

                if (_server_sync_timer.getElapsedTime().asSeconds() > _server_sync_step) {
                    _server_sync_timer.restart();
                    auto& player = players[player_idx];
                    _client->send(a_cli_player_sync{player->extract_client_input_state(),
                                                    player->evt_counter(),
                                                    player->get_position(),
                                                    player->get_velocity()});
                }
            } else {
                server().work(client_player_update_overloaded{this});

                if (_server_sync_timer.getElapsedTime().asSeconds() > _server_sync_step) {
                    _server_sync_timer.restart();
                    for (size_t i = 0; i < players.size(); ++i) {
                        auto& player = players[i];
                        auto& gun    = player->get_gun();

                        server().send_to_all(
                            a_srv_player_physic_sync{i,
                                                     player->get_position(),
                                                     player->get_velocity(),
                                                     player->extract_client_input_state(),
                                                     gun ? gun.get_weapon()->section() : "",
                                                     player->evt_counter(),
                                                     gun ? gun.ammo_elapsed() : 0,
                                                     player->get_on_left()});
                    }
                }
            }

            sim.update(60, 1.0f);
            update_cam();

            for (auto& pl : players) {
                if (pl->id() == player_idx) {
                    std::vector<a_spawn_bullet::bullet_data_t> bullets;
                    float mass = 0.f;
                    pl->game_update(sim, _bm, true, bullet_spawn_callback{&bullets, &mass});

                    if (!bullets.empty()) {
                        if (_client)
                            _client->send(a_spawn_bullet{pl->id(), mass, std::move(bullets)});
                        else
                            server().send_to_all(a_spawn_bullet{pl->id(), mass, std::move(bullets)});
                    }
                } else {
                    pl->game_update(sim, _bm, false, {});
                }

                if (pl->collision_box()->get_position().y > 2100.f)
                    on_dead(pl.get());
            }

            _kick_mgr.update();
            _adj_box_mgr.update(sim);
        } else {
            if (_client) {
                if (_client->hello) {
                    _on_game = true;
                }
                _client->receiver(act_from_server_overloaded{this});
            } else {
                server().work(client_player_update_overloaded{this});
            }
        }
    }

    void on_window_resize(u32 width, u32 height) override {
        apply_window_size(width, height);
    }

    void on_dead(player* player) {
        player->increase_deaths();
        spawn(player);

        LOG_UPDATE("Deaths: {}", players | std::ranges::views::transform([](auto&& v) {
                                     return v->get_deaths();
                                 }));
    }

    void spawn(player* player = nullptr) {
        if (player) {
            player->position({levels[level_idx]->level_size().x / 2.f, 0.f});
            player->velocity({0.f, 0.f});
            player->enable_double_jump();
        } else {
            for (auto& p : players)
                spawn(p.get());
        }
    }

    void update_ai() {
        ai_mgr().provide_bullets(_bm.bullets(), [](const bullet& bl) {
            return ai_mgr_singleton::bullet_t{
                bl.physic()->get_position(),
                bl.physic()->get_velocity(),
                bl.physic()->get_mass(),
                bl.group()
            };
        });

        ai_mgr().provide_players(players, [gy = sim.gravity().y](u32 id, const std::shared_ptr<player>& pl) {
            auto& gun = pl->get_gun();

            return ai_mgr_singleton::player_t{
                pl->get_position(),
                pl->collision_box()->get_direction(),
                pl->get_size(),
                pl->get_velocity(),
                pl->barrel_pos(),
                id,
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

    void setup_ai() {
        ai_mgr().provide_level(levels[0]->level_size());
        ai_mgr().provide_platforms(levels[0]->get_platforms(), [](const level::platform_t& pl) {
            auto pos = pl.ph.get_position();
            auto len = pl.ph.length();
            return ai_mgr_singleton::platform_t{pos, sf::Vector2f(pos.x + len, pos.y)};
        });

        update_ai();
    }

private:
    void apply_window_size(u32 width, u32 height) {
        auto& level = levels[level_idx];
        auto f = (level->view_size().x / float(width)) * (float(height) / level->view_size().y);
        sf::Vector2f view_size {
            level->view_size().x,
            f * level->view_size().y
        };

        _view.setSize(view_size);
        window().setView(_view);
    }

    void update_cam() {
        float timestep = cam_timer.elapsed();
        cam_timer.restart();
        if (!approx_equal(_cam_pos.x, _view.getCenter().x, 0.001f) ||
            !approx_equal(_cam_pos.y, _view.getCenter().y, 0.001f)) {
            auto diff = _cam_pos - _view.getCenter();
            auto dir  = normalize(diff);
            if (!std::isinf(dir.x) && !std::isinf(dir.y)) {
                auto mov = dir * timestep * 600.f;
                auto c   = _view.getCenter();
                auto nc  = c + mov;
                if ((c.x < _cam_pos.x && nc.x > _cam_pos.x) ||
                    (c.x > _cam_pos.x && nc.x < _cam_pos.x))
                    nc.x = _cam_pos.x;
                if ((c.y < _cam_pos.y && nc.y > _cam_pos.y) ||
                    (c.y > _cam_pos.y && nc.y < _cam_pos.y))
                    nc.y = _cam_pos.y;

                _view.setCenter(nc);
                window().setView(_view);
            }
        }

        sf::Vector2f center_min = {std::numeric_limits<float>::max(),
                                   std::numeric_limits<float>::max()};
        sf::Vector2f center_max = {std::numeric_limits<float>::lowest(),
                                   std::numeric_limits<float>::lowest()};
        for (size_t i = 0; i < players.size(); ++i) {
            auto& pl = players[i];
            auto pos = pl->collision_box()->get_position();
            center_min.x = std::min(center_min.x, pos.x);
            center_max.x = std::max(center_max.x, pos.x);

            /* TODO: check all players that be controlled by this PC */
            if (i != player_idx)
                continue;

            center_min.y = std::min(center_min.y, pos.y);
            center_max.y = std::max(center_max.y, pos.y);
        }
        auto center = (center_min + center_max) / 2.f;

        auto pl_pos_x = players[player_idx]->get_position().x;
        auto view_sz_x = _view.getSize().x * 0.46f;
        if (center.x - pl_pos_x > view_sz_x)
            center.x = pl_pos_x + view_sz_x;
        if (pl_pos_x - center.x > view_sz_x)
            center.x = pl_pos_x - view_sz_x;

        auto& level = levels[level_idx];

        if (center.x + _view.getSize().x / 2.f > level->level_size().x)
            center.x = level->level_size().x - _view.getSize().x / 2.f;
        if (center.x - _view.getSize().x / 2.f < 0.f)
            center.x = _view.getSize().x / 2.f;
        if (center.y + _view.getSize().y / 2.f > level->level_size().y)
            center.y = level->level_size().y - _view.getSize().y / 2.f;
        if (center.y - _view.getSize().y / 2.f < 0.f)
            center.y = _view.getSize().y / 2.f;

        _cam_pos = center;
    }

private:
    physic_simulation sim;

    bullet_mgr         _bm;
    instant_kick_mgr   _kick_mgr;
    adjustment_box_mgr _adj_box_mgr;

    std::vector<std::shared_ptr<player>>      players;
    std::vector<std::shared_ptr<level>>       levels;
    std::vector<std::shared_ptr<ai_operator>> ai_operators;

    control_keys control_ks;
    control_keys control_ks2{1};

    sf::RectangleShape point_shape;
    sf::RectangleShape line_shape;

    timer cam_timer;
    sf::View _view;
    sf::Vector2f _cam_pos = {0.f, 0.f};

    u32 player_idx = 0;
    u32 level_idx = 0;

    std::shared_ptr<client_state> _client;
    sf::Clock _server_sync_timer;
    float     _server_sync_step = 0.06f;
    bool _on_game = false;

    bool _physic_debug = false;
    float _self_ping = 0.f;
};

}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    return dfdh::diefastdiehard().run(dfdh::args_view(argc, argv));
}
