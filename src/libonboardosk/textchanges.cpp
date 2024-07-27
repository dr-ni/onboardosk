
#include "tools/string_helpers.h"
#include "tools/ustringmain.h"
#include "tools/time_helpers.h"

#include "textchanges.h"


TextSpan::~TextSpan()
{}

TextSpan& TextSpan::union_inplace(const TextSpan& span)
{
    TextPos begin = std::min(this->begin(), span.begin());
    TextPos end   = std::max(this->end(),   span.end());
    TextLength l = end - begin;
    TextPos middle = l / 2;
    this->text   = this->text.slice(0, middle - this->text_pos) +
                   span.text.slice(middle - span.text_pos);
    this->pos    = begin;
    this->length = l;
    this->last_modified = std::max(this->last_modified,
                                   span.last_modified);
    return *this;
}

UString TextSpan::get_text(const Noneable<TextPos>& begin, const Noneable<TextPos>& end) const
{
    TextPos b = begin.is_none() ? this->begin() : begin.value;
    TextPos e = begin.is_none() ? this->end() : end.value;
    return this->text.slice(b - this->text_pos, e - this->text_pos);
}

UString TextSpan::get_span_text() const
{
    UString s = this->text;
    return s.slice(begin(), end());
}

UString TextSpan::get_text_until_span()
{
    return this->text.slice(0, end() - this->text_pos);
}

UString TextSpan::get_text_from_span()
{
    return this->text.slice(this->pos - this->text_pos);
}

UString TextSpan::get_text_after_span()
{
    return this->text.slice(end() - this->text_pos);
}

UString TextSpan::get_char_before_span() const
{
    TextPos p = this->pos - this->text_pos;
    return this->text.slice(p - 1, p);
}

UString TextSpan::get_last_char_in_span() const
{
    TextPos p = this->end() - this->text_pos;
    return this->text.slice(p - 1, p);
}

UString TextSpan::escape(const UString& text_) const
{
    return text_.replace("\n", R"(\n)");
}

std::ostream& operator<<(std::ostream& s, const TextSpan* span){
    if (span)
        s << *span;
    else
        s << "null";
    return s;
}

std::ostream& operator<<(std::ostream& s, const TextSpan& span){
    s << "TextSpan(" << span.pos
      << ", "  << span.length
      << ", " << span.escape(span.text)
      << ", " << span.text_begin()
      << ", " << seconds_since_epoch(span.last_modified)
      << ")";
    return s;
}



TextChanges::TextChanges()
{
}

TextChanges::TextChanges(const TextSpans& spans) :
    m_spans(spans)
{
}

TextChanges::TextChanges(const TextSpans&& spans) :
    m_spans(spans)
{
}

void TextChanges::clear()
{
    m_spans.clear();

    this->insert_count = 0;
    this->delete_count = 0;
}

bool TextChanges::empty() const
{
    return m_spans.empty();
}

const TextSpans& TextChanges::get_spans() const
{
    return m_spans;
}

void TextChanges::remove_span_ptr(const TextSpanPtr& span)
{
    remove_spans_if([&](const TextSpanPtr& s){return span == s;});
}

size_t TextChanges::get_change_count()
{
    return this->insert_count + this->delete_count;
}

TextSpans TextChanges::insert(TextPos pos, TextLength length, Noneable<TextLength> include_length)
{
    TextSpans spans_to_update;

    // shift all existing spans after position
    for (auto& span : m_spans)
    {
        if (span->pos > pos)
        {
            span->pos += length;
            spans_to_update.emplace_back(span);
        }
    }

    if (include_length == -1)
    {
        // include all of the insertion
        TextSpanPtr span = find_span_at(pos);
        if (span)
        {
            span->length += length;
        }
        else
        {
            m_spans.emplace_back(std::make_shared<TextSpan>(pos, length));
            span = m_spans.back();
        }
        spans_to_update.emplace_back(span);
    }
    else
    {
        // include the insertion up to include_length only
        TextPos max_include = std::min(length,
                                       include_length.is_none() ? include_length.value : 0);

        TextSpanPtr span = find_span_at(pos);
        if (span)
        {
            // cut existing span
            TextLength old_length = span->length;
            span->length = pos - span->pos + max_include;
            spans_to_update.emplace_back(span);

            // new span for the cut part
            TextLength l = old_length - span->length;
            if (l > 0 ||
                (l == 0 && include_length.is_none()))
            {
                m_spans.emplace_back(std::make_shared<TextSpan>(pos + length, l));
                TextSpanPtr span2 = m_spans.back();
                spans_to_update.emplace_back(span2);
            }
        }

        else
            if (!include_length.is_none())
            {
                m_spans.emplace_back(std::make_shared<TextSpan>(pos, max_include));
                span = m_spans.back();
                spans_to_update.emplace_back(span);
            }
    }

    auto t = TextSpan::Clock::now();
    for (auto s : spans_to_update)
        s->last_modified = t;

    if (!spans_to_update.empty())
        this->insert_count++;

    return spans_to_update;
}

TextSpans TextChanges::delete_(TextPos pos, TextLength length, bool record_empty_spans)
{
    TextPos begin = pos;
    TextPos end   = pos + length;
    TextSpans spans_to_update;

    // cut/remove existing spans
    remove_spans_if([&](const TextSpanPtr& span) -> bool
    {
        bool remove = false;

        if (span->pos <= pos)
        {          // span begins before deletion point?
            TextPos k = std::min(span->end() - begin, length);   // intersecting length
            if (k >= 0)
            {
                span->length -= k;
                spans_to_update.emplace_back(span);
            }
        }
        else
        {                        // span begins after deletion point
            TextPos k = end - span->begin();   // intersecting length
            if (k >= 0)
            {
                span->pos += k;
                span->length -= k;
            }
            span->pos -= length;       // shift by deleted length

            // remove spans fully contained in the deleted range
            if (span->length < 0)
            {
                remove = true;
            }
            else
            {
                spans_to_update.emplace_back(span);
            }
        }
        return remove;
    });

    // Add new empty span
    if (record_empty_spans)
    {
        TextSpanPtr span = find_span_excluding(pos);
        if (!span)
        {
            // Create empty span when deleting too, because this
            // is still a change that can result in a word to learn.
            m_spans.emplace_back(std::make_shared<TextSpan>(pos, 0));
        }

        consolidate_spans(m_spans, m_spans, span);
        if (span)
            spans_to_update.emplace_back(span);
    }

    if (!spans_to_update.empty())
        delete_count += 1;

    return spans_to_update;
}

void TextChanges::consolidate_spans(const TextSpans& spans_in,
                                    TextSpans& spans_out,
                                    TextSpanPtr& tracked_span)
{
    TextSpans spans = spans_in;
    sort_text_spans(spans);

    spans_out.clear();
    TextSpanPtr slast;
    for (auto& s : spans)
    {
        if (slast &&
            slast->end() >= s->begin())
        {
            slast->union_inplace(*s);
            if (tracked_span == s)
                tracked_span = slast;
        }
        else
        {
            spans_out.emplace_back(s);
            slast = s;
        }
    }
}

TextSpanPtr TextChanges::find_span_at(TextPos pos)
{
    for (auto& span : m_spans)
    {
        if (span->pos <= pos && pos <= span->pos + span->length)
            return span;
    }
    return {};
}

TextSpanPtr TextChanges::find_span_excluding(TextPos pos)
{
    for (auto span : m_spans)
    {
        if (span->pos == pos || \
            (span->pos <= pos && pos < span->pos + span->length))
            return span;
    }
    return {};
}

std::vector<Span> TextChanges::get_span_ranges()
{
    return to_span_ranges(m_spans);
}

std::vector<Span> TextChanges::to_span_ranges(const TextSpans& text_spans)
{
    std::vector<Span> spans;
    for (auto& ts : text_spans)
        spans.emplace_back(Span{ts->begin(), ts->length});

    std::stable_sort(spans.begin(), spans.end(),
                     [](const Span& a, const Span& b)
    {
        if (a.begin == b.begin)
            return a.length < b.length;
        return a.begin < b.begin;
    });

    return spans;
}


void sort_text_spans(TextSpans& spans)
{
    std::stable_sort(spans.begin(), spans.end(),
                     [](const TextSpanPtr& a, const TextSpanPtr& b)
    {
        if (a->begin() == b->begin())
            return a->end() < b->end();
        return a->begin() < b->begin();
    });
}

std::ostream&operator<<(std::ostream& s, const TextChanges& tc)
{
    s << "TextChanges(" << tc.m_spans << ")";
    return s;
}


