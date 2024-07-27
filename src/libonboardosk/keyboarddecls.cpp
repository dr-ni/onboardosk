#include <map>

#include "tools/container_helpers.h"
#include "keyboarddecls.h"

std::string to_string(KeySynthEnum::Enum e)
{
    static std::map<KeySynthEnum::Enum, std::string> m = {
        {KeySynthEnum::AUTO, "AUTO"},
        {KeySynthEnum::XTEST, "XTEST"},
        {KeySynthEnum::UINPUT, "UINPUT"},
        {KeySynthEnum::ATSPI, "ATSPI"},
        {KeySynthEnum::GNOME_SHELL, "GNOME_SHELL"},
    };
    return get_value(m, e);
}

std::ostream&operator<<(std::ostream& out, KeySynthEnum::Enum& e)
{
    out << to_string(e) << "(" << static_cast<int>(e) << ")";
    return out;
}

MouseButton::Enum MouseButton::from_event_button(int button)
{
    if (button == 1)
        return LEFT;
    if (button == 2)
        return MIDDLE;
    if (button == 3)
        return RIGHT;
    if (button == 4)
        return WHEEL_UP;
    if (button == 5)
        return WHEEL_DOWN;
    return NONE;
}



PauseLearning::Enum PauseLearning::step(PauseLearning::Enum e)
{
    switch (e)
    {
        case OFF: return LATCHED;
        case LATCHED: return LOCKED;
        case LOCKED: return OFF;
    }
    return {};
}
