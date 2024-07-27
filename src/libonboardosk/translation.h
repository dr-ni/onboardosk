#ifndef TRANSLATION_H
#define TRANSLATION_H

#include <string>
// wrapper around <libintl.h> that implements the
// configure --disable-nls option.
#include "gettext.h"

#include "project_config.h"

//#define _(msg) dgettext (PACKAGE, msg)

inline const char* _(const char* s)
{
    return dgettext(PACKAGE, s);
}

inline const char* _(const std::string& s)
{
    return dgettext(PACKAGE, s.c_str());
}

void init_translation_for_library();

#endif // TRANSLATION_H
