#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>

#include "args_view.hpp"

namespace dfdh {

class engine {
public:
    engine(): _wnd(sf::VideoMode(1400, 1000), "diefastdiehard") {}
    virtual ~engine() = default;

    int run(args_view args) {
        on_init(std::move(args));

        _wnd.setVerticalSyncEnabled(true);
        _wnd.setFramerateLimit(60);

        while (_wnd.isOpen()) {
            game_update();

            sf::Event evt;
            while (_wnd.pollEvent(evt)) {
                if (evt.type == sf::Event::Closed)
                    _wnd.close();
                else if (evt.type == sf::Event::Resized) {
                    on_window_resize(evt.size.width, evt.size.height);
                }
                else
                    handle_event(evt);
            }

            _wnd.clear();
            render_update(_wnd);
            _wnd.display();
        }

        on_destroy();
        return EXIT_SUCCESS;
    }

    sf::RenderWindow& window() {
        return _wnd;
    }

    virtual void on_init(args_view args) = 0;
    virtual void on_destroy() = 0;
    virtual void handle_event(const sf::Event& event) = 0;
    virtual void render_update(sf::RenderWindow& wnd) = 0;
    virtual void game_update() = 0;
    virtual void on_window_resize(u32 width, u32 height) = 0;

private:
    sf::RenderWindow _wnd;
};

}
