#include <math.h>

#include "tools/cairocontext.h"
#include "tools/logger.h"
#include "tools/rect.h"

#include "colorscheme.h"
#include "configuration.h"
#include "drawingcontext.h"
#include "inputeventreceiver.h"
#include "inputsequence.h"
#include "keyboard.h"
#include "keyboardpopup.h"
#include "keyboardview.h"
#include "layoutkey.h"
#include "layoutroot.h"
#include "layoutview.h"
#include "onboardoskcallbacks.h"
#include "theme.h"

using std::string;

LayoutView::LayoutView(const ContextBase& context,
                       OnboardOskViewType view_type_,
                       const std::string& name_) :
    Super(context, view_type_, name_),
    m_input_event_receiver(InputEventReceiver::make(context, this))
{
}

void LayoutView::on_layout_loaded()
{
    invalidate_shadow_quality();
}

void LayoutView::reset_lod()
{
    if (get_lod() != LOD::FULL)
    {
        m_lod = LOD::FULL;
        auto keyboard = get_keyboard();
        keyboard->invalidate_context_ui();
        keyboard->invalidate_canvas();
        //keyboard->commit_ui_updates();
    }
}

void LayoutView::invalidate_for_resize()
{
    auto layout = get_layout_root(get_layout());
    if (layout)
    {
        invalidate_keys();
        if (get_lod() == LOD::FULL)
            invalidate_shadows();
        // layout->invalidate_font_sizes();
        // invalidate_label_extents();
        //get_keyboard()->invalidate_for_resize();
    }
}

void LayoutView::invalidate_keys()
{
    auto layout = get_layout();
    if (layout)
    {
        layout->for_each_key([](const LayoutItemPtr& item)
        {
            LayoutKey* key = dynamic_cast<LayoutKey*>(item.get());
            key->invalidate_key();
        });
    }
}

void LayoutView::invalidate_images()
{
    auto layout = get_layout();
    if (layout)
    {
        layout->for_each_key([](const LayoutItemPtr& item)
        {
            LayoutKey* key = dynamic_cast<LayoutKey*>(item.get());
            key->invalidate_image();
        });
    }
}

void LayoutView::invalidate_shadows()
{
    auto layout = get_layout();
    if (layout)
    {
        layout->for_each_key([](const LayoutItemPtr& item)
        {
            LayoutKey* key = dynamic_cast<LayoutKey*>(item.get());
            key->invalidate_shadow();
        });
    }
}

void LayoutView::invalidate_label_extents()
{
    auto layout = get_layout();
    if (layout)
    {
        layout->for_each_key([](const LayoutItemPtr& item)
        {
            LayoutKey* key = dynamic_cast<LayoutKey*>(item.get());
            key->invalidate_label_extents();
        });
    }
}

void LayoutView::redraw_items(const vector<LayoutItemPtr> items, bool invalidate)
{
    if (items.empty())
        return;

    auto layout = get_layout();

    Noneable<Rect> area;
    for (auto item  : items)
    {
        // Is item part of this view's layout subtree?
        if (layout->is_parent_of(item))
        {
            Rect rect = item->get_canvas_border_rect();
            if (area.is_none())
                area = rect;
            else
                area = rect.union_(area);

            // assume keys need to be refreshed when actively redrawn
            // e.g. for pressed state changes, dwell progress updates...
            if (invalidate &&
                item->is_key())
            {
                auto key = std::dynamic_pointer_cast<LayoutKey>(item);
                key->invalidate_key();
            }
        }
    }

    // account for stroke width, anti-aliasing
    if (this->get_layout())
    {
        Size extra_size = items[0]->get_extra_render_size();
        area = area.value.inflate(extra_size);
    }

    redraw_area(area);
}

void LayoutView::process_updates()
{
    auto callbacks = get_global_callbacks();
    if (callbacks->view_process_updates)
        callbacks->view_process_updates(this);
}

void LayoutView::draw(DrawingContext& dc)
{
    auto layout = get_layout();
    if (!layout)
        return;

    BackgroundStyle bgstyle = get_background_style();
    auto layer_ids = layout->get_layer_ids();

    dc.lod = get_lod();
    dc.draw_rect = get_damage_rect(dc);
    dc.draw_cached = false;
    dc.draw_layer_background_func = [&](DrawingContext& dc_, LayoutItemPtr& item) {
        draw_layer_background(dc_, item, layer_ids);
    };

    // draw background
    draw_background(dc, bgstyle);

    // draw layer 0 && None-layer background
    double alpha;
    if (config()->window->transparent_background)
    {
        alpha = 0.0;
    }
    else
    if (is_decorated())
    {
        alpha = get_background_rgba().a;
    }
    else
    {
        alpha = 1.0;
    }
    draw_layer_key_background(dc, alpha);
    if (!layer_ids.empty())
    {
        this->draw_layer_key_background(dc, alpha, {}, layer_ids[0]);
    }

    // draw all visible layout items
    layout->draw_tree(dc);
}

void LayoutView::show_popup_layout(const LayoutKeyPtr& key,
                                   const LayoutItemPtr& sublayout)
{
    LayoutPopupPtr popup = create_key_popup();

    LayoutBuilder builder(*this);
    LayoutRootPtr layout;
    double frame_width;
    std::tie(layout, frame_width) = builder.build(key, sublayout);
    popup->set_layout(layout, frame_width);

    show_key_popup(popup, key);
}

void LayoutView::show_popup_alternative_chars(
        const LayoutKeyPtr& key,
        const std::vector<std::string>& alternatives)
{
    LayoutPopupPtr popup = create_key_popup();

    LayoutBuilderAlternatives builder(*this);
    LayoutRootPtr layout;
    double frame_width;
    std::tie(layout, frame_width) = builder.build(key, alternatives);
    popup->set_layout(layout, frame_width);

    show_key_popup(popup, key);
}

LayoutPopupPtr LayoutView::create_key_popup()
{
    auto popup = std::make_shared<LayoutPopup>(
            *this, "LayoutPopup: alternatives");
    popup->set_originating_view(getptr());
    popup->set_close_callback([&](LayoutPopup*)
            {get_keyboard_view()->close_key_popup();});
    return popup;
}

void LayoutView::show_key_popup(const LayoutPopupPtr& popup,
                                const LayoutKeyPtr& key)
{
    auto keyboard_view = get_keyboard_view();

    Rect r = key->get_canvas_border_rect();
    Rect root_rect = canvas_to_root(r);
    Size sz = popup->compute_size();
    Point pt = popup->compute_position(root_rect, sz);

    popup->set_key(key);  // for unpressing the originating key

    keyboard_view->close_key_popup();  // just to be sure

    add_toplevel_view(popup.get());
    popup->notify_toplevel_added();
    popup->move_resize({pt, sz});

    keyboard_view->set_key_popup(popup);
    keyboard_view->hide_touch_feedback();
}

BackgroundStyle LayoutView::get_background_style()
{
    BackgroundStyle bgstyle = BackgroundStyle::CLEAR;
    if (config()->is_keep_xembed_frame_aspect_ratio_enabled())
    {
        if (supports_alpha())
            bgstyle = BackgroundStyle::TRANSPARENT_XEMBED;
        else
            bgstyle = BackgroundStyle::OPAQUE;
    }

    else
    if (config()->is_xid_mode())
    {
        bgstyle = BackgroundStyle::OPAQUE;
    }

    else
    if (config()->has_window_decoration())
    {
        // decorated window
        if (supports_alpha() &&
           config()->window->transparent_background)
            bgstyle = BackgroundStyle::CLEAR;
        else
            bgstyle = BackgroundStyle::OPAQUE;
    }

    else
    {
        // undecorated window
        if (supports_alpha())
        {
            if (config()->window->transparent_background)
                bgstyle = BackgroundStyle::CLEAR;
            else
                bgstyle = BackgroundStyle::TRANSPARENT;
        }
        else
        {
            bgstyle = BackgroundStyle::OPAQUE;
        }
    }
    return bgstyle;
}

// Draw keyboard background
void LayoutView::draw_background(DrawingContext& dc, BackgroundStyle bgstyle)
{
    switch (bgstyle)
    {
        case BackgroundStyle::CLEAR:
            clear_background(dc);
            break;

        case BackgroundStyle::OPAQUE:
            draw_opaque_background(dc);
            draw_xembed_background(dc);
            break;

        case BackgroundStyle::TRANSPARENT:
            draw_transparent_background(dc);
            break;

        case BackgroundStyle::TRANSPARENT_XEMBED:
            draw_xembed_background(dc);
            break;

    }
}

// Clear the whole gtk background.
// Makes the whole strut transparent in xembed mode.
void LayoutView::clear_background(DrawingContext& dc)
{
    auto cr = dc.get_cc();
    cr->save();
    cr->set_operator(CairoContext::Operator::CLEAR);
    cr->paint();
    cr->restore();
}

// fill with plain layer 0 color; no alpha support required
void LayoutView::draw_opaque_background(DrawingContext& dc, size_t layer_index)
{
    auto rgba = get_layer_fill_rgba(layer_index);
    auto cr = dc.get_cc();
    cr->set_source_rgba(rgba);
    cr->paint();
}

void LayoutView::draw_layer_background(DrawingContext& dc, LayoutItemPtr& item, const vector<string>& layer_ids)
{
    size_t layer_index = find_index(layer_ids, item->layer_id);
    auto parent = item->get_parent();
    if (parent && \
       layer_index != 0)
    {
        auto cr = dc.get_cc();
        Rect rect = parent->get_canvas_rect();
        cr->rectangle(rect.inflate(1));

        RGBA rgba;
        auto color_scheme = config()->current_color_scheme();
        if (color_scheme)
        {
            rgba = color_scheme->get_layer_fill_rgba(layer_index);
        }
        else
        {
            rgba = {0.5, 0.5, 0.5, 0.9};
        }
        cr->set_source_rgba(rgba);
        cr->fill();

        // per-layer key background
        draw_layer_key_background(dc, 1.0, item, item->layer_id);
    }
}

// fill background and decorations with transparent colors
void LayoutView::draw_transparent_background(DrawingContext& dc)
{
    double corner_radius = config()->corner_radius;
    Rect rect = get_view_frame_rect();
    RGBA fill = this->get_background_rgba();
    auto theme = config()->current_theme();

    if (can_draw_sidebars())
        draw_side_bars(dc);

    auto cr = dc.get_cc();

    double fill_gradient = theme->background_gradient;
    if (dc.lod == LOD::MINIMAL ||
        fill_gradient == 0.0)
    {
        cr->set_source_rgba(fill);
    }
    else
    {
        fill_gradient /= 100.0;
        double direction = theme->key_gradient_direction;
        double alpha = -M_PI/2.0 + M_PI * direction / 180.0;
        Line line = cr->gradient_line(rect, alpha);

        LinearGradient pat(cr, line);
        RGBA rgba = fill.brighten(+fill_gradient * 0.5);
        pat.add_color_stop_rgba(0, rgba);
        rgba = fill.brighten(-fill_gradient * 0.5);
        pat.add_color_stop_rgba(1, rgba);
        cr->set_source(pat);
    }

    bool needs_frame = false;
    if (config()->is_xid_mode())
        needs_frame = false;
    else
        needs_frame = can_draw_frame();

    if (needs_frame)
        cr->roundrect_arc(rect, corner_radius);
    else
        cr->rectangle(rect);

    cr->fill();

    if (needs_frame)
    {
        draw_keyboard_frame(dc);
        draw_view_frame(dc);
    }
}

// Transparent bars left and right of the aspect corrected
// keyboard frame.
void LayoutView::draw_side_bars(DrawingContext& dc)
{
    RGBA rgba = get_background_rgba();
    rgba.a = 0.4;
    Rect rwin = get_canvas_rect();
    Rect rframe = get_view_frame_rect();

    if (rwin.w > rframe.w)
    {
        auto cr = dc.get_cc();

        Rect r = rframe;
        cr->set_source_rgba(rgba);
        cr->set_line_width(0);

        r.x = rwin.left();
        r.w = rframe.left() - rwin.left();
        cr->rectangle(r);
        cr->fill();

        r.x = rframe.right();
        r.w = rwin.right() - rframe.right();
        cr->rectangle(r);
        cr->fill();
    }
}

// overloaded in KeyboardWidget
bool LayoutView::can_draw_frame()
{
    return !config()->is_dock_expanded();
}

// overloaded in KeyboardWidget
bool LayoutView::can_draw_sidebars()
{
    return config()->is_keep_docking_frame_aspect_ratio_enabled();
}

// overloaded in LayoutPopup
void LayoutView::draw_view_frame(DrawingContext& dc)
{
    (void)dc;
}

// draw frame around the (potentially aspect corrected) keyboard
void LayoutView::draw_keyboard_frame(DrawingContext& dc)
{
    double corner_radius = config()->corner_radius;
    Rect rect = get_view_frame_rect();
    RGBA fill = get_background_rgba();
    RGBA rgba = fill.brighten(0.3);

    // inner decoration line
    Rect line_rect = rect.deflate(2);
    auto cr = dc.get_cc();
    cr->set_source_rgba(rgba);
    cr->roundrect_arc(line_rect, corner_radius);
    cr->set_line_width(1.0);
    cr->stroke();
}

// fill with plain layer 0 color; no alpha support required
void LayoutView::draw_xembed_background(DrawingContext& dc)
{
    (void)dc;
    /*
    Rect rect = get_rect();
    // draw background image
    if (config()->get_xembed_background_image_enabled())
    {
        pixbuf = this->_get_xembed_background_image();
        if (pixbuf)
        {
            src_size = (pixbuf.get_width(), pixbuf.get_height());
            x, y = 0, rect.bottom() - src_size[1];
            Gdk.cairo_set_source_pixbuf(context, pixbuf, x, y);
            cr->paint();
        }
    }
    // draw solid colored bar on top (with transparency, usually)
    rgba = config.get_xembed_background_rgba();
    if (rgba is None)
    {
        rgba = this->get_background_rgba();
        rgba[3] = 0.5;
    }
    cr->set_source_rgba(*rgba);
    cr->rectangle(*rect);
    cr->fill();
    */
}

void LayoutView::draw_layer_key_background(DrawingContext& dc, double alpha,
                                           LayoutItemPtr item, string layer_id)
{
    // TODO
    (void)dc;
    (void)alpha;
    (void)item;
    (void)layer_id;
    //draw_dish_key_background(context, alpha, item, layer_id);
    //draw_shadows(context, layer_id, lod);
}

RGBA LayoutView::get_layer_fill_rgba(size_t layer_index)
{
    auto color_scheme = config()->current_color_scheme();
    if (color_scheme)
        return color_scheme->get_layer_fill_rgba(layer_index);
    else
        return RGBA(0.5, 0.5, 0.5, 1.0);
}

// layer 0 color * background_transparency
RGBA LayoutView::get_background_rgba()
{
    auto layer0_rgba = get_layer_fill_rgba(0);
    double background_alpha = config()->window->get_background_opacity();
    layer0_rgba.a *= background_alpha;
    return layer0_rgba;
}

RGBA LayoutView::get_popup_window_rgba(const std::string& element = "border")
{
    RGBA rgba;
    auto color_scheme = config()->current_color_scheme();
    if (color_scheme)
    {
        rgba = color_scheme->get_window_rgba("key-popup", element);
    }
    else
    {
        rgba = {0.8, 0.8, 0.8, 1.0};
    }
    double background_alpha = config()->window->get_background_opacity();
    rgba.a *= background_alpha;
    return rgba;
}

Rect LayoutView::get_damage_rect(DrawingContext& dc)
{
    Rect clip_rect = dc.get_cc()->get_clip_rect();

    // Draw a little more than just the clip_rect.
    // Prevents glitches around pressed keys in at least classic theme.
    Size extra_size;
    auto layout = this->get_layout();
    if (layout)
        extra_size = layout->get_context()->scale_log_to_canvas(Size(2.0, 2.0));
    return clip_rect.inflate(extra_size);
}

// Rectangle of the potentially aspect-corrected
// frame around the layout.
Rect LayoutView::get_view_frame_rect()
{
    Rect rect;
    auto layout = get_layout();
    if (layout)
    {
        rect = layout->get_canvas_border_rect().round();
        rect = rect.inflate(get_keyboard_view()->get_frame_width());
    }
    else
    {
        rect = get_canvas_rect();
    }
    return rect.floor();
}

void LayoutView::on_event(OnboardOskEvent* event)
{
    auto ir = get_input_event_receiver();
    ir->on_event(event);
}

// Canvas rect excluding resize frame
Rect LayoutView::get_canvas_content_rect()
{
    return get_canvas_rect().deflate(get_keyboard_view()->get_frame_width());
}

// Rect with aspect ratio of the layout as defined in the SVG file
Rect LayoutView::get_base_aspect_rect()
{
    auto layout = this->get_layout();
    if (!layout)
        return {0, 0, 1.0, 1.0};
    return layout->get_border_rect();
}

// Aspect correction specifically targets xembedding in unity-greeter
// and gnome-screen-saver. Else we would potentially disrupt embedding
// in existing kiosk applications.
Rect LayoutView::get_aspect_corrected_layout_rect(const Rect& rect, const Rect& base_aspect_rect)
{
    Rect result = rect;
    bool keep_aspect = config()->is_keep_frame_aspect_ratio_enabled();
    bool xembedding = config()->is_xid_mode();
    bool unity_greeter = config()->launched_by == Launcher::UNITY_GREETER;

    double x_align = 0.5;

    AspectChangeRange aspect_change_range = {0, 100};

    if (keep_aspect)
    {
        if (xembedding)
        {
            aspect_change_range = config()->xembed_aspect_change_range;
        }
        else
        if (config()->is_docking_enabled() &&
            config()->is_dock_expanded())
        {
            aspect_change_range = get_docking_aspect_change_range();
        }

        Rect ra = rect.resize_to_aspect_range(base_aspect_rect,
                                              aspect_change_range.min_aspect,
                                              aspect_change_range.max_aspect);
        if (xembedding &&
            unity_greeter)
        {
            double padding = result.w - ra.w;
            double offset = config()->xembed_unity_greeter_offset_x;
            // Attempt to left align to unity-greeters password box,
            // but use the whole width on small screens.
            if (offset >= 0.0 &&
                padding > 2 * offset)
            {
                result.x += offset;
                result.w -= offset;
                x_align = 0.0;
            }
        }

        result = result.align_rect(ra, x_align);
    }

    return result;
}

// recalculate item rectangles
void LayoutView::update_layout()
{
    auto layout = get_layout();
    if (!layout)
        return;

    Rect canvas_rect = get_canvas_content_rect();

    layout->update_log_rects();  // update logical tree to base aspect ratio
    Rect rect = get_aspect_corrected_layout_rect(canvas_rect, get_base_aspect_rect());
    layout->do_fit_inside_canvas(rect);  // update contexts to final aspect

//    layout->dump_tree(std::cout);
}

LayoutKeyPtr LayoutView::get_key_at_location(const Point& point)
{
    auto layout = get_layout();
    auto keyboard = get_keyboard();
    if (layout)  // may be gone on exit
    {
        auto item = layout->get_item_at(point, keyboard->get_active_layer_ids());
        if (item)
            return std::dynamic_pointer_cast<LayoutKey>(item);
    }
    return {};
}

bool LayoutView::has_input_sequences()
{
    return m_input_event_receiver->has_input_sequences();
}

void LayoutView::on_input_sequence_update(const InputSequencePtr& sequence)
{
    m_last_sequence_point = sequence->point;
}

LayoutView* LayoutView::get_originating_view()
{
    if (m_originating_view)
        return m_originating_view.get();
    return this;
}

void LayoutView::set_originating_view(const LayoutViewPtr& view)
{
    m_originating_view = view;
}


