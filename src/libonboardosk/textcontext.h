#ifndef TEXTCONTEXT_H
#define TEXTCONTEXT_H

#include <memory>
#include <string>

#include "tools/textdecls.h"

#include "layoutdecls.h"
#include "onboardoskglobals.h"
#include "textchangesdecls.h"


class TextChanges;
class TextDomain;
class UString;

// Keep track of the text of the currently focused text entry
class TextContext : public ContextBase
{
    public:
        using Super = ContextBase;
        TextContext(const ContextBase& context);
        virtual ~TextContext();

        virtual void reset() = 0;

        virtual void enable(bool enable) = 0;

        virtual TextDomain* get_text_domain() const = 0;

        // Still attached to a focused UIElement?
        virtual bool has_focus() const = 0;

        // Can delete or insert text into the accessible?
        virtual bool can_insert_text() = 0;

        virtual void insert_text(TextPos offset, const UString& text) = 0;
        virtual void insert_text_at_caret(const UString& text) = 0;
        virtual void delete_text(TextPos offset, TextLength length=1) = 0;
        virtual void delete_text_before_caret(TextLength length=1) = 0;

        // Returns the predictions context, i.e. some range of
        // text before the caret position.
        virtual UString get_context() const = 0;

        // Returns the predictions context with
        // begin-of-text marker (at text begin).
        virtual UString get_bot_context() const = 0;
        virtual UString get_bot_marker() const = 0;
        virtual TextPos get_bot_offset() const = 0;

        virtual UString get_line() const = 0;
        virtual UString get_line_past_caret() const = 0;
        virtual TextPos get_line_caret_pos() const = 0;

        virtual TextSpanPtr get_selection_span() const = 0;
        virtual TextSpanPtr get_span_at_caret() const  = 0;
        virtual TextPos get_caret_pos() const = 0;

        virtual TextChanges* get_changes() = 0;
        // Are there any changes to learn?
        virtual bool has_changes() const = 0;
        virtual void clear_changes() = 0;

        virtual bool can_auto_punctuate()  const = 0;

        // Remember this separator span for later insertion.
        virtual void set_pending_separator(const TextSpanPtr& separator_span={}) = 0;

        // Return current pending separator span or None
        virtual TextSpanPtr get_pending_separator() const = 0;

        // Context including bot marker and pending separator.
        virtual UString get_pending_bot_context() const = 0;

        virtual void on_onboard_typing(const LayoutKeyPtr& key, ModMask mod_mask) = 0;

        virtual bool on_text_context_changed() = 0;


        static std::unique_ptr<TextContext> make_atspi(const ContextBase& context);
};




#endif // TEXTCONTEXT_H
