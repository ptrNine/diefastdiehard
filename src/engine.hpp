#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>

#include "nuklear.hpp"
#include "devconsole.hpp"
#include "main_menu.hpp"
#include "args_view.hpp"
#include "command_buffer.hpp"
#include "config.hpp"
#include "profiler.hpp"

namespace dfdh {

class global_ctx {
public:
    static global_ctx& init() {
        static global_ctx inst;
        return inst;
    }

    global_ctx() {
        cfg().try_parse("data/settings/window.cfg");
        wnd_size = cfg().get_or_write_default("window", "size", sf::Vector2f(800.f, 600.f), "data/settings/window.cfg", true);
    }

    [[nodiscard]]
    u32 wnd_w() const {
        return u32(wnd_size.x);
    }

    [[nodiscard]]
    u32 wnd_h() const {
        return u32(wnd_size.y);
    }

private:
    sf::ContextSettings settings{24, 8, 4, 3, 3};
    sf::Vector2f wnd_size;
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
        _wnd.setFramerateLimit(240);

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


            /*
            loop_prof.try_print([](auto& prof) {
                LOG("prof: {}", prof);
            });
            */
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
        cfg().try_write("window", "size", sf::Vector2f(float(width), float(height)), true, true);
    }

private:
    global_ctx& _ctx_init__ = global_ctx::init();

protected:
    ui_ctx ui;

private:
    sf::RenderWindow _wnd;
    devconsole       _devcons;
    main_menu        _main_menu;
    profiler         loop_prof;
};

}
