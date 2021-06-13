#pragma once

#include <optional>
#include "nuklear.hpp"

namespace dfdh {

inline std::optional<sf::Color> ui_color_picker(ui_ctx& ui, const sf::Color& color, float y_size) {
    ui.layout_row_dynamic(y_size, 1);
    auto colorf         = sfml_color_to_nk_float(color);
    colorf              = ui.color_picker(colorf, NK_RGB);
    sf::Color new_color = nk_color_to_sfml(colorf);

    ui.layout_row_dynamic(25.f, 1);
    new_color.r = u8(u32(ui.propertyi("#R:", 0, new_color.r, 255, 1, 1)));
    new_color.g = u8(u32(ui.propertyi("#G:", 0, new_color.g, 255, 1, 1)));
    new_color.b = u8(u32(ui.propertyi("#B:", 0, new_color.b, 255, 1, 1)));

    return color != new_color ? new_color : std::optional<sf::Color>{};
}

template <typename T>
inline void ui_weapon_property(
    ui_ctx& ui, const std::string& name, T value, nk_size max_value, T bar_modifier = 1) {

    ui.label(name.data(), NK_TEXT_LEFT);
    auto progress  = std::min(nk_size(value * bar_modifier), max_value);
    ui.progress(&progress, max_value, false);
    ui.label(std::to_string(value).data(), NK_TEXT_CENTERED);
}

struct ui_row {
    enum row_t {
        dynamic,
        variable,
        fixed
    };

    ui_row() = default;
    ui_row(row_t row_type = variable): type(row_type) {}
    ui_row(float width, row_t row_type = fixed): w(width), type(row_type) {}

    float w = 0.f;
    row_t type = dynamic;
};

inline void ui_layout_row(ui_ctx& ui, float row_height, std::initializer_list<ui_row> rows) {
    ui.layout_row_template_begin(row_height);

    for (auto& r : rows) {
        switch (r.type) {
            case ui_row::dynamic:
                ui.layout_row_template_push_dynamic();
                break;
            case ui_row::variable:
                ui.layout_row_template_push_variable(r.w);
                break;
            case ui_row::fixed:
                ui.layout_row_template_push_static(r.w);
                break;
        }
    }

    ui.layout_row_template_end();
}

} // namespace dfdh
