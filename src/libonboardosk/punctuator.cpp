
#include "configuration.h"
#include "keyboardkeylogic.h"
#include "layoutkey.h"
#include "punctuator.h"
#include "textcontext.h"
#include "textchanger.h"
#include "textchanges.h"
#include "wordsuggestions.h"


const std::vector<UString> Punctuator::m_punctuation_no_capitalize
{
    ",", ":", ";", ")", "}", "]",
    "’",
    "”", "»", "›",  // language dependent :/
};
const std::vector<UString> Punctuator::m_punctuation_capitalize
{
    ".", "?", "!"
};
const std::vector<UString> Punctuator::m_punctuation =
        Punctuator::m_punctuation_capitalize +
        Punctuator::m_punctuation_no_capitalize;

Punctuator::Punctuator(const ContextBase& context) :
    Super(context)
{}

Punctuator::~Punctuator()
{}

void Punctuator::insert_separator(const TextSpanPtr& separator_span)
{
    if (separator_span)
    {
        auto text = separator_span->get_span_text();

        // Don't use direct insertion here, because many keys
        // always generate key-strokes and are likely to arrive
        // after the separator.
        auto key_logic = get_key_logic();
        key_logic->get_text_changer()->insert_string_at_caret(text);
        key_logic->set_last_typed_was_separator(true);
    }
}

void Punctuator::insert_separator_at_distance(const TextSpanPtr& separator_span)
{
    auto ws = get_word_suggestions();
    if (!ws)
        return;

    if (separator_span)
    {
        auto text = separator_span->get_span_text();

        const auto caret_span = get_span_at_caret();
        if (caret_span &&
            (ws->can_direct_insert_text() ||
             text.size() < 50))  // abort if too many key-presses necessary
        {
            TextPos separator_pos = separator_span->begin();
            ws->replace_text(Span{separator_pos, 0},
                             caret_span->begin(),
                             text);
        }
    }
}

void Punctuator::update_text_context()
{
    if (auto ws = get_word_suggestions())
        if (auto text_context = ws->get_text_context())
            text_context->on_text_context_changed();
}

void Punctuator::delete_at_caret()
{
    get_text_changer()->delete_at_caret();
}

TextSpanPtr Punctuator::get_span_at_caret()
{
    if (auto ws = get_word_suggestions())
        if (auto text_context = ws->get_text_context())
            return text_context->get_span_at_caret();
    return {};
}

TextContext*Punctuator::get_text_context()
{
    if (auto ws = get_word_suggestions())
        return ws->get_text_context();
    return {};
}

TextChanger*Punctuator::get_text_changer()
{
    auto key_logic = get_key_logic();
    return key_logic->get_text_changer();
}

TextChangerKeyStroke*Punctuator::get_text_changer_key_stroke()
{
    auto key_logic = get_key_logic();
    return key_logic->get_text_changer_keystroke();
}



PunctuatorImmediateSeparators::PunctuatorImmediateSeparators(const ContextBase& context) :
    Super(context)
{}

PunctuatorImmediateSeparators::~PunctuatorImmediateSeparators()
{}

void PunctuatorImmediateSeparators::reset()
{
    m_added_separator_span = {};
    m_separator_removed = false;
}

void PunctuatorImmediateSeparators::set_separator(const TextSpanPtr& separator_span)
{
    m_added_separator_span = separator_span;
}

void PunctuatorImmediateSeparators::on_before_press(const LayoutKeyPtr& key)
{
    bool caps_mode = false;

    // before punctuation remove the separator again
    if (m_added_separator_span &&
        key->is_text_changing())
    {
        // Only act if we are still at the caret position
        // where the separator was added.
        auto caret_span = get_span_at_caret();
        if (caret_span)
        {
            if (caret_span->begin() ==
                m_added_separator_span->end())
            {
                UString c = key->get_label();
                if (contains(m_punctuation_no_capitalize, c))
                {
                    delete_at_caret();
                    m_separator_removed = true;
                }

                else
                    if (contains(m_punctuation_capitalize, c))
                    {
                        delete_at_caret();
                        m_separator_removed = true;
                        if (config()->is_auto_capitalization_enabled())
                            caps_mode = true;
                    }
            }
        }

        m_added_separator_span = {};
    }

    if (caps_mode &&
        config()->is_auto_capitalization_enabled())
    {
        get_key_logic()->request_capitalization(true);
    }
}

void PunctuatorImmediateSeparators::on_after_release(const LayoutKeyPtr& key)
{
    (void)key;

    // after punctuation re-add the separator (always space)
    if (m_separator_removed)
    {
        m_separator_removed = false;
        // No direct insertion here. Space must always arrive
        // last, i.e. after the released key was generated.
        get_text_changer_key_stroke()->press_keysyms("space");
    }
}



PunctuatorDelayedSeparators::PunctuatorDelayedSeparators(const ContextBase& context) :
    Super(context)
{
}

PunctuatorDelayedSeparators::~PunctuatorDelayedSeparators()
{
}

void PunctuatorDelayedSeparators::reset()
{
    m_key_down_labels.clear();
}

void PunctuatorDelayedSeparators::set_separator(const TextSpanPtr& separator_span)
{
    set_pending_separator(separator_span);
}

void PunctuatorDelayedSeparators::set_pending_separator(const TextSpanPtr& separator_span)
{
    if (auto text_context = get_text_context())
        text_context->set_pending_separator(separator_span);
}

TextSpanPtr PunctuatorDelayedSeparators::get_pending_separator()
{
    if (auto text_context = get_text_context())
        return text_context->get_pending_separator();
    return {};
}

void PunctuatorDelayedSeparators::on_before_press(const LayoutKeyPtr& key)
{
    // clear label in case there was no after call
    m_key_down_labels[key.get()] = {};

    auto separator_span = get_pending_separator();
    bool insert;
    bool caps_mode;
    std::tie(separator_span, insert, caps_mode) =
            handle_key_event(key, separator_span, true);
    if (insert)
    {
        insert_separator(separator_span);
        separator_span = {};
    }
    if (caps_mode &&
        config()->is_auto_capitalization_enabled())
    {
        get_key_logic()->request_capitalization(true);
    }

    set_pending_separator(separator_span);
}

void PunctuatorDelayedSeparators::on_before_release(const LayoutKeyPtr& key)
{
    auto separator_span = get_pending_separator();
    bool insert;
    bool caps_mode;
    std::tie(separator_span, insert, caps_mode) =
            handle_key_event(key, separator_span, false);

    if (caps_mode &&
        config()->is_auto_capitalization_enabled())
    {
        get_key_logic()->request_capitalization(true);
    }
}

void PunctuatorDelayedSeparators::on_after_release(const LayoutKeyPtr& key)
{
    auto separator_span = get_pending_separator();
    bool insert;
    bool caps_mode;
    std::tie(separator_span, insert, caps_mode) =
            handle_key_event(key, separator_span, false);

    if (insert)
    {
        insert_separator(separator_span);
        separator_span = {};
    }

    set_pending_separator(separator_span);
    m_key_down_labels[key.get()] = {};
}

std::tuple<TextSpanPtr, bool, bool> PunctuatorDelayedSeparators::handle_key_event(const LayoutKeyPtr& key, const TextSpanPtr& pending_separator_span_in, bool before)
{
    auto ws = get_word_suggestions();
    if (!ws)
        return {};

    TextSpanPtr pending_separator_span = pending_separator_span_in;
    bool after = !before;
    bool caps_mode = false;

    if (pending_separator_span &&
        config()->word_suggestions->punctuation_assistance)
    {
        bool insert_now = false;

        if (key->is_layer_button() ||
            key->is_modifier())
        {
            // do nothing, keep pending separator for the next key
        }
        else
            if (key->is_prediction_key())
            {
                if (before)
                {
                    insert_now = true;
                }
                else if (after)
                {
                    // Separators that aren't space are always inserted
                    // right away. Too confusing otherwise.
                    if (pending_separator_span->get_span_text() != " ")
                        insert_now = true;
                }
            }
            else if (key->is_separator_cancelling())
            {
                pending_separator_span = {};
            }
            else if (key->is_text_changing())
            {
                auto caret_span = get_span_at_caret();
                if (caret_span)
                {
                    UString c = get_key_down_label(key, before);
                    bool punctuation_no_capitalize = \
                            contains(m_punctuation_no_capitalize, c);
                    bool punctuation_capitalize =
                            contains(m_punctuation_capitalize, c);
                    bool punctuation = punctuation_no_capitalize ||
                                       punctuation_capitalize;
                    TextPos caret_pos = caret_span->begin();
                    TextPos separator_pos = pending_separator_span->begin();

                    if (punctuation && ws->can_auto_punctuate())
                    {
                        // Text context often hasn't caught up to recent changes
                        // for delayed stroke keys like "?". Allow
                        // range of caret positions.
                        if (after && (caret_pos >= separator_pos &&
                                      caret_pos <= separator_pos + 1))
                        {
                            // Move span after the punctuation character.
                            // Make copy for fast comparison.
                            pending_separator_span =
                                    pending_separator_span->clone();
                            pending_separator_span->pos += 1;
                            pending_separator_span->text_pos += 1;

                            if (punctuation_capitalize)
                            {
                                caps_mode = true;
                            }
                        }
                    }
                    else
                    {
                        // Only act if we are still at the caret position
                        // where the separator is to be added.
                        if (before && caret_pos == separator_pos)
                        {
                            insert_now = true;
                        }
                        else
                            if (after && (caret_pos >= separator_pos &&
                                          caret_pos <= separator_pos + 1))
                            {
                                insert_now = true;
                            }
                    }
                }
                else
                {
                    pending_separator_span = {};
                }
            }
            else
            {
                pending_separator_span = {};
            }

        if (pending_separator_span &&
            insert_now)
        {
            return {pending_separator_span, true, caps_mode};
        }
    }

    return {pending_separator_span, false, caps_mode};
}

UString PunctuatorDelayedSeparators::get_key_down_label(const LayoutKeyPtr& key, bool before)
{
    (void) before;

    UString label;
    if (!get_value(m_key_down_labels,
                   const_cast<const LayoutKey*>(key.get()),
                   label))
    {
        label = key->get_label();
        m_key_down_labels[key.get()] = label;
    }
    return label;
}
