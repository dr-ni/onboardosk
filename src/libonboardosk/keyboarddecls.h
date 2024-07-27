#ifndef KEYBOARDDECLS_H
#define KEYBOARDDECLS_H

#include <ostream>
#include <set>
#include <string>

#include "tools/keydecls.h"

#include "layoutdecls.h"


// Name of the /dev/uinput device when key-synth is set to uinput.
constexpr const char* UINPUT_DEVICE_NAME = "Onboard on-screen keyboard";


class KeySynthEnum
{
    public:
        enum Enum {
            AUTO,
            XTEST,
            UINPUT,
            ATSPI,
            GNOME_SHELL,
        };
};
std::string to_string(KeySynthEnum::Enum e);
std::ostream& operator<< (std::ostream& out, KeySynthEnum::Enum& e);


class TouchInputMode
{
    public:
        enum Enum {
        NONE,
        SINGLE,
        MULTI,
    };
};


class InputEventSourceEnum
{
    public:
        enum Enum {
            GTK,
            XINPUT,
        };
};

class MouseButton
{
    public:
        enum Enum {
            NONE,
            LEFT,
            MIDDLE,
            RIGHT,
            WHEEL_UP,
            WHEEL_DOWN,
        };

    public:
        static Enum from_event_button(int button);
};

// enum of event types for key press/release
class EventType
{
    public:
        enum Enum {
            CLICK,
            DOUBLE_CLICK,
            DWELL,
        };
};

class DockingEdge
{
    public:
        enum Enum {
            TOP = 0,
            BOTTOM = 3,
        };
};

class DockingMonitor
{
    public:
        enum Enum {
            ACTIVE,
            PRIMARY,
            MONITOR0,
            MONITOR1,
            MONITOR2,
            MONITOR3,
            MONITOR4,
            MONITOR5,
            MONITOR6,
            MONITOR7,
            MONITOR8,
        };
};

class SpellcheckBackend
{
    public:
        enum Enum {
            HUNSPELL,
            ASPELL,
            NONE=100,
        };
};

class LearningBehavior
{
    public:
        enum Enum {
            NOTHING,
            KNOWN_ONLY,
            ALL,
        };
};

// not in gsettings
class PauseLearning
{
    public:
        enum Enum {
            OFF = 0,
            LATCHED = 1,
            LOCKED = 2,
        };
        static Enum step(Enum e);
};

typedef std::set<LayoutItemPtr> ItemsToInvalidate;

#endif // KEYBOARDDECLS_H
