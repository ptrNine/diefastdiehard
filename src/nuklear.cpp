#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SFML_GL3_IMPLEMENTATION

#include <SFML/Window.hpp>
#include <nuklear.h>
#include "nuklear_sfml_gl3.h"

NK_LIB void nk_draw_selectable_icon(struct nk_command_buffer*         out,
                                    nk_flags                          state,
                                    const struct nk_style_selectable* style,
                                    nk_bool                           active,
                                    const struct nk_rect*             bounds,
                                    const struct nk_rect*             icon,
                                    const struct nk_image*            img) {
    const struct nk_style_item* background;

    /* select correct colors/images */
    if (!active) {
        if (state & NK_WIDGET_STATE_ACTIVED)
            background = &style->pressed;
        else if (state & NK_WIDGET_STATE_HOVER)
            background = &style->hover;
        else
            background = &style->normal;
    }
    else {
        if (state & NK_WIDGET_STATE_ACTIVED)
            background = &style->pressed_active;
        else if (state & NK_WIDGET_STATE_HOVER)
            background = &style->hover_active;
        else
            background = &style->normal_active;
    }
    /* draw selectable background and text */
    if (background->type == NK_STYLE_ITEM_IMAGE)
        nk_draw_image(out, *bounds, &background->data.image, nk_white);
    else
        nk_fill_rect(out, *bounds, style->rounding, background->data.color);
    if (icon)
        nk_draw_image(out, *icon, img, nk_white);
}

NK_LIB nk_bool nk_do_selectable_icon(nk_flags*                         state,
                                     struct nk_command_buffer*         out,
                                     struct nk_rect                    bounds,
                                     nk_bool*                          value,
                                     const struct nk_image*            img,
                                     const struct nk_style_selectable* style,
                                     const struct nk_input*            in) {
    nk_bool        old_value;
    struct nk_rect touch;
    struct nk_rect icon;

    NK_ASSERT(state);
    NK_ASSERT(out);
    NK_ASSERT(value);
    NK_ASSERT(style);

    if (!state || !out || !value || !style) return 0;
    old_value = *value;

    /* toggle behavior */
    touch.x = bounds.x - style->touch_padding.x;
    touch.y = bounds.y - style->touch_padding.y;
    touch.w = bounds.w + style->touch_padding.x * 2;
    touch.h = bounds.h + style->touch_padding.y * 2;
    if (nk_button_behavior(state, touch, in, NK_BUTTON_DEFAULT))
        *value = !(*value);

    icon.x = bounds.x + 2 * style->padding.x;
    icon.y = bounds.y + style->padding.y;
    icon.w = bounds.w - 2 * style->padding.x;
    icon.h = bounds.h - 2 * style->padding.y;

    icon.x += style->image_padding.x;
    icon.y += style->image_padding.y;
    icon.w -= 2 * style->image_padding.x;
    icon.h -= 2 * style->image_padding.y;

    /* draw selectable */
    if (style->draw_begin) style->draw_begin(out, style->userdata);
    nk_draw_selectable_icon(out, *state, style, *value, &bounds, &icon, img);
    if (style->draw_end) style->draw_end(out, style->userdata);
    return old_value != *value;
}

NK_API nk_bool nk_selectable_image(struct nk_context* ctx,
                                   struct nk_image    img,
                                   nk_bool*           value) {
    struct nk_window*      win;
    struct nk_panel*       layout;
    const struct nk_input* in;
    const struct nk_style* style;

    enum nk_widget_layout_states state;
    struct nk_rect               bounds;

    NK_ASSERT(ctx);
    NK_ASSERT(value);
    NK_ASSERT(ctx->current);
    NK_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout || !value)
        return 0;

    win    = ctx->current;
    layout = win->layout;
    style  = &ctx->style;

    state = nk_widget(&bounds, ctx);
    if (!state)
        return 0;
    in = (state == NK_WIDGET_ROM || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
    return nk_do_selectable_icon(&ctx->last_widget_state,
                                 &win->buffer,
                                 bounds,
                                 value,
                                 &img,
                                 &style->selectable,
                                 in);
}
