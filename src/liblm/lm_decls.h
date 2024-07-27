#ifndef LM_DECLS_H
#define LM_DECLS_H

#include <string>
#include "tools/ustringmain.h"

namespace lm {

enum PredictOptions
{
    CASE_INSENSITIVE         = 1<<0, // case insensitive completion,
                                     // affects all characters
    CASE_INSENSITIVE_SMART   = 1<<1, // case insensitive completion,
                                     // only for lower case chars
    ACCENT_INSENSITIVE       = 1<<2, // accent insensitive completion
                                     // affects all characters
    ACCENT_INSENSITIVE_SMART = 1<<3, // accent insensitive completion
                                     // only for non-accent characters
    IGNORE_CAPITALIZED       = 1<<4, // ignore capitalized words,
                                     // only affects first character
    IGNORE_NON_CAPITALIZED   = 1<<5, // ignore non-capitalized words
                                     // only affects first character
    INCLUDE_CONTROL_WORDS    = 1<<6, // include <s>, <num>, ...
    NO_SORT                  = 1<<7, // don't sort by weight

    // Default to not do explicit normalization for performance
    // reasons. Often results will be implicitely normalized already
    // and predictions for word choices just need the correct word order.
    // Normalization has to be enabled for entropy/perplexity
    // calculations or other verification purposes.
    NORMALIZE              = 1<<8, // explicit normalization for
                                   // overlay and loglinint, everything
                                   // else ought to be normalized already.
    FILTER_OPTIONS         = CASE_INSENSITIVE |
                             ACCENT_INSENSITIVE |
                             ACCENT_INSENSITIVE_SMART |
                             IGNORE_CAPITALIZED |
                             IGNORE_NON_CAPITALIZED,
    DEFAULT_OPTIONS        = 0
};

inline PredictOptions operator ~(PredictOptions a)
{ return static_cast<PredictOptions>(~static_cast<int>(a)); }
inline PredictOptions operator | (PredictOptions a, PredictOptions b)
{ return static_cast<PredictOptions>(static_cast<int>(a) | static_cast<int>(b)); }
inline PredictOptions operator & (PredictOptions a, PredictOptions b)
{ return static_cast<PredictOptions>(static_cast<int>(a) & static_cast<int>(b)); }
inline PredictOptions& operator |= (PredictOptions& a, PredictOptions b)
{ a = a | b; return a;}


enum Smoothing
{
    SMOOTHING_NONE,
    JELINEK_MERCER_I,    // jelinek-mercer interpolated
    WITTEN_BELL_I,       // witten-bell interpolated
    ABS_DISC_I,          // absolute discounting interpolated
    KNESER_NEY_I,        // kneser-ney interpolated
};


struct PredictResult
{
    std::wstring word;
    double p;
};
struct UPredictResult {
    UString word;
    double p;
};
typedef std::vector<PredictResult> PredictResults;
typedef std::vector<UPredictResult> UPredictResults;

class LanguageModel;

}

#endif // LM_DECLS_H

