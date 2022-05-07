#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>

#include "nuklear.hpp"
#include "devconsole.hpp"
#include "main_menu.hpp"
#include "args_view.hpp"
#include "command_buffer.hpp"
#include "cfg.hpp"
#include "profiler.hpp"

namespace dfdh {

class global_ctx {
public:
    static global_ctx& instance() {
        static global_ctx inst;
        return inst;
    }

    global_ctx() {
        auto sect = conf.get_or_create("window"_sect);
        wnd_size  = sect.value_or_default_and_set("size", vec2f(800.f, 600.f));
        conf.commit();
    }

    [[nodiscard]]
    u32 wnd_w() const {
        return u32(wnd_size.x);
    }

    [[nodiscard]]
    u32 wnd_h() const {
        return u32(wnd_size.y);
    }

    void on_resize(u32 w, u32 h) {
        wnd_size = vec2f{float(w), float(h)};
        auto sect = conf.get_or_create("window"_sect);
        sect.set("size", wnd_size);
    }

private:
    sf::ContextSettings settings{24, 8, 4, 3, 3};
    vec2f wnd_size;
    cfg conf{"data/settings/window.cfg", cfg_mode::create_if_not_exists | cfg_mode::commit_at_destroy};
};

class engine {
public:
    engine(): _wnd(sf::VideoMode(_ctx_init__.wnd_w(), _ctx_init__.wnd_h()), "diefastdiehard") {
        _wnd.setActive(true);
        ui = ui_ctx(_wnd);
    }

    virtual ~engine() = default;

    int run(args_view args) {
        on_init(std::move(args));
        auto wnd_size = _wnd.getSize();
        _devcons.set_size(ui, {float(wnd_size.x), float(wnd_size.y)});
        _main_menu.set_size({float(wnd_size.x), float(wnd_size.y)});

        _wnd.setVerticalSyncEnabled(true);
        _wnd.setFramerateLimit(60);

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
                    _devcons.handle_event(ui, evt);

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
                auto prof = loop_prof.scope("ui");
                ui_update();
            }

            {
                auto prof = loop_prof.scope("render_upd");
                _wnd.clear();
                render_update(_wnd);
            }

            {
                auto prof = loop_prof.scope("render");
                ui.render();
                _wnd.display();
            }

            {
                auto prof = loop_prof.scope("commands");
                command_buffer().run_handlers();
                post_command_update();
            }

            if (profiler_print) {
                loop_prof.try_print([](auto& prof) { LOG("min|max|avg: {}", prof); });
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
        devcons().update(ui);
    }

    virtual void on_init(args_view args) = 0;
    virtual void on_destroy() = 0;
    virtual void handle_event(const sf::Event& event) = 0;
    virtual void render_update(sf::RenderWindow& wnd) = 0;
    virtual void game_update() = 0;
    virtual void post_command_update() = 0;

    virtual void on_window_resize(u32 width, u32 height) {
        _ctx_init__.on_resize(width, height);
    }

    void enable_profiler_print(bool value) {
        profiler_print = value;
    }

private:
    global_ctx& _ctx_init__ = global_ctx::instance();

protected:
    ui_ctx ui;

private:
    sf::RenderWindow _wnd;
    devconsole       _devcons;
    main_menu        _main_menu;
    profiler         loop_prof;
    bool             profiler_print = false;
};

}
