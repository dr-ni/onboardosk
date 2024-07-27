#include "translation.h"

void init_translation_for_library()
{
    static bool translation_init_done{false};
    if (!translation_init_done)
    {
        bindtextdomain (PACKAGE, ONBOARDOSK_LOCALEDIR);
        translation_init_done = true;
    }
}
