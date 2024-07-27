#ifndef VEC2_H
#define VEC2_H

#include <ostream>
#include <cmath>

#include "point_decl.h"


template<class T>
TVec2<T> TVec2<T>::floor() const
{
    TVec2<T> v;
    v.x = std::floor(x);
    v.y = std::floor(y);
    return v;
}

template<class T>
double TVec2<T>::length()
{
    return std::sqrt(x * x + y * y);
}


template<class T>
std::ostream& operator<<(std::ostream& s, const TPoint<T>& pt) {
    s << "Point(" << pt.x << ", " << pt.y << ")";
    return s;
}

template<class T>
std::ostream& operator<<(std::ostream& s, const TSize<T>& sz) {
    s << "Size(" << sz.w << ", " << sz.h << ")";
    return s;
}

template<class T>
std::ostream& operator<<(std::ostream& s, const TOffset<T>& pt) {
    s << "Offset(" << pt.x << ", " << pt.y << ")";
    return s;
}

template<class T>
std::ostream& operator<<(std::ostream& s, const TScale<T>& pt) {
    s << "Scale(" << pt.x << ", " << pt.y << ")";
    return s;
}

#endif // VEC2_H


