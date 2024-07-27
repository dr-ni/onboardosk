#ifndef POINT_DECL_H
#define POINT_DECL_H

#include "point_fwd.h"

template<class T>
class TVec2
{
    public:
        TVec2() = default;
        TVec2(const TVec2<T>& v) = default;
        constexpr TVec2(T x_, T y_) noexcept:
            x(x_), y(y_)
        { }


        void operator=(const TVec2<T>& v)
        {
            x = v.x;
            y = v.y;
        }

        bool operator==(const TVec2<T>& v)
        {
            return x == v.x && y == v.y;
        }

        bool operator!=(const TVec2<T>& v)
        {
            return !operator==(v);
        }

        TVec2<T> floor() const;

        double min()
        {
            return x > y ? x : y;
        }

        // squared euclidean length
        double length2()
        {
            return x * x + y * y;
        }

        // euclidean length
        double length();

        union {
            T x{};
            T w;
        };
        union {
            T y{};
            T h;
        };
};
typedef TVec2<double> Vec2;
typedef TVec2<int> Vec2Int;


template <typename T>
TVec2<T> operator+ (const TVec2<T>& v, const TVec2<T>& v2)
{ return TVec2<T>(v.w + v2.w, v.h + v2.h); }
template <typename T>
TVec2<T> operator+ (const TVec2<T>& v, const T& k)
{ return TVec2<T>(k + v.w, k + v.h); }

template <typename T>
void operator+= (TVec2<T>& v, const T& k)
{ v.x += k; v.y += k; }
template <typename T>
void operator+= (TVec2<T>& v, const TVec2<T>& v2)
{ v.x += v2.x; v.y += v2.y; }

template <typename T>
TVec2<T> operator- (const TVec2<T>& sz, const TVec2<T>& sz2)
{ return TVec2<T>(sz.w - sz2.w, sz.h - sz2.h); }
template <typename T>
TVec2<T> operator- (const TVec2<T>& sz, const T& k)
{ return TVec2<T>(sz.w - k, sz.h - k); }
template <typename T>
TVec2<T> operator- (const T& k, const TVec2<T>& sz)
{ return TVec2<T>(k - sz.w, k - sz.h); }

template <typename T>
TVec2<T> operator- (const TVec2<T>& sz)     // unary "-"
{ return TVec2<T>(-sz.w, -sz.h); }

template <typename T>
TVec2<T> operator* (const TVec2<T>& sz, const TVec2<T>& sz2)
{ return TVec2<T>(sz.w * sz2.w, sz.h * sz2.h); }
template <typename T>
TVec2<T> operator* (const TVec2<T>& sz, const T& k)
{ return TVec2<T>(k * sz.w, k * sz.h); }

template <typename T>
void operator*= (TVec2<T>& v, const T& k)
{ v.x *= k; v.y *= k; }

template <typename T>
TVec2<T> operator/ (const TVec2<T>& sz, const TVec2<T>& sz2)
{ return TVec2<T>(sz.w / sz2.w, sz.h / sz2.h); }
template <typename T>
TVec2<T> operator/ (const TVec2<T>& sz, const T& k)
{ return TVec2<T>(sz.w / k, sz.h / k); }


template<class T>
class TPoint : public TVec2<T> {
    public:
        TPoint() = default;
        constexpr TPoint(T x_, T y_) noexcept:
            TVec2<T>(x_, y_)
        { }
        TPoint(const TVec2<T>& v) :
            TVec2<T>(v)
        {}

        // squared euclidean distance
        double distance2(const TPoint& pt)
        {
            auto p = *this - pt;
            return p.x * p.x + p.y * p.y;
        }
};
typedef TPoint<double> Point;
typedef TPoint<int> PointInt;


template<class T>
class TSize : public TVec2<T> {
    public:
        TSize() = default;
        constexpr TSize(T x_, T y_) noexcept:
            TVec2<T>(x_, y_)
        { }
        TSize(const TVec2<T>& v) :
            TVec2<T>(v)
        {}
};
typedef TSize<double> Size;
typedef TSize<int> SizeInt;


template<class T>
class TOffset : public TVec2<T> {
    public:
        TOffset() = default;
        constexpr TOffset(T x_, T y_) noexcept:
            TVec2<T>(x_, y_)
        { }
        TOffset(const TVec2<T>& v) :
            TVec2<T>(v)
        {}
};
typedef TOffset<double> Offset;
typedef TOffset<int> OffsetInt;


template<class T>
class TScale : public TVec2<T> {
    public:
        TScale() = default;
        constexpr TScale(T x_, T y_) noexcept:
            TVec2<T>(x_, y_)
        { }
        TScale(const TVec2<T>& v) :
            TVec2<T>(v)
        {}
};
typedef TScale<double> Scale;
typedef TScale<int> ScaleInt;


template<class T>
class TLine {
    public:
        TLine() = default;
        constexpr TLine(TPoint<T> begin_, TPoint<T> end_) noexcept:
            begin(begin_), end(end_)
        { }
        TPoint<T> begin;
        TPoint<T> end;
};
typedef TLine<double> Line;

#endif // POINT_DECL_H
