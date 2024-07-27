#include <chrono>

#include "tools/time_helpers.h"
#include "tools/point.h"

#include "inputsequence.h"
#include "layoutkey.h"


constexpr const InputSequenceId InputSequence::POINTER_SEQUENCE;

InputSequence::~InputSequence()
{}

void InputSequence::init_from_button_event(OnboardOskEvent* event)
{
    this->id          = POINTER_SEQUENCE;
    this->point       = {event->x, event->y};
    this->root_point  = {event->x_root, event->y_root};
    this->button      = MouseButton::from_event_button(event->button);
    this->time        = event->time;
    this->update_time = milliseconds_since_epoch();
}

void InputSequence::init_from_motion_event(OnboardOskEvent* event)
{
    this->id          = POINTER_SEQUENCE;
    this->point       = {event->x, event->y};
    this->root_point  = {event->x_root, event->y_root};
    this->state       = event->state;
    this->time        = event->time;
    this->update_time = milliseconds_since_epoch();
}

void InputSequence::init_from_touch_event(OnboardOskEvent* event)
{
    this->id          = event->sequence_id;
    this->point       = {event->x, event->y};
    this->root_point  = {event->x_root, event->y_root};
    this->button      = MouseButton::LEFT;
    this->state       = ONBOARD_OSK_EMPTY_MASK;  //Gdk.ModifierType.BUTTON1_MASK;
    this->time        = event->time;
    this->update_time = milliseconds_since_epoch();
}


std::ostream&operator<<(std::ostream& out, const InputSequencePtr& ptr)
{
    if (ptr)
        out << *ptr;
    else
        out << "nullptr";
    return out;
}

std::ostream&operator<<(std::ostream& out, const InputSequence& s)
{
    out << "InputSequence(id=" << s.id
        << " point=" << s.point
        << " root_point=" << s.root_point
        << " button=" << s.button
        << " state=" << s.state
        << " event_type=" << s.event_type
        << " time=" << s.time
        << " primary=" << s.primary
        << " delivered=" << s.delivered;

    if (s.active_item)
        out << " active_item=" << *s.active_item;
    else
        out << " active_item=" << "nullptr";

    if (s.active_key)
        out << " active_key=" << *s.active_key;
    else
        out << " active_key=" << "nullptr";

    out << ")";
    return out;
}

