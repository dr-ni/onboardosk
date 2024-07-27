#include <memory>
#include <sstream>

#define U_CHARSET_IS_UTF8 1
#include <unicode/regex.h> // icu
#include <unicode/schriter.h>

#include "tools/string_helpers.h"
#include "tools/ustringmain.h"
#include "tools/ustringregex.h"

#include "lm_tokenize.h"

namespace lm {

std::unique_ptr<UStringPattern> sentence_pattern{std::make_unique<UStringPattern>(
    " .*?                                     "
    "     (?:                                 "
    "           (?:[.;:!?](?:(?=[\\s]) | \")) "   // punctuation
    "         | (?:\\s*\\n\\s*)+(?=[\\n])     "   // multiples newlines
    "         | <s>                           "   // sentence end mark
    "     )                                   "
    "   | .+$                                 ",   // last sentence fragment
    UStringRegexFlag::VERBOSE | UStringRegexFlag::DOTALL)};


void split_sentences(std::vector<UString>& sentences,
                     std::vector<Span>& spans,
                     const UString& text, bool disambiguate)
{

    static UStringPattern disambiguate_pattern {"[.;:!?]\"?$"};

    // Remove carriage returns from Moby Dick.
    // Don't change the text's length, keep it in sync with spans.
    UString filtered = replace_all(text, "\r", " ");

    // split into sentence fragments
    UStringMatcher matcher(sentence_pattern.get(), filtered);
    while (matcher.find())
    {
        // filter matches
        auto sentence = matcher.group();

        // not only newlines? remove fragments with only double newlines
        if (true) //not re.match("^\s*\n+\s*$", sentence, re.UNICODE):
        {
            TextPos begin = matcher.begin();
            TextPos end   = matcher.end();

            // strip whitespace including newlines
            auto l = sentence.size();
            sentence = sentence.lstrip();
            begin += l - sentence.size();

            l = sentence.size();
            sentence = sentence.rstrip();
            end -= l - sentence.size();

            // remove <s>
            sentence = replace_all(sentence, "<s>", "   ");

            // remove newlines and double spaces - no, invalidates spans
            //sentence = re.sub(u"\s+", u" ", sentence)

            // strip whitespace from the cuts, remove carriage returns
            l = sentence.size();
            sentence = sentence.rstrip();
            end -= l - sentence.size();
            l = sentence.size();
            sentence = sentence.lstrip();
            begin += l - sentence.size();

            // add <s> sentence separators if the end of the sentence is
            // ambiguous - required by the split_corpus tool where the
            // result of split_sentences is saved to a text file and later
            // fed back to split_sentences again.
            if (disambiguate)
            {
                UStringMatcher dmatcher(&disambiguate_pattern, sentence);
                if (dmatcher.find())
                    sentence += " <s>";
            }

            sentences.emplace_back(sentence);
            spans.emplace_back(begin, end-begin);
        }
    }
}

static std::string get_text_pattern(const std::string& trailing_characters,
                                    const std::string& standalone_operators)
{  // \S*(\S)\2{3,}+\S*
    std::string s = R"(
    (                                     (?# <unk>)
      (?:^|(?<=\s))
        \S*(\S)\2{3,}+\S*              (?# char repeated more than 3 times)
        | [-]{3}                          (?# dash repeated more than 2 times)
      (?=\s|$)
      | :[^\s:@]+?@                       (?# password in URL)
    ) |
    (                                     (?# <num>)
      (?:[-+]?\d+(?:[.,]\d+)*)            (?# anything numeric looking)
      | (?:[.,]\d+)
    ) |
    (                                     (?# word)
      (?:[-]{0,2}                         (?# allow command line options)
        [^\W\d]\w*(?:[-'´΄][\w]+)*        (?# word, not starting with a digit)
        [{trailing_characters}'´΄]?)
      | <unk> | <s> | </s> | <num>        (?# pass through control words)
      | <bot:[a-z]*>                      (?# pass through begin of text merkers)
      | (?:^|(?<=\s))
          (?:
            \| {standalone_operators}     (?# common space delimited operators)
          )
        (?=\s|$)
    )
    )";

    s = replace_all(s, "{trailing_characters}", trailing_characters);
    s = replace_all(s, "{standalone_operators}", standalone_operators);

    return s;
}

void tokenize_sentence(std::vector<UString>& tokens,
                       std::vector<Span>& spans,
                       const UString& sentence, bool is_context)
{
    // Don't learn "-" or "--" as standalone tokens...
    static UStringPattern text_pattern {
        get_text_pattern("", "").c_str(),
        UStringRegexFlag::VERBOSE | UStringRegexFlag::DOTALL};

    // ...but recognize them in a prediction context as start
    // of a cmd line option.
    static UStringPattern context_pattern {
        get_text_pattern("-", "| [-]{1,2}").c_str(),
        UStringRegexFlag::VERBOSE | UStringRegexFlag::DOTALL};

    UStringPattern* pattern = is_context ? &context_pattern : &text_pattern;
    UStringMatcher matcher(pattern, sentence);
    while (matcher.find())
    {
        TextPos begin = matcher.begin();
        TextPos end   = matcher.end();
        if (auto group = matcher.group(4);
            !group.empty())
        {
            tokens.emplace_back(group);
            spans.emplace_back(begin, end-begin);
        }
        else if (group = matcher.group(3);
                 !group.empty())
        {
            tokens.emplace_back("<num>");
            spans.emplace_back(begin, end-begin);
        }
        else if (group = matcher.group(1);
                 !group.empty())
        {
            tokens.emplace_back("<unk>");
            spans.emplace_back(begin, end-begin);
        }
    }
}

void tokenize_text(std::vector<UString>& tokens,
                   std::vector<Span>& spans,
                   const UString& text, bool is_context)
{
    std::vector<UString> sentences;
    std::vector<Span> sentence_spans;
    split_sentences(sentences, sentence_spans, text);
    for (size_t i=0; i<sentences.size(); i++)
    {
        const auto& sentence = sentences[i];
        std::vector<UString> ts;
        std::vector<Span> ss;
        tokenize_sentence(ts, ss, sentence, is_context);

        TextPos sbegin = sentence_spans[i].begin;
        for (auto& span : ss)
            span.begin += sbegin;

        // sentence begin?
        if (i > 0)
        {
            tokens.emplace_back("<s>");      // prepend sentence begin marker
            spans.emplace_back(sbegin, 0); // empty span
        }
        tokens.insert(tokens.end(), ts.begin(), ts.end());
        spans.insert(spans.end(), ss.begin(), ss.end());
    }
}

void tokenize_context(std::vector<UString>& tokens,
                      std::vector<Span>& spans,
                      const UString& text)
{
    static UStringPattern pattern{R"(
                  ^$                             # empty string?
                | .*[-'´΄\w]$                    # word at the end?
                | (?:^|.*\s)[|]=?$               # recognized operator?
                | .*(\S)\\1{3,}$                 # anything repeated > 3 times?
            )",
            UStringRegexFlag::VERBOSE | UStringRegexFlag::DOTALL};

    tokenize_text(tokens, spans, text, true);

    // filter matches
    if (!pattern.matches(text))
    {
        tokens.emplace_back();
        TextPos tend = static_cast<TextPos>(text.size());
        spans.emplace_back(tend, 0);  // empty span
    }
}

void extract_ngrams(NGrams& ngrams,
                    const std::vector<std::string>& tokens, OrderType order)
{
    std::vector<std::string> ngram;
    for_each_ngram_in(tokens, order, ngram, [&]
    {
        ngrams.emplace_back(ngram);
    });
}



}  // namespace
