#pragma once

#include "types.hpp"
#include "nuklear.hpp"

namespace dfdh {

class main_menu {
public:
    void update(ui_ctx& ui) {
        auto style = &ui.nk_ctx()->style;
        nk_style_push_color(ui.nk_ctx(), &style->window.background, nk_rgba(20, 20, 20, 210));
        ui.style_push_style_item(&style->window.fixed_background, nk_style_item_color(nk_rgba(20, 20, 20, 210)));

        if (ui.begin("main_menu", {0.f, 0.f, _size.x, _size.y}, NK_WINDOW_BACKGROUND | NK_WINDOW_NO_SCROLLBAR)) {
            auto content_region_size = ui.window_get_content_region_size();

            ui.layout_space_begin(NK_STATIC, 0, 3);

            auto start = content_region_size.y / 2.f - 60.f;

            ui.layout_space_push(nk_rect(0, start, content_region_size.x, 30));
            ui.button_label("Start game");
            ui.layout_space_push(nk_rect(0, start + 40.f, content_region_size.x, 30));
            ui.button_label("Settings");
            ui.layout_space_push(nk_rect(0, start + 80.f, content_region_size.x, 30));
            ui.button_label("Exit");
        }
        ui.end();

        ui.style_pop_color();
        ui.style_pop_style_item();
    }

    void set_size(const sf::Vector2f& size) {
        _size = size;
    }

private:
    sf::Vector2f _size;
};

}
