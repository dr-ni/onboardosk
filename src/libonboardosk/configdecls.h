#ifndef CONFIGDECLS_H
#define CONFIGDECLS_H

#include <map>
#include <ostream>
#include <string>

#include "layoutdecls.h"

struct LabelOverrideValue {
    std::string label, group;
    bool operator==(const LabelOverrideValue& other) const
    { return label == other.label && group == other.group;}
};
typedef  std::map<KeyId, LabelOverrideValue> LabelOverrideMap;

inline std::ostream& operator<<(std::ostream& s, const LabelOverrideValue& val){
    s << "LabelOverrideValue(" << val.label << ", " << val.group << ")";
    return s;
}

struct SnippetsValue {
    std::string label, text;
    bool operator==(const SnippetsValue& other) const
    { return label == other.label && text == other.text;}
};
typedef std::map<int, SnippetsValue> SnippetsMap;

inline std::ostream& operator<<(std::ostream& s, const SnippetsValue& val){
    s << "SnippetsValue(" << val.label << ", " << val.text << ")";
    return s;
}

class StatusIconProvider
{
    public:
        enum Enum {
            AUTO,
            GTKSTATUSICON,
            APPINDICATOR,
        };
};

class AspectChangeRange
{
    public:
        bool operator!=(const AspectChangeRange& other)
        {return !operator==(other);}

        bool operator==(const AspectChangeRange& other)
        {
            return min_aspect == other.min_aspect &&
                   max_aspect == other.max_aspect;
        }

    public:
        double min_aspect{};
        double max_aspect{100};
};

inline std::ostream& operator<<(std::ostream& s, const AspectChangeRange& acr){
    s << "AspectChangeRange(" << acr.min_aspect << ", " << acr.max_aspect << ")";
    return s;
}

#endif // CONFIGDECLS_H
