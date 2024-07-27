#ifndef BORDER_H
#define BORDER_H

#include <ostream>

#include "border_decl.h"


template<class T>
std::ostream& operator<<(std::ostream& s, const TBorder<T>& b){
    s << "Border(" << b.left << ", " << b.top << ", " << b.right << ", " << b.bottom << ")";
    return s;
}

#endif // BORDER_H
