#ifndef ENUM_HELPERS_H
#define ENUM_HELPERS_H

#include <map>
#include <ostream>
#include <string>

template<class T>
void parse_make_enum_string(std::map<T, std::string>& dict, const std::string& str)
{
    size_t len = str.length();
    std::string name, value, temp;
    enum {LABEL, VALUE, DONE} state{};
    int n = 0;
    for(size_t i = 0; i < len; i++)
    {
        char c = str[i];
        if(isspace(c))
            continue;

        if (state == LABEL)
        {
            if(c == '=')
            {
                name = temp;
                temp.clear();
                state = VALUE;
            }
            else if(c == ',')
            {
                name = temp;
                temp.clear();
                state = DONE;
            }
            else
                temp += c;
        }
        else if(state == VALUE)
        {
            if(c == ',')
            {
                value = temp;
                temp.clear();
                state = DONE;
            }
            else
                temp += c;
        }

        if(state == DONE)
        {
            if (!value.empty())
                n = std::stoi(value, nullptr, 0);
            dict[static_cast<T>(n)] = name;
            n++;
            name.clear();
            value.clear();
            state = LABEL;
        }
    }
}

// Variadic macro for enums with strings.
// Last entry of the enum must end with a comma (else the enum element __COUNT can't be defined).
#define MAKE_ENUM(name, ...) enum class name { __VA_ARGS__ __COUNT}; \
inline std::string to_string(name value) { \
    std::map<name, std::string> dict; \
    parse_make_enum_string(dict, #__VA_ARGS__); \
    return dict[value]; } \
inline std::string strfull(name value) { return std::string(#name) + "::" + to_string(value);} \
inline std::ostream& operator<<(std::ostream& os, name value) { os << to_string(value) << "(" << static_cast<unsigned>(value) << ")"; return os; } \

#endif // ENUM_HELPERS_H
