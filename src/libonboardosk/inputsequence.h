#ifndef INPUTSEQUENCE_H
#define INPUTSEQUENCE_H

#include <memory>
#include <ostream>

#include "tools/point_decl.h"

#include "keyboarddecls.h"
#include "layoutdecls.h"
#include "onboardoskevent.h"


typedef int InputSequenceId;
typedef int64_t InputSequenceTime;

class InputSequence
{
    public:
        static constexpr const InputSequenceId POINTER_SEQUENCE = 0;

    public:
        InputSequenceId id{POINTER_SEQUENCE}; // sequence id, POINTER_SEQUENCE for mouse events
        Point point;                        // (x, y)
        Point root_point;                   // (x, y)
        MouseButton::Enum button{};         // GDK button number, 1 for touch
        EventType::Enum event_type{};       // Keyboard.EventType
        OnboardOskStateMask state{};        // Button and modifier state mask (Gdk.ModifierType)
        OnboardOskEventTime time{};         // event time
        InputSequenceTime update_time{};    // redundant, only used by _discard_stuck_input_sequences

        bool primary{false};                // Only primary sequences may move/resize windows.
        bool delivered{false};              // Sent to listeners (keyboard views)?

        LayoutItemPtr active_item{};        // LayoutItem currently controlled by this sequence.
        LayoutKeyPtr active_key{};          // Onboard key currently pressed by this sequence.
        LayoutKeyPtr initial_active_key{};  // First Onboard key pressed by this sequence.
        bool cancel_key_action{false};      // Cancel key action, e.g. due to long press.

    public:
        using Ptr = std::shared_ptr<InputSequence>;
        ~InputSequence();
        InputSequence& operator=(const InputSequence& s) = default;

        void init_from_button_event(OnboardOskEvent* event);
        void init_from_motion_event(OnboardOskEvent* event);
        void init_from_touch_event(OnboardOskEvent* event);

        bool is_touch()
        {
            return this->id != POINTER_SEQUENCE;
        }

        Ptr clone()
        {
            auto sequence = std::make_shared<InputSequence>();
            *sequence = *this;
            return sequence;
        }
};

typedef InputSequence::Ptr InputSequencePtr;

std::ostream& operator<<(std::ostream& out, const InputSequencePtr& ptr);
std::ostream& operator<<(std::ostream& out, const InputSequence& s);

#endif // INPUTSEQUENCE_H
