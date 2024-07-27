#ifndef PUNCTUATOR_H
#define PUNCTUATOR_H

#include "tools/ustringmain.h"

#include "layoutdecls.h"
#include "onboardoskglobals.h"
#include "textchangesdecls.h"

class TextChanger;
class TextChangerKeyStroke;


class Punctuator : public ContextBase
{
    public:
        using Super = ContextBase;

        Punctuator(const ContextBase& context);
        virtual ~Punctuator();

        virtual void reset() = 0;

        virtual void set_separator(const TextSpanPtr& separator_span) = 0;

        // Insert pending separator even if it is located at some
        // distance away from the caret. Caret position will be
        // restored afterwards.
        void insert_separator_at_distance(const TextSpanPtr& separator_span);

        virtual void on_before_press(const LayoutKeyPtr& key) = 0;
        virtual void on_before_release(const LayoutKeyPtr& key) = 0;
        virtual void on_after_release(const LayoutKeyPtr& key) = 0;

    protected:
         // Insert pending separator at the caret.
        void insert_separator(const TextSpanPtr& separator_span);

        void update_text_context();

        void delete_at_caret();
        TextSpanPtr get_span_at_caret();

        TextContext* get_text_context();
        TextChanger* get_text_changer();
        TextChangerKeyStroke* get_text_changer_key_stroke();

    protected:
        static const std::vector<UString> m_punctuation_no_capitalize;
        static const std::vector<UString> m_punctuation_capitalize;
        static const std::vector<UString> m_punctuation;
};


class PunctuatorImmediateSeparators : public Punctuator
{
    public:
        using Super = Punctuator;

        PunctuatorImmediateSeparators(const ContextBase& context);
        virtual ~PunctuatorImmediateSeparators();

        virtual void reset() override;

        virtual void set_separator(const TextSpanPtr& separator_span) override;

    private:
        // Chance to insert separators before key-press.
        virtual void on_before_press(const LayoutKeyPtr& key) override;

        // Chance to request capitalization before key-release.
        virtual void on_before_release(const LayoutKeyPtr& key) override
        {
            (void)key;
        }

        // Chance to insert separators after key-release.
        virtual void on_after_release(const LayoutKeyPtr& key) override;

    private:
        TextSpanPtr m_added_separator_span;
        bool m_separator_removed{false};
};


// Insert word separators lazily.
class PunctuatorDelayedSeparators : public Punctuator
{
    public:
        using Super = Punctuator;

        PunctuatorDelayedSeparators(const ContextBase& context);
        virtual ~PunctuatorDelayedSeparators();

        virtual void reset() override;

        virtual void set_separator(const TextSpanPtr& separator_span) override;

        // Remember this separator span for later insertion.
        void set_pending_separator(const TextSpanPtr& separator_span={});

        // Return current pending separator span or None
        TextSpanPtr get_pending_separator();

    private:
        // Chance to insert separators before key-press.
        virtual void on_before_press(const LayoutKeyPtr& key) override;

        // Chance to request capitalization before key-release.
        virtual void on_before_release(const LayoutKeyPtr& key) override;

        // Chance to insert separators after key-release.
        virtual void on_after_release(const LayoutKeyPtr& key) override;

        virtual std::tuple<TextSpanPtr, bool, bool> handle_key_event(
                const LayoutKeyPtr& key,
                const TextSpanPtr& pending_separator_span_in,
                bool before=true);

        // Return the label key had before it was pressed down.
        // Keys may change labels when modifiers are activated or released,
        // but we need the same label before and after the press here.
        // Else ":" turns to "." in Compact and requests capitalization.
        // Multiple pressed keys may be in flight, due to multi-touch
        // so keep the label per key.
        UString get_key_down_label(const LayoutKeyPtr& key, bool before);

    private:
        std::map<const LayoutKey*, UString> m_key_down_labels;
};


#endif // PUNCTUATOR_H
