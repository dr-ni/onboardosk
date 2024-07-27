#ifndef RECT_DECL_H
#define RECT_DECL_H

#include "border_fwd.h"
#include "point_fwd.h"

#include <vector>

// Rectangle template class.
// Left and top are included, right and bottom excluded.
template<class T=double>
class TRect {
    public:
        TRect() = default;
        TRect(const TRect<T>& other) = default;
        TRect(T x_, T y_, T w_, T h_) :
            x(x_), y(y_), w(w_), h(h_)
        { }
        TRect(const TPoint<T>& pt, const TSize<T>& sz) :
            x(pt.x), y(pt.y), w(sz.w), h(sz.h)
        { }

        TRect<T>& operator=(const TRect<T>& other) = default;
        bool operator==(const TRect<T>& other) {
            return x == other.x && y == other.y &&
                   w == other.w && h == other.h;
        }

        bool operator!=(const TRect<T>& other) {
            return !operator==(other);
        }

        T& operator[](size_t index);

        // New Rect from two points.
        // x0 and y0 are considered inside, x1 and y1 are just outside the Rect.
        static TRect<T> from_extents(T x0, T y0, T x1, T y1)
        {
            return TRect<T>(x0, y0, x1 - x0, y1 - y0);
        }

        // New Rect from two tuples.
        static TRect<T> from_position_size(const TPoint<T>& position, const TSize<T>& size)
        {
            return TRect<T>(position.x, position.y, size.w, size.h);
        }

        // New Rect from two points, left-top and right-botton.
        // The former lies inside, while the latter is considered to be
        // just outside the rect.
        static TRect<T> from_points(const TPoint<T>& p0, const TPoint<T>& p1)
        {
            return TRect<T>(p0.x, p0.y, p1.x - p0.x, p1.y - p0.y);
        }
        /*
        def to_extents(self):
            return x, y , x + w, y + h

        def to_position_size(self):
            return (x, y), (w, h)
        */

        bool empty() const
        { return w <= 0 or h <= 0; }

        TPoint<T> left_top() const
        { return {x, y}; }

        TPoint<T> right_bottom() const
        { return {right(), bottom()}; }

        TPoint<T> get_position() const
        { return {x, y}; }

        TSize<T> get_size() const
        { return {w, h}; }

        TPoint<T> get_center() const
        { return {x + w / 2.0, y + h / 2.0}; }

        T get_center_x() const
        { return x + w / 2; }

        T get_center_y() const
        { return y + h / 2; }

        T left() const
        { return x; }

        T top() const
        { return y; }

        T right() const
        { return x + w; }

        T bottom() const
        { return y + h; }

        void to_extents(T& l, T& t, T& r, T& b)
        {
            l = left();
            t = top();
            r = right();
            b = bottom();
        }

        bool contains(const TPoint<T>& pt) const {
            return contains(pt.x, pt.y);
        }

        bool contains(T x_, T y_) const {
            return this->x <= x_ && this->x + this->w > x_ &&
                   this->y <= y_ && this->y + this->h > y_;
        }

        TRect<T> round() const;

        TRect<T> floor() const;

        TRect<int> to_int() const {
            return {int(x), int(y), int(w), int(h)};
        }

        TRect<T> scale(T k) const {
            return scale(k, k);
        }

        TRect<T> scale(T kx, T ky) const {
            return {x * kx, y * ky, w * kx, h * ky};
        }

        // Returns a new Rect displaced by dx and dy.
        TRect<T> offset(const TVec2<T>& v) const {
            return offset(v.x, v.y);
        }

        // Returns a new Rect displaced by dx and dy.
        TRect<T> offset(T dx, T dy) const {
            return {x + dx, y + dy, w, h};
        }

        TRect<T> inflate(T k) const {
            return inflate(k, k);
        }

        TRect<T> inflate(const TVec2<T>& v) const {
            return inflate(v.x, v.y);
        }

        TRect<T> inflate(T kx, T ky) const {
            TRect<T> r = {x - kx, y - ky, w + 2 * kx, h + 2 * ky};
            return r;
        }

        // add border to rect on all four sides
        TRect<T> inflate(const TBorder<T>& b) const;

        TRect<T> deflate(T k) const {
            return deflate(k, k);
        }

        TRect<T> deflate(TVec2<T> v) const {
            return deflate(v.x, v.y);
        }

        TRect<T> deflate(T kx, T ky) const {
            TRect<T> r = {x + kx, y + ky, w - 2*kx, h - 2*ky};
            return r;
        }

        // remove border from rect on all four sides
        TRect<T> deflate(const TBorder<T>& b) const {
            return {x + b.left, y + b.top,
                    w - b.left - b.right,
                    h - b.top - b.bottom};
        }

        // Returns a new Rect with its size multiplied by k.
        TRect<T> grow(T k) const
        {
            return grow(k, k);
        }
        TRect<T> grow(T kx, T ky) const
        {
            TRect<T> r = *this;
            grow(r, kx, ky);
            return r;
        }

        // multiply w, h by kx, ky, inplace
        void grow(TRect<T>& r, T kx) const
        {
            grow(r, kx, kx);
        }
        void grow(TRect<T>& r, T kx, T ky) const
        {
            T w_ = r.w * kx;
            T h_ = r.h * ky;
            r.x = r.x + (r.w - w_) / 2.0;
            r.y = r.y + (r.h - h_) / 2.0;
            r.w = w_;
            r.h = h_;
        }

        bool intersects(TRect<T> rect) const
        {
            return !(x >= rect.x + rect.w ||
                     x + w <= rect.x ||
                     y >= rect.y + rect.h ||
                     y + h <= rect.y);
        }

        TRect<T> intersection(const TRect<T>&rect) const;

        TRect<T> union_(const TRect<T>& rect) const;

        // Returns a new Rect with the aspect ratio of self
        // that fits inside the given rectangle.
        TRect<T> inscribe_with_aspect(const TRect<T>& rect,
                                      double x_align=0.5,
                                      double y_align=0.5) const
        {
            if (empty() || rect.empty())
                return {};

            double src_aspect = w / static_cast<double>(h);
            double dst_aspect = rect.w / static_cast<double>(rect.h);

            TRect<T> result = rect;
            if (dst_aspect > src_aspect)
            {
                result.w = static_cast<T>(rect.h * src_aspect);
                result.x += static_cast<T>(x_align * (rect.w - result.w));
            }
            else
            {
                result.h = static_cast<T>(rect.w / src_aspect);
                result.y += static_cast<T>(y_align * (rect.h - result.h));
            }
            return result;
        }

        // Resize self to the aspect ratio of aspect_rect.
        TRect<T> resize_to_aspect(const TRect<T>& aspect_rect) const
        {
            if (empty() or aspect_rect.empty())
                return {};

            double src_aspect = aspect_rect.w / static_cast<double>(aspect_rect.h);
            double dst_aspect = w / static_cast<double>(h);

            TRect<T> result = *this;
            if (dst_aspect > src_aspect)
                result.w = static_cast<T>(h * src_aspect);
            else
                result.h = static_cast<T>(w / src_aspect);
            return result;
        }

        // Resize self to get the aspect ratio of aspect_rect, but limited
        // by the given aspect range.
        TRect<T> resize_to_aspect_range(const TRect<T>& aspect_rect,
                                        T min_aspect, T max_aspect) const;

        // Aligns the given rect inside of self.
        // x/y_align = 0.5 centers rect.
        TRect<T> align_rect(const TRect<T>& rect,
                            double x_align = 0.5,
                            double y_align = 0.5) const
        {
            return {static_cast<T>(this->x + (this->w - rect.w) * x_align),
                    static_cast<T>(this->y + (this->h - rect.h) * y_align),
                    rect.w, rect.h};
        }

        // Aligns the given rect to a point.
        // x/y_align = 0.5 centers rect.
        TRect<T> align_at_point(T x_, T y_,
                                double x_align = 0.5,
                                double y_align = 0.5) const
        {
            return {static_cast<T>(x_ - this->w * x_align),
                    static_cast<T>(y_ - this->h * y_align),
                    this->w, this->h};
        }

        // Divide self into columns x rows sub-rectangles
        std::vector<TRect<T>> subdivide(size_t columns, size_t rows, T spacing={}) const
        {
            return subdivide(columns, rows, spacing, spacing);
        }
        std::vector<TRect<T>> subdivide(size_t columns, size_t rows, TVec2<T> spacing) const
        {
            return subdivide(columns, rows, spacing.x, spacing.y);
        }

        std::vector<TRect<T>> subdivide(size_t columns, size_t rows,
                                        T x_spacing, T y_spacing) const;

        // Layout num_items sub-rectangles of size item_rect in grow_horizontal or
        // columns-first order.
        void flow_layout(std::vector<TRect<T>>& rects_out,
                         TRect<T>& bounds_out,
                         const TRect<T>& item_rect, size_t num_items,
                         T x_spacing, T y_spacing,
                         bool flow_horizontally=true,
                         bool grow_horizontally=true) const;

    public:
        T x{};
        T y{};
        T w{};
        T h{};
};

typedef TRect<double> Rect;
typedef TRect<int>    RectInt;


#endif // RECT_DECL_H

