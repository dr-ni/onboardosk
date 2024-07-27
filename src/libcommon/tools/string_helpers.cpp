
#include <algorithm>
#include <regex>

#define U_CHARSET_IS_UTF8 1
#include <unicode/unistr.h>  // icu
#include <unicode/schriter.h>
#include <unicode/uchar.h>
//#include <unicode/utypes.h>   /* Basic ICU data types */
//#include <unicode/ucnv.h>     /* C   Converter API    */
//#include <unicode/ustring.h>  /* some more string fcns*/
//#include <unicode/uloc.h>

#include "tools/glib_helpers.h"
#include "tools/string_helpers.h"

// like python's split(), but no default delimiters yet
std::vector<std::string> split(const std::string& s)
{
    std::vector<std::string> elems;
    split(s, ',', std::back_inserter(elems));
    for (auto& e : elems)
        e = strip(e);
    return elems;
}
std::vector<std::string> split(const std::string& s, const char delim)
{
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}

std::string join(const std::vector<std::string>& v, const std::string& delim)
{
    std::string result;
    const size_t n = v.size();
    for (size_t i=0; i<n; ++i)
    {
        result += v[i];
        if (i<n-1)
            result += delim;
    }
    return result;
}

bool rfind_index(const std::string& s, const std::string& search, size_t& pos_out) {
    auto it = std::find_end (s.begin(), s.end(), search.begin(), search.end());
    if (it == s.end())
        return false;
    pos_out = static_cast<size_t>(it - s.begin());
    return true;
}

bool startswith(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;  // search backwards from position 0
}

bool endswith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() &&
           !s.compare(s.size()-suffix.size(), suffix.size(), suffix);
}

int to_int(const std::string& s, int base)
{
    try {
        return std::stoi(s, nullptr, base);
    } catch (const std::invalid_argument&) {
        return {};
    }
}

size_t to_size_t(const std::string& s, int base)
{
    try {
        return std::stoul(s, nullptr, base);
    } catch (const std::invalid_argument&) {
        return {};
    }
}



double to_double(const std::string& s)
{
    double d = 0.0;
    std::istringstream in(s);
    //in.imbue(std::locale("C"));
    in.imbue(std::locale::classic());
    in >> d;  // doesn't throw exception by default
    return d;
}

double to_double_with_locale(const std::string& s)
{
    try {
        return std::stod(s);
    } catch (const std::invalid_argument&) {
        return {};
    }
}

std::string repr(const int& value)
{
    return std::to_string(value);
}

std::string repr(const double& value)
{
    return std::to_string(value);
}

std::string repr(const bool& value)
{
    return value ? "true" : "false";
}

std::string repr(const std::string& value)
{
    return std::string("'") + value + "'";
}

// does not return the delimiter (unlike python re.split)
std::vector<std::string> re_split(const std::string& s, const std::string& delimiter)
{
    std::vector<std::string> tokens;
    std::regex regex(delimiter);

    std::sregex_token_iterator iter(s.begin(), s.end(), regex, -1);
    std::sregex_token_iterator end;

    while (iter != end)
    {
        tokens.emplace_back(*iter);
        ++iter;
    }

    return tokens;   // ought to move rather than copy, starting from C++11
}

std::vector<std::string> re_findall(const std::string& s, const std::string& pattern)
{
    std::vector<std::string> tokens;
    std::regex regex(pattern);

    std::sregex_token_iterator iter(s.begin(), s.end(), regex, 0);
    std::sregex_token_iterator end;

    while (iter != end)
    {
        tokens.emplace_back(*iter);
        ++iter;
    }

    return tokens;   // ought to move rather than copy, starting from C++11
}

// match the whole string and return the groups of the match
std::vector<std::string> re_match(const std::string& s, const std::string& pattern)
{
    std::vector<std::string> groups;

    std::regex regex(pattern);
    std::smatch matches;

    if(std::regex_match(s, matches, regex))
    {
        if (matches.size() == 1)   // no groups in pattern?
        {
            groups.emplace_back(matches[0].str());
        }
        else
        {
            for (size_t i = 0; i < matches.size(); ++i)
            {
                if (i >= 1)  // first group is the whole match
                    groups.emplace_back(matches[i].str());
                //std::cout << i << " '" << pattern << "': " << matches[i].str() << std::endl;
            }
        }
    }

    return groups;
}

// search the whole string and return the groups of the first match
std::vector<std::string> re_search(const std::string& s, const std::string& pattern)
{
    std::vector<std::string> groups;

    std::regex regex(pattern);
    std::smatch matches;

    if(std::regex_search(s, matches, regex))
    {
        if (matches.size() == 1)   // no groups in pattern?
        {
            groups.emplace_back(matches[0].str());
        }
        else
        {
            for (size_t i = 0; i < matches.size(); ++i)
            {
                if (i >= 1)  // first group is the whole match
                    groups.emplace_back(matches[i].str());
                //std::cout << i << " '" << pattern << "': " << matches[i].str() << std::endl;
            }
        }
    }

    return groups;
}

// like python's str.replace()
std::string replace_all(const std::string& str, const std::string& del, const std::string& insert)
{
    std::string s = str;
    if(!del.empty())
    {
        size_t pos = 0;
        while((pos = s.find(del, pos)) != std::string::npos)
        {
            s.replace(pos, del.length(), insert);
            pos += insert.length();
        }
    }
    return s;
}

// python like slicing for strings
// negative indices allowed
// no exception when out or range
std::string slice(const std::string& s, int begin, int end)
{
    int n = static_cast<int>(s.size());

    if (begin < 0)
        begin = n + begin;
    if (end < 0)
        end = n + end;

    if (begin >= n)
        return {};
    if (begin <= 0)
        begin = 0;
    if (end < 0)
        return {};
    if (end >= n)
        end = n;
    if (begin > end)
        return {};

    return s.substr(static_cast<size_t>(begin),
                    static_cast<size_t>(end-begin));
}

// Iterate over tag and non-tag sections of a markup string.
template<typename F>
void for_each_markup_element(const std::string& markup, const F& func)
{
    static std::regex tag_pattern(
        "(?:"
            "<[\\w\\-_]+"                            // tag
            "(?:\\s+[\\w\\-_]+=[\"'][^\"']*[\"'])*"  // attributes
            "/?>"
        ")|"
        "(?:"
            "</?[\\w\\-_]+>"
        ")");

    std::smatch::difference_type pos = 0;
    std::sregex_iterator iter(markup.begin(), markup.end(), tag_pattern);
    std::sregex_iterator end;
    std::string text;

    while (iter != end)
    {
        std::smatch m = *iter;
        text = slice(markup, static_cast<int>(pos),
                     static_cast<int>(m.position()));
        if (!text.empty())
             func(text, false);
        func(m.str(), true);
        pos = m.position() + m.length();
        ++iter;
    }
    text = slice(markup, static_cast<int>(pos));
    if (!text.empty())
        func(text, false);
}

// Escape strings of uncertain content for use in Gtk markup.
// If requested, markup tags are skipped && won't be escaped.
std::string escape_markup(const std::string& markup, bool preserve_tags)
{
    std::string result = "";
    for_each_markup_element(markup, [&](const std::string& s, bool is_tag)
    {
        if (is_tag && preserve_tags)
        {
            result += s;
        }
        else
        {
            GStrPtr escaped(
                g_markup_escape_text(s.c_str(), static_cast<gssize>(s.size())),
                g_free);
            if (escaped)
                result += escaped.get();
        }
    });
    return result;
}

// UTF-8 aware functions
std::string strip(const std::string &s)
{
    auto us = icu::UnicodeString::fromUTF8(s);
    std::string out;
    return us.trim().toUTF8String(out);
}

std::string lower(const std::string& s)
{
    auto us = icu::UnicodeString::fromUTF8(s);
    std::string out;
    return us.toLower().toUTF8String(out);
}

std::string upper(const std::string& s)
{
    auto us = icu::UnicodeString::fromUTF8(s);
    std::string out;
    return us.toUpper().toUTF8String(out);
}

std::string capitalize(const std::string& s)
{
    auto us = icu::UnicodeString::fromUTF8(s);
    if (us.isEmpty())
        return {};

    icu::StringCharacterIterator it(us);
    icu::UnicodeString first(it.next32PostInc());
    icu::UnicodeString rest(us, it.getIndex(), it.getLength());

    std::string out;
    return (first.toUpper() + rest).toUTF8String(out);
}

// http://userguide.icu-project.org/strings
bool isalpha_str(const std::string &s)
{
    if (s.empty())
        return false;

    auto us = icu::UnicodeString::fromUTF8(s);
    icu::StringCharacterIterator it(us);
    while(it.hasNext()) {
        UChar32 c = it.next32PostInc();
        if (!u_isalpha(c))
            return false;
    }
    return true;
}

bool isalnum_str(const std::string &s)
{
    if (s.empty())
        return false;

    auto us = icu::UnicodeString::fromUTF8(s);
    icu::StringCharacterIterator it(us);
    while(it.hasNext()) {
        UChar32 c = it.next32PostInc();
        if (!u_isalnum(c))
            return false;
    }
    return true;
}

bool isdigit_str(const std::string &s)
{
    if (s.empty())
        return false;

    auto us = icu::UnicodeString::fromUTF8(s);
    icu::StringCharacterIterator it(us);
    while(it.hasNext()) {
        UChar32 c = it.next32PostInc();
        if (!u_isdigit(c))
            return false;
    }
    return true;
}

bool has_more_codepoints_than(const std::string &s, size_t n)
{
    if (s.empty())
        return false;

    auto us = icu::UnicodeString::fromUTF8(s);
    return us.hasMoreChar32Than(0, INT32_MAX, n);
}

size_t count_codepoints(const std::string &s)
{
    if (s.empty())
        return false;

    auto us = icu::UnicodeString::fromUTF8(s);
    return static_cast<size_t>(us.countChar32(0, INT32_MAX));
}

long char_at(const std::string &s, size_t index)
{
    if (s.empty())
        return {};

    auto us = icu::UnicodeString::fromUTF8(s);

    size_t i = 0;
    icu::StringCharacterIterator it(us);
    while(it.hasNext()) {
        UChar32 c = it.next32PostInc();
        if (i == index)
            return c;
        i++;
    }
    return 0;
}



