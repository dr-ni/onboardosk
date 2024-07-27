#include <numeric>

#include "tools/iostream_helpers.h"
#include "tools/logger.h"

#include "configuration.h"
#include "keyboard.h"
#include "learnstrategy.h"
#include "textchanges.h"
#include "textcontext.h"
#include "textdomain.h"
#include "timer.h"
#include "wordsuggestions.h"
#include "wpengine.h"


std::ostream&operator<<(std::ostream& s, const TokenizeSpanResults& r){
    s << "{" << r.tokens << ", " << r.spans << ", " << r.span_before << "}";
    return s;
}



LearnStrategy::LearnStrategy(const ContextBase& context) :
    Super(context)
{
}

LearnStrategy::~LearnStrategy()
{
}

void LearnStrategy::learn_spans(const std::vector<TextSpanPtr>& spans,
                                const UString& bot_marker,
                                Noneable<TextPos> bot_offset,
                                const TextDomain* text_domain)
{
    if (logger()->can_log(LogLevel::DEBUG))
    {
        LOG_DEBUG << "bot_marker=" << bot_marker
                  << ", bot_offset=" << bot_offset
                  << ", text_domain=" << text_domain
                  << ", can_auto_learn=" << config()->word_suggestions->can_auto_learn()
                  << " (" << config()->word_suggestions->get_can_auto_learn_debug_string()
                  << ")";
    }

    if (config()->word_suggestions->can_auto_learn())
    {
        auto texts = get_learn_texts(spans, bot_marker, bot_offset,
                                     text_domain);

        // LOG_WARNING << "learning " << texts;

        auto engine = get_wp_engine();
        for (auto text : texts)
        {
            engine->learn_text(text,
                               config()->word_suggestions->can_learn_new_words());
        }
    }
}

void LearnStrategy::learn_scratch_spans(std::vector<TextSpanPtr> spans,
                                        const TextDomain* text_domain)
{
    if (config()->word_suggestions->can_auto_learn())
    {
        auto engine = get_wp_engine();
        engine->clear_scratch_models();
        if (!spans.empty())
        {
            auto texts = get_learn_texts(spans, {}, {}, text_domain);

            for (const auto& text : texts)
                engine->learn_scratch_text(text);
        }
    }
}

std::vector<UString> LearnStrategy::get_learn_texts(
        const std::vector<TextSpanPtr>& spans,
        const UString& bot_marker,
        Noneable<TextPos> bot_offset,
        const TextDomain* text_domain)
{
    std::vector<std::vector<UString>> token_sets;
    get_learn_tokens(token_sets, spans, bot_marker, bot_offset, text_domain);
    std::vector<UString> results;
    for (auto& tokens : token_sets)
        results.emplace_back(join(tokens, " "));
    return results;
}

void LearnStrategy::get_learn_tokens(
        std::vector<std::vector<UString>>& token_sets,
        const std::vector<TextSpanPtr>& text_spans,
        const UString& bot_marker,
        Noneable<TextPos> bot_offset,
        const TextDomain* text_domain)
{
    std::vector<std::vector<Span>> span_sets;
    std::vector<TextSpanPtr> text_spans_;
    if (text_domain)
        text_spans_ = grow_spans(text_spans, text_domain);
    else
        text_spans_ = text_spans;

    sort_text_spans(text_spans_);

    for (const auto& text_span : text_spans_)
    {
        // Tokenize with one additional token in front so we can
        // spot and join adjacent token sets.
        TokenizeSpanResults r;
        tokenize_span(r, text_span, {});

        bool merged = false;
        if (!r.tokens.empty())
        {
            // Do previous tokens to merge with, exist?
            if (!token_sets.empty())
            {
                const auto& prev_tokens = token_sets.back();
                const auto& prev_spans  = span_sets.back();
                auto link_span = r.span_before.value_or(r.spans[0]);
                for (size_t i=0; i<prev_spans.size(); i++)
                {
                    const Span& prev_span = prev_spans[i];
                    if (prev_span == link_span)
                    {
                        int k = static_cast<int>(r.span_before ? i + 1 : i);
                        token_sets.back() = slice(prev_tokens, 0, k) + r.tokens;
                        span_sets.back()  = slice(prev_spans, 0, k) + r.spans;
                        merged = true;
                    }
                }
            }
            // No previous tokens exist. The current ones are the
            // very first tokens at lowest offset.
            else
            {
                // prepend begin-of-text marker
                if (!bot_marker.empty() and
                    !bot_offset.is_none())
                {
                    if (!r.span_before ||
                        (r.span_before.value().end() < bot_offset &&
                         bot_offset <= r.spans[0].begin))
                    {
                        r.tokens.insert(r.tokens.begin(), bot_marker);
                        r.spans.insert(r.spans.begin(), {-1, -1}); // dummy span, don't use
                    }
                }
            }
        }
        if (!merged &&
            !r.tokens.empty())
        {
            token_sets.emplace_back(r.tokens);
            span_sets.emplace_back(r.spans);
        }
    }
}

void LearnStrategy::tokenize(std::vector<UString>& tokens,
                             std::vector<Span>& spans,
                             const UString& text)
{
    auto engine = get_wp_engine();
    engine->tokenize_text(tokens, spans, text);
}

void LearnStrategy::tokenize_span(TokenizeSpanResults& r,
                                  const TextSpanPtr& text_span,
                                  size_t prepend_tokens)
{
    TextPos offset = text_span->text_begin();
    std::vector<UString> tokens;
    std::vector<Span> spans;
    tokenize(tokens, spans, text_span->get_text());

    auto itokens = intersect_span(text_span, spans);

    if (prepend_tokens &&
        !itokens.empty())
    {
        auto first = itokens[0];
        size_t n = std::min(prepend_tokens, first);
        std::vector<size_t> v(n);
        std::iota(v.begin(), v.end(), first-n);
        itokens.insert(itokens.begin(), v.begin(), v.end());
    }
    else if (!itokens.empty())
    {
        // Always include a preceding sentence marker to link
        // upper-case words with it when learning.
        auto first = itokens[0];
        if (first > 0 &&
            tokens.at(first - 1) == "<s>")
        {
            itokens.insert(itokens.begin(), first - 1);
        }
    }

    // Return an additional span for linking with other token lists:
    // span of the token before the first returned token.
    if (!itokens.empty() && itokens[0] > 0)
    {
        size_t k = itokens[0] - 1;
        r.span_before = Span{offset + spans[k].begin, spans[k].length};
    }

    for (size_t i : itokens)
    {
        r.tokens.emplace_back(tokens[i]);
        r.spans.emplace_back(spans[i].begin + offset, spans[i].length);
    }
}

std::vector<TextSpanPtr> LearnStrategy::grow_spans(const std::vector<TextSpanPtr>& spans,
                                                   const TextDomain* text_domain)
{
    bool modified{false};

    std::vector<TextSpanPtr> new_text_spans;
    for (const auto& s : spans)  // make a copy
        new_text_spans.emplace_back(s->clone());

    for (auto& text_span : new_text_spans)
    {
        TextPos begin;
        TextLength length;
        UString _text;
        std::tie(begin, length, _text) = text_domain->grow_learning_span(text_span);
        TextPos end = begin + length;
        if (begin < text_span->begin() ||
            end > text_span->end())
        {
            text_span->pos = begin;
            text_span->length = length;
            modified = true;
        }
    }

    if (modified)
    {
        TextSpanPtr _tracked_span;
        TextChanges::consolidate_spans(new_text_spans, new_text_spans,
                                       _tracked_span);
    }

    return new_text_spans;
}

std::vector<size_t> LearnStrategy::intersect_span(const TextSpanPtr& text_span,
                                                  const std::vector<Span>& spans)
{
    std::vector<size_t> itokens;
    TextPos offset = text_span->text_begin();
    TextPos begin  = text_span->begin() - offset;
    TextPos end    = text_span->end() - offset;
    TextPos length = end - begin;

    for (size_t i=0; i< spans.size(); i++)
    {
        const auto& s = spans[i];
        if (s.intersects(begin, length))
            itokens.emplace_back(i);
    }

    return itokens;
}



LearnStrategyLRU::LearnStrategyLRU(const ContextBase& context) :
    Super(context),
    m_timer(std::make_unique<Timer>(context))
{}

LearnStrategyLRU::~LearnStrategyLRU()
{}

void LearnStrategyLRU::reset()
{
    m_timer->stop();
    m_rate_limiter.stop();

    m_insert_count = 0;
    m_delete_count = 0;
}

void LearnStrategyLRU::commit_changes()
{
    reset();

    auto ws = get_word_suggestions();
    auto text_context = get_text_context();
    auto wpengine = get_wp_engine();
    if (!ws || !text_context || !wpengine)
        return;

    const auto& changes = text_context->get_changes();
    const auto& spans = changes->get_spans();

    LOG_DEBUG << "changes=" << changes
              << "wpengine=" << wpengine;
    if (!spans.empty())
    {
        auto bot_marker = text_context->get_bot_marker();
        auto bot_offset = text_context->get_bot_offset();
        auto domain = ws->get_text_domain();
        learn_spans(spans, bot_marker, bot_offset, domain);
        wpengine->clear_scratch_models();  // clear short term memory
    }

    text_context->clear_changes();
}

void LearnStrategyLRU::discard_changes()
{
    auto wpengine = get_wp_engine();
    if (wpengine)
        wpengine->clear_scratch_models();  // clear short term memory
    reset();
}

bool LearnStrategyLRU::commit_expired_changes()
{
    auto text_context = get_text_context();
    if (!text_context)
        return {};

    auto changes = text_context->get_changes();
    auto& spans = changes->get_spans();

    // find most recently update span
    TextSpanPtr most_recent;
    for (auto& span : spans)
    {
        if (!most_recent ||
             most_recent->last_modified < span->last_modified)
        {
            most_recent = span;
        }
    }

    // learn expired spans
    std::vector<TextSpanPtr> expired_spans;
    for (auto& span : spans)
    {
        if (span != most_recent &&
            TextSpan::Clock::now() - span->last_modified >= m_learn_delay)
        {
            expired_spans.emplace_back(span);
            changes->remove_span_ptr(span);
        }
    }

    learn_spans(expired_spans);

    return !changes->empty();
}

void LearnStrategyLRU::on_text_entry_activated()
{
}

void LearnStrategyLRU::on_text_context_changed()
{
    auto text_context = get_text_context();
    if (!text_context)
        return;

    auto changes = text_context->get_changes();
    if (!changes->empty())
        handle_timed_learning();

    maybe_update_scratch_models();
}

void LearnStrategyLRU::handle_timed_learning()
{
    auto text_context = get_text_context();
    if (!text_context)
        return;

    auto changes = text_context->get_changes();
    if (!changes->empty() &&
        !m_timer->is_running())
    {
        // begin polling for text changes to learn every x seconds
        if (0)// disabled for now
            m_timer->start(m_polling_time, [this]{return poll_changes();});
    }
}

bool LearnStrategyLRU::poll_changes()
{
    return commit_expired_changes();
}

void LearnStrategyLRU::maybe_update_scratch_models()
{
    auto text_context = get_text_context();
    auto changes = get_text_changes();
    if (text_context &&
        changes)
    {
        // Whitespace inserted or delete to whitespace?
        bool inserted = m_insert_count < changes->insert_count;
        bool deleted = m_delete_count < changes->delete_count;
        if (inserted || deleted)
        {
            // caret inside a word?
            bool in_word{false};
            auto caret_span = text_context->get_span_at_caret();
            if (caret_span)
            {
                auto c = caret_span->get_char_before_span();
                in_word = !c.isspace();
            }

            if (!in_word)
            {
                // run now, or guaranteed very soon, but not too often
                m_rate_limiter.enqueue(deleted);
            }
        }
    }
}

void LearnStrategyLRU::update_scratch_memory(bool update_ui)
{
    auto keyboard = get_keyboard();
    auto changes = get_text_changes();
    if (changes)
    {
        const auto& spans = changes->get_spans();  // by reference
        learn_scratch_spans(spans);

        m_insert_count = changes->insert_count;
        m_delete_count = changes->delete_count;

        // Reflect the model change in the wordlist,
        // e.g. when deleting new words.
        if (update_ui)
        {
            keyboard->invalidate_context_ui();
            // called from timer -> allowed to commit
            keyboard->commit_ui_updates();
        }
    }
}

const TextChanges* LearnStrategyLRU::get_text_changes() const
{
    auto tc = get_text_context();
    if (tc)
        return  tc->get_changes();
    return nullptr;
}

TextContext* LearnStrategyLRU::get_text_context() const
{
    auto ws = get_word_suggestions();
    if (ws)
        return  ws->get_text_context();
    return nullptr;
}

