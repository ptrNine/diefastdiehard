#include <iostream>
#include "src/engine.hpp"
#include "src/physic_simulation.hpp"
#include "src/config.hpp"
#include "src/player.hpp"
#include "src/level.hpp"
#include "src/bullet.hpp"
#include "src/weapon.hpp"

#include <SFML/Graphics/RectangleShape.hpp>

namespace dfdh {
class diefastdiehard : public dfdh::engine {
public:
    diefastdiehard(): _bm("bm1", sim, player_hit_callback) {}

    void on_init() final {
        //cfg();

        levels.push_back(level::create("lvl_aes", sim));

        auto pl = player::create(sim, {50.f, 90.f});
        pl->set_body("body.png");
        pl->set_face("face0.png");
        players.push_back(pl);
        pl->setup_pistol("wpn_glk17");

        auto pl2 = player::create(sim, {50.f, 90.f});
        pl2->set_body("body.png", {109, 68, 26});
        pl2->set_face("face1.png");
        pl2->setup_pistol("wpn_416");
        players.push_back(pl2);
        //players.push_back(player::create(sim, {50.f, 80.f}));
        //players.push_back(player::create(sim, {50.f, 80.f}));
        //players.push_back(player::create(sim, {50.f, 80.f}));
        spawn();

        sim.add_update_callback("player", [this](float timestep) {
            for (auto& p : players)
                p->physic_update(timestep);
        });

        sim.add_platform_callback("player", [this](physic_point* pnt) {
            for (auto& p : players)
                if (pnt == p->collision_box())
                    p->enable_double_jump();
        });

        apply_window_size(window().getSize().x, window().getSize().y);
    }

    void on_destroy() final {

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

    void game_update() final {
        players[1]->ai_operate();

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
        spawn(player);
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
                auto mov = dir * timestep * 2000.f;
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

    std::vector<std::shared_ptr<player>> players;
    std::vector<std::shared_ptr<level>> levels;

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
