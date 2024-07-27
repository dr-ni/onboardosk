
#include <map>

#include "tools/container_helpers.h"
#include "layoutdecls.h"

KeyStyle::Enum KeyStyle::to_key_style(const std::string s)
{
    static std::map<std::string, Enum> m = {
        {"flat", FLAT},
        {"gradient", GRADIENT},
        {"dish", DISH},
    };
    return get_value_default(m, s, Enum::NONE);
}

KeyAction::Enum KeyAction::layout_string_to_key_action(const std::string s)
{
    static std::map<std::string, Enum> m = {
        {"single-stroke",  SINGLE_STROKE},
        {"delayed-stroke", DELAYED_STROKE},
        {"double-stroke",  DOUBLE_STROKE},
        {"none",           NONE},
    };
    return get_value_default(m, s, Enum::NONE);
}

std::string to_string(KeyAction::Enum e)
{
    static std::map<KeyAction::Enum, std::string> m = {
        {KeyAction::SINGLE_STROKE, "SINGLE_STROKE"},
        {KeyAction::DELAYED_STROKE, "DELAYED_STROKE"},
        {KeyAction::DOUBLE_STROKE, "DOUBLE_STROKE"},
        {KeyAction::NONE, "NONE"},
    };
    return get_value(m, e);
}

std::ostream&operator<<(std::ostream& out, KeyAction::Enum& e)
{
    out << to_string(e) << "(" << static_cast<int>(e) << ")";
    return out;
}

