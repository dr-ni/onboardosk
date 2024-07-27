#ifndef USTRINGREGEX_H
#define USTRINGREGEX_H

#include <memory>
#include <ostream>
#include <vector>

#ifndef U_CHARSET_IS_UTF8
#define U_CHARSET_IS_UTF8 1
#endif
#include <unicode/regex.h> // icu

#include "ustringmain.h"
#include "textdecls.h"

class RegexMatcher;
class RegexPattern;
class UStringPattern;
class UStringMatcher;


class UStringRegexFlag
{
    public:
        enum Enum
        {
            NONE = 0,
            DOTALL = UREGEX_DOTALL,
            VERBOSE = UREGEX_COMMENTS,
        };
};

inline UStringRegexFlag::Enum operator ~(UStringRegexFlag::Enum a)
{ return static_cast<UStringRegexFlag::Enum>(~static_cast<int>(a)); }
inline UStringRegexFlag::Enum operator | (UStringRegexFlag::Enum a, UStringRegexFlag::Enum b)
{ return static_cast<UStringRegexFlag::Enum>(static_cast<int>(a) | static_cast<int>(b)); }
inline UStringRegexFlag::Enum operator & (UStringRegexFlag::Enum a, UStringRegexFlag::Enum b)
{ return static_cast<UStringRegexFlag::Enum>(static_cast<int>(a) & static_cast<int>(b)); }
inline UStringRegexFlag::Enum& operator |= (UStringRegexFlag::Enum& a, UStringRegexFlag::Enum b)
{ a = a | b; return a;}


class UStringMatch
{
    public:
        std::vector<UString> groups;
        std::vector<Span> spans;
        Span span;

        UString group() const
        {
            if (!groups.empty())
                return groups[0];
            return {};
        }

        TextPos begin() const
        {return this->span.begin;}
        TextPos end() const
        {return this->span.end();}
        TextPos length() const
        {return this->span.length;}
};

std::ostream& operator<<(std::ostream& s, const UStringMatch& m);

class UStringMatcher
{
    public:
        UStringMatcher(const UStringPattern* pattern, const UString& text);
        bool find();

        UString group() const;
        UString group(size_t index) const;
        size_t group_count() const;
        TextPos begin() const;
        TextPos end() const;

    private:
        mutable UErrorCode m_status{};
        const UStringPattern* m_pattern;
        std::unique_ptr<icu::RegexMatcher> m_matcher;
};

class UStringPattern
{
    public:
        UStringPattern(const char* pattern_str,
                       UStringRegexFlag::Enum flags=UStringRegexFlag::NONE);
        UStringPattern(const std::string& pattern_str,
                       UStringRegexFlag::Enum flags=UStringRegexFlag::NONE);
        ~UStringPattern();
        UStringPattern(const UStringPattern& pattern) = delete;
        UStringPattern(UStringPattern&& pattern) noexcept :
            m_pattern(std::move(pattern.m_pattern))
        {
        }
        UStringPattern& operator=(const UStringPattern& pattern) = delete;
        UStringPattern& operator=(UStringPattern&& pattern) noexcept
        {
            m_pattern = std::move(pattern.m_pattern);
            return *this;
        }

        UString get_string() const;

        bool matches(const UString& text) const;
        bool find(const UString& text, UStringMatch& match) const;

        void find_all(const UString& text, std::vector<UStringMatch>& matches);
        void find_all(const UString& text, std::vector<UString>& tokens);
        void find_all(const UString& text, std::vector<UString>& tokens, std::vector<Span>& spans);

        void split_all(const UString& text, std::vector<UString>& tokens);

    private:
        void construct(const char* pattern_str, UStringRegexFlag::Enum flags);
        void fill_match(icu::RegexMatcher* matcher, UStringMatch& match, UErrorCode& status) const;

    private:
        std::unique_ptr<icu::RegexPattern> m_pattern;

        friend UStringMatcher;
};


#endif // USTRINGREGEX_H
