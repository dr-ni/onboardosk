#ifndef ONBOARDOSKEVENT_H
#define ONBOARDOSKEVENT_H

#include <stdint.h>  // uintptr_t

#include "tools/keydecls.h"

typedef enum
{
    ONBOARD_OSK_EVENT_NONE,
    ONBOARD_OSK_EVENT_BUTTON_PRESS,
    ONBOARD_OSK_EVENT_BUTTON_RELEASE,
    ONBOARD_OSK_EVENT_MOTION,
    ONBOARD_OSK_EVENT_TOUCH_BEGIN,
    ONBOARD_OSK_EVENT_TOUCH_UPDATE,
    ONBOARD_OSK_EVENT_TOUCH_END,
    ONBOARD_OSK_EVENT_TOUCH_CANCEL,
    ONBOARD_OSK_EVENT_ENTER,
    ONBOARD_OSK_EVENT_LEAVE,
    ONBOARD_OSK_EVENT_KEY_PRESS,
    ONBOARD_OSK_EVENT_KEY_RELEASE,

    ONBOARD_OSK_EVENT_DEVICE_ADDED = 1000,
    ONBOARD_OSK_EVENT_DEVICE_REMOVED,
    ONBOARD_OSK_EVENT_DEVICE_CHANGED,
    ONBOARD_OSK_EVENT_SLAVE_ATTACHED,
    ONBOARD_OSK_EVENT_SLAVE_DETACHED,
} OnboardOskEventType;

typedef enum   // = ClutterModifierType
{
    ONBOARD_OSK_EMPTY_MASK    = 0,

    ONBOARD_OSK_SHIFT_MASK    = 1 << 0,
    ONBOARD_OSK_CAPS_MASK     = 1 << 1,
    ONBOARD_OSK_CONTROL_MASK  = 1 << 2,
    ONBOARD_OSK_MOD1_MASK     = 1 << 3,
    ONBOARD_OSK_MOD2_MASK     = 1 << 4,
    ONBOARD_OSK_MOD3_MASK     = 1 << 5,
    ONBOARD_OSK_MOD4_MASK     = 1 << 6,
    ONBOARD_OSK_MOD5_MASK     = 1 << 7,
    ONBOARD_OSK_BUTTON1_MASK  = 1 << 8,
    ONBOARD_OSK_BUTTON2_MASK  = 1 << 9,
    ONBOARD_OSK_BUTTON3_MASK  = 1 << 10,
    ONBOARD_OSK_BUTTON4_MASK  = 1 << 11,
    ONBOARD_OSK_BUTTON5_MASK  = 1 << 12,

    ONBOARD_OSK_BUTTON123_MASK = ONBOARD_OSK_BUTTON1_MASK |
                                 ONBOARD_OSK_BUTTON2_MASK |
                                 ONBOARD_OSK_BUTTON3_MASK,
} OnboardOskStateMask;

typedef enum   // = ClutterInputDeviceType
{
    ONBOARD_OSK_POINTER_DEVICE,
    ONBOARD_OSK_KEYBOARD_DEVICE,
    ONBOARD_OSK_EXTENSION_DEVICE,
    ONBOARD_OSK_JOYSTICK_DEVICE,
    ONBOARD_OSK_TABLET_DEVICE,
    ONBOARD_OSK_TOUCHPAD_DEVICE,
    ONBOARD_OSK_TOUCHSCREEN_DEVICE,
    ONBOARD_OSK_PEN_DEVICE,
    ONBOARD_OSK_ERASER_DEVICE,
    ONBOARD_OSK_CURSOR_DEVICE,
    ONBOARD_OSK_UNKNOWN_DEVICE = 1000,
} OnboardOskDeviceType;

typedef uintptr_t OnboardOskDeviceId;
typedef int OnboardOskSequenceId;
typedef unsigned long OnboardOskEventTime;  // in ms (presumably) in clutter's case

struct _OnboardOskView;
typedef struct _OnboardOskView OnboardOskView;

struct _OnboardOskEvent
{
    OnboardOskEventType type;
    OnboardOskSequenceId sequence_id;
    double x;
    double y;
    double x_root;
    double y_root;
    int button;
    OnboardOskEventTime time;
    OnboardOskStateMask state;          // modifier state
    OnboardOskDeviceId device_id;
    KeyCode keycode;
    KeySym keysym;
    OnboardOskDeviceId source_device_id;
    OnboardOskDeviceType source_device_type;
    char source_device_name[128];
    OnboardOskView* toplevel;
};
typedef struct _OnboardOskEvent OnboardOskEvent;


#endif // ONBOARDOSKEVENT_H
