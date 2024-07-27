#ifndef ONBOARD_OSK_H
#define ONBOARD_OSK_H

struct _cairo;
typedef struct _cairo cairo_t;

#include "onboardoskcallbacks.h"
#include "onboardoskevent.h"
#include "onboardoskview.h"


// C interface

#ifdef __cplusplus
extern "C" {
#endif

struct _OnboardOskContext
{
    void* user_data;
};
typedef struct _OnboardOskContext OnboardOskContext;

void onboard_osk_init_stack_trace();

OnboardOskContext* onboard_osk_context_new();
void onboard_osk_context_destroy(OnboardOskContext* ooc);

struct OnboardOskCallbacks* onboard_osk_context_get_callbacks(OnboardOskContext* ooc);
int onboard_osk_context_startup(OnboardOskContext* ooc, int argc, char** argv);

void onboard_osk_context_toggle_visible(OnboardOskContext* ooc);
void onboard_osk_context_show(OnboardOskContext* ooc);
void onboard_osk_context_hide(OnboardOskContext* ooc);
int onboard_osk_context_is_visible(OnboardOskContext* ooc);

void onboard_osk_context_on_keymap_changed(OnboardOskContext* ooc);
void onboard_osk_context_on_group_changed(OnboardOskContext* ooc);

int onboard_osk_context_get_n_toplevel_views(OnboardOskContext* ooc);
OnboardOskView* onboard_osk_context_get_toplevel_view_at(OnboardOskContext* ooc, int index);

// Full transfer of result. Free with g_free.
const char* onboard_osk_context_get_language_full_name(OnboardOskContext* ooc,
                                                       const char* lang_id);

void onboard_osk_context_set_active_language_id(OnboardOskContext* ooc,
                                                       const char* lang_id, int add_to_mru);
void onboard_osk_context_on_language_selection_closed(OnboardOskContext* ooc);

void onboard_osk_view_draw(OnboardOskView* view, cairo_t* cr);
void onboard_osk_view_on_event(OnboardOskView* view, OnboardOskEvent* event);

void onboard_osk_view_on_toplevel_geometry_changed(OnboardOskView* view);
#ifdef __cplusplus
}
#endif



// C++ interface

#ifdef __cplusplus

#if !(__cplusplus > 201402L)
    #error "C++17 support required for guaranteed copy elision"
#endif

#include <memory>
#include <vector>

#include "layoutdecls.h"

#include "tools/rect_fwd.h"

class DrawingContext;
class ViewBase;

// The main class
class OnboardOsk : public _OnboardOskContext
{
    public:
        std::unique_ptr<OnboardOsk> make();   // for C++
        static OnboardOsk* alloc();           // for C interface, call delete when done

        virtual ~OnboardOsk() = default;

        virtual bool startup(const std::vector<std::string>& args={}) = 0;

        virtual void toggle_visible() = 0;
        virtual void show() = 0;
        virtual void hide() = 0;
        virtual bool is_visible() = 0;

        virtual void on_keymap_changed() = 0;
        virtual void on_group_changed() = 0;

        virtual std::vector<ViewBase*>& get_toplevel_views() = 0;

        virtual void draw(ViewBase* view, DrawingContext& dc) = 0;

        virtual void on_toplevel_view_geometry_changed(ViewBase* view) = 0;
        virtual void on_event(ViewBase* view, OnboardOskEvent* event) = 0;

        virtual std::string get_language_full_name(const std::string& lang_id) = 0;
        virtual void set_active_language_id(const std::string& lang_id, bool add_to_mru=false) = 0;
        virtual void on_language_selection_closed() = 0;

    public:
        // platform- and toolkit-dependent functions
        OnboardOskCallbacks toolkit_callbacks{};
};

#endif

#endif
