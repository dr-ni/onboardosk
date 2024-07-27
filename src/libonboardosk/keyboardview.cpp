#include <algorithm>
#include <memory>

#include "tools/logger.h"
#include "tools/point.h"
#include "tools/rect.h"
#include "tools/time_helpers.h"

#include "autohide.h"
#include "autoshow.h"
#include "buttoncontroller.h"
#include "configuration.h"
#include "dwellprogresscontroller.h"
#include "fadetimer.h"
#include "inputsequence.h"
#include "keyboardanimator.h"
#include "keyboarddecls.h"
#include "keyboardkeylogic.h"
#include "keyboardpopup.h"
#include "keyboardview.h"
#include "layoutkey.h"
#include "layoutview.h"
#include "onboardoskcallbacks.h"
#include "timer.h"


// Waits for the inactivity delay and transitions between
// active and inactive state.
// Inactivity here means, the pointer has left the keyboard window.
class InactivityTimer : public Timer
{
    public:
        using Super = Timer;
        InactivityTimer(const ContextBase& context) :
            Super(context)
        {}

        bool is_enabled()
        {
            return is_composited() &&
                   config()->is_inactive_transparency_enabled() &&
                   config()->window->enable_inactive_transparency &&
                   !config()->is_xid_mode();
        }

        bool is_active()
        {
            return m_active;
        }

        void begin_transition(bool active)
        {
            m_active = active;
            if (active)
            {
                Super::stop();
                auto animator = get_keyboard_view()->get_animator();
                if (animator->transition_active_to(true))
                    animator->commit_transition();
            }
            else
            {
                if (!config()->is_xid_mode())
                {
                    Super::start(config()->window->inactive_transparency_delay);
                }
            }
        }

        virtual bool on_timer() override
        {
            auto animator = get_keyboard_view()->get_animator();
            animator->transition_active_to(false);
            animator->commit_transition();
            return false;
        }

    private:
        bool m_active{false};
};


// Save and restore window position and size.
class ViewRectPersist : public ContextBase
{
    public:
        ViewRectPersist(ViewBase* view) :
            ContextBase(*view),
            m_view(view),
            m_save_position_timer(std::make_unique<Timer>(*view)),
            m_screen_size_changed_timer(std::make_unique<Timer>(*view))
        {}
        ~ViewRectPersist()
        {
            m_save_position_timer->finish();
        }

        // detect screen rotation (tablets)
        void on_screen_size_changed()
        {
            // Give the screen time to settle, the window manager
            // may block the move to previously invalid positions and
            // when docked, the slide animation may be drowned out by all
            // the action in other processes.
            m_screen_size_changed_timer->start(
                1.5, [&](){restore_view_rect(); return false;});
        }

        // Restore window size and position.
        void restore_view_rect(bool startup=false)
        {
            // Run pending save operations now, so they don't
            // interfere with the window rect after it was restored.
            m_save_position_timer->finish();

            bool is_landscape = is_screen_landscape();

            Rect rect = m_view->read_window_rect(is_landscape);

            config()->set_screen_landscape(is_landscape);
            m_view->set_rect(rect);
            LOG_DEBUG << "restore_view_rect rect=" << rect
                      << ", is_landscape=" << repr(is_landscape);

            // Give the derived class a chance to modify the rect
            // for auto-positioning by auto-show.
            rect = m_view->on_restore_view_rect(rect);
            m_view->set_rect(rect);

            // only toplevels are allowed to move; move this elsewhere
            #if 0
            // move/resize the window
            if (startup)
            {
                // TODO: test if still necessary
                // gnome-shell doesn't take kindly to an initial move_resize().
                // The window ends up at (0, 0) on && goes back there
                // repeatedly when hiding && unhiding.
                // set_default_size(rect.w, rect.h);
                // move(rect.get_position());
                m_view->move_resize(rect);
            }
            else
            {
                m_view->move_resize(rect);
            }
            #endif

            // Initialize shadow variables with valid values so they
            // don't get taken from the unreliable window.
            // Fixes bad positioning of the very first auto-show.
            if (startup)
            {
                m_view->set_rect(rect);
                // Ignore frame dimensions; still better than asking the window.
                m_view->set_origin(rect.left_top());
            }
        }

        // Save window size and position.
        void save_view_rect(Noneable<bool> is_landscape={}, Noneable<Rect> rect={})
        {
            if (is_landscape.is_none())
                is_landscape = config()->is_screen_landscape();
            if (rect.is_none())
                rect = m_view->get_rect();

            // Give the derived class a chance to modify the rect,
            // for example to override it for auto-show.
            rect = m_view->on_save_view_rect(rect);

            m_view->write_window_rect(is_landscape, rect);

            LOG_DEBUG << "save_view_rect rect=" << rect
                      << ", is_landscape=" << repr(is_landscape);
        }

        // Trigger saving position and size to gsettings
        // Delay this a few seconds to avoid excessive disk writes.
        // Remember the current rect and rotation as the screen may have been
        // rotated when the saving happens.
        void start_save_position_timer()
        {
            bool is_landscape = is_screen_landscape();
            Rect r = m_view->get_rect();
            m_save_position_timer->start(5.0,
            [=](){save_view_rect(is_landscape, r); return false;});
        }

        void stop_save_position_timer()
        {
            m_save_position_timer->stop();
        }

        // Current orientation of the screen (tablet rotation).
        // Only the aspect ratio is taken into account at this time.
        // This appears to cover more cases than looking at monitor rotation,
        // in particular with multi-monitor screens.
        bool is_screen_landscape()
        {
            auto toplevel = m_view->get_first_toplevel_view();
            if (toplevel)
            {
                auto callbacks = get_global_callbacks();
                if (callbacks->view_get_screen_rect)
                {
                    Rect r;
                    callbacks->view_get_screen_rect(toplevel, &r.x, &r.y, &r.w, &r.h);
                    return r.w >= r.h;
                }
            }
            return true;
        }

    private:
        ViewBase* m_view;
        std::unique_ptr<Timer> m_save_position_timer;
        std::unique_ptr<Timer> m_screen_size_changed_timer;
};


std::unique_ptr<KeyboardView> KeyboardView::make(const ContextBase& context,
                                                 const std::string& name)
{
    return std::make_unique<This>(context, name);
}


KeyboardView::KeyboardView(const ContextBase& context,
                           const std::string& name_) :
    Super(context, ONBOARD_OSK_VIEW_TYPE_KEYBOARD, name_),
    m_view_rect_persist(std::make_unique<ViewRectPersist>(this)),
    m_animator(std::make_unique<KeyboardAnimator>(context)),
    m_auto_show(std::make_unique<AutoShow>(context)),
    m_auto_hide(std::make_unique<AutoHide>(context)),
    m_inactivity_timer(std::make_unique<InactivityTimer>(context)),
    m_dwell_update_timer(std::make_unique<Timer>(context)),
    m_auto_position_poll_timer(std::make_unique<Timer>(context)),
    m_outside_click_timer(std::make_unique<Timer>(context)),
    m_touch_handles_hide_timer(std::make_unique<Timer>(context)),
    m_touch_handles_fade(std::make_unique<FadeTimer>(context)),
    m_touch_feedback(std::make_unique<TouchFeedback>(context))
{
    m_auto_show->enable(config()->is_auto_show_enabled());
    m_auto_hide->enable(config()->is_auto_hide_enabled());
}

KeyboardView::~KeyboardView()
{}

KeyboardKeyLogic* KeyboardView::get_key_logic()
{
    return get_keyboard()->get_key_logic();
}

// Highest level visibility change for direct user interaction.
// Start of transition may be delayed until all keys have
// been released.
// Main method to toggle visibility manually.
void KeyboardView::request_visibility(bool visible)
{
    if (m_visibility_locked)
        m_visibility_requested = visible;
    else
        set_visible_interactive(visible);
}

void KeyboardView::request_visibility_toggle()
{
    bool visible;
    if (m_visibility_locked &&
        !m_visibility_requested.is_none())
        visible = m_visibility_requested;
    else
        visible = is_visible();
    this->request_visibility(!visible);
}

bool KeyboardView::is_visible()
{
    return m_visible;
}

// Interactive visibility change with transition.
// Transition starts immediately.
// Locks auto-show visible (if visible==true).
void KeyboardView::set_visible_interactive(bool visible)
{
    unlock_visibility();  // unlock frequently in case of stuck keys
    update_auto_show_on_visibility_change(visible);

    set_visible_with_transition(visible);
}

// Start transition that leads to visibility change.
void KeyboardView::set_visible_with_transition(bool visible)
{
    m_animator->transition_visible_to(visible,
                                      std::chrono::milliseconds(0));

    // briefly present the window
    if (visible && m_inactivity_timer->is_enabled())
    {
        m_animator->transition_active_to(true,
                                         std::chrono::milliseconds(0));

        m_inactivity_timer->begin_transition(false);
    }

    m_animator->commit_transition();
}

// Low level immediate visibility change of toplevel windows/actors.
void KeyboardView::set_visible(bool visible)
{
    if (!visible)
    {
        //hide_touch_feedback();  // TODO : port touchfeedback
    }

    m_visible = visible;
    for (auto& view : get_toplevel_views())
        view->ViewBase::set_visible(visible);
}

void KeyboardView::set_startup_visibility()
{
    using namespace std::chrono;

    // Show the Keyboard when turning off auto-show.
    // Hide the Keyboard when turning on auto-show.
    //   (Fix this when we know how to get the active accessible)
    // Hide the Keyboard on start when start-minimized is set.
    // Start with active transparency if the inactivity_timer is enabled.
    //
    // start_minimized            false true  false true
    // auto_show                  false false true  true
    // --------------------------------------------------
    // window visible on start    true  false false false

    bool visible = config()->is_visible_at_start();

    // Start with low opacity to stop opacity flashing
    // when inactive transparency is enabled.

    if (is_composited() &&
        m_inactivity_timer->is_enabled())
    {
        this->set_opacity(0.05);  // keep it slightly visible just in case
    }

    // transition to initial opacity
    m_animator->transition_visible_to(visible,
                                      milliseconds(0), milliseconds(400));
    m_animator->transition_active_to(true, milliseconds(0));
    m_animator->commit_transition();

    // kick off inactivity timer, i.e. inactivate on timeout
    if (m_inactivity_timer->is_enabled())
        m_inactivity_timer->begin_transition(false);

    // Be sure to initially show/hide window && icon palette
    set_visible_with_transition(visible);
}

void KeyboardView::lock_visibility()
{
    m_visibility_locked = true;
    auto_show_lock(LOCK_REASON_KEY_PRESSED);
}

void KeyboardView::unlock_visibility()
{
    m_visibility_locked = false;
    m_visibility_requested.set_none();
}

void KeyboardView::unlock_and_apply_visibility()
{
    if (m_visibility_locked)
    {
        Noneable<bool> visible = m_visibility_requested;

        unlock_visibility();

        if (!visible.is_none())
            set_visible_with_transition(visible);
    }

    // Unlock auto-show, && if the state has changed since locking,
    // transition to hide the keyboard.
    auto_show_unlock_and_apply_visibility(LOCK_REASON_KEY_PRESSED);
}

bool KeyboardView::set_opacity(double opacity)  // [0..1.0]
{
    bool changed = false;
    if (m_opacity != opacity)
    {
        m_opacity = opacity;
        get_keyboard()->invalidate_canvas();
        changed = true;
    }
    return changed;
}

double KeyboardView::get_opacity()
{
    return m_opacity;
}

void KeyboardView::update_transparency()
{
    m_animator->transition_active_to(true);
    m_animator->commit_transition();
    if (m_inactivity_timer->is_enabled())
        m_inactivity_timer->begin_transition(false);
    else
        m_inactivity_timer->stop();
    redraw();  // for background transparency
}

void KeyboardView::begin_inactivity_timer_transition(bool active)
{
    if (m_inactivity_timer->is_enabled())
        m_inactivity_timer->begin_transition(active);
}

void KeyboardView::on_transition_done(bool visible_before, bool visible_now)
{
    for (auto& view : get_keyboard_layout_views())
        view->on_transition_done(visible_before, visible_now);
    update_docking();
}

void KeyboardView::update_auto_show_on_visibility_change(bool visible)
{
    if (config()->is_auto_show_enabled())
    {
        // showing keyboard while auto-hide is pausing auto-show?
        if (visible && m_auto_hide->is_auto_show_locked())
        {
            auto_show_lock_visible(false);
            m_auto_hide->auto_show_unlock();
        }
        else
        {
            auto_show_lock_visible(visible);
        }

        // Make sure to drop the 'key-pressed' lock in case it still
        // exists due to e.g. stuck keys.
        if (!visible)
            auto_show_unlock(this->LOCK_REASON_KEY_PRESSED);
    }
}

void KeyboardView::auto_position()
{
    m_auto_position_started = true;

    // Leave the first commit to AutoShow._begin_transition()
    update_position(false);

    // With docking enabled, when focusing the search entry of a
    // maximized firefox window, it changes position when the work
    // area shrinks && ends up below Onboard.
    // -> periodically update the window position for a little while,
    //    this way slow systems can catch up too eventually (Nexus 7).
    m_poll_auto_position_start_time = SteadyClock::now();
    auto start_delay = std::chrono::milliseconds(100);
    m_auto_position_poll_timer->start(start_delay,
        [=]() -> bool {return on_auto_position_poll(start_delay);});
}

void KeyboardView::stop_auto_positioning()
{
    m_auto_position_poll_timer->stop();
}

bool KeyboardView::is_auto_show_locked(const std::string& reason)
{
    return m_auto_show->is_locked(reason);
}

void KeyboardView::auto_show_lock(const std::string& reason, Noneable<double> duration, bool lock_show, bool lock_hide)
{
    if (config()->is_auto_show_enabled())
    {
        if (!duration.is_none())
        {
            if (duration == 0.0)
                return;          // do nothing

            // negative means auto-hide is off
            if (duration < 0.0)
                duration.set_none();
        }

        m_auto_show->lock(reason, duration, lock_show, lock_hide);
    }
}

void KeyboardView::auto_show_unlock(const std::string& reason)
{
    if (config()->is_auto_show_enabled())
        m_auto_show->unlock(reason);
}

void KeyboardView::auto_show_unlock_and_apply_visibility(const std::string& reason)
{
    if (config()->is_auto_show_enabled())
    {
        Noneable<bool> visibility = m_auto_show->unlock(reason);
        if (!visibility.is_none())
            m_auto_show->request_keyboard_visible(visibility, 0.0);
    }
}

void KeyboardView::auto_show_lock_and_hide(const std::string& reason, Noneable<double> duration)
{
    if (config()->is_auto_show_enabled())
    {
        LOG_DEBUG << "auto_show_lock_and_hide(" << repr(reason)
                  << ", " << duration << ")";

        // Attempt to hide the keyboard.
        // If it doesn't hide immediately, e.g. due to currently
        // pressed keys, we get a second chance the next time
        // apply_pending_state() is called, i.e. on key-release.
        if (!m_auto_show->is_locked(reason))
            m_auto_show->request_keyboard_visible(false, 0.0);

        // Block showing the keyboard.
        m_auto_show->lock(reason, duration, true, false);
    }
}

void KeyboardView::auto_show_lock_visible(bool visible)
{
    if (config()->is_auto_show_enabled())
        m_auto_show->lock_visible(visible);
}

bool KeyboardView::on_auto_position_poll(std::chrono::milliseconds delay)
{
    this->update_position();

    // start another timer for progressively longer intervals
    delay = std::min(delay * 2, std::chrono::milliseconds(1000));
    if (SteadyClock::now() + delay <
        m_poll_auto_position_start_time + std::chrono::milliseconds(3000))
    {
        m_auto_position_poll_timer->start(delay,
                                          [=]() -> bool {return on_auto_position_poll(delay);});
    }
    return false;
}

// higher-level function to update the target position of the animator
void KeyboardView::update_position(bool commit)
{
    Rect home_rect = get_home_rect();
    Rect rect;

    if (get_keyboard_layout_views().empty())
        return;
    auto view = get_keyboard_layout_views()[0].get();  // FIXME

    if (!get_repositioned_window_rect(rect, view, home_rect))
    {
        // move back home
        rect = home_rect;
    }

    if (get_position() != rect.get_position())
    {
        m_animator->transition_position_to(rect.get_position());
        if (commit)
            m_animator->commit_transition();
    }
}

bool KeyboardView::get_auto_show_repositioned_window_rect(Rect& result,
                                                          const LayoutView* view,
                                                          const Rect& home,
                                                          const vector<Rect>& limit_rects,
                                                          const Border& test_clearance,
                                                          const Border& move_clearance,
                                                          bool horizontal, bool vertical)
{
    if (!m_auto_show) // may happen on exit, rarely
        return {};

    return m_auto_show->get_repositioned_window_rect(result,
                view, home, limit_rects,
                test_clearance, move_clearance,
                horizontal, vertical);

}

bool KeyboardView::get_repositioned_window_rect(Rect& result,
                                                const LayoutView* view,
                                                const Rect& home_rect)
{
    bool repositioned = false;

    if (config()->can_auto_show_reposition())
    {
        Border clearance = config()->auto_show->widget_clearance;
        Border test_clearance{clearance};
        Border move_clearance{clearance};
        vector<Rect> limit_rects;  // empty: all monitors

        // No test clearance when docking. Make it harder to jump
        // out of the dock, for example for the bottom search box
        // in maximized firefox.
        if (config()->is_docking_enabled())
        {
            test_clearance = {clearance.left, 0.0, clearance.right, 0.0};

            // limit the horizontal freedom to the docking monitor
            Rect area, geom;
            get_docking_monitor_rects(area, geom);
            limit_rects.emplace_back(area);
        }
        else
        {
            limit_rects = get_monitor_rects();
        }

        bool horizontal, vertical;
        get_repositioning_constraints(horizontal, vertical);
        repositioned = get_auto_show_repositioned_window_rect(
                   result, view,
                   home_rect, limit_rects,
                   test_clearance, move_clearance,
                   horizontal, vertical);
    }

    return repositioned;
}

// Return allowed respositioning directions for auto-show.
void KeyboardView::get_repositioning_constraints(bool& horizontal, bool& vertical)
{
    if (config()->is_docking_enabled() &&
        config()->is_dock_expanded())
    {
        horizontal = false;
        vertical = true;
    }
    else
    {
        horizontal = true;
        vertical = true;
    }
}

bool KeyboardView::is_dwelling()
{
    return m_dwell_key != nullptr;
}

bool KeyboardView::already_dwelled(const LayoutItemPtr& key)
{
    return m_last_dwelled_key == key;
}

void KeyboardView::reset_already_dwelled()
{
    m_last_dwelled_key = nullptr;
}

void KeyboardView::start_dwelling(LayoutView* view, const LayoutKeyPtr& key)
{
    using namespace std::chrono;
    cancel_dwelling();
    auto dwc = key->get_dwell_progress_controller();
    if (dwc)
    {
        milliseconds delay;
        if (config()->is_local_hover_click_enabled())
            delay = duration_cast<milliseconds>(duration<double>(
                        config()->get_local_hover_click_dwell_delay()));
        else
            delay = milliseconds(4000);

        m_dwell_view = view;
        m_dwell_key = key;
        m_last_dwelled_key = key;

        dwc->start_dwelling(delay);

        m_dwell_update_timer->start(milliseconds(50),
            [&]() -> bool {return on_dwell_update_timer();});
    }
}

void KeyboardView::cancel_dwelling()
{
    stop_dwelling();
    reset_already_dwelled();
}

void KeyboardView::stop_dwelling()
{
    if (m_dwell_key)
    {
        m_dwell_update_timer->stop();
        auto dwc = m_dwell_key->get_dwell_progress_controller();
        if (dwc)
            dwc->stop_dwelling();

        get_keyboard()->invalidate_key(m_dwell_key); // remove progress

        m_dwell_key = nullptr;
    }
}

void KeyboardView::maybe_start_dwelling(LayoutView* view,
                                        const LayoutKeyPtr& key,
                                        const Point& point)
{
    bool can_dwell = false;

    if (!is_dwelling() &&
        !config()->scanner->enabled &&
        !config()->lockdown->disable_dwell_activation)
    {
        bool local_hover_click = config()->is_local_hover_click_enabled();

        // Maybe allow the same key to dwell again:
        // with local hover click, once the pointer moved threshhold away
        // from the end of the last dwell.
        if (local_hover_click &&
            already_dwelled(key))
        {
            double d2 = config()->get_local_hover_click_dwell_threshold();
            if (m_dwell_end_point.distance2(point) > d2 * d2)
            {
                reset_already_dwelled();
            }
        }

        if (!already_dwelled(key))
        {
            if (local_hover_click)
            {
                can_dwell = true;
            }
            else
            {
                auto controller = get_keyboard()->get_button_controller(key);
                can_dwell = controller && controller->can_dwell();
            }
        }
    }

    if (can_dwell)
    {
        start_dwelling(view, key);
    }
}

// cancel dwelling when the hit key changes
void KeyboardView::maybe_cancel_dwelling(const LayoutKeyPtr& key)
{
    if ((m_dwell_key && m_dwell_key != key) ||
        (m_last_dwelled_key && m_last_dwelled_key != key))
    {
        this->cancel_dwelling();
    }
}

bool KeyboardView::on_dwell_update_timer()
{
    bool ret = true;
    auto& key = m_dwell_key;
    if (key)
    {
        auto keyboard = get_keyboard();
        auto dpc = key->get_dwell_progress_controller();

        if (dpc->is_done())
        {
            dpc->stop_dwelling();

            auto sequence = std::make_shared<InputSequence>();
            sequence->button = MouseButton::LEFT;
            sequence->event_type = EventType::DWELL;
            sequence->active_key = key;
            sequence->point = key->get_canvas_rect().get_center();
            //sequence->root_point =
            //        canvas_to_root_window_point(sequence->point);

            auto key_logic = get_key_logic();
            key_logic->interactive_key_down(m_dwell_view, sequence);
            key_logic->interactive_key_up(m_dwell_view, sequence);

            m_dwell_end_point = m_dwell_view->get_last_sequence_point();

            stop_dwelling();

            ret = false;
        }

        keyboard->invalidate_key(key);

        // timer -> allowed to commit
        keyboard->commit_ui_updates();
    }

    return ret;
}

LayoutPopup* KeyboardView::get_key_popup()
{
    return m_key_popup.get();
}

void KeyboardView::set_key_popup(const LayoutPopupPtr& popup)
{
    m_key_popup = popup;
}

void KeyboardView::close_key_popup()
{
    if (m_key_popup)
    {
        // Unpress the key the popup originated from.
        auto key_logic = get_key_logic();
        auto key = m_key_popup->get_key();
        if (key)  // should be always there
            key_logic->key_unpress(key);

        // Remove popup from everywhere and delete it.
        m_key_popup->destroy();
        m_key_popup = nullptr;
    }
}

void KeyboardView::start_click_polling()
{
    auto key_logic = get_keyboard()->get_key_logic();
    if (key_logic->has_latched_sticky_keys() ||
        m_key_popup ||
        config()->are_word_suggestions_enabled())
    {
        m_outside_click_timer->start(std::chrono::milliseconds(10),
                                     [&]() -> bool {return on_click_polling_timer();});
        m_outside_click_detected = false;
        m_outside_click_start_time = std::chrono::steady_clock::now();
        m_outside_click_num = 0;
    }
}

void KeyboardView::stop_click_polling()
{
    m_outside_click_timer->stop();
}

bool KeyboardView::on_click_polling_timer()
{
    double x{0.0};
    double y{0.0};
    OnboardOskStateMask mask{};
    auto keyboard = get_keyboard();

    auto callbacks = get_global_callbacks();
    if (callbacks->get_current_pointer_state)
        callbacks->get_current_pointer_state(&x, &y, &mask);

    if (mask & ONBOARD_OSK_BUTTON123_MASK)
    {
        m_outside_click_detected = true;
        m_outside_click_button_mask = mask;
    }
    else if (m_outside_click_detected)
    {
        m_outside_click_detected = false;

        // A button was released anywhere outside of Onboard's control.
        LOG_DEBUG << "click polling: outside click";

        close_key_popup();

        MouseButton::Enum button = \
                get_button_from_mask(m_outside_click_button_mask);

        // When clicking left, don't stop polling right away. This allows
        // the user to select some text && paste it with middle click,
        // while the pending separator is still inserted.
        m_outside_click_num += 1;
        const int max_clicks = 4;

        if (button != 1)
        {  // middle && right click stop polling immediately
            stop_click_polling();
            keyboard->on_outside_click(button);
        }
        else if (button == MouseButton::LEFT &&
                 m_outside_click_num == 1)
        {
            if (!config()->word_suggestions->
                delayed_word_separators_enabled)
            {
                stop_click_polling();
            }
            keyboard->on_outside_click(button);
        }

        // allow a couple of left clicks with delayed separators
        else if (m_outside_click_num >= max_clicks)
        {
            stop_click_polling();
            keyboard->on_cancel_outside_click();
        }

        return true;
    }

    // stop polling after 30 seconds
    if (std::chrono::steady_clock::now() - m_outside_click_start_time >
        std::chrono::seconds(30))
    {
        stop_click_polling();
        keyboard->on_cancel_outside_click();
        return false;
    }

    return true;
}

void KeyboardView::redraw()
{
    for (auto& view : get_layout_views())
        view->redraw();
}

void KeyboardView::redraw_item(const LayoutItemPtr& key, bool invalidate)
{
    for (auto& view : get_layout_views())
        view->redraw_items({key}, invalidate);
}

void KeyboardView::redraw_items(const vector<LayoutItemPtr>& keys, bool invalidate)
{
    for (auto& view : get_layout_views())
        view->redraw_items(keys, invalidate);
}

void KeyboardView::draw(DrawingContext& dc)
{
    for (auto& view : get_children())
        view->draw(dc);
}

void KeyboardView::on_event(OnboardOskEvent* event)
{
    (void)event;

    // Events must be routed to leaf views.
    // KeyboardView can at best be a toplevel view.
    assert(false);
}

bool KeyboardView::has_input_sequences()
{
    for (auto& view : get_layout_views())
        if (view->has_input_sequences())
            return true;
    return false;
}

bool KeyboardView::can_move_into_view()
{
    return !config()->is_xid_mode() &&
           !config()->has_window_decoration() &&
           !config()->is_docking_enabled();
}

void KeyboardView::remember_rect(const Rect& rect)
{
    while (m_known_view_rects.size() >= 3)
        m_known_view_rects.pop();
    m_known_view_rects.push(rect);

    // Remembering the rects doesn't help if repositioning outside
    // of the work area in compiz with force-to-top mode disabled.
    // WM corrects window positions to fit into the viewable area.
    // -> add timing based block
    ignore_configure_events();
}

void KeyboardView::ignore_configure_events()
{
    m_last_ignore_configure_time = SteadyClock::now();
}

void KeyboardView::update_home_rect(bool update_home_position)
{
    if (config()->is_docking_enabled())
        return;

    // update home rect
    Rect rect = get_rect();
    if (!update_home_position)
    {
        rect.x = m_home_rect.x;
        rect.y = m_home_rect.y;
    }

    // Make sure the move button stays visible
    if (can_move_into_view())
    {
        Point pt = limit_position(rect.get_position());
        rect.x = pt.x;
        rect.y = pt.y;
    }

    m_home_rect = rect;
    m_view_rect_persist->start_save_position_timer();

    // Make transitions aware of the new position,
    // undoubtedly reached by user positioning.
    // Else, window snaps back to the last transition position.
    m_animator->sync_transition_position(rect);
}

Rect KeyboardView::get_home_rect()
{
    Rect rect;
    if (config()->is_docking_enabled())
        rect = get_dock_rect();
    else
        rect = m_home_rect;
    return rect;
}

Rect KeyboardView::get_visible_rect()
{
    Rect home_rect = get_home_rect();  // aware of docking
    Rect rect = home_rect;

    /*
    if (config()->is_auto_show_enabled())
        get_repositioned_window_rect(rect, view, home_rect);  // FIXME has to move to LayoutViewKeyboard
    */
    return rect;
}

Rect KeyboardView::get_dock_rect()
{
    Rect area, geom;
    get_docking_monitor_rects(area, geom);
    DockingEdge::Enum edge = config()->window->docking_edge;

    Size size = config()->get_dock_size();
    Rect rect{area.x, 0, area.w, size.h};
    if (edge == DockingEdge::BOTTOM)
        rect.y = area.y + area.h - size.h;
    else  // Top
        rect.y = area.y;

    bool expand = config()->is_dock_expanded();
    if (expand)
    {
        rect.w = area.w;
        rect.x = area.x;
    }
    else
    {
        rect.w = std::min(size.w, area.w);
        rect.x = rect.x + (area.w - rect.w); // 2
    }
    return rect;
}

Rect KeyboardView::get_docking_hideout_rect(const Rect& reference_rect)
{
    Rect area, geom;
    get_docking_monitor_rects(area, geom);
    Rect rect = get_dock_rect();
    Rect hideout = rect;

    Point mc = geom.get_center();
    Point c;
    if (!reference_rect.empty())
        c = reference_rect.get_center();
    else
        c = rect.get_center();

    double clearance = 10.0;
    if (c.y > mc.y)
        hideout.y = geom.bottom() + clearance;  // below Bottom
    else
        hideout.y = geom.top() - rect.h - clearance; // above Top

    return hideout;
}

Rect KeyboardView::get_keyboard_frame_rect()
{
    Noneable<Rect> rect;
    for (auto& view : get_keyboard_layout_views())
    {
        Rect r = view->get_view_frame_rect();;
        if (rect.is_none())
            rect = r;
        else
            rect = r.union_(rect);
    }
    return rect;
}

Rect KeyboardView::get_base_aspect_rect()
{
    auto& views = get_keyboard_layout_views();
    if (!views.empty())
        return views[0]->get_base_aspect_rect();
    return {0, 0, 1.0, 1.0};
}

double KeyboardView::get_frame_width()
{
    if (config()->is_xid_mode())
    {
        return config()->undecorated_frame_width;
    }
    if (config()->has_window_decoration())
    {
        return 0.0;
    }
    if (config()->is_dock_expanded())
    {
        return 2.0;
    }
    if (config()->window->transparent_background)
    {
        return 3.0;
    }
    return config()->undecorated_frame_width;
}

void KeyboardView::get_docking_monitor_rects(Rect& area, Rect& geom)
{
    int monitor_index = get_docking_monitor_index();

    if (!get_value(m_monitor_workarea, monitor_index, area))
        area = update_monitor_workarea(monitor_index);

    auto callbacks = get_global_callbacks();
    if (callbacks->get_monitor_geometry)
        callbacks->get_monitor_geometry(get_cinstance(),
                                        monitor_index,
                                        &geom.x, &geom.y, &geom.w, &geom.h);
}

int KeyboardView::get_docking_monitor_index(bool force_update)
{
    int monitor_index;

    if (m_current_docking_monitor_index.is_none() || force_update)
    {
        auto callbacks = get_global_callbacks();
        assert(callbacks->get_n_monitors);
        assert(callbacks->get_monitor_at_active_window);
        assert(callbacks->get_primary_monitor);

        DockingMonitor::Enum docking_monitor = config()->window->docking_monitor;

        monitor_index = static_cast<int>(docking_monitor);
        if (monitor_index < 100)
        {
            if (monitor_index < 0 ||
                monitor_index >= callbacks->get_n_monitors(get_cinstance()))
                docking_monitor = DockingMonitor::PRIMARY;
        }

        if (docking_monitor == DockingMonitor::ACTIVE)
            monitor_index = callbacks->get_monitor_at_active_window(get_cinstance());
        else if (docking_monitor == DockingMonitor::PRIMARY)
            monitor_index = callbacks->get_primary_monitor(get_cinstance());
    }
    else
    {
        monitor_index = m_current_docking_monitor_index;
    }
    return monitor_index;
}

bool KeyboardView::update_docking_monitor_index()
{
    int mon_before = m_current_docking_monitor_index;
    int mon_now = get_docking_monitor_index(true);
    m_current_docking_monitor_index = mon_now;
    return mon_before != mon_now;
}

void KeyboardView::reset_monitor_workarea()
{
    m_monitor_workarea.clear();
}

Rect KeyboardView::update_monitor_workarea(int monitor_index)
{
    Rect area = get_monitor_workarea(monitor_index);
    m_monitor_workarea[monitor_index] = area;
    return area;
}

Rect KeyboardView::get_monitor_workarea(int monitor_index)
{
    auto callbacks = get_global_callbacks();
    assert(callbacks->get_monitor_workarea);
    Rect r;
    callbacks->get_monitor_workarea(get_cinstance(),
                                    monitor_index, &r.x, &r.y, &r.w, &r.h);
    return r;
}

void KeyboardView::update_docking()
{
    bool enable = config()->is_docking_enabled();
    Rect rect;
    if (enable)
        rect = get_dock_rect();

    bool shrink = config()->window->docking_shrink_workarea;
    bool expand = config()->is_dock_expanded();
    DockingEdge::Enum edge = config()->window->docking_edge;
    int monitor_index = get_docking_monitor_index(true);

    if (m_docking_enabled != enable ||
        (m_docking_enabled &&
         (m_docking_rect != rect ||
          m_shrink_work_area != shrink ||
          m_dock_expand != expand ||
          m_current_struts.empty() != shrink ||
          m_docking_monitor_index != monitor_index)
         ))
    {
        m_current_docking_monitor_index = monitor_index;

        realize_docking(enable);

        m_shrink_work_area = shrink;
        m_dock_expand = expand;
        m_docking_edge = edge;
        m_docking_monitor_index = monitor_index;
        m_docking_enabled = enable;
        m_docking_rect = rect;
    }
}

void KeyboardView::realize_docking(bool enable)
{
    if (enable)
    {
        set_docking_struts(config()->window->docking_shrink_workarea,
                           config()->window->docking_edge,
                           config()->is_dock_expanded());
        restore_view_rect();  // knows about docking
        m_realize_docking_time = SteadyClock::now();
    }
    else
    {
        restore_view_rect();
        clear_docking_struts();
        m_realize_docking_time = {};
    }
}

void KeyboardView::clear_docking_struts()
{
    set_docking_struts(false);
}

void KeyboardView::set_docking_struts(bool enable, DockingEdge::Enum edge, bool expand)
{
    (void)enable;
    (void)edge;
    (void)expand;
}

Size KeyboardView::get_min_window_size()
{
    Size min_mm{50.0, 20.0};  // just large enough to grab with a 3 finger gesture
    return physical_to_monitor_pixel_size(this, min_mm, {150, 100});
}

Rect KeyboardView::get_hidden_rect()
{
    if (config()->is_docking_enabled())
        return get_docking_hideout_rect();
    return get_visible_rect();
}

// Returns the window rect with auto-show
// repositioning taken into account.
Rect KeyboardView::get_current_rect()
{
    Rect rect;
    if (is_visible())
        rect = get_visible_rect();
    else
        rect = get_hidden_rect();
    return rect;
}

void KeyboardView::restore_view_rect(bool startup)
{
    m_view_rect_persist->restore_view_rect(startup);

    recalc_geometry();  // update KeyboardView -> LayoutViews, always

    // Only toplevel views have windows/actors and only
    // those can be moved.
    auto& toplevel_views = get_toplevel_views();
    for (auto& view : toplevel_views)
        view->move_resize(view->get_rect());
}

Rect KeyboardView::on_restore_view_rect(const Rect& rect)
{
    Rect result = rect;

    if (!config()->is_docking_enabled())
        m_home_rect = rect;

    // check for alternative auto-show position
    Rect r = get_current_rect();
    if (r != result)
    {
        // remember our rects to distinguish from user move/resize
        remember_rect(r);
        result = r;
    }

    m_animator->sync_transition_position(result);
    return result;
}

Rect KeyboardView::on_save_view_rect(const Rect& rect)
{
    (void)rect;
    // Ignore <rect> (m_rect), it may just be a temporary one
    // set by auto-show. Save the user-selected home_rect instead.
    return m_home_rect;
}

Rect KeyboardView::read_window_rect(bool is_landscape)
{
    auto co = config()->get_window_orientation_co(is_landscape);

    // limit size due to LP #1633284, gsettings values might be negative
    Size min_size = get_min_window_size();
    Rect rect{co->x, co->y,
              std::max(co->width.get(), min_size.x),
              std::max(co->height.get(), min_size.y)};
    return rect;
}

void KeyboardView::write_window_rect(bool is_landscape, const Rect& rect)
{
    // There are separate rects for normal && rotated screen (tablets).
    auto co = config()->get_window_orientation_co(is_landscape);

    // remember that we wrote this rect to gsettings
    //this->_written_window_rects[orientation] = rect.copy();  // TODO: still necessary?

    // write to gsettings && trigger notifications
    co->delay();
    co->x = rect.x;
    co->y = rect.y;
    co->width = rect.w;
    co->height = rect.h;
    co->apply();
}

void KeyboardView::write_docking_size(const Size& size)
{
    auto co = config()->get_window_orientation_co();
    bool expand = config()->is_dock_expanded();

    // write to gsettings && trigger notifications
    co->delay();
    if (!expand)
        co->dock_width = size.w;
    co->dock_height = size.h;
    co->apply();
}

bool KeyboardView::are_touch_handles_active()
{
    return m_touch_handles_active;
}

void KeyboardView::show_touch_handles(bool show, bool auto_hide)
{
    if (show && config()->lockdown->disable_touch_handles)
        return;

    double start;
    double end;

    if (show)
    {
        set_touch_handles_active(true);

        for (auto& view : get_keyboard_layout_views())
            view->show_touch_handles(show);

        m_touch_handles_auto_hide = auto_hide;
        if (auto_hide)
            start_touch_handles_auto_hide();

        start = 0.0;
        end = 1.0;
    }
    else
    {
        stop_touch_handles_auto_hide();
        start = 1.0;
        end = 0.0;
    }

    if (m_touch_handles_fade->target_value != end)
    {
        m_touch_handles_fade->time_step = std::chrono::milliseconds(25);
        m_touch_handles_fade->fade_to(start, end, std::chrono::milliseconds(200),
           [&](double value, bool done) {on_touch_handles_opacity(value, done);});
    }
}

void KeyboardView::on_touch_handles_opacity(double opacity, bool done)
{
    if (done && opacity < 0.1)
        set_touch_handles_active(false);

    for (auto& view : get_keyboard_layout_views())
        view->on_touch_handles_opacity(opacity, done);
}

void KeyboardView::set_touch_handles_active(bool active)
{
    m_touch_handles_active = active;
    for (auto& view : get_keyboard_layout_views())
        view->set_touch_handles_active(active);
}

void KeyboardView::reset_touch_handles()
{
    for (auto& view : get_keyboard_layout_views())
        view->reset_touch_handles();
}

void KeyboardView::start_touch_handles_auto_hide()
{
    if (m_touch_handles_active && m_touch_handles_auto_hide)
    {
        m_touch_handles_hide_timer->start(std::chrono::seconds(4),
            [&]() {show_touch_handles(false); return false;});
    }
}

void KeyboardView::stop_touch_handles_auto_hide()
{
    m_touch_handles_hide_timer->stop();
}

void KeyboardView::show_touch_feedback(const LayoutKeyPtr key, const LayoutView* view)
{
    m_touch_feedback->show(key, view);
}

void KeyboardView::hide_touch_feedback(const LayoutKeyPtr key)
{
    m_touch_feedback->hide(key);
}

void KeyboardView::on_user_positioning_begin()
{
    //stop_save_position_timer();
    stop_auto_positioning();
    auto_show_lock(LOCK_REASON_USER_POSITIONING);

    m_user_positioning_begin_rect = get_rect();
}

// Incoming "configure event" from toolkit-dependent code.
// Only sent if this is a toplevel view.
void KeyboardView::on_toplevel_geometry_changed()
{
    update_geometry();
    recalc_geometry();
    get_keyboard()->invalidate_size();

    for (auto& view : get_keyboard_layout_views())
        view->on_geometry_changed();

    Super::on_toplevel_geometry_changed();
}

// Always called when the size of this view changes.
void KeyboardView::on_geometry_changed()
{
}

// KeyboardView is the only toplevel.
// Find LayoutViews' geometry from the known
// KeyboardView geometry
void KeyboardView::recalc_geometry()
{
    LayoutViews& views = get_keyboard_layout_views();
    if (views.size() == 1)
    {
        auto& view = views[0];
        if (view->is_toplevel())
            view->set_rect(get_rect());
        else
            view->set_rect(get_canvas_rect());
    }
    else if (views.size() == 2)
    {
        Rect parent_rect = get_canvas_rect();
        // split view left and right and take sizes from aspect ratio
        for (size_t i=0; i<2; i++)
        {
            auto& view = views[0];
            Rect base_aspect = view->get_base_aspect_rect();
            Rect r = base_aspect.inscribe_with_aspect(parent_rect);

            double align = i==0 ? 0.0 : 1.0;
            view->set_rect(parent_rect.align_rect(r, align));
        }
    }
}

void KeyboardView::on_user_positioning_done(bool was_moving)
{
    // this->detect_docking()
    if (config()->is_docking_enabled())
    {
        write_docking_size(get_size());
        update_docking();
    }
    else
    {
        // Attempt to filter out accidental resize frame clicks to prevent
        // losing the home position without clear user intention.
        double proximity = 30;
        Rect rh = get_home_rect();
        bool began_near_home =
            m_user_positioning_begin_rect.inflate(proximity)
            .intersection(rh) == rh;
        bool moved = was_moving;
        bool update_home_position = began_near_home || moved;

        update_home_rect(update_home_position);
    }

    // Thaw auto show only after a short delay to stop the window
    // from hiding due to spurios focus events after a system resize.
    auto_show_lock("user-positioning", 1.0);
}

void KeyboardView::reposition(const Point& pt)
{
    ViewBase* toplevel = get_first_toplevel_view();
    if (!toplevel)
        return;

    // remember rects to distinguish from user move/resize
    Size sz = get_size();
    remember_rect({pt, sz});

    // In Trusty, Compiz, floating window with force-to-top enabled,
    // this window's configure-event lies about the window position
    // after un-hiding by auto-show, which leads to jumping when
    // moving || resizing afterwards.
    //
    // Test case:
    // 1) Start onboard with auto-show enabled
    // 2) Focus text entry to show the keyboard
    // 3) Move the keyboard
    // 4) Unfocus all text entries to hide the keyboard
    // 5) Focus text entry to show the keyboard
    // 6) Move the keyboard
    //    -> keyboard jumps to the previous position
    //
    // Workaround: force position change to get new, hopefully
    //             correct configure-events
    if (m_auto_position_started)
    {
        m_auto_position_started = false;
        auto callbacks = get_global_callbacks();
        if (callbacks->must_fix_configure_event &&
            callbacks->must_fix_configure_event())
            toplevel->move({pt.x-1, pt.y});
    }

    toplevel->move(pt);
}

// compute the union of the LayoutView's always_visible_rects
Rect KeyboardView::get_always_visible_rect()
{
    Noneable<Rect> bounds;
    for (auto& view : get_keyboard_layout_views())
    {
        if (bounds.is_none())
        {
            Rect r = view->get_always_visible_rect();
            if (!r.empty())
            {
                if (bounds.is_none())
                    bounds = r;
                else
                    bounds = r.union_(bounds);
            }
        }
    }
    return bounds;
}

MouseButton::Enum KeyboardView::get_button_from_mask(OnboardOskStateMask mask)
{
    if (mask & ONBOARD_OSK_BUTTON1_MASK)
        return MouseButton::LEFT;
    if (mask & ONBOARD_OSK_BUTTON2_MASK)
        return MouseButton::MIDDLE;
    if (mask & ONBOARD_OSK_BUTTON2_MASK)
        return MouseButton::RIGHT;
    return MouseButton::NONE;
}

