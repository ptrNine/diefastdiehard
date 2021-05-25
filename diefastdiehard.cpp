#include <iostream>
#include "src/engine.hpp"
#include "src/physic_simulation.hpp"
#include "src/config.hpp"
#include "src/player.hpp"
#include "src/level.hpp"
#include "src/bullet.hpp"
#include "src/weapon.hpp"
#include "src/ai.hpp"
#include "src/rand_pool.hpp"
#include "src/networking.hpp"

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

namespace dfdh {


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

    player_states_t input_states{false, false, false, false, false};

    sf::IpAddress serv_ip = "127.0.0.1";
    u16 serv_port = SERVER_DEFAULT_PORT;
    act_socket sock = act_socket(CLIENT_DEFAULT_PORT);
};

class diefastdiehard : public dfdh::engine {
public:
    diefastdiehard(): _bm("bm1", sim, player_hit_callback) {}

    void on_init(args_view args) final {
        if (args.get("--client")) {
            _client = std::make_shared<client_state>();
            player_idx = 1;
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

        sim.add_update_callback("player", [this](float timestep) {
            for (auto& p : players)
                p->physic_update(timestep);
        });

        sim.add_platform_callback("player", [this](physic_point* pnt) {
            for (auto& p : players)
                if (pnt == p->collision_box())
                    p->enable_double_jump();
        });

        //ai_operators.push_back(ai_operator::create(ai_difficulty::ai_hard, 0));
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
    player_states_t server_player_states{false, false, false, false, false};

    void handle_event(const sf::Event& evt) final {
        if (!_on_game)
            return;

        players[player_idx]->update_input(control_ks, evt, sim, _bm);

        if (!_client) {
            auto states = players[player_idx]->extract_client_input_state();
            if (states != server_player_states) {
                server_player_states = states;
                server().send_to_all(a_srv_player_states{server_player_states, player_idx});
            }
        } else {
            auto states = players[player_idx]->extract_client_input_state();
            if (_client->input_states != states) {
                _client->input_states = states;
                _client->send(a_cli_player_states{states});
            }
        }

        if (evt.type == sf::Event::KeyPressed) {
            if (evt.key.code == sf::Keyboard::Space) {
                ++cur_wpn;
                if (cur_wpn == wpns.size())
                    cur_wpn = 0;

                for (auto& plr : players) plr->setup_pistol(wpns[cur_wpn]);

                /*
                ++player_idx;
                if (player_idx == players.size())
                    player_idx = 0;
                    */
            }
        }
    }

    void render_update(sf::RenderWindow& wnd) final {
        levels[level_idx]->draw(wnd);

        for (auto& player : players)
            player->draw(wnd);

        _bm.draw(wnd);

        /*
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
        */

        /* TODO: debug platforms */
        /*
        for (auto& p : sim.platforms()) {
            sf::Vertex line[] = {
                sf::Vertex(p.get_position()),
                sf::Vertex(p.get_position() + sf::Vector2f(p.length(), 0.f)),
            };
            wnd.draw(line, 2, sf::Lines);
        }
        */
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
        }
        update_ai();
    }

    struct client_player_update_overloaded {
        void operator()(u32 player_id, const auto& act) {
            it->client_player_update(player_id, act);
        }

        diefastdiehard* it;
    };

    struct act_from_server_overloaded {
        void operator()(const sf::IpAddress&, u16, const auto& act) {
            it->act_from_server(act);
        }

        diefastdiehard* it;
    };

    void act_from_server(const a_srv_player_states& player_states) {
        players[player_states.player_id]->update_from_client(player_states.st);
    }

    void act_from_server(const a_srv_player_physic_sync& sync_state) {
        auto& player = players[sync_state.player_id];

        //player->update_from_client(sync_state.st);
        player->position(sync_state.position);
        player->velocity(sync_state.velocity);
        //player->on_left(sync_state.on_left);

        auto old_gun_name = player->get_gun() ? player->get_gun()->get_weapon()->section() : "";
        if (!sync_state.cur_wpn_name.empty() && old_gun_name != sync_state.cur_wpn_name) {
            player->setup_pistol(sync_state.cur_wpn_name);
        }

        if (player->get_gun())
            player->get_gun()->ammo_elapsed(sync_state.ammo_elapsed);
    }

    void act_from_server(const a_srv_player_random_pool_init& act) {
        auto& player = players[act.player_id];
        std::cout << "Reset pool" << std::endl;
        player->rand_pool(act.pool);
    }

    void act_from_server(const auto&) {} /* dummy */

    void client_player_update(u32 player_id, const a_cli_i_wanna_play&) {
        //std::this_thread::sleep_for(500ms);
        //for (size_t i = 0; i < players.size(); ++i)
        //    server().send(player_id, a_srv_player_random_pool_init{i, players[i]->rand_pool()}, true);

        /* TODO: handle this */
        _on_game = true;
    }

    void client_player_update(u32 player_id, const a_cli_player_params& params) {
        players[player_id]->set_params_from_client(params);
    }

    void client_player_update(u32 player_id, const a_cli_player_states& states) {
        players[player_id]->update_from_client(states.st);
    }

    void game_update() final {
        if (_on_game) {
            if (!ai_operators.empty())
                ai_operators_consume();

            if (_client)
                _client->receiver(act_from_server_overloaded{this});
            else {
                server().work(client_player_update_overloaded{this});

                if (_server_sync_timer.getElapsedTime().asSeconds() > _server_sync_step) {
                    _server_sync_timer.restart();
                    for (size_t i = 0; i < players.size(); ++i) {
                        auto& player = players[i];
                        auto  gun    = player->get_gun();
                        server().send_to_all(
                            a_srv_player_physic_sync{i,
                                                     player->get_position(),
                                                     player->get_velocity(),
                                                     player->extract_client_input_state(),
                                                     gun ? gun->get_weapon()->section() : "",
                                                     gun ? gun->ammo_elapsed() : 0,
                                                     player->get_on_left()});
                    }
                }
            }

            sim.update(60, 1.0f);
            update_cam();

            for (auto& pl : players) {
                pl->game_update(sim, _bm);
                if (pl->collision_box()->get_position().y > 2100.f)
                    on_dead(pl.get());
            }
        } else {
            if (_client) {
                if (_client->hello)
                    _on_game = true;
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

        std::cout << "Deaths: ";
        for (auto& pl : players)
            std::cout << pl->get_deaths() << " ";
        std::cout << std::endl;
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
            auto gun = pl->get_gun();

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
                gun ? gun->get_weapon()->get_dispersion() : 0.01f,
                pl->get_group(),
                gun ? gun->get_bullet_vel() : 1500.f,
                gun ? gun->get_weapon()->get_fire_rate() : 100.f,
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

    //std::shared_ptr<physic_point> test_point;
    //std::shared_ptr<physic_line> test_line;
    bullet_mgr _bm;

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
};

}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    return dfdh::diefastdiehard().run(dfdh::args_view(argc, argv));
}
