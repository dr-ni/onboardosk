#ifndef VERSION_H
#define VERSION_H

#include <ostream>
#include <string>

#ifdef major
#undef major
#endif

#ifdef minor
#undef minor
#endif

// Simple class to encapsulate a version number
class Version
{
    public:
        Version() = default;
        constexpr Version(int major_, int minor_=0) :
            major(major_),
            minor(minor_)
        {}

        std::string to_string() const;
        static Version from_string(std::string str);

        bool operator==(const Version& other) {return _cmp(other) == 0;}
        bool operator!=(const Version& other) {return _cmp(other) != 0;}
        bool operator< (const Version& other) {return _cmp(other) < 0;}
        bool operator<=(const Version& other) {return _cmp(other) <= 0;}
        bool operator> (const Version& other) {return _cmp(other) > 0;}
        bool operator>=(const Version& other) {return _cmp(other) >= 0;}

    private:
        int _cmp(const Version& other) const;

    public:
        int major{};
        int minor{};

};

inline std::ostream& operator<< (std::ostream& out, const Version& v) {
    out << v.to_string();
    return out;
}

#endif // VERSION_H
