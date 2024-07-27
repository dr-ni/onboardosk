#ifndef TEXTDOMAIN_H
#define TEXTDOMAIN_H

#include <memory>
#include <vector>

#include "tools/ustringmain.h"
#include "tools/keydecls.h"
#include "tools/textdecls.h"

#include "exception.h"
#include "keydefinitions.h"
#include "onboardoskglobals.h"
#include "textchangesdecls.h"
#include "uielement.h"

class PartialURLParser;
class TextSpan;
class TextDomain;
class UIElement;
class UStringPattern;


struct ReadContextResults
{
    UString context;
    UString line;
    TextPos line_caret{0};
    TextSpanPtr selection_span;
    bool begin_of_text{false};
    TextPos begin_of_text_offset{0};
};

struct GetTextAfterPromptResults
{
    std::vector<UString> context_lines;
    TextLength prompt_length;
    UString line;
    TextPos line_start;
    TextPos line_caret;

    bool operator==(const GetTextAfterPromptResults& other) const
    {
        return context_lines == other.context_lines &&
             prompt_length == other.prompt_length &&
             line == other.line &&
             line_start == other.line_start &&
             line_caret == other.line_caret;
    }
};

std::ostream& operator<<(std::ostream& s, const GetTextAfterPromptResults& r);


// Abstract base for domain specific functionalty.
class TextDomain : public ContextBase
{
    public:
        TextDomain(const ContextBase& context);
        virtual ~TextDomain();

        virtual bool matches(const UIElement* element);

        virtual bool read_context(const UIElement* element, ReadContextResults& results) = 0;

        virtual UString get_text_begin_marker();

        // Get word separator to add after inserting a prediction choice.
        virtual UString get_auto_separator(const UString& context);

        // Search for a valid filename backwards across separators.
        UString search_valid_file_name(const std::vector<UString>& strings);

        // Grow span before learning to include e.g. whole URLs.;
        std::tuple<TextPos, TextLength, UString> grow_learning_span(const TextSpanPtr& text_span) const;

        virtual bool can_record_insertion(const UIElement* element,
                                          const Span& span);

        virtual bool can_give_keypress_feedback();

        virtual bool can_spell_check(const TextSpanPtr& section_span);

        virtual bool can_auto_correct(const TextSpanPtr& section_span);

        virtual bool can_auto_punctuate(bool has_begin_of_text);

        // Can give word suggestions before typing has started?
        virtual bool can_suggest_before_typing();

        virtual std::tuple<bool, Noneable<bool>> handle_key_press(KeyCode keycode, ModMask mod_mask);


        // Split text at whitespace and other delimiters where
        // growing learning spans should stop.
        void split_growth_sections(const UString& text, std::vector<UString>& tokens, std::vector<Span>& spans) const;

    protected:
        std::unique_ptr<PartialURLParser> m_url_parser;
};

std::ostream& operator<<(std::ostream& s, const TextDomain* d);

// Do-nothing domain, no focused accessible.
class DomainNOP : public TextDomain
{
    public:
        using Super = TextDomain;

        DomainNOP(const ContextBase& context);

        virtual bool matches(const UIElement* element) override;

        virtual bool read_context(const UIElement* element, ReadContextResults& results) override;

        // Get word separator to add after inserting a prediction choice.
        virtual UString get_auto_separator(const UString& context) override;
};


// Do-nothing domain for password entries
class DomainPassword : public DomainNOP
{
    public:
        using Super = DomainNOP;

        DomainPassword(const ContextBase& context);

        virtual bool matches(const UIElement* element) override;

        virtual bool can_give_keypress_feedback() override;
};


// Default domain for generic text entry
class DomainGenericText : public TextDomain
{
    public:
        using Super = TextDomain;

        DomainGenericText(const ContextBase& context);

        virtual bool matches(const UIElement* element) override;

        // Extract prediction context from the accessible
        virtual bool read_context(const UIElement* element, ReadContextResults& results) override;

        // Can we auto-correct this span?.;
        virtual bool can_spell_check(const TextSpanPtr& section_span) override;

        // Can we auto-correct this span?.
        virtual bool can_auto_correct(const TextSpanPtr& section_span) override;

        virtual bool can_auto_punctuate(bool has_begin_of_text) override
        {
            (void)has_begin_of_text;
            return true;
        }

        virtual UString get_text_begin_marker() override
        {
            return "<bot:txt>";
        }
};

// (Firefox) address bar
class DomainURL : public DomainGenericText
{
    public:
        using Super = DomainGenericText;

        DomainURL(const ContextBase& context);

        virtual bool matches(const UIElement* element) override;

        // Get word separator to add after inserting a prediction choice.
        virtual UString get_auto_separator(const UString& context) override;

        virtual UString get_text_begin_marker() override;

        virtual bool can_spell_check(const TextSpanPtr& section_span) override;
};


// Terminal entry, in particular gnome-terminal
class DomainTerminal : public TextDomain
{
    private:
        static std::vector<UStringPattern> m_prompt_patterns;
        static std::vector<UStringPattern> m_prompt_blacklist_patterns;

    public:
        using Super = TextDomain;

        DomainTerminal(const ContextBase& context);

        virtual bool matches(const UIElement* element) override;

        // Extract prediction context from the accessible
        virtual bool read_context(const UIElement* element, ReadContextResults& results) override;

        // Return text from the input area of the terminal after the prompt.
        void get_text_after_prompt(const UIElement* element, TextPos caret_offset,
                                   Noneable<bool> last_typed_was_separator,
                                   GetTextAfterPromptResults& results);

        // Search for a prompt and return the offset where the user input starts.
        // Until we find a better way just look for some common prompt patterns.
        TextPos find_prompt(const UString& context);

        TextPos find_blacklisted_prompt(const UString& context);

        virtual UString get_text_begin_marker() override;

        virtual bool can_record_insertion(const UIElement* element,
                                          const Span& span) override;

        // Can give word suggestions before typing has started?
        virtual bool can_suggest_before_typing() override;

        // End recording and learn when pressing [Return]
        // because text that is scrolled out of view is
        // lost in a terminal.
        virtual std::tuple<bool, Noneable<bool>> handle_key_press(KeyCode keycode, ModMask mod_mask) override;

        // Only auto-punctuate in Terminal when no prompt was detected.;
        // Intention is to allow punctuation assistance in editors, but disable;
        // it when entering commands at the prompt, e.g. for "cd ..".;
        virtual bool can_auto_punctuate(bool has_begin_of_text) override;
};


class TextDomains : public ContextBase
{
    public:
        TextDomains(const ContextBase& context);

        TextDomains(const TextDomains& context) = delete;
        TextDomains& operator=(const TextDomains&) = delete;

        TextDomain* find_match(const UIElement* element);
        TextDomain* get_nop_domain();

    private:
        std::vector<std::unique_ptr<TextDomain>> m_domains;
};



#endif // TEXTDOMAIN_H
