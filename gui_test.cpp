#include "src/nuklear.hpp"
#include <SFML/Graphics/RenderWindow.hpp>

using namespace dfdh;

int main() {
    auto wnd = sf::RenderWindow(sf::VideoMode(1400, 1000), "wnd");
    wnd.setActive();

    auto ui = ui_ctx(wnd);

    while (wnd.isOpen()) {
        ui.input_begin();
        sf::Event evt;
        while (wnd.pollEvent(evt)) {
            ui.handle_event(evt);

            if (evt.type == sf::Event::Closed) {
                wnd.close();
                goto end;
            }
        }
        ui.input_end();

        wnd.clear();
        ui.render();
        wnd.display();
    }

end:
    return 0;
}
