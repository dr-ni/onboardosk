#include <cassert>
#include <regex>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "tools/container_helpers.h"
#include "tools/logger.h"
#include "tools/path_helpers.h"
#include "tools/string_helpers.h"
#include "tools/ustringregex.h"

#include "keyboardkeylogic.h"
#include "partialurlparser.h"
#include "textdomain.h"
#include "textchanges.h"


std::ostream& operator<<(std::ostream& s, const GetTextAfterPromptResults& r)
{
    s << "{" << r.context_lines
      << ", " << r.prompt_length
      << ", " << r.line
      << ", " << r.line_start
      << ", " << r.line_caret << "}";
    return s;
}

std::ostream& operator<<(std::ostream& s, const TextDomain* d)
{
    if (d)
        s << typeid(d).name();
    else
        s << "nullptr";
    return s;
}


TextDomain::TextDomain(const ContextBase& context) :
    ContextBase(context),
    m_url_parser(std::make_unique<PartialURLParser>())
{
}

TextDomain::~TextDomain()
{
}

bool TextDomain::matches(const UIElement* element)
{
    // Weed out unity text entries that report being editable but don't
    // actually provide methods of the Atspi.Text interface.
    return element->is_text_entry();
}

UString TextDomain::get_text_begin_marker()
{
    return {};
}
#include <iostream>
UString TextDomain::get_auto_separator(const UString& context)
{
    UString separator = " ";

    // Split at whitespace to catch whole URLs/file names and
    // keep separators.
    std::vector<UString> strings;
    static UStringPattern pattern(R"((\s+))");
    pattern.split_all(context, strings);

    if (!strings.empty())
    {
        UString s = strings.back();
        if (m_url_parser->is_maybe_url(s))
        {
            separator = m_url_parser->get_auto_separator(s);
        }
        else
        {
            UString fn = search_valid_file_name(strings);
            if (!fn.empty())
                s = fn;

            if (m_url_parser->is_maybe_filename(s))
            {
                UString url = UString("file://") + s;
                separator = m_url_parser->get_auto_separator(url);
            }
        }
    }

    return separator;
}

UString TextDomain::search_valid_file_name(const std::vector<UString>& strings)
{
    // Search backwards across spaces for an absolute filename.
    size_t max_sections = 16;  // allow this many path sections (separators+tokens)
    int n = static_cast<int>(std::min(max_sections, strings.size()));
    for (int i=0; i<n; i++)
    {
        UString fn = join(slice(strings, -1-i));
        std::string cfn = fn.to_utf8();

        // Is it (part of) a valid absolute filename?
        // Do least impact checks first.
        if (m_url_parser->is_maybe_filename(fn) &&
            fs::path(cfn).is_absolute())
        {
            // Does a file or directory of this name exist?
            if (fs::exists(cfn))
                return fn;

            // Is it at least an incomplete filename of
            // an existing file?
            if (path_exists_starting_with(cfn))
                return fn;
        }
    }

    return {};
}

std::tuple<TextPos, TextLength, UString>  TextDomain::grow_learning_span(
        const TextSpanPtr& text_span) const
{
    UString text   = text_span->get_text();
    TextPos offset = text_span->text_begin();
    TextPos begin  = text_span->begin() - offset;
    TextPos end    = text_span->end() - offset;
    TextLength length   = end - begin;

    std::vector<UString> sections;
    std::vector<Span> spans;
    split_growth_sections(text, sections, spans);

    for (size_t i=0; i<spans.size(); i++)
    {
        auto& s = spans[i];
        if (s.intersects(begin, length))  // intersects?
        {
            const UString& section = sections[i];
            const Span& span = spans[i];
            if (m_url_parser->is_maybe_url(section) ||
                m_url_parser->is_maybe_filename(section))
            {
                begin = std::min(begin, span.begin);
                end = std::max(end, span.end());
                length = end - begin;
            }
        }
    }

    return {begin + offset, length, text.slice(begin, end)};
}

bool TextDomain::can_record_insertion(const UIElement* element, const Span& span)
{
    (void)element;
    (void)span;
    return true;
}

bool TextDomain::can_give_keypress_feedback()
{
    return true;
}

bool TextDomain::can_spell_check(const TextSpanPtr& section_span)
{
    (void)section_span;
    return false;
}

bool TextDomain::can_auto_correct(const TextSpanPtr& section_span)
{
    (void)section_span;
    return false;
}

bool TextDomain::can_auto_punctuate(bool has_begin_of_text)
{
    (void) has_begin_of_text;
    return false;
}

bool TextDomain::can_suggest_before_typing()
{
    return true;
}

std::tuple<bool, Noneable<bool> > TextDomain::handle_key_press(KeyCode keycode, ModMask mod_mask)
{
    (void)keycode;
    (void)mod_mask;
    return {true, {}};  // entering_text, end_of_editing
}


void TextDomain::split_growth_sections(const UString& text,
                                       std::vector<UString>& tokens,
                                       std::vector<Span>& spans) const
{
    static UStringPattern pattern(R"([^\s?#@]+)");
    pattern.find_all(text, tokens, spans);
}




DomainNOP::DomainNOP(const ContextBase& context) :
    Super(context)
{}

bool DomainNOP::matches(const UIElement* element)
{
    (void) element;
    return true;
}

bool DomainNOP::read_context(const UIElement* element, ReadContextResults& results)
{
    (void)element;
    results = {};
    return false;
}

UString DomainNOP::get_auto_separator(const UString& context)
{
    (void)context;
    return {};
}



DomainPassword::DomainPassword(const ContextBase& context) :
    Super(context)
{}

bool DomainPassword::matches(const UIElement* element)
{
    return element->is_password_entry();
}

bool DomainPassword::can_give_keypress_feedback()
{
    return false;
}


DomainGenericText::DomainGenericText(const ContextBase& context) :
    Super(context)
{
}

bool DomainGenericText::matches(const UIElement* element)
{
    return Super::matches(element);
}

bool DomainGenericText::read_context(const UIElement* element, ReadContextResults& results)
{
    // get caret position from selection
    Noneable<Span> selection = element->get_selection();

    TextSpanPtr line_span;
    TextLength count{};


    // get text around the caret position
    try
    {
        count = element->get_character_count();

        if (selection.is_none())
        {
            TextPos offset = element->get_caret_offset();

            // In Zesty, firefox 50.1 often returns caret position -1
            // when typing into the urlbar. Assume we are at the end
            // of the text when that happens.
            if (offset < 0)
            {
                LOG_WARNING << "Atspi.Text.get_caret_offset()"
                            << " returned invalid " << offset
                            << " Pretending the cursor is at the end "
                            << "of the text at offset " << count << ".";
                offset = count;
            }

            selection = Span{offset, 0};
        }

        line_span = element->get_line_at_offset(selection.value.begin);

        results.line = replace_all(line_span->text, "\n", "");
        results.line_caret = std::max(selection.value.begin - line_span->begin(), 0);

        TextPos begin = std::max(selection.value.begin - 256, 0);
        TextPos end   = std::min(selection.value.begin + 100, count);

        // Not all text may be available for large selections, but we only need the
        // part before the begin of the selection/caret.
        results.selection_span = element->get_text({begin, end-begin});
        results.selection_span->pos = selection.value.begin;
        results.selection_span->length = selection.value.length;
        results.selection_span->text_pos = begin;

        results.context = results.selection_span->text.slice(0, selection.value.begin - begin);
        results.begin_of_text = begin == 0;
        results.begin_of_text_offset = 0;
    }
    catch (const AtspiException& ex)
    {
        LOG_INFO << "AT-SPI call failed: " << ex.what();
        return false;
    }

    return true;
}

bool DomainGenericText::can_spell_check(const TextSpanPtr& section_span)
{
    auto section = section_span->get_span_text();
    return !m_url_parser->is_maybe_url(section) &&
           !m_url_parser->is_maybe_filename(section);
}

bool DomainGenericText::can_auto_correct(const TextSpanPtr& section_span)
{
    return can_spell_check(section_span);
}



DomainURL::DomainURL(const ContextBase& context) :
    Super(context)
{}

bool DomainURL::matches(const UIElement* element)
{
    return element->is_urlbar();
}

UString DomainURL::get_auto_separator(const UString& context)
{
    return m_url_parser->get_auto_separator(context);
}

UString DomainURL::get_text_begin_marker()
{
    return "<bot:url>";
}

bool DomainURL::can_spell_check(const TextSpanPtr& section_span)
{
    (void) section_span;
    return false;
}



std::vector<UStringPattern> DomainTerminal::m_prompt_patterns;
std::vector<UStringPattern> DomainTerminal::m_prompt_blacklist_patterns;

DomainTerminal::DomainTerminal(const ContextBase& context) :
    Super(context)
{
    if (m_prompt_patterns.empty())
    {
        for (auto p : array_of<const char*>
            (
                 R"(^gdb$ )",
                 R"(^>>> )",              // python
                 R"(^In \[[0-9]*\]: )",   // ipython
                 R"(^:)",                 // vi command mode
                 R"(^/)",                 // vi search
                 R"(^\?)",                // vi reverse search
                 R"(\$ )",                // generic prompt
                 R"(# )",                 // root prompt
                 R"(^.*?@.*?/.*?> )"      // fish
            ))
        {
            m_prompt_patterns.emplace_back(p);
        }

        for (auto p : array_of<const char*>
            (
                 R"(^\(.*\)`.*': )"  // bash incremental search
            ))
        {
            m_prompt_blacklist_patterns.emplace_back(p);
        }
    }
}

bool DomainTerminal::matches(const UIElement* element)
{
    return Super::matches(element) &&
            element->is_terminal();
}

bool DomainTerminal::read_context(const UIElement* element, ReadContextResults& results)
{
    auto key_logic = get_key_logic();
    try
    {
        TextPos offset = element->get_caret_offset();
        GetTextAfterPromptResults r;
        get_text_after_prompt(element, offset, key_logic->get_last_typed_was_separator(), r);

        if (r.prompt_length)
        {
            results.begin_of_text = true;
            results.begin_of_text_offset = r.line_start;
        }
        else
        {
            results.begin_of_text = false;
            results.begin_of_text_offset = 0;
        }

        results.context = join(r.context_lines, "");
        auto before_line = join(slice(r.context_lines, 0, -1), "");
        results.selection_span = std::make_unique<TextSpan>(
            offset, 0,
            before_line + r.line,
            r.line_start - static_cast<int>(before_line.size()));

        results.line_caret = r.line_caret;
    }
    catch (const AtspiException& ex)
    {
        LOG_INFO << "AT-SPI call failed: " << ex.what();
        return false;
    }

    return true;
}

void DomainTerminal::get_text_after_prompt(const UIElement* element,
                                           TextPos caret_offset,
                                           Noneable<bool> last_typed_was_separator,
                                           GetTextAfterPromptResults& results)
{
    TextSpanPtr r = element->get_line_at_offset(caret_offset);
    auto line = r->text;
    TextPos line_start = r->begin();
    TextPos line_caret = caret_offset - line_start;

    // remove prompt from the current or previous lines
    results.context_lines.clear();
    TextLength prompt_length = 0;
    auto l = line.slice(0, line_caret);

    // Zesty: byobu running in gnome-terminal doesn't report trailing
    // spaces in text && caret-position.
    // Awful hack: assume there is always a trailing space when the caret
    // is at the end of the line && we just typed a separator.
    if (line.slice(line_caret) == "\n" &&
        last_typed_was_separator &&
        element->is_byobu())
    {
        l += " ";
    }

    for (size_t i=0; i<2; i++)
    {
        // matching blacklisted prompt? -> cancel whole context
        if (find_blacklisted_prompt(l))
        {
            results.context_lines.clear();
            prompt_length = 0;
            break;
        }

        prompt_length = find_prompt(l);
        results.context_lines.insert(results.context_lines.begin(), l.slice(prompt_length));
        if (i == 0)
        {
            line = line.slice(prompt_length);  // cut prompt from input line
            line_start += prompt_length;
            line_caret -= prompt_length;
        }
        if (prompt_length)
            break;

        // no prompt yet -> let context reach
        // across one more line break
        r = element->get_line_before_offset(caret_offset);
        l = r->text;
    }

    results.prompt_length = prompt_length;
    results.line = line;
    results.line_start = line_start;
    results.line_caret = line_caret;
}

TextPos DomainTerminal::find_prompt(const UString& context)
{
    for (const auto& pattern : m_prompt_patterns)
    {
        UStringMatch match;
        if (pattern.find(context, match))
        {
            auto p = pattern.get_string();
            return match.end();
        }
    }
    return 0;
}

TextPos DomainTerminal::find_blacklisted_prompt(const UString& context)
{
    for (const auto& pattern : m_prompt_blacklist_patterns)
    {
        UStringMatch match;
        if (pattern.find(context, match))
            return match.end();
    }
    return 0;
}

UString DomainTerminal::get_text_begin_marker()
{
    return "<bot:term>";
}

bool DomainTerminal::can_record_insertion(const UIElement* element, const Span& span)
{
    // Only record (for learning) when there is a known prompt in sight.
    // Problem: learning won't happen for uncommon prompts, but less random
    // junk scrolling by should enter the user model in return.
    GetTextAfterPromptResults params;
    get_text_after_prompt(element, span.begin, {}, params);
    bool result = params.prompt_length >= 0;

    LOG_ATSPI << "span=" << span
              << " params=" << params
              << " result=" << repr(result);

    return result;
}

bool DomainTerminal::can_suggest_before_typing()
{
    // Mostly prevent updates to word suggestions while text is scrolling by
    return false;
}

std::tuple<bool, Noneable<bool> > DomainTerminal::handle_key_press(KeyCode keycode, ModMask mod_mask)
{
    if (keycode == KeyCodeSymbols::Return ||
        keycode == KeyCodeSymbols::KP_Enter)
    {
        return {false, true};
    }
    else
        if (keycode == KeyCodeSymbols::C &&
            mod_mask & Modifier::CTRL)
        {
            return {false, false};
        }

    return {true, {}};  // entering_text, end_of_editing
}

bool DomainTerminal::can_auto_punctuate(bool has_begin_of_text)
{
    return !has_begin_of_text;
}




TextDomains::TextDomains(const ContextBase& context) :
    ContextBase(context)
{
    m_domains.emplace_back(std::make_unique<DomainTerminal>(context));
    m_domains.emplace_back(std::make_unique<DomainURL>(context));
    m_domains.emplace_back(std::make_unique<DomainPassword>(context));
    m_domains.emplace_back(std::make_unique<DomainGenericText>(context));
    m_domains.emplace_back(std::make_unique<DomainNOP>(context));
}

TextDomain*TextDomains::find_match(const UIElement* element)
{
    for (auto& domain : m_domains)
        if (domain->matches(element))
            return domain.get();
    assert(false);  // should never happen, default domain always matches
    return {};
}

TextDomain*TextDomains::get_nop_domain()
{
    assert(!m_domains.empty());
    return m_domains.back().get();
}


