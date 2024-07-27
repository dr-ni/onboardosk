#ifndef LM_WRAPPER_H
#define LM_WRAPPER_H

#include <string>
#include <vector>
#include <climits>
#include <cassert>

namespace lm {


template <typename T>
std::vector<T> slice(const std::vector<T>& v, int begin, int end=INT_MAX, int step=1)
{
    std::vector<T> result;
    int n = v.size();
    int b = begin >= 0 ? begin : n + begin;
    int e =   end >= 0 ? end   : n + end;

    if (step >= 0)
    {
        b = std::max(b, 0);
        e = std::min(e, n);
        for (int i=b; i<e; i+=step)
            result.emplace_back(v[i]);
    }
    else
    {
        b = std::max(b, n-1);
        e = std::min(b, 0);
        for (int i=b; i>=e; i+=step)
            result.emplace_back(v[i]);
    }
    return result;
}


}

#endif // LM_WRAPPER_H
