#ifndef LEARNSTRATEGY_H
#define LEARNSTRATEGY_H

#include <chrono>
#include <vector>

#include "tools/textdecls.h"
#include "tools/noneable.h"
#include "tools/ustringmain.h"

#include "callonce.h"
#include "onboardoskglobals.h"
#include "textchangesdecls.h"


class TextContext;
class TextDomain;
class UString;

struct TokenizeSpanResults
{
    std::vector<UString> tokens;
    std::vector<Span> spans;
    std::optional<Span> span_before;

    bool operator==(const TokenizeSpanResults& other) const
    {
        return tokens == other.tokens &&
               spans == other.spans &&
               span_before == other.span_before;
    }
};
std::ostream& operator<<(std::ostream& s, const TokenizeSpanResults& r);

// Base class of learn strategies (word suggestions).
class LearnStrategy : public ContextBase
{
    public:
        using Super = ContextBase;

        LearnStrategy(const ContextBase& context);
        virtual ~LearnStrategy();

        // Learn and clear all changes
        virtual void commit_changes() = 0;

        // Discard and remove all changes
        virtual void discard_changes() = 0;

        virtual void on_text_entry_activated() = 0;
        virtual void on_text_context_changed() = 0;

        // Get disjoint sets of tokens to learn.
        // Tokens of overlapping or adjacent spans are joined.
        // Expected overlap is at most one word at begin and end.
        void get_learn_tokens(std::vector<std::vector<UString>>& token_sets,
                const std::vector<TextSpanPtr>& text_spans,
                const UString& bot_marker={},
                Noneable<TextPos> bot_offset={},
                const TextDomain* text_domain={});

        // Expand spans to word boundaries and return as tokens.
        // Include <prepend_tokens> tokens before the span.
        void tokenize_span(TokenizeSpanResults& r,
                           const TextSpanPtr& text_span,
                           size_t prepend_tokens=0);

    protected:
        void learn_spans(const std::vector<TextSpanPtr>& spans,
                                 const UString& bot_marker={},
                                 Noneable<TextPos> bot_offset={},
                                 const TextDomain* text_domain={});

        void learn_scratch_spans(std::vector<TextSpanPtr> spans,
                                 const TextDomain* text_domain={});

        std::vector<UString> get_learn_texts(const std::vector<TextSpanPtr>& spans,
                                             const UString& bot_marker={},
                                             Noneable<TextPos> bot_offset={},
                                             const TextDomain* text_domain={});

        void tokenize(std::vector<UString>& tokens,
                      std::vector<Span>& spans,
                      const UString& text);

        // Grow spans to include e.g. whole URLs.
        std::vector<TextSpanPtr> grow_spans(const std::vector<TextSpanPtr>& spans,
                                            const TextDomain* text_domain);

        // Returns indices of tokens the given text_span touches.
        std::vector<size_t> intersect_span(const TextSpanPtr& text_span,
                                           const std::vector<Span>& spans);
};


class Timer;
class TextChanges;

// Delay learning individual spans of changed text to reduce the
// rate of junk entering the language model.
class LearnStrategyLRU : public LearnStrategy
{
    public:
        using Super = LearnStrategy;

        LearnStrategyLRU(const ContextBase& context);
        virtual ~LearnStrategyLRU();

        // Learn and clear all changes
        virtual void commit_changes() override;

        // Discard and remove all changes
        virtual void discard_changes() override;

        virtual void on_text_entry_activated() override;
        virtual void on_text_context_changed() override;

    private:
        void reset();

        // Learn and remove spans that have reached their timeout.
        // Keep the most recent span untouched,so it can be
        // worked on indefinitely.
        bool commit_expired_changes();

        void handle_timed_learning();

        bool poll_changes();

        // Update short term memory if the time is right.
        // This may be a time consuming task, so we try to limit the
        // frequency of updates.
        void maybe_update_scratch_models();

        // Update short term memory with changes that haven't been learned yet.
        void update_scratch_memory(bool update_ui);

        TextContext* get_text_context() const;
        const TextChanges* get_text_changes() const;

    private:
        // seconds from last modification until spans are learned
        const std::chrono::seconds m_learn_delay{60};

        // seconds between polling for timed-out spans
        const std::chrono::seconds m_polling_time{2};

        std::unique_ptr<Timer> m_timer;
        CallOnce<bool> m_rate_limiter{*this,
            [this](bool update_ui) {update_scratch_memory(update_ui);},
            std::chrono::milliseconds(500)};

        size_t m_insert_count{0};
        size_t m_delete_count{0};
};


#endif // LEARNSTRATEGY_H
