
#include "tools/container_helpers.h"
#include "tools/logger.h"
#include "tools/point.h"
#include "tools/time_helpers.h"

#include "configuration.h"
#include "inputeventreceiver.h"
#include "inputsequence.h"
#include "inputsequencetarget.h"
#include "timer.h"
#include "toplevelview.h"


template<typename T>
constexpr T square(T x) { return x*x;}

// [ms] until two finger tap&drag is detected
static const std::chrono::milliseconds GESTURE_DETECTION_SPAN{100};

// [ms] Suspend delayed sequence begin for this
// amount of time after the last key press.
static const std::chrono::milliseconds GESTURE_DELAY_PAUSE{3000};

// No delivery, i.e. no key-presses after
// gesture detection, but delays press-down.
static constexpr const bool DELAY_SEQUENCE_BEGIN = true;

// square of the distance in pixels until
// a drag gesture is detected.
static constexpr const int DRAG_GESTURE_THRESHOLD2 = square(40);


std::unique_ptr<InputEventReceiver> InputEventReceiver::make(const ContextBase& context,
                                                             InputSequenceTarget* target)
{
    return std::make_unique<InputEventReceiver>(context, target);
}


InputEventReceiver::InputEventReceiver(const ContextBase& context,
                                       InputSequenceTarget* target) :
    Super(context),
    m_target(target),
    m_gesture_timer(std::make_unique<Timer>(context))
{}

InputEventReceiver::~InputEventReceiver()
{}

bool InputEventReceiver::has_input_sequences() const
{
    return !m_input_sequences.empty();
}

bool InputEventReceiver::last_event_was_touch() const
{
    return m_last_event_was_touch;
}

bool InputEventReceiver::can_handle_pointer_event(const OnboardOskEvent* event) const
{
    return !config()->are_touch_events_enabled() ||
           event->source_device_type != ONBOARD_OSK_TOUCHSCREEN_DEVICE ||
           !is_device_touch_active(event->source_device_id);
}

void InputEventReceiver::set_device_touch_active(const OnboardOskDeviceId device_id)
{
    m_touch_active.emplace_back(device_id);
}

bool InputEventReceiver::is_device_touch_active(const OnboardOskDeviceId device_id) const
{
    return std::find(m_touch_active.begin(), m_touch_active.end(), device_id)
            != m_touch_active.end();
}

void InputEventReceiver::clear_touch_active()
{
    m_touch_active.clear();
}

void InputEventReceiver::on_event(OnboardOskEvent* event)
{
    switch (event->type)
    {
        case ONBOARD_OSK_EVENT_BUTTON_PRESS:
            on_button_press_event(event);
            break;
        case ONBOARD_OSK_EVENT_BUTTON_RELEASE:
            on_button_release_event(event);
            break;
        case ONBOARD_OSK_EVENT_MOTION:
            on_motion_event(event);
            break;
        case ONBOARD_OSK_EVENT_TOUCH_BEGIN:
            on_touch_event(event);
            break;
        case ONBOARD_OSK_EVENT_TOUCH_UPDATE:
            on_touch_event(event);
            break;
        case ONBOARD_OSK_EVENT_TOUCH_END:
            on_touch_event(event);
            break;
        case ONBOARD_OSK_EVENT_TOUCH_CANCEL:
            on_touch_event(event);
            break;
        case ONBOARD_OSK_EVENT_ENTER:
            on_enter_notify_event(event);
            break;
        case ONBOARD_OSK_EVENT_LEAVE:
            on_leave_notify_event(event);
            break;

        default:
            LOG_WARNING << "unknown event received: " << event->type;
    }
}

bool InputEventReceiver::on_enter_notify_event(OnboardOskEvent* event)
{
    m_target->on_enter_notify(event);
    return true;
}

bool InputEventReceiver::on_leave_notify_event(OnboardOskEvent* event)
{
    m_target->on_leave_notify(event);
    return true;
}

bool InputEventReceiver::on_button_press_event(OnboardOskEvent* event)
{
    LOG_EVENT << "on_button_press_event1 " << config()->are_touch_events_enabled()
              << " " << can_handle_pointer_event(event)
              << " " << event->source_device_id
              << " " << event->source_device_type;

    if (!can_handle_pointer_event(event))
    {
        LOG_EVENT << "_on_button_press_event2 abort";
        return false;
    }

    // - Ignore double clicks (GDK_2BUTTON_PRESS),
    //   we're handling those ourselves.
    // - Ignore mouse wheel button events
    LOG_EVENT << "on_button_press_event3 " << event->type
              << " " << event->button;
    if (event->type == ONBOARD_OSK_EVENT_BUTTON_PRESS &&
        1 <= event->button && event->button <= 3)
    {
        auto sequence = std::make_shared<InputSequence>();
        sequence->init_from_button_event(event);
        sequence->primary = true;
        m_last_event_was_touch = false;

        LOG_EVENT << "on_button_press_event4";
        input_sequence_begin(sequence);
    }

    return true;
}


bool InputEventReceiver::on_button_release_event(OnboardOskEvent* event)
{
    auto sequence = get_value(m_input_sequences, InputSequence::POINTER_SEQUENCE);
    LOG_EVENT << "on_button_release_event " << sequence;
    if (sequence)
    {
        sequence->point      = {event->x, event->y};
        sequence->root_point = {event->x_root, event->y_root};
        sequence->time       = event->time;

        input_sequence_end(sequence);
    }

    return true;
}

bool InputEventReceiver::on_motion_event(OnboardOskEvent* event)
{
    if (!can_handle_pointer_event(event))
        return false;

    auto sequence = get_value(m_input_sequences, InputSequence::POINTER_SEQUENCE);
    if (!sequence &&
        !(event->state & ONBOARD_OSK_BUTTON123_MASK))
    {
        sequence = std::make_shared<InputSequence>();
        sequence->primary = true;
    }

    if (sequence)
    {
        sequence->init_from_motion_event(event);

        m_last_event_was_touch = false;
        input_sequence_update(sequence);
    }

    return true;
}

bool InputEventReceiver::on_touch_event(OnboardOskEvent* event)
{
    LOG_EVENT << "on_touch_event1 " << event->source_device_id
              << " " << event->source_device_id;

    // Set source_device touch-active to block processing of pointer events.
    // "touch-screens" that don't send touch events will keep having pointer
    // events handled (Wacom devices with gestures enabled).
    // This assumes that for devices that emit both touch && pointer
    // events, the touch event comes first. Else there will be a dangling
    // touch sequence. _discard_stuck_input_sequences would clean that up,
    // but a key might get still get stuck in pressed state.
    set_device_touch_active(event->source_device_id);

    if (!can_handle_touch_event(event))
    {
        LOG_EVENT << "_on_touch_event2 abort";
        return false;
    }

    m_last_event_was_touch = true;

    if (event->type == ONBOARD_OSK_EVENT_TOUCH_BEGIN)
    {
        auto sequence = std::make_shared<InputSequence>();
        sequence->init_from_touch_event(event);
        if (m_input_sequences.empty())
            sequence->primary = true;

        input_sequence_begin(sequence);
    }

    else if (event->type == ONBOARD_OSK_EVENT_TOUCH_UPDATE)
    {
        auto sequence = get_value(m_input_sequences, event->sequence_id);
        if (sequence)
        {
            sequence->point       = {event->x, event->y};
            sequence->root_point  = {event->x_root, event->y_root};
            sequence->time        = event->time;
            sequence->update_time = milliseconds_since_epoch();

            input_sequence_update(sequence);
        }
    }

    else
    {
        if (event->type == ONBOARD_OSK_EVENT_TOUCH_END)
        {
        }
        else if (event->type == ONBOARD_OSK_EVENT_TOUCH_CANCEL)
        {
        }

        auto sequence = get_value(m_input_sequences, event->sequence_id);
        if (sequence)
        {
            sequence->time = event->time;
            input_sequence_end(sequence);
        }
    }

    return true;
}

void InputEventReceiver::input_sequence_begin(const InputSequencePtr& sequence)
{
    LOG_EVENT << "input_sequence_begin1 " << sequence;

    using namespace std::chrono;

    gesture_sequence_begin(sequence);

    bool first_sequence = m_input_sequences.empty();
    bool multi_touch_enabled = config()->is_multi_touch_enabled();

    if (first_sequence ||
        multi_touch_enabled)
    {
        m_input_sequences[sequence->id] = sequence;

        if (!m_gesture_detected)
        {
            if (first_sequence &&
                multi_touch_enabled &&
                DELAY_SEQUENCE_BEGIN &&
                milliseconds(sequence->time) -
                milliseconds(m_last_sequence_time) > GESTURE_DELAY_PAUSE &&
                m_target->can_delay_sequence_begin(sequence))  // ask Keyboard
            {
                // Delay the first tap; we may have to stop it from
                // reaching the keyboard.
                m_gesture_timer->start(GESTURE_DETECTION_SPAN,
                                      [sequence, this]() -> bool{return on_delayed_sequence_begin(sequence, sequence->point);});
            }
            else
            {
                // Tell the keyboard right away.
                this->deliver_input_sequence_begin(sequence);
            }
        }

        m_last_sequence_time = sequence->time;
    }
}

// Overloaded in LayoutView to veto delay for move buttons.


bool InputEventReceiver::on_delayed_sequence_begin(const InputSequencePtr& sequence, const Point& point)
{
    if (!m_gesture_detected)
    {
        // work around race condition
        sequence->point = point; // return to the original begin point
        deliver_input_sequence_begin(sequence);
        m_gesture_cancelled = true;
    }
    return false;   // one-shot
}

void InputEventReceiver::deliver_input_sequence_begin(const InputSequencePtr& sequence)
{
    LOG_EVENT << "deliver_input_sequence_begin " << sequence;
    m_target->on_input_sequence_begin(sequence);
    sequence->delivered = true;
}

// Pointer motion/touch update
void InputEventReceiver::input_sequence_update(const InputSequencePtr& sequence)
{
    gesture_sequence_update(sequence);
    if (!(sequence->state & ONBOARD_OSK_BUTTON123_MASK) ||
        !in_gesture_detection_delay(sequence))
    {
        m_gesture_timer->finish();   // run delayed begin before update
        m_target->on_input_sequence_update(sequence);
    }
}

void InputEventReceiver::input_sequence_end(const InputSequencePtr& sequence)
{
    LOG_EVENT << "input_sequence_end1 " << sequence;
    gesture_sequence_end(sequence);
    m_gesture_timer->finish();  // run delayed begin before end
    if (contains(m_input_sequences, sequence->id))
    {
        remove(m_input_sequences, sequence->id);

        if (sequence->delivered)
        {
            LOG_EVENT << "input_sequence_end2 " << sequence;
            m_target->on_input_sequence_end(sequence);
        }
    }

    if (!m_input_sequences.empty())
    {
        discard_stuck_input_sequences();
    }

    m_last_sequence_time = sequence->time;
}

// Input sequence handling requires guaranteed balancing of
// begin, update and end events. There is no indication yet this
// isn't always the case, but still, it seems like a good idea
// to prepare for the worst.
// -> Clear out aged input sequences, so Onboard can start from a
// fresh slate and not become terminally unresponsive.
void InputEventReceiver::discard_stuck_input_sequences()
{
    if (m_input_sequences.empty())
        return;

    InputSequenceTime expired_time = milliseconds_since_epoch() - 30*1000;
    for (auto it = m_input_sequences.begin();
         it != m_input_sequences.end(); )
    {
        auto it_erase = it++;
        auto& sequence = it_erase->second;
        if (sequence->update_time < expired_time)
        {
            LOG_WARNING << "discarding expired input sequence " << sequence->id;
            m_input_sequences.erase(it_erase);
        }
    }
}

bool InputEventReceiver::are_gestures_enabled()
{
    return config()->are_touch_events_enabled();
}

bool InputEventReceiver::in_gesture_detection_delay(
        const InputSequencePtr& sequence)
{
    using namespace std::chrono;
    auto span = milliseconds(sequence->time) -
                milliseconds(m_gesture_begin_time);
    return span < GESTURE_DETECTION_SPAN;
}

void InputEventReceiver::gesture_sequence_begin(
        const InputSequencePtr& sequence)
{
    // first tap?
    if (m_num_tap_sequences == 0)
    {
        m_gesture = Gesture::NONE;
        m_gesture_detected = false;
        m_gesture_cancelled = false;
        m_gesture_begin_point = sequence->point;
        m_gesture_begin_time = sequence->time; // event time
    }
    else
    { // subsequent taps
        if (in_gesture_detection_delay(sequence) &&
            !m_gesture_cancelled)
        {
            m_gesture_timer->stop();  // cancel delayed sequence begin
            m_gesture_detected = true;
        }
    }
    m_num_tap_sequences += 1;
}

void InputEventReceiver::gesture_sequence_update(
        const InputSequencePtr& sequence)
{
    if (m_gesture_detected &&
        sequence->state & ONBOARD_OSK_BUTTON123_MASK &&
        m_gesture == Gesture::NONE)
    {
        double d2 = m_gesture_begin_point.distance2(sequence->point);

        // drag gesture?
        if (d2 >= DRAG_GESTURE_THRESHOLD2)
        {
            int num_touches = static_cast<int>(m_input_sequences.size());
            m_gesture = Gesture::DRAG;
            m_target->on_drag_gesture_begin(num_touches);
        }
    }
}

void InputEventReceiver::gesture_sequence_end(
        const InputSequencePtr& sequence)
{
    if (m_input_sequences.size() == 1)  // last sequence of the gesture?
    {
        if (m_gesture_detected)
        {
            if (m_gesture == Gesture::NONE)
            {
                // tap gesture?
                auto elapsed = sequence->time - m_gesture_begin_time;
                if (elapsed <= 300)
                {
                    m_target->on_tap_gesture(m_num_tap_sequences);
                }
            }

            else if (m_gesture == Gesture::DRAG)
            {
                m_target->on_drag_gesture_end(0);
            }
        }

        m_num_tap_sequences = 0;
    }
}

void InputEventReceiver::redirect_sequence_update(
        const InputSequencePtr& sequence,
        const ViewBase* src, const ViewBase* dst,
        std::function<void (const InputSequencePtr&)> func)
{
    auto redir_sequence = get_redir_sequence(sequence, src, dst);
    func(redir_sequence);
}

void InputEventReceiver::redirect_sequence_end(
        const InputSequencePtr& sequence,
        const ViewBase* src, const ViewBase* dst,
        std::function<void (const InputSequencePtr&)> func)
{
    auto redir_sequence = get_redir_sequence(sequence, src, dst);

    // Make sure has_input_sequences() returns false inside of func().
    // Class Keyboard needs this to detect the end of input.
    remove(m_input_sequences, sequence->id);

    func(redir_sequence);
}

InputSequencePtr InputEventReceiver::get_redir_sequence(
        const InputSequencePtr& sequence,
        const ViewBase* src, const ViewBase* dst)
{
    auto redir_sequence = get_value(m_input_sequences, sequence->id);
    if (!redir_sequence)
    {
        redir_sequence = sequence->clone();
        redir_sequence->initial_active_key = nullptr;
        redir_sequence->active_key = nullptr;
        redir_sequence->cancel_key_action = false; // was canceled by long press

        m_input_sequences[redir_sequence->id] = redir_sequence;
    }

    // Don't rely on sequence->root_point, it might not always be
    // filled in (GNOME Shell).
    Point pt = src->canvas_to_root(sequence->point);
    redir_sequence->point = dst->root_to_canvas(pt);

    return redir_sequence;
}

