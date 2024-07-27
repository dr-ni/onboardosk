#ifndef KEYDECLS_H
#define KEYDECLS_H

typedef int ModMask;  // modifier mask
typedef unsigned char KeyCode;
typedef unsigned long KeySym;

typedef unsigned int KeymapGroup;

#ifdef __cplusplus

#define INVALID_GROUP (static_cast<KeymapGroup>(-1))
inline bool is_valid(KeymapGroup group) {return group != INVALID_GROUP;}

#endif

#endif // KEYDECLS_H
