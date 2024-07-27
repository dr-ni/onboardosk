#include "tools/string_helpers.h"

#include "version.h"

Version Version::from_string(std::string str)
{
    auto components = split(str, '.');

    int major = 0;
    int minor = 0;
    if (components.size() >= 1)
        major = stoi(components[0]);
    if (components.size() >= 2)
        minor = stoi(components[1]);

    return Version(major, minor);
}

int Version::_cmp(const Version& other) const
{
    if (major < other.major)
        return -1;
    if (major > other.major)
        return 1;
    if (minor < other.minor)
        return -1;
    if (minor > other.minor)
        return 1;
    return 0;
}

std::string Version::to_string() const
{
    return sstr() << major << "." << minor;
}

