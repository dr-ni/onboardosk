#ifndef SPELLCHECKER_H
#define SPELLCHECKER_H

#include <deque>
#include <memory>

#include "tools/textdecls.h"
#include "tools/ustringmain.h"

#include "keyboarddecls.h"
#include "onboardoskglobals.h"

class LanguageDB;
class SCBackend;
class UString;

typedef TSpan<UString> USpan;


inline std::ostream& operator<<(std::ostream& s, const USpan& span){
    s << "USpan(" << span.begin << ", " << span.length << ", '" << span.text << "')";
    return s;
}


class SpellChecker : public ContextBase
{
    public:
        struct Result
        {
            USpan span;
            std::vector<UString> choices;
            bool operator==(const Result& other) const
            {
                return span == other.span &&
                       choices == other.choices;
            }
        };
        using Super = ContextBase;
        using CachedQuery = std::pair<UString, std::vector<Result>>;
        using CachedQueries = std::deque<CachedQuery>;

        SpellChecker(const ContextBase& context);
        ~SpellChecker();

        // Switch spell check backend on the fly
        void set_backend(SpellcheckBackend::Enum backend);

        std::vector<std::string> get_supported_dict_ids();

        bool set_dict_ids(const std::vector<std::string>& dict_ids);

        // Return spelling suggestions for word.
        // Multiple result sets may be returned, as the spell
        // checkers may return more than one result for certain tokens,
        // e.g. before and after hyphens.
        USpan find_corrections(std::vector<UString>& corrections,
                               const UString& word, TextPos caret_offset);

        // Return misspelled spans (begin- to end-index) inside word.
        // Multiple result sets may be returned, as the spell
        // checkers may return more than one result for certain tokens,
        // e.g. before and after hyphens.
        void find_incorrect_spans(const UString& word, std::vector<USpan>& spans);

        void invalidate_query_cache();

        void query(std::vector<SpellChecker::Result>& results,
                   const UString& word);

    private:
        std::vector<std::string> find_matching_dicts(const std::vector<std::string>& dict_ids);

        // Try to match up the given dict_id with the available dictionaries.
        // Look for alternatives if there is no direct match.
        std::string find_matching_dict(const std::string& dict_id);

        // Return cached query or ask the backend if necessary.
        const std::vector<SpellChecker::Result>& query_cached(const UString& word);

    private:
        const size_t m_max_query_cache_size = 100;    // max number of cached queries

        std::unique_ptr<SCBackend> m_backend;
        CachedQueries m_cached_queries;

};

#endif // SPELLCHECKER_H
