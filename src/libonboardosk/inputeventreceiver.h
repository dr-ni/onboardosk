#ifndef INPUTEVENTRECEIVER_H
#define INPUTEVENTRECEIVER_H

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "tools/point_decl.h"

#include "inputsequencedecls.h"
#include "keyboarddecls.h"
#include "onboardoskevent.h"
#include "onboardoskglobals.h"

class InputSequenceTarget;
class Timer;

typedef int InputSequenceId;
typedef int64_t InputSequenceTime;

class Gesture
{
    public:
        enum Enum {
        NONE,
        DRAG,
    };
};

// Unified handling of multi-touch sequences and conventional pointer input.
class InputEventReceiver : public ContextBase
{
    public:
        using Super = ContextBase;
        InputEventReceiver(const ContextBase& context,
                           InputSequenceTarget* target);
        virtual ~InputEventReceiver();

        static std::unique_ptr<InputEventReceiver> make(const ContextBase& context,
                                                        InputSequenceTarget* target);

        // Are any clicks/touches still ongoing?
        bool has_input_sequences() const;

        // Was there just a touch event?
        bool last_event_was_touch() const;

        // Rely on pointer events? true for non-touch devices
        // and wacom touch-screens with gestures enabled.
        bool can_handle_pointer_event(const OnboardOskEvent* event) const;

        // Rely on touch events? true for touch devices
        // and wacom touch-screens with gestures disabled.
        bool can_handle_touch_event(const OnboardOskEvent* event) const
        {
            return !can_handle_pointer_event(event);
        }

        void on_event(OnboardOskEvent* event);

        // redirect input sequence update to this
        void redirect_sequence_update(
            const InputSequencePtr& sequence,
            const ViewBase* src, const ViewBase* dst,
            std::function<void(const InputSequencePtr& sequence)> func);

        // Redirect input sequence end to this
        void redirect_sequence_end(const InputSequencePtr& sequence,
            const ViewBase* src, const ViewBase* dst,
            std::function<void(const InputSequencePtr& sequence)> func);

    private:
        // Mark source device as actively receiving touch events
        void set_device_touch_active(const OnboardOskDeviceId device_id);
        bool is_device_touch_active(const OnboardOskDeviceId device_id) const;
        void clear_touch_active();

        bool on_enter_notify_event(OnboardOskEvent* event);
        bool on_leave_notify_event(OnboardOskEvent* event);
        bool on_button_press_event(OnboardOskEvent* event);
        bool on_button_release_event(OnboardOskEvent* event);
        bool on_motion_event(OnboardOskEvent* event);
        bool on_touch_event(OnboardOskEvent* event);

        // Button press/touch begin
        void input_sequence_begin(const InputSequencePtr& sequence);
        bool on_delayed_sequence_begin(const InputSequencePtr& sequence, const Point& point);
        void deliver_input_sequence_begin(const InputSequencePtr& sequence);

        void input_sequence_update(const InputSequencePtr& sequence);
        // Button release/touch end
        void input_sequence_end(const InputSequencePtr& sequence);

        void discard_stuck_input_sequences();

        bool are_gestures_enabled();

        // Are we still in the time span where sequence begins aren't delayed
        // and can't be undone after gesture detection?
        bool in_gesture_detection_delay(const InputSequencePtr& sequence);

        void gesture_sequence_begin(const InputSequencePtr& sequence);
        void gesture_sequence_update(const InputSequencePtr& sequence);
        void gesture_sequence_end(const InputSequencePtr& sequence);

        // Return a copy of <sequence>, managed in the target window.
        InputSequencePtr get_redir_sequence(
            const InputSequencePtr& sequence,
            const ViewBase* src, const ViewBase* dst);

    private:
        InputSequenceTarget* m_target{};

        using InputSequenceMap = std::map<InputSequenceId, InputSequencePtr>;
        InputSequenceMap m_input_sequences;

        bool m_last_event_was_touch{false};
        InputSequenceTime m_last_sequence_time{0};

        // Set of source_device_ids  (ClutterDevice id, XIDevice id,
        // GdkX11DeviceXI2 id, ...).
        // For devices not contained here only pointer events are considered.
        // Wacom devices with enabled gestures never become touch-active,
        // i.e. they don't generate touch events.
        std::vector<OnboardOskDeviceId> m_touch_active;

        bool m_gesture_detected{false};
        bool m_gesture_cancelled{false};
        Gesture::Enum m_gesture;
        Point m_gesture_begin_point;
        OnboardOskEventTime m_gesture_begin_time{};

        int m_num_tap_sequences{0};

        std::unique_ptr<Timer> m_gesture_timer;
};

#endif // INPUTEVENTRECEIVER_H
