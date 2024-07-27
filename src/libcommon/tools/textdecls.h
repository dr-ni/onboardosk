#ifndef TEXTDECLS_H
#define TEXTDECLS_H

#include <memory>
#include <ostream>

typedef int TextPos;
typedef int TextLength;
typedef int32_t CodePoint;

struct _Span
{
    _Span()
    {}
    _Span(TextPos begin_, TextLength length_) :
        begin(begin_),
        length(length_)
    {}

    bool operator==(const _Span& other) const
    { return begin == other.begin && length == other.length;}

    TextPos end() const {return begin + length;}

    bool intersects(TextPos begin_, TextLength length_) const
    {
        TextPos end_ = begin_ + length_;
        return begin_ < this->end() && end_ > this->begin;
    }

    TextPos begin{0};
    TextLength length{0};
};
typedef _Span Span;

inline std::ostream& operator<<(std::ostream& s, const Span& span){
    s << "Span(" << span.begin << ", " << span.length << ")";
    return s;
}

template<class T>
struct TSpan : public Span
{
    using Super = Span;

    TSpan()
    {}

    TSpan(TextPos begin_, TextLength length_, const T& text_) :
        Span(begin_, length_),
        text(text_)
    {}

    bool operator==(const TSpan& other) const
    { return begin == other.begin &&
             length == other.length &&
             text == other.text;
    }

    T text;
};


#endif // TEXTDECLS_H
