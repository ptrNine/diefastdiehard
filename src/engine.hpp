#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>

#include "base/args_view.hpp"
#include "base/cfg.hpp"
#include "base/profiler.hpp"

#include "ui/nuklear.hpp"
#include "ui/devconsole.hpp"
#include "ui/main_menu.hpp"

#include "command_buffer.hpp"

namespace dfdh {

namespace defaults {
    static constexpr auto window_size = vec2u{800, 600};
}

class engine {
public:
    static constexpr auto config_path = "data/settings/engine.cfg";

    static sf::VideoMode video_mode(vec2u wnd_size) {
        return {wnd_size.x, wnd_size.y};
    }

    engine():
        _conf(config_path, cfg_mode::create_if_not_exists | cfg_mode::commit_at_destroy),
        _engine_conf(_conf.get_or_create("engine"_sect)),
        _wnd(video_mode(_engine_conf.value_or_default_and_set("window_size", defaults::window_size)), "diefastdiehard"),
        _devcons(ui) {

        _wnd.setActive(true);
        ui = ui_ctx(_wnd);
    }

    virtual ~engine() {
        _conf.commit();
    }

    int run(args_view args) {
        init_window();
        on_init(std::move(args));

        auto wnd_size = window_size_float();
        _devcons.place_into_window(wnd_size);
        _main_menu.set_size(wnd_size);

        _wnd.setVerticalSyncEnabled(_engine_conf.value_or_default_and_set("vsync", true));
        _wnd.setFramerateLimit(_engine_conf.value_or_default_and_set("framerate_limit", 60U));

        profiler_print = _engine_conf.value_or_default_and_set("profiler", false);

        while (_wnd.isOpen()) {
            {
                auto prof = loop_prof.scope("logic");
                game_update();
            }

            {
                auto prof = loop_prof.scope("events");
                ui.input_begin();
                sf::Event evt;
                while (_wnd.pollEvent(evt)) {
                    ui.handle_event(evt);
                    _devcons.handle_event(evt);

                    if (evt.type == sf::Event::Closed) {
                        _wnd.close();
                        goto end;
                    }
                    else if (evt.type == sf::Event::Resized) {
                        on_window_resize(evt.size.width, evt.size.height);
                        _main_menu.set_size({float(evt.size.width), float(evt.size.height)});
                    }
                    else {
                        if (!_devcons.is_active())
                            handle_event(evt);
                    }
                }
                ui.input_end();
            }

            {
                auto prof = loop_prof.scope("ui", false);
                ui_update();
            }

            {
                auto prof = loop_prof.scope("render");
                _wnd.clear();
                render_update(_wnd);
            }

            {
                auto prof = loop_prof.scope("ui");
                ui.render();
            }

            {
                auto prof = loop_prof.scope("swapbuffers");
                _wnd.display();
            }

            {
                auto prof = loop_prof.scope("commands");
                command_buffer().run_handlers();
            }

            if (profiler_print) {
                loop_prof.try_print(
                    [](auto& prof) { glog().detail(prof.short_print_format() ? "{}" : "min|max|avg: {}", prof); });
            }
        }

end:
        on_destroy();
        return EXIT_SUCCESS;
    }

    sf::RenderWindow& window() {
        return _wnd;
    }

    devconsole& devcons() {
        return _devcons;
    }

    main_menu& mainmenu() {
        return _main_menu;
    }

    virtual void ui_update() {
        devcons().update_internal();
    }

    virtual void on_init(args_view args) = 0;
    virtual void on_destroy() = 0;
    virtual void handle_event(const sf::Event& event) = 0;
    virtual void render_update(sf::RenderWindow& wnd) = 0;
    virtual void game_update() = 0;
    virtual void post_update() = 0;

    virtual void on_window_resize(u32 width, u32 height) {
        _engine_conf.set("window_size", vec2u{width, height});
        _engine_conf.set("window_pos", window_position());
    }

    void enable_profiler_print(bool value) {
        profiler_print = value;
    }

    vec2u window_size() const {
        return _wnd.getSize();
    }

    vec2i window_position() const {
        return _wnd.getPosition();
    }

    vec2f window_size_float() const {
        auto sz = window_size();
        return {float(sz.x), float(sz.y)};
    }

    vec2u screen_size() const {
        auto desktop_mode = sf::VideoMode::getDesktopMode();
        return {desktop_mode.width, desktop_mode.height};
    }

    void resize_window(vec2u size) {
        _wnd.setSize(size);
    }

private:
    void init_window() {
        auto wnd_size  = window_size();
        auto screen_sz = screen_size();

        if (screen_sz.x < wnd_size.x)
            wnd_size.x = screen_sz.x;
        if (screen_sz.y < wnd_size.y)
            wnd_size.y = screen_sz.y;
        if (sf::Vector2u(wnd_size) != _wnd.getSize())
            resize_window(wnd_size);

        auto wnd_pos =
            _engine_conf.value_or_default_and_set("window_pos", static_cast<vec2i>((screen_sz / 2) - (wnd_size / 2)));
        _wnd.setPosition(wnd_pos);
    }

protected:
    ui_ctx ui;

private:
    cfg                 _conf;
    cfg_section<false>  _engine_conf;
    sf::ContextSettings _settings{24, 8, 4, 3, 3};
    sf::RenderWindow    _wnd;
    devconsole          _devcons;
    main_menu           _main_menu;
    profiler            loop_prof;
    bool                profiler_print = false;
};
}
