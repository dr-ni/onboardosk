#ifndef TEXTCHANGES_H
#define TEXTCHANGES_H

#include <chrono>
#include <algorithm>
#include <ostream>
#include <vector>

#include "tools/noneable.h"
#include "tools/textdecls.h"
#include "tools/ustringmain.h"


// Span of text;
class TextSpan : public std::enable_shared_from_this<TextSpan>
{
    public:
        using Ptr = std::shared_ptr<TextSpan>;
        using Clock = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        TextSpan(TextPos pos_ = 0, TextLength length_ = 0,
                 const UString& text_ = {},
                 TextPos text_pos_ = 0) :
            pos(pos_),
            length(length_),
            text_pos(text_pos_),
            text(text_)
        {}

        ~TextSpan();

        //TextSpan(const TextSpan& other) = default;
        //TextSpan(TextSpan&& other) noexcept = default;
        //TextSpan& operator=(const TextSpan& other) = default;
        //TextSpan& operator=(TextSpan&& other) noexcept = default;

        bool operator==(const TextSpan& other) const
        { return this->pos == other.pos &&
                 this->length == other.length &&
                 this->text_pos == other.text_pos &&
                 this->text == other.text;
        }
        bool operator!=(const TextSpan& other) const
        {return !operator==(other);}

        Ptr getptr() {
            return std::enable_shared_from_this<TextSpan>::shared_from_this();
        }

        Ptr clone() const {
            Ptr s = std::make_shared<TextSpan>();
            *s = *this;
            return s;
        }

        TextPos begin() const
        {
            return this->pos;
        }

        TextPos end() const
        {
            return this->pos + this->length;
        }

        Span span() const
        {
            return {this->pos,  this->length};
        }

        TextPos text_begin() const
        {
            return this->text_pos;
        }

        bool empty() const
        {
            return this->length == 0;
        }

        bool contains(TextPos pos_) const
        {
            return pos_ >= begin() && pos_ < end();
        }

        bool intersects(const TextSpan& span) const
        {
            return !intersection(span).empty();
        }

        TextSpan intersection(const TextSpan& span) const
        {
           TextPos p0 = std::max(this->pos, span.pos);
           TextPos p1 = std::min(this->pos + this->length,  span.pos + span.length);
           if (p0 > p1)
               return TextSpan();
           else
               return TextSpan(p0, p1 - p0);
        }

        // Join two spans, result in this;
        TextSpan& union_inplace(const TextSpan& span);

        // Return the whole available text
        const UString& get_text() const
        {
            return this->text;
        }

        UString get_text(const Noneable<TextPos>& begin,
                             const Noneable<TextPos>& end={}) const;

        // Return just the span's part of the text.
        UString get_span_text() const;

        // Return the beginning of the whole available text,
        // ending with && including the span.;
        UString get_text_until_span();

        // Return the end of the whole available text,
        // starting from && including the span.;
        UString get_text_from_span();

        // Return the remaining available text after the span.;
        UString get_text_after_span();

        // Character right before the span.;
        UString get_char_before_span() const;

        // Character right before the span.;
        UString get_last_char_in_span() const;

        UString escape(const UString& text_) const;

    public:
        TextPos pos{};           // document caret position
        TextLength length{};     // span length
        TextPos text_pos{};      // document position of text begin
        UString text;            // text that includes span, but may be larger
        TimePoint last_modified{};

        friend std::ostream& operator<<(std::ostream& s, const TextSpan& span);
};
typedef TextSpan::Ptr TextSpanPtr;
typedef std::vector<TextSpanPtr> TextSpans;

inline bool is_equal(const TextSpanPtr& a, const TextSpanPtr& b)
{
    return a == b || (a && b && *a == *b);
}

std::ostream& operator<<(std::ostream& s, const TextSpan* span);
std::ostream& operator<<(std::ostream& s, const TextSpan& span);



// Collection of text spans yet to be learned.
class TextChanges
{
    public:
        TextChanges();
        TextChanges(const TextSpans& spans);
        TextChanges(const TextSpans&& spans);

        void clear();
        bool empty() const;

        const TextSpans& get_spans() const;

        template<typename F>
        void remove_spans_if(const F& predicate)
        {
            m_spans.erase(std::remove_if(
                          m_spans.begin(), m_spans.end(), predicate),
                          m_spans.end());
        }

        // Remove span by pointer comparison.
        void remove_span_ptr(const TextSpanPtr& span);

        size_t get_change_count();

        // Record insertion up to <include_length> characters,
        // counted from the start of the insertion. The remaining
        // inserted characters are excluded from spans. This may split
        // an existing span.
        //
        // A small but non-zero <include_length> allows to skip over
        // possible whitespace at the start of the insertion and
        // will often result in including the very first word(s) for learning.
        //
        // include_length =   -1: include length
        // include_length =   +n: include n
        // include_length = None: include nothing, don't record
        //                        zero length span either
        TextSpans insert(Span sp,
                         Noneable<TextLength> include_length=-1)
        {return insert(sp.begin, sp.length, include_length);}

        TextSpans insert(TextPos pos, TextLength length,
                         Noneable<TextLength> include_length=-1);

        // Record deletion.
        // record_empty_spans == true:  record extra zero length spans
        //                              at deletion point
        // record_empty_spans == false: no extra new spans, but keep existing
        //                              ones that become zero length (terminal
        //                              scrolling)
        TextSpans delete_(const Span& sp,
                         bool record_empty_spans = true)
        {return delete_(sp.begin, sp.length, record_empty_spans);}

        TextSpans delete_(TextPos pos, TextLength length,
                          bool record_empty_spans = true);

        // join touching or intersecting text spans
        static void consolidate_spans(const TextSpans& spans_in,
                                      TextSpans& spans_out,
                                      TextSpanPtr& tracked_span);

        TextSpanPtr find_span_at(TextPos pos);
        TextSpanPtr find_span_excluding(TextPos pos);

        std::vector<Span> get_span_ranges();
        static std::vector<Span> to_span_ranges(const TextSpans& text_spans);

    public:
        // some counts for book-keeping, not used by this class itself
        size_t insert_count{};
        size_t delete_count{};

    private:
        TextSpans m_spans;


        friend std::ostream& operator<<(std::ostream& s, const TextChanges& tc);
};


void sort_text_spans(TextSpans& spans);

std::ostream& operator<<(std::ostream& s, const TextChanges& tc);

#endif // TEXTCHANGES_H
