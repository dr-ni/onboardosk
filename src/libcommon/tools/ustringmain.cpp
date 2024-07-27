
#include <memory>
#include <sstream>

#include <unicode/utypes.h>   /* Basic ICU data types */
#include <unicode/uchar.h>
#include <unicode/ucnv.h>


#include "ustringmain.h"


UString::CodePointIterator::CodePointIterator(const UString* s) :
    m_it(s ? s->m_us : ""),
    m_at_end(!s)
{
    if (s)
    {
        m_cp = m_it.next32PostInc();
    }
}

UString::CodePointIterator& UString::CodePointIterator::operator++()
{
    m_cp = m_it.next32PostInc();
    if (m_cp == m_it.DONE)
        m_at_end = true;
    return *this;
}

CodePoint UString::CodePointIterator::operator*()
{
    return m_cp;
}

bool UString::CodePointIterator::operator==(const UString::CodePointIterator& it)
{
    return m_at_end == it.m_at_end;
}

UString::CharacterBreakIterator::CharacterBreakIterator(const UString* s) :
    m_s(s)
{
    if (s)
    {
        UErrorCode status = U_ZERO_ERROR;
        m_it = icu::BreakIterator::createCharacterInstance(NULL, status);
        m_it->setText(s->m_us);

        m_boundary_start = m_it->first();
        m_boundary_end = m_it->next();
        if (m_boundary_start == icu::BreakIterator::DONE)
            m_at_end = true;
    }
    else
    {
        m_at_end = true;
    }
}

UString::CharacterBreakIterator::~CharacterBreakIterator()
{
    delete m_it;
}

UString::CharacterBreakIterator&UString::CharacterBreakIterator::operator++()
{
    m_boundary_start = m_boundary_end;
    m_boundary_end = m_it->next();
    if (m_boundary_start == icu::BreakIterator::DONE)
        m_at_end = true;
    return *this;
}

std::string UString::CharacterBreakIterator::operator*()
{
    const icu::UnicodeString& s = m_s->m_us;
    auto tmp = s.tempSubStringBetween(m_boundary_start, m_boundary_end);
    std::string result;
    return tmp.toUTF8String(result);
}

bool UString::CharacterBreakIterator::operator==(const UString::CharacterBreakIterator& it)
{
    return m_at_end == it.m_at_end;
}



UString::UString(const icu::UnicodeString& s) :
    m_us(s)
{
    sync_debug_str();
}

UString::UString(const icu::UnicodeString&& s) :
    m_us(s)
{
    sync_debug_str();
}

#if 0
UString& UString::operator=(const icu::UnicodeString& s)
{
    m_us = s;
    sync_debug_str();
    return *this;
}

UString& UString::operator=(const icu::UnicodeString&& s)
{
    m_us = std::move(s);
    sync_debug_str();
    return *this;
}
#endif

UString::UString()
{
}

UString::UString(const char* s) :
    m_us(icu::UnicodeString::fromUTF8(s))
{
    sync_debug_str();
}

UString::UString(const std::string& s) :
    m_us(icu::UnicodeString::fromUTF8(s))
{
    sync_debug_str();
}

static_assert(sizeof(wchar_t) == sizeof(int32_t));

UString::UString(const std::wstring& s) :
    m_us(icu::UnicodeString::fromUTF32(reinterpret_cast<const UChar32*>(s.data()),
                                       static_cast<int32_t>(s.size())))
{
    sync_debug_str();
}

UString::UString(const UString& s) :
    m_us(s.m_us),
    m_n_logchars(s.m_n_logchars)
{
    sync_debug_str();
}

UString::UString(const UString&& s) noexcept :
    m_us(std::move(s.m_us)),
    m_break_iterator(std::move(s.m_break_iterator)),
    m_n_logchars(s.m_n_logchars)
{
    sync_debug_str();
}

UString& UString::operator=(const UString& s)
{
    m_us = s.m_us;
    m_n_logchars = s.m_n_logchars;
    sync_debug_str();
    return *this;
}

UString& UString::operator=(const UString&& s) noexcept
{
    m_us = std::move(s.m_us);
    m_n_logchars = s.m_n_logchars;
    m_break_iterator = std::move(s.m_break_iterator);
    sync_debug_str();
    return *this;
}

bool UString::operator==(const UString& s) const
{
    return m_us == s.m_us;
}

bool UString::operator<(const UString& s) const
{
    return m_us < s.m_us;
}

UString UString::operator+(const UString& s) const
{
    UString us;
    us.m_us = m_us + s.m_us;
    us.sync_debug_str();
    return us;
}

UString& UString::operator+=(const UString& s)
{
    m_us += s.m_us;
    sync_debug_str();
    return *this;
}

std::string UString::to_utf8() const
{
    std::string out;
    return m_us.toUTF8String(out);
}

std::wstring UString::to_wstring() const
{
    static_assert(sizeof(wchar_t) == sizeof(int32_t));

    size_t n = size();
    std::wstring out(n, '*');
    icu::StringCharacterIterator it(m_us);
    for(size_t i=0; i<n; i++)
    {
        auto cp = it.next32PostInc();
        if (cp == it.DONE)
            break;
        out[i] = cp;
    }
    return out;

}

std::string UString::encode(const char* encoding) const
{
    UErrorCode error;
    int32_t length;

    error = U_ZERO_ERROR;
    std::unique_ptr<UConverter, decltype (&ucnv_close)> conv =
        {ucnv_open(encoding, &error), ucnv_close};

    if(U_FAILURE(error))
        throw std::runtime_error(
            "UString::encode: ucnv_open failed (" + std::to_string(error) +
            "), unknown encoding?");

    error = U_ZERO_ERROR;
    length = m_us.extract(NULL, 0, conv.get(), error);
    if (error != U_BUFFER_OVERFLOW_ERROR)
        throw std::runtime_error(
            "UString::encode: m_us.extract length failed (" + std::to_string(error) + ")");

    #if !(__cplusplus > 201402L)
        #error "C++17 support required for std::strings with guaranteed continuous buffers"
    #endif

    std::string result(static_cast<size_t>(length), 0);
    error = U_ZERO_ERROR;
    length = m_us.extract(result.data(), length, conv.get(), error);
    if (U_FAILURE(error))
        throw std::runtime_error(
            "UString::encode: m_us.extract failed (" + std::to_string(error) + ")");

    return result;
}

UString UString::decode(const char* text, const char* encoding)
{
    icu::UnicodeString us(text, encoding);
    if (us.isBogus())
    {
        std::stringstream ss;
        ss << "creating UnicodeString failed"
           << ", unknown encoding '" << encoding << "'?";
        throw std::runtime_error(ss.str());
    }
    return {us};
}

UString::CodePointIterator UString::begin()
{
    return CodePointIterator(this);
}

UString::CodePointIterator UString::end()
{
    return CodePointIterator(nullptr);
}

UString::CharacterBreakIterator UString::begin_log()
{
    return CharacterBreakIterator(this);
}

UString::CharacterBreakIterator UString::end_log()
{
    return CharacterBreakIterator(nullptr);
}

bool UString::empty() const
{
    return m_us.isEmpty();
}

bool UString::isspace() const
{
    icu::StringCharacterIterator it(m_us);
    while(true)
    {
        auto cp = it.next32PostInc();
        if (cp == it.DONE)
            break;
        if (!u_isspace(cp))
            return false;
    }
    return true;
}

bool UString::islower() const
{
    icu::StringCharacterIterator it(m_us);
    while(true)
    {
        auto cp = it.next32PostInc();
        if (cp == it.DONE)
            break;
        if (!u_islower(cp))
            return false;
    }
    return true;
}

bool UString::isupper() const
{
    icu::StringCharacterIterator it(m_us);
    while(true)
    {
        auto cp = it.next32PostInc();
        if (cp == it.DONE)
            break;
        if (!u_isupper(cp))
            return false;
    }
    return true;
}

size_t UString::size() const
{
    return static_cast<size_t>(m_us.countChar32());
}

size_t UString::size_logchar() const
{
    if (m_n_logchars == NPOS)
    {
        auto it = get_break_iterator();
        size_t c = 0;
        int32_t begin = it->first();
        if (begin != icu::BreakIterator::DONE)
        {
            while (true)
            {
                begin = it->next();
                if (begin == icu::BreakIterator::DONE)
                    break;
                c++;
            }
        }
        m_n_logchars = c;
    }
    return m_n_logchars;
}

int32_t UString::operator[](size_t index) const
{
    return m_us.char32At(static_cast<int32_t>(index));
}

void UString::clear()
{
    m_us = "";
}

UString UString::lower() const
{
    icu::UnicodeString us = m_us;
    return us.toLower();
}

UString UString::upper() const
{
    icu::UnicodeString us = m_us;
    return us.toUpper();
}

UString UString::capitalize() const
{
    return slice(0, 1).upper() + slice(1);
}

UString UString::strip() const
{
    icu::UnicodeString us = m_us;
    return us.trim();
}

UString UString::lstrip() const
{
    icu::StringCharacterIterator it(m_us);
    while(true)
    {
        int32_t begin = it.getIndex();
        auto cp = it.next32PostInc();
        if (cp == it.DONE)
            break;

        if (!u_isspace(cp))
        {
            int32_t end = it.endIndex();
            return icu::UnicodeString{m_us, begin, end - begin};
        }
    }

    return {};
}

UString UString::rstrip() const
{
    icu::StringCharacterIterator it(m_us);
    auto cp = it.last32();
    int32_t end = it.endIndex();
    while(cp != icu::CharacterIterator::DONE)
    {
        if (!u_isspace(cp))
        {
            int32_t begin = 0;
            return icu::UnicodeString{m_us, begin, end - begin};
        }
        end = it.getIndex();
        cp = it.previous32();
    }

    return {};
}

UString UString::slice(int begin, int end) const
{
    int n = 0;
    if (begin < 0 || end < 0)
        n = static_cast<int>(size());

    if (begin < 0)
        begin = n + begin;
    if (end < 0)
        end = n + end;

    icu::StringCharacterIterator it(m_us);

    int i = 0;
    int32_t result_begin{-1};
    int32_t result_end{-1};
    while(true)
    {
        auto utf16_index = it.getIndex();
        auto cp = it.next32PostInc();
        if (cp == it.DONE)
            break;
        auto utf16_nextindex = it.getIndex();

        if (i >= begin)
        {
            if (result_begin == -1)
                //result_begin = i;
                result_begin = utf16_index;
            result_end = utf16_index;

            if (i >= end)
                break;
            result_end = utf16_nextindex;
        }

        i++;
    }

    if (result_begin < 0 && result_end < 0)
        return {};

    UString result{icu::UnicodeString{m_us, result_begin, result_end-result_begin}};
    return result;
}

UString UString::slice_logchar(int begin, int end) const
{
    int n = 0;
    if (begin < 0 || end < 0)
        n = static_cast<int>(size_logchar());

    if (begin < 0)
        begin = n + begin;
    if (end < 0)
        end = n + end;

    auto it = get_break_iterator();

    int i = 0;
    int32_t result_begin{-1};
    int32_t result_end{-1};
    int32_t boundary_begin = it->first();
    while (boundary_begin != icu::BreakIterator::DONE)
    {
        if (i >= begin)
        {
            if (result_begin == -1)
                result_begin = boundary_begin;
            result_end = boundary_begin;

            if (i >= end)
                break;
        }

        boundary_begin = it->next();
        i++;
    }

    if (result_begin < 0 && result_end < 0)
        return {};

    return UString{icu::UnicodeString{m_us, result_begin, result_end-result_begin}};
}

UString UString::replace(const UString& del, const UString& insert) const
{
    UString result(m_us);
    result.m_us.findAndReplace(del.m_us, insert.m_us);
    return result;
}

bool UString::startswith(const UString& prefix) const
{
    return m_us.startsWith(prefix.m_us);
}

bool UString::endswith(const UString& prefix) const
{
    return m_us.endsWith(prefix.m_us);
}

bool UString::contains(const UString& search) const
{
    return m_us.indexOf(search.m_us) >= 0;
}

std::vector<int32_t> UString::get_code_points()
{
    std::vector<int32_t> cps;
    icu::StringCharacterIterator it(m_us);
    while(true)
    {
        auto cp = it.next32PostInc();
        if (cp == it.DONE)
            break;
        cps.emplace_back(cp);
    }
    return cps;
}

icu::BreakIterator* UString::get_break_iterator() const
{
    if (!m_break_iterator)
    {
        auto p = std::unique_ptr<icu::BreakIterator>(
            icu::BreakIterator::createCharacterInstance(NULL, m_uerror));
        m_break_iterator = std::move(p);
        m_break_iterator->setText(m_us);
    }
    return m_break_iterator.get();
}

std::ostream&operator<<(std::ostream& s, const UString& us){
    s << "U\"" << us.to_utf8() << "\"";
    return s;
}

std::string repr(const UString& value)
{
    return std::string("'") + value.to_utf8() + "'";
}

UString join(const std::vector<UString>& v, const UString& delim)
{
    UString result;
    const size_t n = v.size();
    for (size_t i=0; i<n; ++i)
    {
        result += v[i];
        if (i<n-1)
            result += delim;
    }
    return result;
}

UString replace_all(const UString& s, const UString& del, const UString& insert)
{
    UString result = s;
    result.m_us.findAndReplace(del.m_us, insert.m_us);
    return result;
}

