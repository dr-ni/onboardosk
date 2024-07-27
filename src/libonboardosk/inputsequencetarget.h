#ifndef INPUTSEQUENCETARGET_H
#define INPUTSEQUENCETARGET_H

#include "inputsequencedecls.h"

struct _OnboardOskEvent;
typedef struct _OnboardOskEvent OnboardOskEvent;

class InputSequenceTarget
{
    public:
        virtual void on_input_sequence_begin(const InputSequencePtr& sequence) = 0;
        virtual void on_input_sequence_update(const InputSequencePtr& sequence) = 0;
        virtual void on_input_sequence_end(const InputSequencePtr& sequence) = 0;

        // Overloaded in LayoutView to veto delay for move buttons.
        virtual bool can_delay_sequence_begin(const InputSequencePtr& sequence)
        { (void)sequence; return true; }

        virtual void on_enter_notify(const OnboardOskEvent* event)
        {(void)event;}
        virtual void on_leave_notify(const OnboardOskEvent* event)
        {(void)event;}

        virtual void on_tap_gesture(int num_touches)
        { (void)num_touches; }

        virtual void on_drag_gesture_begin(int num_touches)
        { (void)num_touches; }

        virtual void on_drag_gesture_end(int num_touches)
        { (void)num_touches; }

};

#endif // INPUTSEQUENCETARGET_H
