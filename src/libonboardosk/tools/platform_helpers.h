#ifndef PLATFORM_HELPERS_H
#define PLATFORM_HELPERS_H

#define GCC_COMPILER (defined(__GNUC__) && !defined(__clang__))

#define UNUSED(x) (void)(x)
template<class T> void unused( const T& ) {}

#endif // PLATFORM_HELPERS_H
