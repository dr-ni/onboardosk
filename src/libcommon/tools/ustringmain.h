#ifndef USTRINGMAIN_H
#define USTRINGMAIN_H

#include <climits>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#ifndef U_CHARSET_IS_UTF8
#define U_CHARSET_IS_UTF8 1
#endif
#include <unicode/unistr.h>  // icu
#include <unicode/schriter.h>
#include <unicode/brkiter.h>

#include "textdecls.h"

class UStringMatcher;
class UStringPattern;

class UString
{
    public:
        class CodePointIterator
        {
            public:
                CodePointIterator(const UString* string);
                CodePointIterator& operator++();
                CodePoint operator*();
                bool operator!=(const CodePointIterator& it) {return !operator==(it);}
                bool operator==(const CodePointIterator& it);

            private:
                icu::StringCharacterIterator m_it;
                CodePoint m_cp{};
                bool m_at_end{false};
        };

        class CharacterBreakIterator
        {
            public:
                CharacterBreakIterator(const UString* s);
                ~CharacterBreakIterator();
                CharacterBreakIterator& operator++();
                std::string operator*();
                bool operator!=(const CharacterBreakIterator& it) {return !operator==(it);}
                bool operator==(const CharacterBreakIterator& it);

            private:
                const UString* m_s{};
                icu::BreakIterator* m_it{};
                int32_t m_boundary_start{};
                int32_t m_boundary_end{};
                bool m_at_end{false};
        };

    private:
        UString(const icu::UnicodeString& s);
        UString(const icu::UnicodeString&& s);
        // UString& operator=(const icu::UnicodeString& s);
        // UString& operator=(const icu::UnicodeString&& s);

    public:
        UString();
        UString(const char* s);
        UString(const std::string& s);   // assumed to be UTF-8
        UString(const std::wstring& s);  // assumed to be 32bit code points
        UString(const UString& s);
        UString(const UString&& s) noexcept;

        UString& operator=(const UString& s);
        UString& operator=(const UString&& s) noexcept;

        bool operator==(const UString& s) const;
        bool operator!=(const UString& s) const {return !operator==(s);}
        bool operator<(const UString& s) const;

        UString operator+(const UString& s) const;
        UString& operator+=(const UString& s);

        std::string to_utf8() const;
        std::wstring to_wstring() const;
        const icu::UnicodeString& to_us() const {return m_us;}

        std::string encode(const char* encoding) const;
        static UString decode(const char* text, const char* encoding);

        CodePointIterator begin();
        CodePointIterator end();
        CharacterBreakIterator begin_log();
        CharacterBreakIterator end_log();

        bool empty() const;
        bool isspace() const;
        bool islower() const;
        bool isupper() const;

        // Count of code points
        size_t size() const;

        // Count of logical characters (grapheme clusters, spanning
        // one or more code points)
        size_t size_logchar() const;

        int32_t operator[](size_t index) const;

        void clear();

        UString lower() const;
        UString upper() const;
        UString capitalize() const;

        UString strip() const;
        UString lstrip() const;
        UString rstrip() const;

        // Python like slicing on code points
        // Negative indices count from the end of the string.
        // Result is truncated for out of range indizes.
        UString slice(int begin, int end=INT_MAX) const;

        // Python like slicing on logical characters (grapheme clusters,
        // spanning one or more code points).
        UString slice_logchar(int begin, int end=INT_MAX) const;

        UString replace(const UString& del, const UString& insert) const;
        bool startswith(const UString& prefix) const;
        bool endswith(const UString& prefix) const;
        bool contains(const UString& search) const;

        std::vector<int32_t> get_code_points();

    private:
        icu::BreakIterator*get_break_iterator() const;
        void sync_debug_str()
        {
            m_debug_str.clear();  // toUTF8String appends??
            m_us.toUTF8String(m_debug_str);
        }

    private:
        icu::UnicodeString m_us;
        std::string m_debug_str;

        static constexpr const size_t NPOS = static_cast<size_t>(-1);

        mutable std::unique_ptr<icu::BreakIterator> m_break_iterator;
        mutable UErrorCode m_uerror{U_ZERO_ERROR};
        mutable size_t m_n_logchars{NPOS};

        friend CodePointIterator;
        friend CharacterBreakIterator;
        friend UStringPattern;
        friend UStringMatcher;
        friend UString replace_all(const UString& s, const UString& del, const UString& insert);
};

typedef std::vector<UString> UStrings;

std::ostream& operator<<(std::ostream& s, const UString& us);

std::string repr(const UString& value);

UString join(const std::vector<UString>& v, const UString& delim={});
UString replace_all(const UString& s, const UString& del, const UString& insert);




#endif // TOOLS_icu::UnicodeString_H



