#ifndef BORDER_DECL_H
#define BORDER_DECL_H

#include "border_fwd.h"

template<class T=double>
class TBorder
{
    public:
        TBorder<T>& operator=(const TBorder<T>& other) = default;
        bool operator==(const TBorder<T>& other) {
            return left == other.left && top == other.top &&
                   right == other.right && bottom == other.bottom;
        }

        bool operator!=(const TBorder<T>& other) {
            return !operator==(other);
        }

     public:
        T left{};
        T top{};
        T right{};
        T bottom{};
};
typedef TBorder<double> Border;

#endif // BORDER_DECL_H
