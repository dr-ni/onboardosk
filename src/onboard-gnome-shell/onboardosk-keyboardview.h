#ifndef ONBOARDOSK_KEYBOARDVIEW_H
#define ONBOARDOSK_KEYBOARDVIEW_H

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

struct _OnboardOskView;
typedef struct _OnboardOskView OnboardOskView;
struct _OnboardOskKeyboardView;
typedef struct _OnboardOskKeyboardView OnboardOskKeyboardView;
struct _cairo;
typedef struct _cairo cairo_t;

#define ONBOARDOSK_TYPE_KEYBOARDVIEW (onboard_osk_keyboardview_get_type ())
G_DECLARE_DERIVABLE_TYPE (OnboardOskKeyboardView, onboard_osk_keyboardview, ONBOARDOSK, KEYBOARDVIEW, GObject)

struct _OnboardOskKeyboardViewClass
{
    GObjectClass parent_class;  // we are inheriting from this

    gpointer padding[20];
};

OnboardOskKeyboardView *onboard_osk_keyboardview_new (void);

const char* onboard_osk_keyboardview_get_name(OnboardOskKeyboardView* self);
gboolean onboard_osk_keyboardview_is_reactive_type(OnboardOskKeyboardView* self);

void onboard_osk_keyboardview_set_internal_view(OnboardOskKeyboardView* self, OnboardOskView* view);
OnboardOskView* onboard_osk_keyboardview_get_internal_view(OnboardOskKeyboardView* self);

void onboard_osk_keyboardview_set_actor(OnboardOskKeyboardView* self, GObject* actor);
GObject* onboard_osk_keyboardview_get_actor(OnboardOskKeyboardView* self);

void onboard_osk_keyboardview_draw(OnboardOskKeyboardView* self, cairo_t* cr);

G_END_DECLS

#endif  // ONBOARDOSK_KEYBOARDVIEW_H

