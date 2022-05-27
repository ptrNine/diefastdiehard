#pragma once

#include "base/signals.hpp"
#include "nuklear.hpp"
#include "base/flags.hpp"

namespace dfdh {

DEF_FLAG_TYPE(ui_wnd_opt, dfdh::flag32_t,
    border           = ui_wnd_opt::def<0, NK_WINDOW_BORDER>,
    movable          = ui_wnd_opt::def<1, NK_WINDOW_MOVABLE>,
    scalable         = ui_wnd_opt::def<2, NK_WINDOW_SCALABLE>,
    closable         = ui_wnd_opt::def<3, NK_WINDOW_CLOSABLE>,
    minimizable      = ui_wnd_opt::def<4, NK_WINDOW_MINIMIZABLE>,
    no_scroll        = ui_wnd_opt::def<5, NK_WINDOW_NO_SCROLLBAR>,
    title            = ui_wnd_opt::def<6, NK_WINDOW_TITLE>,
    auto_hide_scroll = ui_wnd_opt::def<7, NK_WINDOW_SCROLL_AUTO_HIDE>,
    background       = ui_wnd_opt::def<8, NK_WINDOW_BACKGROUND>,
    scale_left       = ui_wnd_opt::def<9, NK_WINDOW_SCALE_LEFT>,
    no_input         = ui_wnd_opt::def<10, NK_WINDOW_NO_INPUT>,
);


class basic_view : public slot_holder {
public:
    basic_view(ui_ctx&     ui_context,
               std::string iview_name,
               ui_wnd_opt  flags = ui_wnd_opt::title | ui_wnd_opt::border | ui_wnd_opt::movable | ui_wnd_opt::scalable |
                                  ui_wnd_opt::closable):
        uictx(&ui_context), view_name(std::move(iview_name)), nk_flags(flags) {}

    basic_view(ui_ctx&      ui_context,
               std::string  iview_name,
               const vec2f& position,
               const vec2f& size,
               ui_wnd_opt flags = ui_wnd_opt::title | ui_wnd_opt::border | ui_wnd_opt::movable | ui_wnd_opt::scalable |
                                  ui_wnd_opt::closable):
        uictx(&ui_context), view_name(std::move(iview_name)), pos(position), sz(size), nk_flags(flags) {}

    virtual ~basic_view() = default;
    virtual void handle_event(const sf::Event& evt) = 0;
    virtual void update() = 0;
    virtual void on_show() = 0;
    virtual void on_close() = 0;
    virtual void style_start() = 0;
    virtual void style_end() = 0;

    void show(bool value) {
        if (is_show && !value) {
            uictx->window_close(view_name.data());
            on_close();
            sig_on_close.emit_immediate();
        }
        if (!is_show && value) {
            on_show();
            sig_on_show.emit_immediate();
        }

        is_show = value;
    }

    void toggle() {
        show(!is_show);
    }

    void update_internal() {
        if (!is_show)
            return;

        style_start();
        if (uictx->begin(view_name.data(), {pos.x, pos.y, sz.x, sz.y}, nk_flags.data())) {
            pos = uictx->window_get_position();
            sz  = uictx->window_get_size();
            update();
        }
        else {
            if (is_show) {
                on_close();
                sig_on_close.emit_immediate();
            }
            is_show = false;
        }
        uictx->end();
        style_end();
    }

    void place_into_window(const vec2f& window_size, const vec2f& border_space = {20.f, 20.f}) {
        auto sz_x = std::max(window_size.x - border_space.x * 2.f, border_space.x);
        auto sz_y = std::max(window_size.y - border_space.y * 2.f, border_space.y);
        position(border_space);
        size({sz_x, sz_y});
    }

    void position(const vec2f& position) {
        pos = position;
        uictx->window_set_position(view_name.data(), pos);
    }

    void size(const vec2f& size) {
        sz = size;
        uictx->window_set_size(view_name.data(), sz);
    }

    [[nodiscard]]
    bool is_active() const {
        return is_show;
    }

    [[nodiscard]]
    const std::string& name() const {
        return view_name;
    }

    [[nodiscard]]
    const vec2f& position() const {
        return pos;
    }

    [[nodiscard]]
    const vec2f& size() const {
        return sz;
    }

    [[nodiscard]]
    ui_ctx& ui() const {
        return *uictx;
    }

    signal<void()> sig_on_show;
    signal<void()> sig_on_close;

private:
    ui_ctx*     uictx;
    std::string view_name;
    vec2f       pos     = {0.f, 0.f};
    vec2f       sz      = {100.f, 100.f};
    bool        is_show = false;
    ui_wnd_opt  nk_flags;
};
}
