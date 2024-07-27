#ifndef LM_HEAPALLOC_H
#define LM_HEAPALLOC_H

#include <stddef.h>

namespace lm {

// define these somewhere to for example call malloc() and free()
extern void* HeapAlloc(size_t size);
extern void HeapFree(void* p);

} // namespace

#endif // LM_HEAPALLOC_H
