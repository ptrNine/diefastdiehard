#include "src/stdafx.hpp"

#include <iostream>

#include "base/serialization.hpp"
#include "ui/player_configurator_ui.hpp"

#include "engine.hpp"
#include "command_buffer.hpp"
#include "game_commands.hpp"
#include "game_state.hpp"

#include "script/lua_init.hpp"

#include <SFML/Graphics/RectangleShape.hpp>

inline constexpr bool MP_MOVE_ADJUSTMENT = true;

namespace dfdh {

class diefastdiehard : public dfdh::engine {
public:
    //diefastdiehard() {}
    void cmd_log(const std::string& cmd, const std::optional<std::string>& value) {
        if (cmd == "time") {
            if (value) {
                if (*value == "on")
                    devcons().show_time();
                else if (*value == "off")
                    devcons().show_time(false);
                else
                    LOG_ERR("log time: unknown action '{}' (must be 'on' or 'off')", *value);
            } else {
                LOG_INFO("log time: {}", devcons().is_show_time() ? "on"sv : "off"sv);
            }
        }
        else if (cmd == "level") {
            if (value) {
                if (*value == "on")
                    devcons().show_level();
                else if (*value == "off")
                    devcons().show_level(false);
                else
                    LOG_ERR("log level: unknown action '{}' (must be 'on' or 'off')", *value);
            } else {
                LOG_INFO("log level: {}", devcons().is_show_level() ? "on"sv : "off"sv);
            }
        }
        else if (cmd == "ring") {
            if (value) {
                try {
                    auto sz = ston<size_t>(*value);
                    devcons().ring_size(sz);
                } catch (...) {
                    LOG_ERR("log ring: '{}' not an unsigned int", *value);
                }
            }
            else {
                LOG_INFO("log ring: {}", devcons().ring_size());
            }
        }
        else if (cmd == "clear") {
            devcons().clear();
        }
        else if (cmd == "help") {
            gc.cmd_help("log");
        }
        else {
            LOG_ERR("log: unknown subcommand '{}'", cmd);
        }
    }

    void init_commands() {
        command_buffer().add_handler("log", &diefastdiehard::cmd_log, this);
        command_buffer().add_handler("profiler", &diefastdiehard::enable_profiler_print, static_cast<engine*>(this));
    }

    void on_init(args_view args) final {
        init_commands();

        if (args.get("--physic-debug"))
            gs.debug_physics = true;

        init_lua();
    }

    void init_lua() {
        lua.emplace(luactx_mgr::global(true));

        lua->ctx().annotate({.argument_names = {"commands"}});
        lua->ctx().provide(LUA_TNAME("cmd"), [](const std::string& commands) {
            for (auto cmd : commands / split('\n')) command_buffer().push(std::string(cmd.begin(), cmd.end()));
        });
        lua->ctx().provide(LUA_TNAME("GS"), &gs);

        lua_handle_event = lua->get_caller<void(const sf::Event&)>("G.handle_event", 2s, true);
        lua_game_update  = lua->get_caller<void(game_state*)>("G.game_update", 2s, true);

        lua->load();
    }

    void on_destroy() final {
        if (!gs.ai_operators.empty())
            ai_mgr().worker_stop();
    }

    void handle_event(const sf::Event& evt) final {
        gs.handle_event(evt);
        lua_handle_event(evt);
    }

    void ui_update() final {
        engine::ui_update();
        gs.ui_update(ui);
        //mainmenu().update(ui);
    }

    void render_update(sf::RenderWindow& wnd) final {
        update_cam();
        gs.render_update(wnd);
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

    void game_update() final {
        gs.game_update();
        lua_game_update(&gs);
    }

    void post_command_update() override {
        while (!gs.events.empty()) {
            switch (gs.events.front()) {
            case game_state_event::level_changed:
                apply_window_size(window().getSize().x, window().getSize().y);
                break;
            case game_state_event::shutdown:
                window().close();
                break;
            }
            gs.events.pop();
        }

        if (gs.lua_cmd_enabled) {
            for (auto& cmd : gs.lua_cmd_queue)
                lua->execute_line(cmd);
            gs.lua_cmd_queue.clear();
        }
    }

    void on_window_resize(u32 width, u32 height) override {
        engine::on_window_resize(width, height);
        apply_window_size(width, height);
    }

    void on_dead(player* player) {
        player->increase_deaths();
        spawn(player);
    }

    void spawn(player* player = nullptr) {
        if (!gs.cur_level) {
            LOG_WARN("Can't spawn players: level was not loaded");
            return;
        }

        if (player) {
            player->position({gs.cur_level->level_size().x / 2.f, 0.f});
            player->velocity({0.f, 0.f});
            player->enable_double_jump();
        } else {
            for (auto& [_, p] : gs.players)
                spawn(p.get());
        }
    }

private:
    void apply_window_size(u32 width, u32 height) {
        if (!gs.cur_level)
            return;

        auto& level = *gs.cur_level;
        auto f = (level.view_size().x / float(width)) * (float(height) / level.view_size().y);
        vec2f view_size {
            level.view_size().x,
            f * level.view_size().y
        };

        _view.setSize(view_size);
        window().setView(_view);
    }

    void update_cam() {
        if (!gs.cur_level)
            return;

        float timestep = cam_timer.elapsed();
        cam_timer.restart();
        if (!approx_equal(gs.cam_pos.x, _view.getCenter().x, 0.001f) ||
            !approx_equal(gs.cam_pos.y, _view.getCenter().y, 0.001f)) {
            auto diff = gs.cam_pos - _view.getCenter();
            auto dir  = normalize(diff);
            if (!std::isinf(dir.x) && !std::isinf(dir.y)) {
                auto mov = dir * timestep * 600.f;
                auto c   = _view.getCenter();
                auto nc  = c + mov;
                if ((c.x < gs.cam_pos.x && nc.x > gs.cam_pos.x) ||
                    (c.x > gs.cam_pos.x && nc.x < gs.cam_pos.x))
                    nc.x = gs.cam_pos.x;
                if ((c.y < gs.cam_pos.y && nc.y > gs.cam_pos.y) ||
                    (c.y > gs.cam_pos.y && nc.y < gs.cam_pos.y))
                    nc.y = gs.cam_pos.y;

                _view.setCenter(nc);
                window().setView(_view);
            }
        }

        vec2f center_min = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        vec2f center_max = {std::numeric_limits<float>::lowest(),
                            std::numeric_limits<float>::lowest()};
        for (auto& [_, pl] : gs.players) {
            auto pos =
                lerp(pl->collision_box()->get_position(),
                     pl->collision_box()->get_position() + pl->collision_box()->get_velocity() * gs.sim.last_timestep(),
                     gs.sim.interpolation_factor());
            center_min.x = std::min(center_min.x, pos.x);
            center_max.x = std::max(center_max.x, pos.x);

            /* TODO: check all players that be controlled by this PC */
            if (!gs.controlled_players.contains(pl->name()))
                continue;

            center_min.y = std::min(center_min.y, pos.y);
            center_max.y = std::max(center_max.y, pos.y);
        }
        auto center = (center_min + center_max) / 2.f;

        /* TODO: check this */
        for (auto& [_, control_data] : gs.controlled_players) {
            auto& this_player = control_data.this_player;
            auto  pl_pos_x    = lerp(this_player->get_position().x,
                                 this_player->get_position().x +
                                     this_player->collision_box()->get_velocity().x * gs.sim.last_timestep(),
                                 gs.sim.interpolation_factor());
            auto view_sz_x = _view.getSize().x * 0.46f;
            if (center.x - pl_pos_x > view_sz_x)
                center.x = pl_pos_x + view_sz_x;
            if (pl_pos_x - center.x > view_sz_x)
                center.x = pl_pos_x - view_sz_x;

            auto& level = *gs.cur_level;

            if (center.x + _view.getSize().x / 2.f > level.level_size().x)
                center.x = level.level_size().x - _view.getSize().x / 2.f;
            if (center.x - _view.getSize().x / 2.f < 0.f)
                center.x = _view.getSize().x / 2.f;
            if (center.y + _view.getSize().y / 2.f > level.level_size().y)
                center.y = level.level_size().y - _view.getSize().y / 2.f;
            if (center.y - _view.getSize().y / 2.f < 0.f)
                center.y = _view.getSize().y / 2.f;
        }

        gs.cam_pos = center;
    }

private:
    game_state    gs;
    game_commands gc{gs};

    sf::RectangleShape point_shape;
    sf::RectangleShape line_shape;

    timer    cam_timer;
    sf::View _view;

    std::optional<luactx_mgr>          lua;
    lua_caller<void(const sf::Event&)> lua_handle_event;
    lua_caller<void(game_state*)>      lua_game_update;
};
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    return std::make_unique<dfdh::diefastdiehard>()->run(dfdh::args_view(argc, argv));
}
