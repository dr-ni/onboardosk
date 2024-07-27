
#include "tools/logger.h"
#include "tools/noneable.h"

#include "configuration.h"
#include "handle.h"
#include "inputsequence.h"
#include "inputeventreceiver.h"
#include "keyboard.h"
#include "keyboardkeylogic.h"
#include "keyboardpopup.h"
#include "keyboardview.h"
#include "layoutkey.h"
#include "layoutviewkeyboard.h"
#include "onboardoskcallbacks.h"
#include "touchhandles.h"
#include "viewmanipulator.h"


// Adds support for handles with function ASPECT_RATIO.
class ViewManipulatorAspectRatio : public ViewManipulator
{
    public:
        using Super = ViewManipulator;

        ViewManipulatorAspectRatio(const ContextBase& context, LayoutViewKeyboard* view) :
            ViewManipulator(context, view),
            m_view(view)
        {
            m_docking_aspect_change_range =
                config()->window->docking_aspect_change_range;
        }

        // GSettings key changed
        void update_docking_aspect_change_range()
        {
            AspectChangeRange value = config()->window->docking_aspect_change_range;
            if (m_docking_aspect_change_range != value)
            {
                m_docking_aspect_change_range = value;
                auto keyboard = get_keyboard();
                keyboard->invalidate_ui();
                keyboard->commit_ui_updates();
            }
        }

        AspectChangeRange& get_docking_aspect_change_range()
        {
            return m_docking_aspect_change_range;
        }

        virtual void on_drag_done() override
        {
            config()->window->docking_aspect_change_range =
                m_docking_aspect_change_range;

            Super::on_drag_done();
        }

        virtual void on_handle_aspect_ratio_pressed() override
        {
            m_drag_start_keyboard_frame_rect = m_view->get_view_frame_rect();
        }

        virtual void on_handle_aspect_ratio_motion(const Offset& delta) override
        {
            Rect keyboard_frame_rect = m_drag_start_keyboard_frame_rect;

            Rect base_aspect_rect = m_view->get_base_aspect_rect();
            double base_aspect = base_aspect_rect.w / base_aspect_rect.h;
            double start_frame_width = m_drag_start_keyboard_frame_rect.w;
            double new_frame_width = start_frame_width + delta.x * 2;

            // snap to screen sides
            double new_aspect_change;
            if (new_frame_width >= m_view->get_rect().w * (1.0 - 0.05))
            {
                new_aspect_change = 100.0;
            }
            else
            {
                new_aspect_change = \
                    new_frame_width / (keyboard_frame_rect.h * base_aspect);
            }

            // limit to minimum combined aspect
            double min_aspect = 0.75;
            double new_aspect = base_aspect * new_aspect_change;
            if (new_aspect < min_aspect)
                new_aspect_change = min_aspect / base_aspect;

            m_docking_aspect_change_range =
                {m_docking_aspect_change_range.min_aspect, new_aspect_change};

            auto keyboard = get_keyboard();
            keyboard->invalidate_size();
            keyboard->invalidate_canvas();
            //keyboard->commit_ui_updates();
        }

    private:
        LayoutViewKeyboard* m_view;
        AspectChangeRange m_docking_aspect_change_range;
        Rect m_drag_start_keyboard_frame_rect;
};


std::unique_ptr<LayoutViewKeyboard> LayoutViewKeyboard::make(
        const ContextBase& context,
        const std::string& name)
{
    return std::make_unique<This>(context, name);
}

LayoutViewKeyboard::LayoutViewKeyboard(const ContextBase& context,
                                       const std::string& name_) :
    Super(context, ONBOARD_OSK_VIEW_TYPE_KEYBOARD, name_),
    m_view_manipulator(std::make_unique<ViewManipulatorAspectRatio>(context, this)),
    m_touch_handles(std::make_unique<TouchHandles>(context))
{
    m_touch_handles->set_view(this);
    update_window_handles();
    update_double_click_time();
}

LayoutViewKeyboard::~LayoutViewKeyboard()
{}

ViewManipulator* LayoutViewKeyboard::get_view_manipulator()
{
    return m_view_manipulator.get();
}

void LayoutViewKeyboard::on_transition_done(bool visible_before, bool visible_now)
{
    (void)visible_before;

    if (visible_now)
        assure_on_current_desktop();
}

// Make sure the window is visible in the current desktop in OpenBox and
// perhaps other WMs, except Compiz/Unity. Dbus Show() then makes Onboard
// appear after switching desktop (LP: 1092166, Raring).
void LayoutViewKeyboard::assure_on_current_desktop()
{
    if (m_moved_to_desktop_count != m_desktop_switch_count)
    {
        auto callbacks = get_global_callbacks();
        if(callbacks->move_to_current_desktop)
        {
            callbacks->move_to_current_desktop();
            m_moved_to_desktop_count = m_desktop_switch_count;
        }
    }
}

// Button press/touch begin
void LayoutViewKeyboard::on_input_sequence_begin(const InputSequencePtr& sequence)
{
    LOG_DEBUG << sequence;

    auto keyboard = get_keyboard();
    auto keyboard_view = get_keyboard_view();
    keyboard->on_activity_detected();

    keyboard_view->stop_click_polling();
    keyboard_view->stop_dwelling();
    keyboard_view->close_key_popup();

    // There's no reliable enter/leave for touch input
    // -> turn up inactive transparency on touch begin
    if (sequence->is_touch())
        keyboard_view->begin_inactivity_timer_transition(true);

    // hit-test touch handles first
    Handle::Enum hit_handle = Handle::NONE;
    if (keyboard_view->are_touch_handles_active())
    {
        hit_handle = m_touch_handles->hit_test(sequence->point);
        m_touch_handles->set_pressed(hit_handle);
        if (hit_handle != Handle::NONE)
        {
            // handle clicked -> stop auto-hide until button release
            keyboard_view->stop_touch_handles_auto_hide();
        }
        else
        {
            // no handle clicked -> hide them now
            keyboard_view->show_touch_handles(false);
        }
    }

    // ask layout tree
    if (hit_handle == Handle::NONE)
    {
        auto layout = this->get_layout();
        if (layout)
            layout->dispatch_input_sequence_begin(sequence);
    }

    if (sequence->active_item)
        sequence->active_item->sequence_begin_retry_func =
            [&](const InputSequencePtr& sequence_)
            {on_input_sequence_begin_delayed(sequence_);};
    else
        on_input_sequence_begin_delayed(sequence, hit_handle);
}

bool LayoutViewKeyboard::on_input_sequence_begin_delayed(const InputSequencePtr& sequence,
                                                         Handle::Enum hit_handle)
{
    auto view_manipulator = get_view_manipulator();
    Point point = sequence->point;
    LayoutKeyPtr key;

    // hit-test keys
    if (hit_handle == Handle::NONE &&
        !sequence->active_item)
    {
        key = get_key_at_location(point);
    }

    // enable/disable the drag threshold
    if (hit_handle != Handle::NONE)
    {
        view_manipulator->enable_drag_protection(false);
    }
    else
    if (key && key->get_id() == "move")
    {
        // Move key needs to support long press;
        // always use the drag threshold.
        view_manipulator->enable_drag_protection(true);
        view_manipulator->reset_drag_protection();
    }
    else
    {
        view_manipulator->enable_drag_protection(config()->drag_protection);
    }

    // handle resizing
    if (!key &&
       !config()->has_window_decoration() &&
       !config()->is_xid_mode())
    {
        if (view_manipulator->handle_press(sequence))
            return true;
    }

    // bail if we are in scanning mode
    if (config()->scanner->enabled)
        return true;

    // press the key
    sequence->active_key = key;
    sequence->initial_active_key = key;
    if (key)
    {
        auto key_logic = get_keyboard()->get_key_logic();

        // single click?
        if (m_last_click_key != key ||
            sequence->time - m_last_click_time > m_double_click_time)
        {
            // handle key press
            sequence->event_type = EventType::CLICK;
            key_logic->interactive_key_down(this, sequence);

            // start long press detection
            using namespace std::chrono;
            duration<double> delay(config()->keyboard->long_press_delay);

            // don't show touch handles too easily
            if (key->get_id() == "move")
                delay += 0.3s;

            key_logic->start_long_press_timer(duration_cast<milliseconds>(delay),
                this, sequence);
        }

        // double click
        else
        {
            sequence->event_type = EventType::DOUBLE_CLICK;
            key_logic->interactive_key_down(this, sequence);
        }

        m_last_click_key = key;
        m_last_click_time = sequence->time;
    }

    return true;
}

// Pointer motion/touch update
void LayoutViewKeyboard::on_input_sequence_update(const InputSequencePtr& sequence)
{
    auto keyboard = get_keyboard();
    auto keyboard_view = get_keyboard_view();
    auto view_manipulator = get_view_manipulator();

    // only drag with the very first sequence
    if (!sequence->primary)
        return;

    // Redirect to long press popup for drag selection.
    auto popup = keyboard_view->get_key_popup();
    if (popup)
    {
        auto ir = popup->get_input_event_receiver();
        ir->redirect_sequence_update(sequence, this, popup,
            [&](const InputSequencePtr& sequence_)
            {popup->on_input_sequence_update(sequence_);});
        return;
    }

    Point point = sequence->point;
    LayoutKeyPtr hit_key;

    // hit-test touch handles first
    Handle::Enum hit_handle = Handle::NONE;
    if (keyboard_view->are_touch_handles_active())
    {
        hit_handle = m_touch_handles->hit_test(point);
        m_touch_handles->set_prelight(hit_handle);
    }

    // ask layout tree
    if (hit_handle == Handle::NONE)
    {
        auto layout = get_layout();
        if (layout)
            layout->dispatch_input_sequence_update(sequence);

        if (!sequence->active_item)
        {
            // hit-test keys
            hit_key = get_key_at_location(point);
        }
        else
        {
            // hack that hides popups when ScrolledLayoutPanel
            // starts scrolling
            keyboard_view->hide_touch_feedback();
        }
    }

    if (sequence->state & ONBOARD_OSK_BUTTON123_MASK)
    {
        auto key_logic = keyboard->get_key_logic();

        // move/resize
        view_manipulator->handle_motion(sequence);

        // stop long press when drag threshold has been overcome
        if (view_manipulator->is_drag_active())
            key_logic->stop_long_press();

        // drag-select new active key
        auto active_key = sequence->active_key;
        if (!view_manipulator->is_drag_initiated() &&
            active_key != hit_key)
        {
            key_logic->stop_long_press();

            if (overcome_initial_key_resistance(sequence) &&
               (!active_key || !active_key->activated) &&
               !popup)
            {
                sequence->active_key = hit_key;
                key_logic->interactive_key_down_update(this, sequence, active_key);
            }
        }
    }

    else
    {
        if (hit_handle != Handle::NONE)
        {
            // handle hovered over: extend the time
            // touch handles are visible
            keyboard_view->start_touch_handles_auto_hide();
        }

        // start dwelling if we have entered a dwell-enabled key
        if (hit_key &&
            hit_key->sensitive)
        {
            keyboard_view->maybe_start_dwelling(this, hit_key, point);
        }

        do_set_cursor_at(point, hit_key);
    }

    // cancel dwelling when the hit key changes
    keyboard_view->maybe_cancel_dwelling(hit_key);

    // Process pending UI updates
    keyboard->commit_ui_updates();

    Super::on_input_sequence_update(sequence);
}

// Button release/touch end
void LayoutViewKeyboard::on_input_sequence_end(const InputSequencePtr& sequence)
{
    LOG_DEBUG << sequence;

    auto keyboard = get_keyboard();
    auto key_logic = keyboard->get_key_logic();
    auto keyboard_view = get_keyboard_view();
    auto view_manipulator = get_view_manipulator();

    // Redirect to long press popup for end of drag-selection.
    auto popup = keyboard_view->get_key_popup();
    if (popup &&
        popup->got_motion())   // keep popup open if it wasn't entered
    {
        auto ir = popup->get_input_event_receiver();
        ir->redirect_sequence_end(sequence, this, popup,
            [&](const InputSequencePtr& sequence_) {popup->on_input_sequence_end(sequence_);});
        return;
    }

    // layout tree
    auto layout = this->get_layout();
    if (layout)
        layout->dispatch_input_sequence_end(sequence);

    // key up
    auto active_key = sequence->active_key;
    if (active_key &&
       !config()->scanner->enabled)
    {
        key_logic->interactive_key_up(this, sequence);
    }

    view_manipulator->stop_drag();
    key_logic->stop_long_press();

    // reset touch handles
    keyboard_view->reset_touch_handles();
    keyboard_view->start_touch_handles_auto_hide();

    // There's no reliable enter/leave for touch input
    // -> start inactivity timer on touch end
    if (sequence->is_touch())
        keyboard_view->begin_inactivity_timer_transition(false);

    // Process pending UI updates
    keyboard->commit_ui_updates();

    // reset cursor when there was no cursor motion
    Point point = sequence->point;
    LayoutKeyPtr hit_key = get_key_at_location(point);
    do_set_cursor_at(point, hit_key);
}

void LayoutViewKeyboard::on_enter_notify(const OnboardOskEvent* event)
{
    auto keyboard = get_keyboard();
    auto keyboard_view = get_keyboard_view();

    keyboard->on_activity_detected();

    update_double_click_time();

    // Ignore event if a mouse button is held down.
    // We'll get the event once the button is released.
    if (event->state & ONBOARD_OSK_BUTTON123_MASK)
        return;

    // ignore unreliable touch enter event for inactivity timer
    // -> smooths startup, only one transition in set_startup_visibility()
    if (event->source_device_type != ONBOARD_OSK_TOUCHSCREEN_DEVICE)
    {
        // stop inactivity timer
        keyboard_view->begin_inactivity_timer_transition(true);
    }

    // stop click polling
    keyboard_view->stop_click_polling();
}

void LayoutViewKeyboard::on_leave_notify(const OnboardOskEvent* event)
{
    auto keyboard_view = get_keyboard_view();
    auto view_manipulator = get_view_manipulator();

    // ignore event if a mouse button is held down
    // we get the event once the button is released
    if (event->state & ONBOARD_OSK_BUTTON123_MASK)
        return;

    // Ignore leave events when the pointer hasn't acually left
    // our window. Fixes window becoming idle-transparent while
    // typing into firefox location bar.
    // Can't use event.mode as that appears to be broken and
    // never seems to become GDK_CROSSING_GRAB (Precise).
    if (get_canvas_rect().contains({event->x, event->y}))
        return;

    keyboard_view->stop_dwelling();
    keyboard_view->reset_touch_handles();

    // start a timer to detect clicks outside of onboard
    keyboard_view->start_click_polling();

    // Start inactivity timer, but ignore the unreliable
    // leave event for touch input.
    if (event->source_device_type != ONBOARD_OSK_TOUCHSCREEN_DEVICE)
    {
        // stop inactivity timer
        keyboard_view->begin_inactivity_timer_transition(false);
    }

    // Reset the cursor, so enabling the scanner doesn't get the last
    // selected one stuck forever.
    view_manipulator->reset_drag_cursor();
}

void LayoutViewKeyboard::show_touch_handles(bool show)
{
    if (show)
    {
        m_touch_handles->set_prelight(Handle::NONE);
        m_touch_handles->set_pressed(Handle::NONE);
        m_touch_handles->set_active(true);

        Size size;
        Size size_mm;
        get_monitor_dimensions(this, size, size_mm);
        m_touch_handles->set_monitor_dimensions(size, size_mm);
        update_touch_handles_positions();
    }
}

void LayoutViewKeyboard::set_touch_handles_active(bool active)
{
    m_touch_handles->set_active(active);
}

void LayoutViewKeyboard::reset_touch_handles()
{
    m_touch_handles->set_prelight(Handle::NONE);
    m_touch_handles->set_pressed(Handle::NONE);
}

void LayoutViewKeyboard::on_touch_handles_opacity(double opacity, bool done)
{
    (void)done;
    m_touch_handles->set_opacity(opacity);
    m_touch_handles->redraw();
}

void LayoutViewKeyboard::update_touch_handles_positions()
{
    m_touch_handles->update_positions(get_view_frame_rect());
}

void LayoutViewKeyboard::update_double_click_time()
{
    auto callbacks = get_global_callbacks();
    if (callbacks->get_double_click_time)
        m_double_click_time = callbacks->get_double_click_time();
}

// Drag-select: Increase the hit area of the initial key
// to make it harder to leave the the key the button was
// pressed down on.
bool LayoutViewKeyboard::overcome_initial_key_resistance(const InputSequencePtr& sequence)
{
    const double DRAG_SELECT_INITIAL_KEY_ENLARGEMENT = 0.4;

    auto active_key = sequence->active_key;
    if (active_key && active_key == sequence->initial_active_key)
    {
        Rect rect = active_key->get_canvas_border_rect();
        double k = std::min(rect.w, rect.h) * DRAG_SELECT_INITIAL_KEY_ENLARGEMENT;
        rect = rect.inflate(k);
        if (rect.contains(sequence->point))
            return false;
    }
    return true;
}

void LayoutViewKeyboard::do_set_cursor_at(const Point& point, const LayoutKeyPtr& hit_key)
{
    auto view_manipulator = get_view_manipulator();

    if (!config()->is_xid_mode())
    {
        bool allow_drag_cursors = !hit_key &&
                                  !config()->has_window_decoration();
        view_manipulator->set_drag_cursor_at(point, allow_drag_cursors);
    }
}

void LayoutViewKeyboard::start_move_view()
{
    m_view_manipulator->start_move();
}

void LayoutViewKeyboard::stop_move_view()
{
    m_view_manipulator->stop_move();
}

Rect LayoutViewKeyboard::get_resize_frame_rect()
{
    return get_view_frame_rect();
}

HandleFunction::Enum LayoutViewKeyboard::get_handle_function(Handle::Enum handle)
{
    if ((handle == Handle::WEST ||
         handle == Handle::EAST) &&
        config()->is_docking_enabled() &&
        config()->is_dock_expanded())
    {
        return HandleFunction::ASPECT_RATIO;
    }
    return HandleFunction::NORMAL;
}

double LayoutViewKeyboard::get_drag_threshold()
{
    return  config()->get_drag_threshold();
}

// Returns the bounding rectangle of all move buttons
// in canvas coordinates.
// Overload for WindowManipulator
Rect LayoutViewKeyboard::get_always_visible_rect()
{
    auto keyboard = get_keyboard();
    static auto is_path_invisible = [](const LayoutItemPtr& item) -> bool
    {return !item->is_path_visible();};

    Noneable<Rect> bounds;
    if (!config()->is_docking_enabled())
    {
        auto keys = keyboard->find_items_from_ids({"move"});
        keys.erase(std::remove_if(keys.begin(), keys.end(), is_path_invisible),
                   keys.end());
        if (keys.empty())
        {   // no visible move key (Small, Phone layout)?
            keys = keyboard->find_items_from_ids({"RTRN"});
            keys.erase(std::remove_if(keys.begin(), keys.end(), is_path_invisible),
                       keys.end());
        }
        for (auto& key : keys)
        {
            Rect r = key->get_canvas_border_rect();
            if (bounds.is_none())
                bounds = r;
            else
                bounds = r.union_(bounds);
        }

        if (bounds.is_none())
            bounds = get_canvas_rect();

        return bounds;
    }
    return get_canvas_rect();
}

Handle::Enum LayoutViewKeyboard::hit_test_move_resize(const Point& point)
{
    return m_touch_handles->hit_test(point);
}

void LayoutViewKeyboard::on_drag_initiated()
{
    get_keyboard_view()->on_user_positioning_begin();
    //grab_xi_pointer(true);  // TODO
}

void LayoutViewKeyboard::on_drag_activated()
{
    if (m_view_manipulator->is_resizing())
        set_lod(LOD::MINIMAL);
    get_keyboard_view()->hide_touch_feedback();
}

// Incoming "configure event" from toolkit-dependent code.
// Only sent if this is a toplevel view.
void LayoutViewKeyboard::on_toplevel_geometry_changed()
{
    update_geometry();
    recalc_geometry();
    get_keyboard()->invalidate_size();
    Super::on_toplevel_geometry_changed();
}

// Always called when the size of this view changes.
void LayoutViewKeyboard::on_geometry_changed()
{
}

// LayoutView(s) is(are) toplevel.
// Find KeyboardView's geometry from the known
// LayoutViews geometries.
void LayoutViewKeyboard::recalc_geometry()
{
    auto keyboard_view = get_keyboard_view();

    Rect rect = get_rect();
    for (auto& view : get_keyboard_layout_views())
        rect.union_(view->get_rect());

    keyboard_view->set_rect(rect);
    reinterpret_cast<ViewBase*>(keyboard_view)->on_geometry_changed();
}

void LayoutViewKeyboard::on_drag_done()
{
    // grab_xi_pointer(false);
    auto toplevel = find_toplevel_view_from_leaf();
    if (toplevel == this)
    {
        // update LayoutView children starting with KeyboardView
        update_geometry();
        recalc_geometry();
    }
    else
    {
        // update LayoutView children starting with KeyboardView
        toplevel->update_geometry();
        toplevel->recalc_geometry();
    }

    get_keyboard_view()->on_user_positioning_done(
        m_view_manipulator->was_moving());

    this->reset_lod();
}

AspectChangeRange LayoutViewKeyboard::get_docking_aspect_change_range()
{
    return m_view_manipulator->get_docking_aspect_change_range();
}

void LayoutViewKeyboard::update_layout()
{
    Super::update_layout();
    update_touch_handles_positions();
}

void LayoutViewKeyboard::draw(DrawingContext& dc)
{
    auto cc = dc.get_cc();
    cc->push_group();

    Super::draw(dc);

    // draw touch handles (enlarged move && resize handles)
    if (m_touch_handles->is_active())
    {
        double corner_radius = is_decorated() ? config()->corner_radius : 0.0;
        m_touch_handles->set_corner_radius(corner_radius);
        m_touch_handles->draw(dc);
    }

    cc->pop_group_to_source();
    cc->paint_with_alpha(get_keyboard_view()->get_opacity());
}

void LayoutViewKeyboard::update_window_handles()
{
    bool docking = config()->is_docking_enabled();

    // frame handles
    m_view_manipulator->set_drag_handles(get_active_drag_handles());
    m_view_manipulator->lock_x_axis(docking);

    // touch handles
    m_touch_handles->set_active_handles(get_active_drag_handles(true));
    m_touch_handles->lock_x_axis(docking);
}

std::vector<Handle::Enum> LayoutViewKeyboard::get_active_drag_handles(bool all_handles)
{
    std::vector<Handle::Enum> handles;
    if (config()->is_xid_mode())   // none when xembedding
    {
        // no handles
    }
    else
    {
        if (config()->is_docking_enabled() &&
            config()->is_dock_expanded())
            handles = {Handle::NORTH, Handle::SOUTH,
                       Handle::WEST, Handle::EAST,
                       Handle::MOVE};
        else
            handles.insert(handles.begin(), Handle::RESIZE_MOVE.begin(), Handle::RESIZE_MOVE.end());

        if (!all_handles)
        {
            // filter through handles enabled in config
            std::vector<Handle::Enum> config_handles = config()->window->window_handles;
            handles.erase(std::remove_if(
                              handles.begin(), handles.end(),
                              [&](Handle::Enum handle)
                              {return !contains(config_handles, handle);}),
                          handles.end());
        }
    }

    return handles;
}

