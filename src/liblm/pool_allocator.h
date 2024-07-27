#ifndef POOL_ALLOCATOR_H
#define POOL_ALLOCATOR_H

#include <stddef.h>

namespace lm {

void* MemAlloc(size_t size);
void MemFree(void* p);

} // namespace

#endif // POOL_ALLOCATOR_H
