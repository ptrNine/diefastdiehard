#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>

#include "nuklear.hpp"
#include "devconsole.hpp"
#include "main_menu.hpp"
#include "args_view.hpp"
#include "command_buffer.hpp"

namespace dfdh {

class global_ctx {
public:
    static global_ctx& init() {
        static global_ctx inst;
        return inst;
    }

private:
    sf::ContextSettings settings{24, 8, 4, 3, 3};
};

class engine {
public:
    engine(): _wnd(sf::VideoMode(1400, 1000), "diefastdiehard") {
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
            command_buffer().run_handlers();
            game_update();

            ui.input_begin();
            sf::Event evt;
            while (_wnd.pollEvent(evt)) {
                ui.handle_event(evt);
                _devcons.handle_event(ui, evt);

                if (evt.type == sf::Event::Closed) {
                    _wnd.close();
                    goto end;
                } else if (evt.type == sf::Event::Resized) {
                    on_window_resize(evt.size.width, evt.size.height);
                    _main_menu.set_size({float(evt.size.width), float(evt.size.height)});
                }
                else
                    handle_event(evt);
            }
            ui.input_end();

            _wnd.clear();
            render_update(_wnd);
            ui.render();
            ui_update();
            _wnd.display();
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
    virtual void on_window_resize(u32 width, u32 height) = 0;

private:
    global_ctx& _ctx_init__ = global_ctx::init();

protected:
    ui_ctx ui;

private:
    sf::RenderWindow _wnd;
    devconsole       _devcons;
    main_menu        _main_menu;
};

}
