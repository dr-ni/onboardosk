#include "tools/container_helpers.h"
#include "tools/cairocontext.h"
#include "tools/logger.h"
#include "tools/point.h"
#include "tools/rect.h"

#include "configuration.h"
#include "inputeventreceiver.h"
#include "inputsequence.h"
#include "keyboardkeylogic.h"
#include "keyboardpopup.h"
#include "keyboardview.h"
#include "layoutkey.h"
#include "layoutroot.h"
#include "onboardoskcallbacks.h"
#include "textrendererpangocairo.h"
#include "timer.h"
#include "toplevelview.h"


static constexpr const double ARROW_HEIGHT = 0.13;
static constexpr const double ARROW_WIDTH  = 0.3;
static constexpr const double LABEL_MARGIN = 0.1;


TouchFeedback::TouchFeedback(const ContextBase& context) :
    ContextBase(context)
{

}

TouchFeedback::~TouchFeedback()
{
    m_visible_key_feedback_popups.clear();
    for (auto& popup : m_key_feedback_popup_pool)
        popup->destroy();
}

void TouchFeedback::show(const LayoutKeyPtr& key, const LayoutView* view)
{
    // not already shown?
    if (!contains(m_visible_key_feedback_popups, key.get()))
    {
        Rect r = key->get_canvas_border_rect();
        Rect root_rect = view->canvas_to_root(r);

        LabelPopupPtr popup = get_free_key_feedback_popup();
        if (!popup)
        {
            size_t n = m_key_feedback_popup_pool.size();

            popup = std::make_shared<LabelPopup>(
                *this, "LabelPopup" + std::to_string(n));

            m_key_feedback_popup_pool.emplace_back(popup);
        }
        popup->set_key(key);

        Size sz = get_popup_size(view, key);
        Point pt = popup->compute_position(root_rect, sz);

        add_toplevel_view(popup.get());
        popup->notify_toplevel_added();
        popup->move_resize({pt, sz});

        m_visible_key_feedback_popups[key.get()] = popup.get();
    }
}

void TouchFeedback::hide(const LayoutKeyPtr& key)
{
    if (!key)
    {
        for (auto it : m_visible_key_feedback_popups)
        {
            do_hide(it.second);
        }
        m_visible_key_feedback_popups.clear();
    }
    else
    {
        auto popup = get_value(m_visible_key_feedback_popups, key.get());
        if (popup)
        {
            remove(m_visible_key_feedback_popups, key.get());
            do_hide(popup);
        }
    }
}

void TouchFeedback::do_hide(LabelPopup* popup)
{
    popup->destroy();
    popup->set_key(nullptr);   // mark it free to use
}

LabelPopupPtr TouchFeedback::get_free_key_feedback_popup()
{
    for (auto popup  : m_key_feedback_popup_pool)
        if (!popup->get_key())
            return popup;
    return {};
}

Size TouchFeedback::get_popup_size(const ViewBase* view_for_monitor,
                                   const LayoutKeyPtr& key)
{
    const double DEFAULT_POPUP_SIZE_MM = 18.0;
    const double MAX_POPUP_SIZE_PX = 120.0;  // fall-back if phys. monitor size unavail.

    auto root = key->get_layout_root();
    std::optional<Size> avgsz = root->get_average_key_canvas_border_size();

    double w = config()->keyboard->touch_feedback_size;
    if (w == 0.0)
    {
        Size sz;
        Size sz_mm;
        get_monitor_dimensions(view_for_monitor, sz, sz_mm);
        if (sz.w > 0 && sz_mm.w > 0)
        {
            double default_size_mm = DEFAULT_POPUP_SIZE_MM;

            // scale for hires displays
            default_size_mm *= config()->get_window_scaling_factor();

            w = sz.w * default_size_mm / sz_mm.w;
        }
        else if (avgsz)
        {
            w = avgsz.value().h * 2.0;
        }
        else if (sz.w > 0)
        {
            w = std::min(sz.w / 12.0, MAX_POPUP_SIZE_PX);
        }
        else
        {   // LP #1633284
            w = MAX_POPUP_SIZE_PX;
        }
    }

    return {w, w * (1.0 + ARROW_HEIGHT)};
}



KeyboardPopup::KeyboardPopup()
{
}

LayoutKeyPtr KeyboardPopup::get_key()
{
    return m_key;
}

void KeyboardPopup::set_key(const LayoutKeyPtr& key)
{
    m_key = key;
}

Point KeyboardPopup::compute_position(const Rect& key_root_rect, const Size& popup_size)
{
    Rect rect{{0, 0}, popup_size};
    rect = rect.align_at_point(key_root_rect.get_center_x(),
                               key_root_rect.y,
                               0.5, 1);
    rect = limit_to_workarea(rect);
    return rect.get_position();
}

Rect KeyboardPopup::limit_to_workarea(const Rect& rect)
{
    Rect visible_rect = Rect(0, 0, rect.w, rect.h);

    Point pt = get_view()->limit_position(rect.get_position(), visible_rect,
                              get_view()->get_monitor_rects());
    return {pt.x, pt.y, rect.w, rect.h};
}

double KeyboardPopup::get_opacity()
{
    return get_view()->get_keyboard_view()->get_opacity();
}

void KeyboardPopup::on_toplevel_geometry_changed()
{
    get_view()->update_geometry();
    get_view()->ViewBase::on_toplevel_geometry_changed();
}

Rect KeyboardPopup::get_always_visible_rect()
{
    return get_view()->get_rect();
}



LabelPopup::LabelPopup(const ContextBase& context,
                       const std::string& name_) :
    Super(context, ONBOARD_OSK_VIEW_TYPE_LABEL_POPUP, name_)
{

}

void LabelPopup::destroy()
{
    Super::destroy();
    set_key(nullptr);
}

void LabelPopup::draw(DrawingContext& dc)
{
    if (!m_key)
        return;

    // With clutter the draw event comes before the
    // allocation-changed event leading to outdated size
    // at drawing. Attempt to get the actual size now.
    if (!is_geometry_valid())
        update_geometry();

    double scaling_factor = config()->get_window_scaling_factor();
    double opacity = get_opacity();
    Rect rect = get_canvas_rect();
    Rect content_rect = Rect(rect.x, rect.y, rect.w,
                             rect.h - rect.h * ARROW_HEIGHT);
    Rect arrow_rect   = Rect(rect.x, content_rect.bottom(), rect.w,
                             rect.h * ARROW_HEIGHT) \
                        .deflate((rect.w - rect.w * ARROW_WIDTH) / 2.0, 0);

    Rect label_rect = content_rect.deflate(rect.w * LABEL_MARGIN);

    auto cc = dc.get_cc();

    // background
    RGBA fill = m_key->get_fill_color();

    cc->save();
    cc->set_operator(CairoContext::Operator::CLEAR);
    cc->paint();
    cc->restore();

    cc->push_group();

    cc->set_source_rgba(fill);
    cc->roundrect_arc(content_rect, config()->corner_radius);
    cc->fill();

    double l, t, r, b;
    arrow_rect.to_extents(l, t, r, b);
    t -= 1;
    cc->move_to(l, t);
    cc->line_to(r, t);
    cc->line_to((l+r) / 2, b);
    cc->fill();

    // draw label/image
    auto image = m_key->get_image(label_rect.get_size());
    if (image)
    {
        RGBA color = m_key->get_image_color();
        ImageStyle image_style = m_key->image_style;
        image->draw_styled(cc, label_rect, color, image_style,
                           scaling_factor);
    }
    else
    {
        std::string label = m_key->get_label();
        if (!label.empty())
        {
            if (label == " ")
                label = "␣";
            RGBA color = m_key->get_label_color();
            draw_text(cc, label, label_rect, color);
        }
    }

    cc->pop_group_to_source();
    cc->paint_with_alpha(opacity);
}

void LabelPopup::on_event(OnboardOskEvent* event)
{
    (void)event;
    // do nothing
}

void LabelPopup::draw_text(CairoContext* cc, const std::string& text, const Rect& rect, const RGBA& rgba)
{
    Size base_extents = calc_label_base_extents(text);
    double font_size = calc_font_size(rect, base_extents);
    auto layout = get_text_renderer(text, font_size);

    // center
    Size sz = layout->get_size();
    Offset offset = rect.align_rect(Rect({0, 0}, sz)).get_position();

    // draw
    cc->move_to(offset);
    cc->set_source_rgba(rgba);
    layout->show(cc);
}

TextRendererPangoCairo* LabelPopup::get_text_renderer(const std::string& text,
                                                      double font_size)
{
    int font_size_int = static_cast<int>(font_size);
    auto tr = Super::get_text_renderer(TextRendererSlot::DEFAULT,
                                       font_size_int);
    if (tr)
        prepare_text_layout(tr, text, font_size_int);
    return tr;
}

void LabelPopup::prepare_text_layout(TextRendererPangoCairo* layout,
                                     const std::string& text,
                                     double font_size)
{
    layout->set_text(text);
    layout->set_font(config()->get_key_label_font(), font_size);
}

Size LabelPopup::calc_label_base_extents(const std::string& label)
{
    const double BASE_FONTDESCRIPTION_SIZE = 10000000.0;
    auto layout = get_text_renderer(label, BASE_FONTDESCRIPTION_SIZE);
    prepare_text_layout(layout, label, BASE_FONTDESCRIPTION_SIZE);
    Size sz = layout->get_size();
    sz.w = std::max(sz.w, 1.0);
    sz.h = std::max(sz.h, 1.0);
    return sz / BASE_FONTDESCRIPTION_SIZE;
}

double LabelPopup::calc_font_size(const Rect& rect, const Size& base_extents)
{
    double size_for_maximum_width  = rect.w / base_extents.w;
    double size_for_maximum_height = rect.h / base_extents.h;
    if (size_for_maximum_width < size_for_maximum_height)
    {
        return int(size_for_maximum_width);
    }
    else
    {
        return int(size_for_maximum_height);
    }
}




LayoutPopup::LayoutPopup(const ContextBase& context,
                         const std::string& name_) :
    Super(context, ONBOARD_OSK_VIEW_TYPE_LAYOUT_POPUP, name_),
    m_unpress_timer(std::make_unique<Timer>(*this))
{
}

LayoutPopup::~LayoutPopup()
{
    // fix label popup staying visible on double click
    Super::get_keyboard_view()->hide_touch_feedback();
}

void LayoutPopup::set_layout(const LayoutItemPtr& layout, double frame_width)
{
    set_layout_subtree(layout);

    m_frame_width = frame_width;

    auto keyboard = Super::get_keyboard();
    keyboard->update_labels(get_layout());
}

void LayoutPopup::set_close_callback(const std::function<void(LayoutPopup*)>& func)
{
    m_close_callback = func;
}

double LayoutPopup::get_frame_width()
{
    return m_frame_width;
}

Size LayoutPopup::compute_size()
{
    Rect layout_canvas_rect = get_layout()->get_canvas_border_rect();
    Rect canvas_rect = layout_canvas_rect.inflate(get_frame_width());
    return canvas_rect.get_size();
}

bool LayoutPopup::got_motion()
{
    return m_drag_selected;
}

void LayoutPopup::on_input_sequence_begin(const InputSequencePtr& sequence)
{
    auto keyboard = get_keyboard();
    auto key_logic = Super::get_key_logic();

    LayoutKeyPtr key = get_key_at_location(sequence->point);
    if (key)
    {
        sequence->active_key = key;
        key_logic->key_down(key, this, sequence);
    }

    // Process pending UI updates
    keyboard->commit_ui_updates();
}

void LayoutPopup::on_input_sequence_update(const InputSequencePtr& sequence)
{
    auto keyboard = get_keyboard();
    auto key_logic = Super::get_key_logic();

    if (sequence->state & ONBOARD_OSK_BUTTON123_MASK)
    {
        LayoutKeyPtr key = get_key_at_location(sequence->point);

        // drag-select new active key
        LayoutKeyPtr active_key = sequence->active_key;
        if (active_key != key &&
           (active_key == nullptr || !active_key->activated))
        {
            sequence->active_key = key;
            key_logic->key_up(active_key, this, sequence, false);
            key_logic->key_down(key, this, sequence, false);
            m_drag_selected = true;
        }
    }

    // Process pending UI updates
    keyboard->commit_ui_updates();
}

void LayoutPopup::on_input_sequence_end(const InputSequencePtr& sequence)
{
    auto keyboard = get_keyboard();
    auto key_logic = get_key_logic();

    LayoutKeyPtr key = sequence->active_key;
    if (key)
        key_logic->key_up(key, this, sequence);

    // Process pending UI updates
    keyboard->commit_ui_updates();

    // Always wait with calling on_popup_done a little because it
    // is going to delete this object.
    double close_delay = (key && !m_drag_selected) ?
                             Super::config()->unpress_delay : 0.02;

    m_unpress_timer->start(close_delay,
                           [this]{on_popup_done(); return false;});
}

void LayoutPopup::draw(DrawingContext& dc)
{
    // Again, with clutter the draw event comes before the
    // allocation-changed event leading to outdated size
    // at drawing. Attempt to get the actual size now.
    if (!is_geometry_valid())
        update_geometry();

    auto cc = dc.get_cc();
    cc->push_group();

    Super::draw(dc);

    cc->pop_group_to_source();
    cc->paint_with_alpha(get_opacity());
}

void LayoutPopup::draw_view_frame(DrawingContext& dc)
{
    double corner_radius = Super::config()->corner_radius;
    RGBA border_rgba = get_popup_window_rgba("border");
    double alpha = border_rgba.a;

    struct Iteration {RGBA rgba; double pos; double width;};
    auto iterations = array_of<Iteration>(
         Iteration{{0.5, 0.5, 0.5, alpha}, 0.0, 1.0},
         Iteration{border_rgba,            1.5, 2.0}
        );

    Rect rect = Super::get_canvas_rect();

    auto cc = dc.get_cc();

    for (auto& it : iterations)
    {
        Rect r = rect.deflate(it.width + 1.0);
        cc->roundrect_arc(r, corner_radius);
        cc->set_line_width(it.width);
        cc->set_source_rgba(it.rgba);
        cc->stroke();
    }
}

void LayoutPopup::on_popup_done()
{
    if (m_close_callback)
        m_close_callback(this);
}



LayoutBuilder::LayoutBuilder(const ContextBase& context) :
    Super(context)
{

}

std::tuple<LayoutRootPtr, double> LayoutBuilder::build(
        const LayoutKeyPtr& source_key,
        const LayoutItemPtr& layout)
{
    LayoutContext* context = source_key->get_context();

    double frame_width = calc_frame_width(context);

    LayoutRootPtr root = LayoutRoot::make(*this);
    root->append_child(layout);

    root->update_log_rects();
    Rect log_rect = root->get_border_rect();
    Rect canvas_rect{frame_width, frame_width,
                     log_rect.w * context->scale_log_to_canvas_x(1.0),
                     log_rect.h * context->scale_log_to_canvas_y(1.0)};
    root->fit_inside_canvas(canvas_rect);

    return {root, frame_width};
}

double LayoutBuilder::calc_frame_width(LayoutContext* context)
{
    // calculate border around the layout
    Size canvas_border = context->scale_log_to_canvas(Size{1, 1});
    return config()->popup_frame_width + canvas_border.min();
}


const size_t MAX_KEY_COLUMNS  = 8;  // max number of keys in one row

LayoutBuilderKeySequence::LayoutBuilderKeySequence(const ContextBase& context) :
    Super(context)
{

}

std::tuple<LayoutRootPtr, double> LayoutBuilderKeySequence::build(
        const LayoutKeyPtr& source_key,
        const std::vector<LayoutKeyPtr>& key_sequence)
{
    // parse sequence into lines
    KeyLines lines;
    size_t ncolumns = layout_sequence(lines, key_sequence);
    return create_layout(source_key, lines, ncolumns);
}

std::tuple<LayoutRootPtr, double> LayoutBuilderKeySequence::create_layout(const LayoutKeyPtr& source_key,
        const KeyLines& lines, size_t ncolumns)
{
    LayoutContext* context = source_key->get_context();
    double frame_width = calc_frame_width(context);

    size_t nrows = lines.size();
    Size spacing{1, 1};
    Size canvas_spacing = context->scale_log_to_canvas(spacing);

    // compute canvas size
    Rect rect = source_key->get_canvas_border_rect();
    Rect layout_canvas_rect{
        frame_width, frame_width,
        rect.w * ncolumns + canvas_spacing.w * (ncolumns - 1),
        rect.h * nrows + canvas_spacing.h * (nrows - 1)};

    // subdivide into logical rectangles for the keys
    Rect layout_rect = context->canvas_to_log_rect(layout_canvas_rect);
    auto key_rects = layout_rect.subdivide(ncolumns, nrows,
                                           spacing.w, spacing.h);

    // create keys, slots for empty labels are skipped
    std::vector<LayoutKeyPtr> keys;
    for (size_t i=0; i<lines.size(); i++)
    {
        auto& line = lines[i];
        for (size_t j=0; j<line.size(); j++)
        {
            auto& item = line[j];
            size_t slot = i * ncolumns + j;
            if (item)
            {
                // control item?
                auto key = item;
                key->set_border_rect(key_rects[slot]);
                key->group = "alternatives";
                key->m_color_scheme = config()->current_color_scheme();
                keys.emplace_back(key);
            }
        }
    }

    LayoutPanelPtr panel = LayoutPanel::make(*this);
    panel->append_children(keys);
    LayoutRootPtr root = LayoutRoot::make(*this);
    root->append_child(panel);

    root->fit_inside_canvas(layout_canvas_rect);

    return {root, frame_width};
}

size_t LayoutBuilderKeySequence::layout_sequence(
        KeyLines& lines, const KeySequence& sequence)
{
    size_t max_columns = MAX_KEY_COLUMNS;
    size_t min_columns = max_columns / 2;
    bool add_close = false;
    bool fill_gaps = true;

    // find the number of columns with the best packing,
    // i.e. the least number of empty slots.
    size_t n = sequence.size();
    if (add_close)
        n += 1;    // +1 for close button

    size_t max_mod = 0;
    size_t ncolumns = max_columns;
    for (size_t i=max_columns; i>min_columns; i--)
    {
        size_t m = n % i;
        if (m == 0)
        {
            max_mod = m;
            ncolumns = i;
            break;
        }
        if (max_mod < m)
        {
            max_mod = m;
            ncolumns = i;
        }
    }

    // limit to size for the single row case
    ncolumns = std::min(n, ncolumns);

    // cut the input into lines of the newly found optimal length
    std::vector<LayoutKeyPtr> line;
    size_t column = 0;
    for (auto item : sequence)
    {
        line.emplace_back(item);
        column += 1;
        if (column >= ncolumns)
        {
            lines.emplace_back(line);
            line.clear();
            column = 0;
        }
    }

    // append close button
    if (add_close)
    {
        size_t n_ = line.size();
        extend(line, KeySequence(ncolumns - (n_+1)));

        LayoutKeyPtr key = LayoutKey::make(*this);
        key->set_id("_close_");
        key->labels = {};
        key->image_filenames = {{ImageSlot::NORMAL,  "close.svg"}};
        key->type = KeyType::BUTTON;
        line.emplace_back(key);
    }

    // fill empty slots with insensitive dummy buttons
    if (fill_gaps)
    {
        size_t n_ = line.size();
        if (n_)
        {
            for (size_t i=0; i < ncolumns - n_; ++i)
            {
                LayoutKeyPtr key = LayoutKey::make(*this);
                key->set_id("_dummy_");
                key->sensitive = false;
                line.emplace_back(key);
            }
        }
    }

    if (!line.empty())
        lines.emplace_back(line);

    return ncolumns;
}



LayoutBuilderAlternatives::LayoutBuilderAlternatives(
        const ContextBase& context) :
    Super(context)
{
}

std::tuple<LayoutRootPtr, double> LayoutBuilderAlternatives::build(
        const LayoutKeyPtr& source_key,
        const std::vector<std::string>& alternatives)
{
    KeySequence key_sequence;
    for (size_t i=0; i<alternatives.size(); ++i)
    {
        auto& label = alternatives[i];

        LayoutKeyPtr key = LayoutKey::make(*this);
        key->set_id("_alternative" + std::to_string(i));
        key->type = KeyType::CHAR;
        key->labels = {{0, label}};
        key->keystr = label;
        key_sequence.emplace_back(key);
    }

    return Super::build(source_key, key_sequence);
}
