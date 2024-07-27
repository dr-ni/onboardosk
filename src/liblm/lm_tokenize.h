#ifndef LM_TOKENIZE_H
#define LM_TOKENIZE_H

#include <memory>
#include <string>
#include <vector>
#include <cassert>

#include "tools/textdecls.h"

#include "lm.h"

class UString;
class UStringPattern;

namespace lm {

extern std::unique_ptr<UStringPattern> sentence_pattern;

    // Split text into sentences.
void split_sentences(std::vector<UString>& sentences,
                     std::vector<Span>& spans,
                     const UString& text, bool disambiguate=false);

// Split list of tokens at separator token.
void tokenize_sentence(std::vector<UString>& tokens,
                       std::vector<Span>& spans,
                       const UString& sentence, bool is_context=false);

// Split text into word tokens.
// The result is ready for use in learn_tokens().
//
// Sentence begins, if detected, are marked with "<s>".
// Numbers are replaced with the number marker <num>.
// Other tokens that could confuse the prediction are
// replaced with the unknown word marker "<unk>".
//
// Examples, text -> tokens:
//     "We saw whales"  -> ["We", "saw", "whales"]
//     "We saw whales " -> ["We", "saw", "whales"]
//     "Hello there! We saw 5 whales "
//                      -> ["Hello", "there", "<s>",
//                          "We", "saw", "<num>", "whales"]
void tokenize_text(std::vector<UString>& tokens,
                   std::vector<Span>& spans,
                   const UString& text, bool is_context = false);

// Split text into word tokens + completion prefix.
// The result is ready for use in predict().
void tokenize_context(std::vector<UString>& tokens,
                      std::vector<Span>& spans,
                      const UString& text);

// similar to python slicing, but only positive indices here.
template<class TInString, class TOutString>
void slice_tokens(std::vector<TOutString>& results,
                  const std::vector<TInString>& tokens,
                  size_t start)
{
    results.clear();
    for (size_t i=start; i<tokens.size(); i++)
        results.emplace_back(tokens[i]);
}


template<class TInString, class TOutString>
void slice_tokens(std::vector<TOutString>& results,
                  const std::vector<TInString>& tokens,
                  size_t start, size_t end)
{
    end = std::min(end, tokens.size());

    results.clear();
    for (size_t i=start; i<end; i++)
    {
        if constexpr (std::is_pointer_v<TOutString> &&
                      std::is_same_v<TInString, std::string_view>)
            results.emplace_back(tokens[i].data());
        else
            results.emplace_back(tokens[i]);
    }
}

inline void slice_tokens(
        std::vector<const wchar_t*>& results,
        const std::vector<std::wstring_view>& tokens,
        size_t start, size_t end)
{
    end = std::min(end, tokens.size());

    results.clear();
    for (size_t i=start; i<end; i++)
        results.emplace_back(tokens[i].data());
}

inline void slice_tokens(
        std::vector<const char*>& results,
        const std::vector<std::string>& tokens,
        size_t start, size_t end)
{
    end = std::min(end, tokens.size());

    results.clear();
    for (size_t i=start; i<end; i++)
        results.emplace_back(tokens[i].data());
}

// Split list of tokens at separator token.
using TokenSections = std::vector<std::vector<std::wstring>>;  // for unit tests
template<class TInString, class TOutString, class TSeparatorString>
void split_tokens(std::vector<std::vector<TOutString>>& token_sections,
                  const std::vector<TInString>& tokens, const TSeparatorString& separator,
                  bool keep_separator = false)
{
    std::vector<TOutString> token_section;
    for (const auto& token : tokens)
    {
        if (token == separator)
        {
            if (!token_section.empty())
               token_sections.emplace_back(token_section);
            if (keep_separator)
                token_section = std::vector<TOutString>{separator};
            else
                token_section.clear();
        }
        else
        {
            token_section.emplace_back(token);
        }
    }

    if (token_section.size() > 1 ||
        (!token_section.empty() && token_section[0] != separator))
    {
        token_sections.emplace_back(token_section);
    }
}

// Patition tokens with splits at the given indices.
// split_indices must be sorted in ascending order.
template<class TInString, class TOutString>
void split_tokens_at(std::vector<std::vector<TOutString>>& token_sections,
                     const std::vector<TInString>& tokens,
                     const std::vector<size_t>& split_indices)
{
    token_sections.clear();
    size_t remaining = 0;
    std::vector<TOutString> section;
    for (auto i : split_indices)
    {
        slice_tokens(section, tokens, remaining, i);
        if (!section.empty())
            token_sections.emplace_back(section);
        remaining = i+1;
    }
    slice_tokens(section, tokens, remaining);
    if (!section.empty())
        token_sections.emplace_back(section);
}

// Extract n-grams from tokens.
template<class TInString, class TOutString, typename F >
void for_each_ngram_in(const std::vector<TInString>& tokens,
                       OrderType order,
                       std::vector<TOutString>& ngram,
                       const F& func)
{
    std::vector<std::vector<std::string_view>> token_sections;

    // Don't let <unk> enter the model.
    // Split the token stream into sections between <unk>s.
    std::vector<std::vector<std::string_view>> unk_sections;
    split_tokens(unk_sections, tokens, "<unk>");
    for (const auto& section : unk_sections)
    {
        // Don't learn across sentence marks.
        split_tokens(token_sections, section, "<s>", true);
    }

    // Run a window of size <order> along the section and yield n-grams.
    //std::vector<const wchar_t*> ngram;

    for (const auto& section : token_sections)
    {
        for (size_t i=0; i<section.size(); i++)
        {
            for (size_t n=0; n<static_cast<size_t>(order); n++)
            {
                if (i+n+1 <= section.size())
                {
                    slice_tokens(ngram, section, i, i+n+1);
                    assert(n == ngram.size()-1);

                    func();
//                    func(&ngram[0],
  //                       static_cast<int>(ngram.size()));
                }
            }
        }
    }
}

// somewhat inefficient, mainly for unit tests
using NGrams = std::vector<std::vector<std::string>>;
void extract_ngrams(NGrams& ngrams,
                    const std::vector<std::string>& tokens, OrderType order);

}  // namespace

#endif // LM_TOKENIZE_H
