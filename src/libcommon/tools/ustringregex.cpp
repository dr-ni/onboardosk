#include <array>
#include <assert.h>

#include <unicode/regex.h> // icu

#include "ustringmain.h"
#include "iostream_helpers.h"

#include "ustringregex.h"


std::ostream& operator<<(std::ostream& s, const UStringMatch& m){
    s << "UStringMatch(" << m.groups << ", " << m.spans << ")";
    return s;
}

UStringPattern::UStringPattern(const char* pattern_str, UStringRegexFlag::Enum flags)
{
    construct(pattern_str, flags);
}

UStringPattern::UStringPattern(const std::string& pattern_str, UStringRegexFlag::Enum flags)
{
    construct(pattern_str.c_str(), flags);
}

void UStringPattern::construct(const char* pattern_str, UStringRegexFlag::Enum flags)
{
    UParseError pe;
    UErrorCode status = U_ZERO_ERROR;
    m_pattern = std::unique_ptr<icu::RegexPattern>(
                    icu::RegexPattern::compile(pattern_str, flags, pe, status));
    assert(U_SUCCESS(status));
}

UStringPattern::~UStringPattern()
{
}

UString UStringPattern::get_string() const
{
    return {m_pattern->pattern()};
}

bool UStringPattern::matches(const UString& text) const
{
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::RegexMatcher> matcher(m_pattern->matcher(text.m_us, status));
    return (matcher->matches(status));
}

bool UStringPattern::find(const UString& text, UStringMatch& match) const
{
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::RegexMatcher> matcher(m_pattern->matcher(text.m_us, status));
    if (matcher->find())
    {
        fill_match(matcher.get(), match, status);
        return true;
    }
    return false;
}

void UStringPattern::find_all(const UString& text, std::vector<UStringMatch>& matches)
{
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::RegexMatcher> matcher(m_pattern->matcher(text.m_us, status));
    while (matcher->find())
    {
        matches.emplace_back();
        UStringMatch& match = matches.back();
        fill_match(matcher.get(), match, status);
    }
}

void UStringPattern::find_all(const UString& text, std::vector<UString>& tokens)
{
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::RegexMatcher> matcher(m_pattern->matcher(text.m_us, status));
    while (matcher->find())
       tokens.emplace_back(UString(matcher->group(status)));
}

void UStringPattern::find_all(const UString& text, std::vector<UString>& tokens, std::vector<Span>& spans)
{
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::RegexMatcher> matcher(m_pattern->matcher(text.m_us, status));
    while (matcher->find())
    {
       TextPos start = matcher->start(status);
       TextPos end = matcher->end(status);
       TextLength length = end - start;
       tokens.emplace_back(UString(matcher->group(status)));
       spans.emplace_back(start, length);
    }
}

void UStringPattern::split_all(const UString& text, std::vector<UString>& tokens)
{
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::RegexMatcher> matcher(m_pattern->matcher(text.m_us, status));
    std::array<icu::UnicodeString, 5> strings;
    icu::UnicodeString remaining = text.m_us;

    tokens.clear();

    while (true)
    {
        std::string s3;
        remaining.toUTF8String(s3);

        size_t n = static_cast<size_t>(
            matcher->split(remaining, &strings[0], strings.size(), status));

        if (n == 0)
            break;

        if (n == 1)
        {
            tokens.emplace_back(UString(strings[0]));
            break;
        }

        for (size_t i=0; i<n-1; i++)
        {
            std::string s2 = UString(strings[i]).to_utf8();
            tokens.emplace_back(UString(strings[i]));
        }

        remaining = strings[n-1];
        if (remaining.isEmpty())
        {
            tokens.emplace_back(UString(remaining));
            break;
        }
    }
}

void UStringPattern::fill_match(icu::RegexMatcher* matcher, UStringMatch& match, UErrorCode& status) const
{
    TextPos start = matcher->start(status);
    TextPos end = matcher->end(status);
    TextLength length = end - start;
    match.span = {start, length};

    int n = matcher->groupCount();
    for (int i=0; i<n; i++)
    {
        UString s{matcher->group(i, status)};
        match.groups.emplace_back(s);

        start = matcher->start(i, status);
        end = matcher->end(i, status);
        length = end - start;
        match.spans.emplace_back(start, length);

    }
}




UStringMatcher::UStringMatcher(const UStringPattern* pattern, const UString& text) :
    m_pattern(pattern)
{
    m_matcher = std::unique_ptr<icu::RegexMatcher>(
                    m_pattern->m_pattern->matcher(text.m_us, m_status));
}

bool UStringMatcher::find()
{
    return m_matcher->find();
}

UString UStringMatcher::group() const
{
    return m_matcher->group(m_status);
}

UString UStringMatcher::group(size_t index) const
{
    return m_matcher->group(static_cast<int32_t>(index), m_status);
}

size_t UStringMatcher::group_count() const
{
    return static_cast<size_t>(m_matcher->groupCount());
}

TextPos UStringMatcher::begin() const
{
    return m_matcher->start(m_status);
}

TextPos UStringMatcher::end() const
{
    return m_matcher->end(m_status);
}



