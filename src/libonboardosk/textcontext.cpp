#include <sstream>

#include "tools/logger.h"
#include "tools/string_helpers.h"
#include "tools/ustringmain.h"

#include "atspistatetracker.h"
#include "keyboardkeylogic.h"
#include "layoutkey.h"
#include "textchanges.h"
#include "textcontext.h"
#include "textdomain.h"
#include "timer.h"
#include "uielement.h"
#include "wordsuggestions.h"


TextContext::TextContext(const ContextBase& context) :
    Super(context)
{}

TextContext::~TextContext()
{}


// Keep track of the current text context with AT-SPI
class TextContextAtspi : public TextContext
{
    public:
        using Super = TextContext;
        using Clock = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        TextContextAtspi(const ContextBase& context);
        virtual ~TextContextAtspi();

    public:
        // TextContext interface
        virtual void reset() override;

        virtual void enable(bool enable) override;
        virtual TextDomain* get_text_domain() const override;

        virtual bool has_focus() const override;

        virtual bool can_insert_text() override;
        virtual void insert_text(TextPos offset, const UString& text) override;
        virtual void insert_text_at_caret(const UString& text) override;
        virtual void delete_text(TextPos offset, TextLength length=1) override;
        virtual void delete_text_before_caret(TextLength length=1) override;
        virtual UString get_context() const override;
        virtual UString get_bot_context() const override;
        virtual UString get_bot_marker() const override;
        virtual TextPos get_bot_offset() const override;

        virtual UString get_line() const override;
        virtual UString get_line_past_caret() const override;
        virtual TextPos get_line_caret_pos() const override;

        virtual TextSpanPtr get_selection_span() const override;
        virtual TextSpanPtr get_span_at_caret() const override;
        virtual TextPos get_caret_pos() const override;
        Noneable<Rect> get_character_extents(TextPos offset);

        virtual TextChanges* get_changes() override;
        virtual bool has_changes() const override;
        virtual void clear_changes() override;

        virtual bool can_auto_punctuate() const override;

        // Remember this separator span for later insertion.
        virtual void set_pending_separator(const TextSpanPtr& separator_span) override;

        // Return current pending separator span or None
        virtual TextSpanPtr get_pending_separator() const override;
        virtual UString get_pending_bot_context() const override;

        virtual void on_onboard_typing(const LayoutKeyPtr& key, ModMask mod_mask) override;

        virtual bool on_text_context_changed() override;

    private:
        bool can_suggest_before_typing() const;
        bool can_record_insertion(const UIElement* element,
                                  const Span& span) const;
        void register_atspi_listeners(bool register_=true);

        void on_text_entry_activated(const UIElementPtr& ui_element);
        void on_text_changed(const ASyncEvent& event);
        void on_text_caret_moved(const ASyncEvent& event);
        void on_atspi_key_pressed(const ASyncEvent& event);

        void handle_key_press(KeyCode keycode, ModMask mod_mask);

        TextSpanPtr record_text_change(const Span& span, bool insert);

        void set_update_context_delay(std::chrono::milliseconds delay);
        void reset_update_context_delay();

        void update_context();


    private:
        AtspiStateTracker* m_atspi_state_tracker{};
        UIElementPtr m_ui_element;
        std::unique_ptr<TextDomains> m_text_domains;
        TextDomain* m_text_domain;

        std::unique_ptr<TextChanges> m_changes;
        bool m_can_insert_text{false};
        bool m_entering_text{false};
        bool m_text_changed{false};
        bool m_begin_of_text = false;   // does context start at begin of text?

        TextPos m_line_caret{0};
        UString m_line;
        UString m_context;
        TextSpanPtr m_selection_span;
        Noneable<TextPos> m_begin_of_text_offset;  // offset of text begin

        TextSpanPtr m_pending_separator_span;
        TimePoint m_last_text_change_time;
        TimePoint m_last_caret_move_time;
        TextPos m_last_caret_move_position{0};

        Noneable<UString> m_last_context;
        Noneable<UString> m_last_line;

        std::unique_ptr<Timer> m_update_context_timer;
        const std::chrono::milliseconds m_update_context_delay_normal{10};
        std::chrono::milliseconds m_update_context_delay{m_update_context_delay_normal};
};


std::unique_ptr<TextContext> TextContext::make_atspi(const ContextBase& context)
{
    return std::make_unique<TextContextAtspi>(context);
}

TextContextAtspi::TextContextAtspi(const ContextBase& context) :
    Super(context),
    m_text_domains(std::make_unique<TextDomains>(context)),
    m_text_domain(m_text_domains->get_nop_domain()),
    m_changes(std::make_unique<TextChanges>()),
    m_update_context_timer(std::make_unique<Timer>(context))
{
}

TextContextAtspi::~TextContextAtspi()
{
    register_atspi_listeners(false);  // disconnect events
}

void TextContextAtspi::reset()
{
}

void TextContextAtspi::enable(bool enable)
{
    register_atspi_listeners(enable);
}

TextDomain* TextContextAtspi::get_text_domain() const
{
    return m_text_domain;
}

bool TextContextAtspi::has_focus() const
{
    return m_ui_element != nullptr;
}

void TextContextAtspi::set_pending_separator(const TextSpanPtr& separator_span)
{
    if (m_pending_separator_span != separator_span)
        m_pending_separator_span = separator_span;
}

TextSpanPtr TextContextAtspi::get_pending_separator() const
{
    return m_pending_separator_span;
}

bool TextContextAtspi::can_insert_text()
{
    // support for inserting is spotty: not in firefox, terminal
    return m_ui_element != nullptr && m_can_insert_text;
}

void TextContextAtspi::insert_text(TextPos offset, const UString& text)
{
    m_ui_element->insert_text(offset, text.to_utf8());

    // Move the caret after insertion if the accessible itself
    // hasn't done so already. This assumes the insertion begins at
    // the current caret position (which always happens to be the case
    // currently).
    // Only the nautilus rename text entry appears to need this.
    TextPos offset_before = offset;
    TextPos offset_after = offset;
    try
    {
        offset_after = m_ui_element->get_caret_offset();
    }
    catch (const AtspiException& ex)
    {
        LOG_INFO << "get_caret_offset() failed: " << ex.what();
        return;
    }

    if (!text.empty() &&
        offset_before == offset_after)
    {
        m_ui_element->set_caret_offset(offset_before +
                                       static_cast<TextLength>(text.size()));
    }
}

void TextContextAtspi::insert_text_at_caret(const UString& text)
{
    TextPos caret_offset{};
    try
    {
        caret_offset = m_ui_element->get_caret_offset();
    }
    catch (const AtspiException& ex)
    {
        LOG_INFO << "get_caret_offset() failed: " << ex.what();
        return;
    }

    insert_text(caret_offset, text);
}

void TextContextAtspi::delete_text(TextPos offset, TextLength length)
{
    m_ui_element->delete_text(offset, offset + length);
}

void TextContextAtspi::delete_text_before_caret(TextLength length)
{
    TextPos caret_offset{};
    try
    {
        caret_offset = m_ui_element->get_caret_offset();
    }
    catch (const AtspiException& ex)
    {
        LOG_INFO << "get_caret_offset() failed: " << ex.what();
        return;
    }

    this->delete_text(caret_offset - length, length);
}

UString TextContextAtspi::get_context() const
{
    if (m_ui_element)
    {
        // Don't update suggestions in scrolling terminals
        if (m_entering_text ||
            !m_text_changed ||
            can_suggest_before_typing())
        {
            return m_context;
        }
    }
    return {};
}

// Returns the predictions context with
// begin of text marker (at text begin).
UString TextContextAtspi::get_bot_context() const
{
    UString context;
    if (m_ui_element)
    {
        context = get_context();

        // prepend domain specific begin-of-text marker
        if (m_begin_of_text)
        {
            UString marker = get_bot_marker();
            if (!marker.empty())
                context = marker + " " + context;
        }
    }

    return context;
}

UString TextContextAtspi::get_bot_marker() const
{
    const auto& domain = get_text_domain();
    if (domain)
        return domain->get_text_begin_marker();
    return {};
}

TextPos TextContextAtspi::get_bot_offset() const
{
    if (m_ui_element)
        return m_begin_of_text_offset;
    return {};
}

UString TextContextAtspi::get_pending_bot_context() const
{
    auto context = get_bot_context();
    if (m_pending_separator_span)
        context += m_pending_separator_span->get_span_text();
    return context;
}

UString TextContextAtspi::get_line() const
{
    if (m_ui_element)
        return m_line;
    return {};
}

UString TextContextAtspi::get_line_past_caret() const
{
    if (m_ui_element)
        return m_line.slice(m_line_caret);
    return {};
}

TextPos TextContextAtspi::get_line_caret_pos() const
{
    if (m_ui_element)
        return m_line_caret;
    return {};
}

TextSpanPtr TextContextAtspi::get_selection_span() const
{
    if (m_ui_element)
        return m_selection_span;
    return {};
}

TextSpanPtr TextContextAtspi::get_span_at_caret() const
{
    if (!m_ui_element ||
        !m_selection_span)
        return {};

    auto span = m_selection_span->clone();
    span->length = 0;
    return span;
}

TextPos TextContextAtspi::get_caret_pos() const
{
    if (m_ui_element)
        return m_selection_span->begin();
    return {};
}

Noneable<Rect> TextContextAtspi::get_character_extents(TextPos offset)
{
    if (m_ui_element)
        return m_ui_element->get_character_extents(offset);
    return {};
}

TextChanges* TextContextAtspi::get_changes()
{
    return m_changes.get();
}

bool TextContextAtspi::has_changes() const
{
    return !m_changes->empty();
}

void TextContextAtspi::clear_changes()
{
    m_changes->clear();
}

bool TextContextAtspi::can_auto_punctuate() const
{
    const auto& domain = get_text_domain();
    if (domain)
        return domain->can_auto_punctuate(m_begin_of_text);
    return false;
}

bool TextContextAtspi::can_suggest_before_typing() const
{
    const auto& domain = get_text_domain();
    if (domain)
        return domain->can_suggest_before_typing();
    return true;
}

bool TextContextAtspi::can_record_insertion(const UIElement* element, const Span& span) const
{
    const auto& domain = get_text_domain();
    if (domain)
        return domain->can_record_insertion(element, span);
    return true;
}

void TextContextAtspi::register_atspi_listeners(bool register_)
{
    if (register_)
    {
        if (!m_atspi_state_tracker)
        {
            m_atspi_state_tracker = get_atspi_state_tracker();
            if (m_atspi_state_tracker)
            {
                m_atspi_state_tracker->text_entry_activated.connect(
                            this, [&](const UIElementPtr& e){on_text_entry_activated(e);});
                m_atspi_state_tracker->text_changed.connect(
                            this, [&](const ASyncEvent& e){on_text_changed(e);});
                m_atspi_state_tracker->text_caret_moved.connect(
                            this, [&](const ASyncEvent& e){on_text_caret_moved(e);});
            }
        }
    }
    else
    {
        if (m_atspi_state_tracker)
        {
            m_atspi_state_tracker->text_entry_activated.disconnect(this);
            m_atspi_state_tracker->text_changed.disconnect(this);
            m_atspi_state_tracker->text_caret_moved.disconnect(this);
        }
        m_atspi_state_tracker = nullptr;
        m_ui_element = nullptr;
    }
}

void TextContextAtspi::on_text_entry_activated(const UIElementPtr& ui_element)
{
    auto* ws = get_word_suggestions();

    // old text_domain still valid here
    ws->on_text_entry_deactivated();

    // keep track of the active accessible asynchronously
    m_ui_element = ui_element;
    m_entering_text = false;
    m_text_changed = false;

    // select text domain matching this accessible
    if (ui_element)
    {
        m_text_domain = m_text_domains->find_match(ui_element.get());
        m_can_insert_text = ui_element->can_insert_text();
    }
    else
    {
        m_text_domain = m_text_domains->get_nop_domain();
        m_can_insert_text = false;
    }

    // log accessible info
    if (logger()->can_log(LogLevel::ATSPI))
    {
        LOG_ATSPI << std::string(70, '-');
        LOG_ATSPI << "Accessible focused: ";
        std::string indent(4, ' ');
        if (ui_element)
        {
            std::stringstream ss;
            ui_element->dump(ss, indent, "\n");
            LOG_ATSPI << ss.str();
            LOG_ATSPI << indent << "text_domain=" << m_text_domain;
            LOG_ATSPI << indent << "can_insert_text=" << repr(m_can_insert_text);
        }
        else
        {
            LOG_ATSPI << indent << "nullptr";
        }
    }

    update_context();

    ws->on_text_entry_activated();
}

void TextContextAtspi::on_text_changed(const ASyncEvent& event)
{
    LOG_DEBUG << "span=" << event.span
              << " insert=" << event.insert;

    auto insertion_span = record_text_change(event.span,
                                             event.insert);
    // synchronously notify of text insertion
    if (insertion_span)
    {
        TextPos caret_offset = -1;
        try
        {
            caret_offset = m_ui_element->get_caret_offset();
        }
        catch (const AtspiException& ex)
        {
            LOG_INFO << "get_caret_offset() failed: " << ex.what();
        }

        if (caret_offset >= 0)
        {
            if (auto ws = get_word_suggestions())
                ws->on_text_inserted(insertion_span, caret_offset);
        }
    }

    m_last_text_change_time = Clock::now();
    update_context();
}

void TextContextAtspi::on_text_caret_moved(const ASyncEvent& event)
{
    m_last_caret_move_time = Clock::now();
    m_last_caret_move_position = event.caret;
    update_context();

    if (auto ws = get_word_suggestions())
        ws->on_text_caret_moved();
}

// disabled, Francesco didn't receive any AT-SPI key-strokes.
void TextContextAtspi::on_atspi_key_pressed(const ASyncEvent& event)
{
    (void)event;
    // keycode = event.hw_code // uh oh, only keycodes...
    //                         // hopefully "c" doesn't move around a lot.
    // modifiers = event.modifiers
    // handle_key_press(keycode, modifiers)
}

void TextContextAtspi::on_onboard_typing(const LayoutKeyPtr& key, ModMask mod_mask)
{
    if (key->is_text_changing())
    {
        KeyCode keycode = 0;
        if (key->is_return())
        {
            keycode = KeyCodeSymbols::KP_Enter;
        }
        else
        {
            std::string& label = key->get_label();
            if (label == "C" || label == "c")
                keycode = KeyCodeSymbols::C;
        }

        handle_key_press(keycode, mod_mask);
    }
}

void TextContextAtspi::handle_key_press(KeyCode keycode, ModMask mod_mask)
{
    if (m_ui_element)
    {
        const auto& domain = get_text_domain();
        if (domain)
        {
            Noneable<bool> end_of_editing;
            std::tie(m_entering_text, end_of_editing) =
                  domain->handle_key_press(keycode, mod_mask);

            if (!end_of_editing.is_none())
            {
                auto ws = get_word_suggestions();
                if (end_of_editing)
                    ws->commit_changes();
                else
                    ws->discard_changes();
            }
        }
    }
}

TextSpanPtr TextContextAtspi::record_text_change(const Span& span, bool insert)
{
    auto key_logic = get_key_logic();
    const auto& ui_element = m_ui_element;

    TextSpanPtr insertion_span;
    Noneable<TextLength> char_count;
    if (!ui_element)
        return {};
    try
    {
        char_count = ui_element->get_character_count();
    }
    catch (const AtspiException& ex)
    {
        LOG_INFO << "get_caret_count() failed: " << ex.what();
    }

    if (logger()->can_log(LogLevel::ATSPI))
    {
        LOG_ATSPI << "1:"
                  << " span=" << span
                  << " insert=" << repr(insert)
                  << " ui_element=" << ui_element
                  << " char_count=" << char_count;
    }

    std::vector<TextSpanPtr> spans_to_update;

    if (!char_count.is_none())
    {
        // record the change
        if (insert)
        {
            // None = remember nothing, just update existing spans.
            Noneable<TextLength> include_length;

            if (m_entering_text)
            {
                bool can_record_insertion =
                        this->can_record_insertion(ui_element.get(), span);

                if (logger()->can_log(LogLevel::ATSPI))
                {
                    LOG_ATSPI << "2:"
                              << " can_record_insertion= " << can_record_insertion
                              << " is_typing=" << key_logic->is_typing();
                }

                if (can_record_insertion)
                {
                    if (key_logic->is_typing() || span.length < 30)
                    {
                        // Remember all of the insertion, might have been
                        // a pressed snippet || wordlist button.
                        include_length = -1;
                    }
                    else
                    {
                        // Remember only the first few characters.
                        // Large inserts can be paste, reload || scroll
                        // operations. Only learn the first word of these.
                        include_length = 2;
                    }

                    // simple span for current insertion
                    TextPos begin = std::max(span.begin - 100, 0);
                    TextPos end = std::min(span.begin + span.length + 100,
                                           char_count.value);
                    TextSpanPtr text;
                    try
                    {
                        text = ui_element->get_text({begin, end-begin});
                    }
                    catch (const AtspiException& ex)
                    {
                        LOG_INFO << "get_text() 1 failed: " << ex.what();
                    }

                    if (text)
                        insertion_span = std::make_shared<TextSpan>(
                                             span.begin, span.length, text->text, begin);
                }
            }

            spans_to_update = m_changes->insert(span, include_length);
        }
        else
        {
            spans_to_update = m_changes->delete_(span, m_entering_text);
        }

        // update text of all modified spans
        for (auto& sp : spans_to_update)
        {
            // Get some more text around the span to hopefully
            // include whole words at beginning and end.
            TextPos begin = std::max(sp->begin() - 100, 0);
            TextPos end = std::min(sp->end() + 100, char_count.value);
            try
            {
                auto t = ui_element->get_text({begin, end-begin});
                sp->text = t->text;
            }
            catch (const AtspiException& ex)
            {
                LOG_INFO << "get_text() 2 failed: " << ex.what();
                sp->text.clear();
            }
            sp->text_pos = begin;
        }
    }

    m_text_changed = true;

    if (logger()->can_log(LogLevel::ATSPI))
    {
        LOG_ATSPI << "3:"
                  << " insertion_span=" << insertion_span
                  << " spans_to_update=" << spans_to_update
                  << " changes=" << *m_changes;
    }

    return insertion_span;
}

void TextContextAtspi::set_update_context_delay(std::chrono::milliseconds delay)
{
    m_update_context_delay = delay;
}

void TextContextAtspi::reset_update_context_delay()
{
    m_update_context_delay = m_update_context_delay_normal;
}

void TextContextAtspi::update_context()
{
    m_update_context_timer->start(m_update_context_delay,
                                  [this]{return on_text_context_changed();});
}

bool TextContextAtspi::on_text_context_changed()
{
    // Clear pending separator when the user clicked to move
    // the cursor away from the separator position.
    if (m_pending_separator_span)
    {
        // Lone caret movement, no recent text change?
        if (m_last_caret_move_time - m_last_text_change_time >
            std::chrono::milliseconds(1000))
        {
            // Away from the separator?
            if (m_last_caret_move_position !=
                m_pending_separator_span->begin())
            {
                set_pending_separator({});
            }
        }
    }

    if (m_text_domain)
    {
        ReadContextResults r;
        if (m_text_domain->read_context(m_ui_element.get(), r))
        {
            m_context = r.context;
            m_line = r.line;
            m_line_caret = r.line_caret;
            m_selection_span = r.selection_span;
            m_begin_of_text = r.begin_of_text;
            m_begin_of_text_offset = r.begin_of_text_offset;

            // make sure to include bot-markers && pending separator
            auto context = get_pending_bot_context();
            bool change_detected = (m_last_context != context ||
                                                      m_last_line != m_line);
            if (change_detected)
            {
                m_last_context = context;
                m_last_line    = m_line;
            }

            auto ws = get_word_suggestions();
            ws->on_text_context_changed(change_detected);
        }
    }

    return false;
}


