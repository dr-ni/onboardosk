#ifndef RECT_H
#define RECT_H

// rectdecls.h: only brief forward declarations, no include dependencies
// rectdefs.h:  medium level function definitions with some include dependencies
// rect.h:      full functionality with more include dependencies

#include <assert.h>
#include <cmath>
#include <ostream>
#include <vector>

#include "border.h"
#include "rect_decl.h"

template<class T>
T& TRect<T>::operator[](size_t index) {
    switch (index)
    {
        case 0: return x;
        case 1: return y;
        case 2: return w;
        case 3: return h;
        default: assert(false);  // invalid index
    }
}

template<class T>
TRect<T> TRect<T>::inflate(const TBorder<T>& b) const {
    return {x - b.left, y - b.top,
                w + b.left + b.right,
                h + b.top + b.bottom};
}

template<class T>
TRect<T> TRect<T>::round() const {
    return {std::round(x), std::round(y), std::round(w), std::round(h)};
}

template<class T>
TRect<T> TRect<T>::floor() const {
    return {std::floor(x), std::floor(y), std::floor(w), std::floor(h)};
}

template<class T>
TRect<T> TRect<T>::intersection(const TRect<T>& rect) const
{
    T x0 = std::max(x, rect.x);
    T y0 = std::max(y, rect.y);
    T x1 = std::min(x + w, rect.x + rect.w);
    T y1 = std::min(y + h, rect.y + rect.h);
    if (x0 > x1 || y0 > y1)
        return {};
    else
        return {x0, y0, x1 - x0, y1 - y0};
}

template<class T>
TRect<T> TRect<T>::union_(const TRect<T>& rect) const
{
    T x0 = std::min(x, rect.x);
    T y0 = std::min(y, rect.y);
    T x1 = std::max(x + w, rect.x + rect.w);
    T y1 = std::max(y + h, rect.y + rect.h);
    return {x0, y0, x1 - x0, y1 - y0};
}

template<class T>
TRect<T> TRect<T>::resize_to_aspect_range(const TRect<T>& aspect_rect, T min_aspect, T max_aspect) const
{
    (void)min_aspect;  // not implemented

    if (empty() || aspect_rect.empty())
        return {};

    TRect<T> r = aspect_rect;
    if (r.h)
    {
        double a0 = r.w / static_cast<double>(r.h);
        double a0_max = a0 * max_aspect;
        double a1 = w / static_cast<double>(h);
        double a = std::min(a1, a0_max);

        r = {0, 0, a, 1.0};
    }
    return resize_to_aspect(r);
}

template<class T>
std::vector<TRect<T>> TRect<T>::subdivide(size_t columns, size_t rows, T x_spacing, T y_spacing) const
{
    T ws = static_cast<T>(
               (this->w - (columns - 1) * x_spacing) / static_cast<double>(columns));
    T hs = static_cast<T>(
               (this->h - (rows - 1)    * y_spacing) / static_cast<double>(rows));

    std::vector<TRect<T>> rects;
    T _y = this->y;
    for (size_t row=0; row < rows; row++)
    {
        T _x = this->x;
        for (size_t column=0; column < columns; column++)
        {
            rects.emplace_back(_x, _y, ws, hs);
            _x += ws + x_spacing;
        }
        _y += hs + y_spacing;
    }

    return rects;
}

template<class T>
void TRect<T>::flow_layout(std::vector<TRect<T> >& rects_out, TRect<T>& bounds_out, const TRect<T>& item_rect, size_t num_items, T x_spacing, T y_spacing, bool flow_horizontally, bool grow_horizontally) const
{
    int nrows;
    int ncols;
    rects_out.clear();
    bounds_out = *this;

    if (grow_horizontally)
    {
        // "This" determines height and minimum width.
        // Items may overflow the minimum width.
        nrows = int((h + y_spacing) / (item_rect.h + y_spacing));
        ncols = int(std::ceil(num_items / static_cast<T>(nrows)));
        bounds_out.w = 0;
    }
    else
    {
        // "This" determines width and minimum height.
        // Items may overflow the minimum height.
        ncols = int((w + x_spacing) / (item_rect.w + x_spacing));
        nrows = int(ceil(num_items / static_cast<T>(ncols)));
        bounds_out.h = 0;
    }

    if (flow_horizontally)
    {
        for (int row=0; row < nrows; row++)
        {
            for (int col=0; col < ncols; col++)
            {
                T _x = x + item_rect.w * col +
                       x_spacing * std::max((col - 1), 0);
                T _y = y + item_rect.h * row +
                       y_spacing * std::max((row - 1), 0);
                TRect<T> r(_x, _y, item_rect.w, item_rect.h);
                bounds_out = bounds_out.union_(r);
                rects_out.push_back(r);

                if (rects_out.size() >= num_items)
                    break;
            }

            if (rects_out.size() >= num_items)
                break;
        }
    }
    else
    {
        for (int col=0; col < ncols; col++)
        {
            for (int row=0; row < nrows; row++)
            {
                T _x = x + item_rect.w * col +
                       x_spacing * std::max((col - 1), 0);
                T _y = y + item_rect.h * row +
                       y_spacing * std::max((row - 1), 0);
                TRect<T> r(_x, _y, item_rect.w, item_rect.h);
                bounds_out = bounds_out.union_(r);
                rects_out.push_back(r);

                if (rects_out.size() >= num_items)
                    break;
            }

            if (rects_out.size() >= num_items)
                break;
        }
    }
}

template<class T>
std::ostream& operator<<(std::ostream& s, const TRect<T>& r){
    s << "Rect(" << r.x << ", " << r.y << ", " << r.w << ", " << r.h << ")";
    return s;
}

#endif // RECT_H
