
#include <algorithm>

#include "tools/logger.h"
#include "tools/point.h"
#include "tools/rect.h"

#include "configuration.h"
#include "keyboardanimator.h"
#include "keyboardkeylogic.h"
#include "keyboardview.h"
#include "timer.h"

using namespace std::chrono;

static constexpr const TransitionDuration
    TRANSITION_DURATION_MOVE = milliseconds(250);
static constexpr const TransitionDuration
    TRANSITION_DURATION_SLIDE = milliseconds(250);
static constexpr const TransitionDuration
    TRANSITION_DURATION_OPACITY_HIDE = milliseconds(300);


// A variable taking part in opacity/position state transitions
class TransitionVariable
{
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        ~TransitionVariable()
        {}

        // Begin transition
        void start_transition(double target, TransitionDuration duration)
        {
            m_start_value = this->value;
            target_value = target;
            m_start_time = Clock::now();
            m_duration = duration;
            m_done = false;
        }

        // Update m_value based on the elapsed time since start_transition.
        void update()
        {
            double lin_progress;
            double range = target_value - m_start_value;

            if (range != 0.0 && m_duration != TransitionDuration::zero())
            {
                using namespace std::chrono;
                auto elapsed  = Clock::now() - m_start_time;
                lin_progress = static_cast<double>(
                                   duration_cast<microseconds>(elapsed).count()) /
                               duration_cast<microseconds>(m_duration).count();
                lin_progress = std::min(lin_progress, 1.0);
            }
            else
            {
                lin_progress = 1.0;
            }
            double sin_progress =
                    (std::sin(lin_progress * M_PI - M_PI / 2.0) + 1.0) / 2.0;
            this->value = m_start_value + sin_progress * range;
            m_done = lin_progress >= 1.0;
        }

        bool is_done() const
        {
            return m_done;
        }

        void set_done(bool done=true)
        {
            m_done = done;
        }

        TransitionDuration get_duration() {return m_duration;}

    public:
        double value{0.0};
        double target_value{0.0};

    private:
        double m_start_value{0.0};
        TimePoint m_start_time{TimePoint::min()};
        TransitionDuration m_duration{TransitionDuration::zero()};
        bool m_done{false};
};


// Set of all state variables involved in transitions.
class TransitionState
{
    public:
        ~TransitionState()
        {}

        void update()
        {
            for (auto var : m_vars)
                var->update();
        }

        bool is_done() const
        {
            return std::all_of(m_vars.begin(), m_vars.end(),
                               [](const TransitionVariable* var) {return var->is_done();});
        }

        TransitionDuration get_max_duration() const
        {
            TransitionDuration d{};
            assert(TransitionDuration{} == TransitionDuration::zero());
            for (auto var : m_vars)
                if (!var->is_done())
                    d = std::max(d, var->get_duration());
            return d;
        }

        void set_all_done()
        {
            for (auto var : m_vars)
                var->set_done();
        }

    public:
        TransitionVariable visible;
        TransitionVariable active;   // for InactivityTimer
        TransitionVariable x;
        TransitionVariable y;

        bool target_visibility{false};

    private:
        vector<TransitionVariable*> m_vars{
            &this->visible,
            &this->active,
            &this->x, &this->y};

};



KeyboardAnimator::KeyboardAnimator(const ContextBase& context) :
    ContextBase(context),
    m_transition_state(std::make_unique<TransitionState>()),
    m_transition_timer(std::make_unique<Timer>(context))
{}

KeyboardAnimator::~KeyboardAnimator()
{}

bool KeyboardAnimator::transition_visible_to(
        bool visible,
        Noneable<TransitionDuration> opacity_duration,
        Noneable<TransitionDuration> slide_duration)
{
    bool result = false;
    auto& state = m_transition_state;
    auto keyboard_view = get_keyboard_view();

    // hide popup
    if (!visible)
        keyboard_view->close_key_popup();

    // bail in xembed mode
    if (config()->is_xid_mode())
    {
        return false;
    }

    // stop reposition updates when we're hiding anyway
    if (!visible)
        keyboard_view->stop_auto_positioning();

    bool opacity_visible;

    if (config()->is_docking_enabled())
    {
        if (slide_duration.is_none())
            slide_duration = milliseconds(TRANSITION_DURATION_SLIDE);

        opacity_duration = milliseconds(0);
        opacity_visible = true;

        bool visible_before = keyboard_view->is_visible();
        bool visible_later  = visible;
        Rect hideout_old_mon = keyboard_view->get_docking_hideout_rect();
        bool mon_changed = keyboard_view->update_docking_monitor_index();
        Rect hideout_new_mon;
        if (mon_changed)
            hideout_new_mon = keyboard_view->get_docking_hideout_rect();
        else
            hideout_new_mon = hideout_old_mon;

        // Only position here if visibility or the active monitor
        // changed. Leave it to auto_position to move the keyboard
        // while it is visible, i.e. not being hidden or shown.
        Rect begin_rect;
        Rect end_rect;
        if (visible_before != visible_later || mon_changed)
        {
            if (visible)
            {
                begin_rect = hideout_new_mon;
                end_rect = keyboard_view->get_visible_rect();
            }
            else
            {
                begin_rect = keyboard_view->get_rect();
                end_rect = hideout_old_mon;
            }
        }
        state->y.value = begin_rect.y;
        double y       = end_rect.y;
        state->x.value = begin_rect.x;
        double x       = end_rect.x;

        result |= init_transition(state->x, x, slide_duration);
        result |= init_transition(state->y, y, slide_duration);
    }
    else
    {
        opacity_visible  = visible;
    }

    if (opacity_duration.is_none())
    {
        if (opacity_visible)
        {
            // No duration when showing. Don't fight with compiz in unity.
            opacity_duration = TransitionDuration::zero();
        }
        else
        {
            opacity_duration = TRANSITION_DURATION_OPACITY_HIDE;
        }
    }
    result |= init_opacity_transition(state->visible, opacity_visible,
                                      opacity_duration);
    state->target_visibility = visible;

    return result;
}

bool KeyboardAnimator::transition_active_to(bool active,
                                            Noneable<TransitionDuration> duration)
{
    // not in xembed mode
    if (config()->is_xid_mode())
        return false;

    if (duration.is_none())
    {
        if (active)
            duration = milliseconds(150);
        else
            duration = milliseconds(300);
    }
    return init_opacity_transition(m_transition_state->active,
                                   active, duration);
}

bool KeyboardAnimator::transition_position_to(const Point& pt)
{
    bool result = false;
    auto& state = m_transition_state;
    auto duration = TRANSITION_DURATION_MOVE;

    // not in xembed mode
    if (config()->is_xid_mode())
        return false;

    Rect begin_rect = get_keyboard_view()->get_rect();
    state->y.value = begin_rect.y;
    state->x.value = begin_rect.x;

    result |= init_transition(state->x, pt.x, duration);
    result |= init_transition(state->y, pt.y, duration);

    return result;
}

void KeyboardAnimator::sync_transition_position(const Rect& rect)
{
    auto& state = m_transition_state;
    state->y.value        = rect.y;
    state->x.value        = rect.x;
    state->y.target_value = rect.y;
    state->x.target_value = rect.x;
}

bool KeyboardAnimator::init_opacity_transition(TransitionVariable& var,
                                               double target_value,
                                               TransitionDuration duration)
{
    // No fade delay for screens that can't fade (unity-2d)
    if (!is_composited())
        duration = TransitionDuration::zero();

    target_value = target_value != 0.0 ? 1.0 : 0.0;

    return init_transition(var, target_value, duration);
}

bool KeyboardAnimator::init_transition(TransitionVariable& var,
                                       double target_value,
                                       TransitionDuration duration)
{
    // Transition !yet in progress?
    if (var.target_value != target_value)
    {
        var.start_transition(target_value, duration);
        return true;
    }
    return false;
}

void KeyboardAnimator::commit_transition()
{
    // not in xembed mode
    if (config()->is_xid_mode())
        return;

    TransitionDuration duration = m_transition_state->get_max_duration();
    if (duration == TransitionDuration::zero())
    {
        on_transition_step();
    }
    else
    {
        m_transition_timer->start(milliseconds(20),
                                  std::bind(&KeyboardAnimator::on_transition_step,
                                            this));
    }
}

bool KeyboardAnimator::on_transition_step()
{
    auto keyboard = get_keyboard();
    auto keyboard_view = get_keyboard_view();

    auto& state = m_transition_state;
    state->update();

    bool done = state->is_done();
    bool commit = false;

    double active_opacity    = config()->window->get_active_opacity();
    double inactive_opacity  = config()->window->get_inactive_opacity();

    double opacity  = inactive_opacity + state->active.value *
                      (active_opacity - inactive_opacity);
    opacity *= state->visible.value;

    if (keyboard_view->set_opacity(opacity))
    {
        LOG_DEBUG << "opacity=" << opacity;
        commit = true;
    }

    bool visible_before = keyboard_view->is_visible();
    bool visible_later  = state->target_visibility;

    // move
    Point ptview = keyboard_view->get_rect().get_position();
    Point pt{state->x.value, state->y.value};
    if (ptview != pt)
    {
        LOG_DEBUG << "moving to " << pt;
        keyboard_view->reposition(pt);
    }

    // show/hide
    bool visible = ((visible_before || visible_later) && !done) ||
                   (visible_later && done);
    LOG_DEBUG << " visible_before=" << visible_before
              << " visible_later=" << visible_later
              << " visible=" << visible;
    if (keyboard_view->is_visible() != visible)
    {
        LOG_DEBUG << " set_visible " << visible;
        keyboard_view->set_visible(visible);

        // on_leave_notify does not start the inactivity timer
        // while the pointer remains inside of the window. Do it
        // here when hiding the window.
        if (!visible)
            keyboard_view->begin_inactivity_timer_transition(false);

        // start/stop on-hide-release timer
        get_keyboard()->get_key_logic()->start_auto_release_timer(visible);

        if (done)
            keyboard_view->on_transition_done(visible_before, visible_later);
    }

    // Mark all variables done, so they don't
    // influence get_max_duration() next time.
    if (done)
        state->set_all_done();

    if (commit)
        keyboard->commit_ui_updates();
    return !done;
}

