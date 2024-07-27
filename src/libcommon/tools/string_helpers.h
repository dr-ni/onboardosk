#ifndef STRING_HELPERS_H
#define STRING_HELPERS_H

#include <climits>
#include <string>
#include <sstream>
#include <vector>

#include "tools/iostream_helpers.h"

inline bool contains(const std::string s, const std::string search)
{
    return s.find(search) != std::string::npos;
}

// stream into string
class sstr
{
    public:
        template <typename T>
        sstr& operator << (const T& value)
        {
            m_stream << value;
            return *this;
        }

        operator std::string () const
        {
            return m_stream.str();
        }

    private:
        std::stringstream m_stream;
};

template <typename... Ts>
std::string _fmt_str(const std::string &fmt, Ts... vs)
{
    char b;
    unsigned required = std::snprintf(&b, 0, fmt.c_str(), vs...) + 1;
    char bytes[required];
    std::snprintf(bytes, required, fmt.c_str(), vs...);
    return std::string(bytes);
}

template<typename Out>
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}

std::vector<std::string> split(const std::string &s);
std::vector<std::string> split(const std::string &s, char delim);
std::string join(const std::vector<std::string>& v, const std::string& delim={});

bool rfind_index(const std::string &s, const std::string& search, size_t& pos_out);

bool startswith(const std::string& s, const std::string& prefix);
bool endswith(const std::string& s, const std::string& suffix);

// base 0 = auto, i.e. 324 will be interpreted as
// decimal, but 0x324 will be as hex
int to_int(const std::string& s, int base=10);
size_t to_size_t(const std::string& s, int base=10);

template <typename T>
std::string to_hex(T n)
{
    std::stringstream ss;
    ss << std::hex << n;
    return ss.str();
}

// locale-independent, alway "." as decimal separator
double to_double(const std::string& s);

// locale-independent, "." or "," as decimal separator, sadly
double to_double_with_locale(const std::string& s);

template<class T>
std::string repr(const std::vector<T>& value)
{
    return to_string(value);
}

template<class T, std::size_t N>
std::string repr(const std::array<T, N>& value)
{
    return to_string(value);
}

template<class T>
std::string repr(const T* value)
{
    if (value == nullptr)
        return "null";
    return to_string(*value);
}

std::string repr(const int& value);
std::string repr(const double& value);
std::string repr(const bool& value);
std::string repr(const std::string& value);


std::vector<std::string> re_split(const std::string& s, const std::string& regex_delimiter);
std::vector<std::string> re_match(const std::string& s, const std::string& pattern);
std::vector<std::string> re_search(const std::string& s, const std::string& pattern);
std::vector<std::string> re_findall(const std::string& s, const std::string& pattern);

std::string replace_all(const std::string& str, const std::string& del, const std::string& insert);

// Don't let std::string and fs::path throw exceptions
// when assigning a nullptr.
inline std::string safe_assign(const char* s)
{
    if (s)
        return s;
    return {};
}

std::string slice(const std::string& s, int begin, int end=INT_MAX);
std::string escape_markup(const std::string& markup, bool preserve_tags=false);


// UTF-8 aware functions
std::string strip(const std::string &s);
std::string lower(const std::string& s);
std::string upper(const std::string& s);
std::string capitalize(const std::string& s);
bool isalpha_str(const std::string &s);
bool isalnum_str(const std::string &s);
bool isdigit_str(const std::string &s);
size_t count_codepoints(const std::string &s);
bool has_more_codepoints_than(const std::string &s, size_t n);
long char_at(const std::string &s, size_t index);


#endif // STRING_HELPERS_H
