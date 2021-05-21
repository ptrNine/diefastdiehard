#include <iostream>
#include "src/engine.hpp"
#include "src/physic_simulation.hpp"
#include "src/config.hpp"
#include "src/player.hpp"
#include "src/level.hpp"
#include "src/bullet.hpp"
#include "src/weapon.hpp"
#include "src/ai.hpp"

#include <SFML/Graphics/RectangleShape.hpp>

namespace dfdh {
class diefastdiehard : public dfdh::engine {
public:
    diefastdiehard(): _bm("bm1", sim, player_hit_callback) {}

    void on_init() final {
        //cfg();

        levels.push_back(level::create("lvl_aes", sim));

        /*
        auto pl = player::create(sim, {50.f, 90.f});
        pl->set_body("body.png", {247, 198, 70});
        pl->set_face("face3.png");
        pl->setup_pistol("wpn_brn10");
        players.push_back(pl);
        */

        for (int i = 0; i < 4; ++i) {
            auto pl2 = player::create(sim, {50.f, 90.f});
            pl2->set_face("face1.png");
            pl2->setup_pistol("wpn_ss50");
            pl2->position({600.f + float(i) * 50.f, 500});
            players.push_back(pl2);
            if (i >= 2) {
                pl2->group(0);
                pl2->set_body("body.png", {0, 0, 0});
            } else {
                pl2->group(1);
                pl2->set_body("body.png", {109, u8(i * 25), 26});
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

        //apply_window_size(window().getSize().x, window().getSize().y);

        ai_operators.push_back(ai_operator::create(ai_difficulty::ai_hard, 0));
        ai_operators.push_back(ai_operator::create(ai_difficulty::ai_hard, 1));
        ai_operators.push_back(ai_operator::create(ai_difficulty::ai_medium, 2));
        ai_operators.push_back(ai_operator::create(ai_difficulty::ai_medium, 3));

        /*
        ai_operators.push_back(ai_operator::create(ai_difficulty::ai_insane, 7));
        ai_operators.push_back(ai_operator::create(ai_difficulty::ai_insane, 8));
        ai_operators.push_back(ai_operator::create(ai_difficulty::ai_insane, 9));
        ai_operators.push_back(ai_operator::create(ai_difficulty::ai_insane, 10));
        ai_operators.push_back(ai_operator::create(ai_difficulty::ai_insane, 11));
        ai_operators.push_back(ai_operator::create(ai_difficulty::ai_insane, 12));
        */

        if (!ai_operators.empty()) {
            setup_ai();
            ai_mgr().worker_start();
        }
    }

    void on_destroy() final {
        if (!ai_operators.empty())
            ai_mgr().worker_stop();
    }

    void handle_event(const sf::Event& evt) final {
        players[player_idx]->update_input(control_ks, evt, sim, _bm);

        /*
        if (players.size() > 1)
            players.back()->update_input(control_ks2, evt, sim, _bm);
            */

        if (evt.type == sf::Event::KeyPressed) {
            if (evt.key.code == sf::Keyboard::Space) {
                ++player_idx;
                if (player_idx == players.size())
                    player_idx = 0;
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

    void game_update() final {
        if (!ai_operators.empty())
            ai_operators_consume();

        sim.update();
        update_cam();

        for (auto& pl : players) {
            pl->game_update(sim, _bm);
            if (pl->collision_box()->get_position().y > 2100.f)
                on_dead(pl.get());
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
                pl->get_on_left(),
                pl->collision_box()->is_lock_y()
            };
        });

        ai_mgr().provide_physic_sim(sim.gravity());
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
        float timestep = timer.getElapsedTime().asSeconds();
        timer.restart();
        if (!approx_equal(_cam_pos.x, _view.getCenter().x, 0.001f) ||
            !approx_equal(_cam_pos.y, _view.getCenter().y, 0.001f)) {
            auto diff = _cam_pos - _view.getCenter();
            auto dir  = normalize(diff);
            if (!std::isinf(dir.x) && !std::isinf(dir.y)) {
                auto mov = dir * timestep * 400.f;
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
        for (auto& pl : players) {
            auto pos = pl->collision_box()->get_position();
            center_min.x = std::min(center_min.x, pos.x);
            center_min.y = std::min(center_min.y, pos.y);
            center_max.x = std::max(center_max.x, pos.x);
            center_max.y = std::max(center_max.y, pos.y);
        }
        auto center = (center_min + center_max) / 2.f;

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

    sf::Clock timer;
    sf::View _view;
    sf::Vector2f _cam_pos = {0.f, 0.f};

    u32 player_idx = 0;
    u32 level_idx = 0;
};

}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    return dfdh::diefastdiehard().run();
}
