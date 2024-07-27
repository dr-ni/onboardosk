
#include "tools/container_helpers.h"
#include "tools/logger.h"
#include "tools/point.h"
#include "tools/rect.h"

#include "atspistatetracker.h"
#include "autoshow.h"
#include "configuration.h"
#include "hardwaresensortracker.h"
#include "keyboardanimator.h"
#include "keyboardview.h"
#include "layoutview.h"
#include "timer.h"
#include "udevtracker.h"
#include "uielement.h"


AutoShow::AutoShow(const ContextBase& context) :
    ContextBase(context),
    m_auto_show_timer(std::make_unique<Timer>(context))
{}

AutoShow::~AutoShow()
{
    reset();
    enable(false);  // disconnect events
}

void AutoShow::reset()
{
    m_auto_show_timer->stop();
    unlock_all();
}

void AutoShow::enable(bool enable)
{
    if (enable)
    {
        if (!m_atspi_state_tracker)
        {
            m_atspi_state_tracker = get_atspi_state_tracker();
            if (m_atspi_state_tracker)
            {
                m_atspi_state_tracker->text_entry_activated.connect(
                    this, [&](const UIElementPtr& e){on_text_entry_activated(e);});
                m_atspi_state_tracker->text_caret_moved.connect(
                    this, [&](const ASyncEvent&){on_text_caret_moved();});
            }
        }
    }
    else
    {
        if (m_atspi_state_tracker)
        {
            m_atspi_state_tracker->text_entry_activated.disconnect(this);
            m_atspi_state_tracker->text_caret_moved.disconnect(this);
        }
        m_atspi_state_tracker = nullptr;
        m_active_ui_element = nullptr;  // stop using m_atspi_state_tracker
    }

    if (enable)
    {
        m_lock_visible = false;
        m_locks.clear();
    }

    enable_tablet_mode_detection(
                enable && config()->is_tablet_mode_detection_enabled());

    enable_keyboard_device_detection(
                enable && config()->is_keyboard_device_detection_enabled());
}

void AutoShow::enable_tablet_mode_detection(bool enable)
{
    if (enable)
    {
        if (!m_hw_sensor_tracker)
        {
            m_hw_sensor_tracker = get_hardware_sensor_tracker();
            if (m_hw_sensor_tracker)
                m_hw_sensor_tracker->tablet_mode_changed.connect(
                    this, [&](bool active){on_tablet_mode_changed(active);});
        }

        // Run/stop GlobalKeyListener when tablet-mode-enter-key or
        // tablet-mode-leave-key change.
        if (m_hw_sensor_tracker)
            m_hw_sensor_tracker->update_sensor_sources();
    }
    else
    {
        if (m_hw_sensor_tracker)
            m_hw_sensor_tracker->tablet_mode_changed.disconnect(this);
        m_hw_sensor_tracker = nullptr;
    }

    LOG_DEBUG << "enable_tablet_mode_detection " << enable << " " << m_hw_sensor_tracker;
}

void AutoShow::enable_keyboard_device_detection(bool enable)
{
    if (enable)
    {
        if (!m_udev_tracker)
        {
            m_udev_tracker = get_udev_tracker();
            if (m_udev_tracker)
            {
                m_udev_tracker->keyboard_detection_changed.connect(
                    this, [&](bool detected){on_keyboard_device_detection_changed(detected);});
            }
        }
    }
    else
    {
        if (m_udev_tracker)
        {
            m_udev_tracker->keyboard_detection_changed.disconnect(this);
        }
        m_udev_tracker = nullptr;
    }

    LOG_DEBUG << "enable_keyboard_device_detection " << enable << " " << m_udev_tracker;
}

void AutoShow::lock(const std::string& reason,
                    const Noneable<double>& duration,
                    bool lock_show, bool lock_hide)
{
    // Discard pending hide/show actions.
    m_auto_show_timer->stop();

    auto& lock = set_default(m_locks, reason);

    if (lock.timer)
        lock.timer->stop();

    if (duration.is_none())
    {
        lock.timer = nullptr;
    }
    else
    {
        lock.timer = std::make_unique<Timer>(*this);
        lock.timer->start(duration, [this, reason]() -> bool
            {return on_lock_timer(reason);});
    }

    lock.lock_show = lock_show;
    lock.lock_hide = lock_hide;

    LOG_DEBUG << "lock(" << repr(reason) << ") " << get_keys(m_locks);
}

bool AutoShow::on_lock_timer(const std::string& reason)
{
    unlock(reason);
    return false;
}

Noneable<bool> AutoShow::unlock(const std::string& reason)
{
    Noneable<bool> result;
    auto it = find_key(m_locks, reason);
    if (it != m_locks.end())
    {
        auto& lock = it->second;
        result = lock.visibility_change;
        if (lock.timer)
            lock.timer->stop();

        // We are potentially in a callback of lock.timer
        // via on_lock_timer and cannot delete the timer until
        // the callback returns. Save it and forget it.
        m_retired_timer = lock.timer;

        m_locks.erase(it);
    }

    LOG_DEBUG << "unlock(" << repr(reason) << ") " << get_keys(m_locks);

    return result;
}

void AutoShow::unlock_all()
{
    for (auto& e : m_locks)
    {
        auto& lock = e.second;
        if (lock.timer)
            lock.timer->stop();
    }
    m_locks.clear();
}

bool AutoShow::is_locked(const std::string& reason)
{
    return contains(m_locks, reason);
}

bool AutoShow::is_show_locked()
{
    for (auto& e : m_locks)
    {
        auto& lock = e.second;
        if (lock.lock_show)
            return true;
    }
    return false;
}

bool AutoShow::is_hide_locked()
{
    for (auto& e : m_locks)
    {
        auto& lock = e.second;
        if (lock.lock_hide)
            return true;
    }
    return false;
}

void AutoShow::lock_visible(bool lock, const Noneable<double>& thaw_time)
{
    LOG_DEBUG << "lock_visible(" << repr(lock) << ", " << thaw_time << ")";

    // Permanently lock visible.
    m_lock_visible = lock;

    // Temporarily stop showing/hiding.
    if (thaw_time != 0.0)
        AutoShow::lock("lock_visible", thaw_time, true, true);

    // Leave the window in its current state,
    // discard pending hide/show actions.
    m_auto_show_timer->stop();

    // Stop pending auto-repositioning
    if (lock)
        get_keyboard_view()->stop_auto_positioning();
}

bool AutoShow::is_text_entry_active()
{
    return m_active_ui_element != nullptr;
}

bool AutoShow::can_hide_keyboard()
{
    if (logger()->can_log(LogLevel::INFO))
    {
        std::vector<std::string> v;
        for (auto& e : m_locks)
            if (e.second.lock_hide)
                v.emplace_back(e.first);
        LOG_INFO << "can_hide_keyboard: " << "locks=" << join(v, ",");
    }

    return !is_hide_locked();
}

bool AutoShow::can_show_keyboard()
{
    bool result = true;

    std::string msg;
    if (logger()->can_log(LogLevel::INFO))
    {
        std::vector<std::string> v;
        for (auto& e : m_locks)
            if (e.second.lock_show)
                v.emplace_back(e.first);
        msg = sstr() << "locks=" << join(v, ",");
    }

    if (!m_locks.empty())
    {
        result = false;
    }
    else
    {
        if (config()->is_tablet_mode_detection_enabled())
        {
            Noneable<bool> tablet_mode;
            if (m_hw_sensor_tracker)
                tablet_mode = m_hw_sensor_tracker->get_tablet_mode();

            msg += "tablet_mode=" + repr(tablet_mode) + " ";

            result = result &&
                     (tablet_mode.is_none() ||
                      tablet_mode == true);
        }

        if (config()->is_keyboard_device_detection_enabled())
        {
            Noneable<bool> detected;
            if (m_udev_tracker)
                detected = m_udev_tracker->is_keyboard_device_detected();

            msg += "keyboard_device_detected=" + repr(detected) + " ";

            result = result &&
                     (detected.is_none() ||
                      detected == true);
        }
    }

    LOG_INFO << "can_show_keyboard " <<  msg;

    return result;
}

void AutoShow::on_text_caret_moved()
{
    if (config()->auto_show->enabled &&
        !get_keyboard_view()->is_visible())
    {
        auto ui_element = m_active_ui_element;
        if (ui_element)
        {
            if (ui_element->is_single_line())
                on_text_entry_activated(ui_element);
        }
    }
}

void AutoShow::on_text_entry_activated(const UIElementPtr& ui_element)
{
    m_active_ui_element = ui_element;
    request_keyboard_visible(ui_element != nullptr);
}

void AutoShow::on_tablet_mode_changed(bool active)
{
    handle_tablet_mode_changed(active);
}

void AutoShow::on_keyboard_device_detection_changed(bool detected)
{
    handle_tablet_mode_changed(!detected);
}

void AutoShow::handle_tablet_mode_changed(bool tablet_mode_active)
{
    bool show;
    if (tablet_mode_active)
    {
        show = is_text_entry_active();
    }
    else
    {
        // hide keyboard even if it was locked visible
        lock_visible(false, 0.0);
        show = false;
    }

    request_keyboard_visible(show);
}

void AutoShow::request_keyboard_visible(bool visible, Noneable<double> delay)
{
    // Remember request per lock. That way we know the time span in
    // which the visibility change occurred.
    for (auto& e  : m_locks)
        e.second.visibility_change = visible;

    // Always allow to show the window even when locked.
    // Mitigates right click on unity-2d launcher hiding
    // onboard before _lock_visible is set (Precise).
    if (m_lock_visible)
        visible = true;

    bool can_hide = can_hide_keyboard();
    bool can_show = can_show_keyboard();

    LOG_DEBUG << "visible=" << repr(visible)
              << " lock_visible=" << repr(m_lock_visible)
              << " can_hide=" << repr(can_hide)
              << " can_show=" << repr(can_show);

    if ((visible == false && can_hide) ||
        (visible == true  && can_show))
    {
        show_keyboard(visible, delay);
    }

    // The active ui_element changed, stop trying to
    // track the position of the previous one.
    // -> less erratic movement during quick focus changes
    get_keyboard_view()->stop_auto_positioning();
}

void AutoShow::show_keyboard(bool show, Noneable<double> delay)
{
    if (delay.is_none())
    {
        // Don't act on each && every focus message. Delay the start
        // of the transition slightly so that only the last of a bunch of
        // focus messages is acted on.
        delay = show ? m_show_reaction_time : m_hide_reaction_time;
    }

    if (delay == 0.0)
    {
        m_auto_show_timer->stop();
        begin_transition(show);
    }
    else
    {
        m_auto_show_timer->start(delay, [=]() -> bool {return begin_transition(show);});
    }
}

bool AutoShow::begin_transition(bool show)
{
    auto keyboard_view = get_keyboard_view();
    auto animator = keyboard_view->get_animator();
    animator->transition_visible_to(show);
    if (show)
        keyboard_view->auto_position();
    animator->commit_transition();
    return false;
}

bool AutoShow::get_repositioned_window_rect(Rect& result,
                                            const LayoutView* view,
                                            const Rect& home,
                                            const std::vector<Rect>& limit_rects,
                                            const Border& test_clearance, const Border& move_clearance,
                                            bool horizontal, bool vertical)
{
    auto ui_element = m_active_ui_element;
    if (!ui_element)
        return false;

    ui_element->invalidate_extents();
    Rect acc_rect = ui_element->get_extents();
    if (acc_rect.empty() ||
        m_lock_visible)
    {
        return false;
    }

    auto method = config()->get_auto_show_reposition_method();
    Noneable<Point> pt;

    // The home_rect doesn't include window decoration,
    // make sure to add decoration for correct clearance.
    Rect rh = home;

    /*
    window = view.get_kbd_window();     // TODO: not necessary in gnome-shell, though, no window decoration planned
    if (window)
    {
        offset = window.get_client_offset();
        rh.w += offset[0];
        rh.h += offset[1];
    }
    */

    // "Follow active window" method
    if (method == RepositionMethod::REDUCE_POINTER_TRAVEL)
    {
        Rect app_rect = ui_element->get_frame_extents();
        pt = find_close_position(view, rh,
                                 app_rect, acc_rect, limit_rects,
                                 test_clearance, move_clearance,
                                 horizontal, vertical);
    }

    // "Only move when necessary" method
    if (method == RepositionMethod::PREVENT_OCCLUSION)
    {
        pt = find_non_occluding_position(view, rh,
                                         acc_rect, limit_rects,
                                         test_clearance, move_clearance,
                                         horizontal, vertical);
    }
    if (!pt.is_none())
    {
        result = {pt.value.x, pt.value.y, home.w, home.h};
        return true;
    }
    return false;
}

Noneable<Point> AutoShow::find_close_position(const LayoutView* view, const Rect& home, const Rect& app_rect, const Rect& acc_rect, const std::vector<Rect>& limit_rects, const Border& test_clearance, const Border& move_clearance, bool horizontal, bool vertical)
{
    auto keyboard_view = get_keyboard_view();

    Rect rh = home;

    // Closer clearance for toplevels. There's usually nothing
    // that can be obscured.
    Border move_clearance_frame{10, 10, 10, 10};

    // Leave a different clearance for the new, yet to be found, positions.
    Rect ra = acc_rect.inflate(move_clearance);
    Rect rp;
    if (!app_rect.empty())
        rp = app_rect.inflate(move_clearance_frame);

    // candidate positions
    struct Candidate{Point pt; Rect r;};
    std::vector<Candidate> vp;
    if (vertical)
    {
        double xc = acc_rect.get_center().x - rh.w / 2;
        if (app_rect.w > rh.w)
        {
            xc = std::max(xc, app_rect.left());
            xc = std::min(xc, app_rect.right() - rh.w);
        }

        if (!app_rect.empty())
        {
            // below window
            vp.emplace_back(Candidate{{xc, rp.bottom()}, app_rect});

            // above window
            vp.emplace_back(Candidate{{xc, rp.top() - rh.h}, app_rect});
        }

        // inside maximized window, y at home.y
        vp.emplace_back(Candidate{{xc, home.y}, acc_rect});

        // below text entry
        vp.emplace_back(Candidate{{xc, ra.bottom()}, acc_rect});

        // above text entry
        vp.emplace_back(Candidate{{xc, ra.top() - rh.h}, acc_rect});
    }

    // limited, non-intersecting candidate rectangles
    Rect rresult;
    for (auto c : vp)
    {
        Point p = c.pt;
        Point pl = keyboard_view->limit_position(
            p, keyboard_view->get_canvas_rect(), limit_rects);
        Rect r = Rect(pl.x, pl.y, rh.w, rh.h);
        Rect ri = c.r;

        // test collision rects
        if (!r.intersects(ri) &&
            !r.intersects(acc_rect))
        {
            rresult = r;
            break;
        }
    }

    if (rresult.empty())
    {
        // try again, this time horizontally and vertically
        return find_non_occluding_position(view, home,
                                           acc_rect, limit_rects,
                                           test_clearance, move_clearance,
                                           horizontal, vertical);
    }
    else
    {
        return rresult.get_position();
    }
}

Noneable<Point> AutoShow::find_non_occluding_position(const LayoutView* view,
                                                      const Rect& home, const Rect& acc_rect,
                                                      const std::vector<Rect>& limit_rects,
                                                      const Border& test_clearance, const Border& move_clearance,
                                                      bool horizontal, bool vertical)
{
    (void)view;
    auto keyboard_view = get_keyboard_view();
    Rect rh = home;

    // Leave some clearance around the ui_element to account for
    // window frames and position errors of firefox entries.
    Rect ra = acc_rect.inflate(test_clearance);

    if (rh.intersects(ra))
    {
        // Leave a different clearance for the new,
        // yet to be found positions.
        ra = acc_rect.inflate(move_clearance);
        Point pt = rh.get_position();

        // candidate positions
        std::vector<Point> vp;

        if (horizontal)
        {
            vp.emplace_back(ra.left() - rh.w, pt.y);
            vp.emplace_back(ra.right(), pt.y);
        }
        if (vertical)
        {
            vp.emplace_back(pt.x, ra.top() - rh.h);
            vp.emplace_back(pt.x, ra.bottom());
        }

        // limited, non-intersecting candidate rectangles
        std::vector<Rect> vr;
        for (const auto& p : vp)
        {
            Point pl = keyboard_view->limit_position(
                p, keyboard_view->get_canvas_rect(), limit_rects);
            Rect r{pl.x, pl.y, rh.w, rh.h};
            if (!r.intersects(ra))
                vr.emplace_back(r);
        }

        // candidate with smallest center-to-center distance wins
        //chx, chy = rh.get_center();
        Point ch = rh.get_center();
        Noneable<double> dmin;
        Rect rmin;
        for (const auto& r : vr)
        {
            Point c= r.get_center();
            double d2 = ch.distance2(c);
            if (dmin.is_none() || dmin > d2)
            {
                dmin = d2;
                rmin = r;
            }
        }

        if (!dmin.is_none())
            return rmin.get_position();
    }

    return {};  // None
}

