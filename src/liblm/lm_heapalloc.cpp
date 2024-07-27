#include "lm_heapalloc.h"

namespace lm {

void* HeapAlloc(size_t size)
{
    return new char[size];
}

void HeapFree(void* p)
{
    delete [] (reinterpret_cast<char*>(p));
}

}  // namespace
