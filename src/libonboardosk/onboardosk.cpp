#include <iostream>
#include <memory>

#include <glib.h>  // g_strdup

#include "tools/logger.h"
#include "tools/stacktrace.h"

#include "drawingcontext.h"
#include "layoutview.h"
#include "onboardosk.h"
#include "onboardoskimpl.h"


OnboardOsk* OnboardOsk::alloc()
{
    return new OnboardOskImpl();
}

std::unique_ptr<OnboardOsk> OnboardOsk::make()
{
    return std::make_unique<OnboardOskImpl>();
}

#define SAFE_CAST(ooc, oo) (safe_cast(ooc, oo, LOG_SRC_LOCATION))

bool safe_cast(OnboardOskContext* ooc, OnboardOsk*& oo, const char* const src_location)
{
    if (!ooc)
    {
        std::cerr << extract_src_location(src_location)
                  << " CRITICAL"
                  << " invalid OnboardOskContext ooc="
                  << ooc << std::endl;
        return false;
    }
    oo = static_cast<OnboardOsk*>(ooc);
    return true;
}

// -----------------------------------------------------------------------
// C interface
// -----------------------------------------------------------------------
void onboard_osk_init_stack_trace()
{
    init_stack_trace();
}

OnboardOskContext* onboard_osk_context_new()
{
    return OnboardOsk::alloc();
}

void onboard_osk_context_destroy(OnboardOskContext* ooc)
{
    if (ooc)
    {
        delete static_cast<OnboardOsk*>(ooc);
    }
}

struct OnboardOskCallbacks* onboard_osk_context_get_callbacks(OnboardOskContext* ooc)
{
    OnboardOsk* oo;
    if (SAFE_CAST(ooc, oo))
        return &oo->toolkit_callbacks;
    return NULL;
}

int onboard_osk_context_startup(OnboardOskContext* ooc, int argc, char** argv)
{
    OnboardOsk* oo;
    if (!SAFE_CAST(ooc, oo))
        return false;
    return oo->startup(std::vector<std::string>(argv, argv + argc));
}

void onboard_osk_context_toggle_visible(OnboardOskContext* ooc)
{
    OnboardOsk* oo;
    if (SAFE_CAST(ooc, oo))
        oo->toggle_visible();
}

void onboard_osk_context_show(OnboardOskContext* ooc)
{
    OnboardOsk* oo;
    if (SAFE_CAST(ooc, oo))
        oo->toggle_visible();
}

void onboard_osk_context_hide(OnboardOskContext* ooc)
{
    OnboardOsk* oo;
    if (SAFE_CAST(ooc, oo))
        oo->toggle_visible();
}

int onboard_osk_context_is_visible(OnboardOskContext* ooc)
{
    OnboardOsk* oo;
    if (!SAFE_CAST(ooc, oo))
        return false;
    return oo->is_visible();
}

void onboard_osk_context_on_keymap_changed(OnboardOskContext* ooc)
{
    OnboardOsk* oo;
    if (!SAFE_CAST(ooc, oo))
        return;
    oo->on_keymap_changed();
}

void onboard_osk_context_on_group_changed(OnboardOskContext* ooc)
{
    OnboardOsk* oo;
    if (!SAFE_CAST(ooc, oo))
        return;
    oo->on_group_changed();
}

int onboard_osk_context_get_n_toplevel_views(OnboardOskContext* ooc)
{
    OnboardOsk* oo;
    if (!SAFE_CAST(ooc, oo))
        return 0;
    auto views = oo->get_toplevel_views();
    return static_cast<int>(views.size());
}

OnboardOskView* onboard_osk_context_get_toplevel_view_at(OnboardOskContext* ooc, int index)
{
    OnboardOsk* oo;
    if (!SAFE_CAST(ooc, oo))
        return NULL;
    auto views = oo->get_toplevel_views();
    return views.at(static_cast<size_t>(index));
}

const char* onboard_osk_context_get_language_full_name(OnboardOskContext* ooc, const char* lang_id)
{
    OnboardOsk* oo;
    if (!SAFE_CAST(ooc, oo))
        return NULL;
    auto name = oo->get_language_full_name(lang_id);
    return g_strdup(name.c_str());
}

void onboard_osk_context_set_active_language_id(OnboardOskContext* ooc, const char* lang_id, int add_to_mru)
{
    OnboardOsk* oo;
    if (!SAFE_CAST(ooc, oo))
        return;
    oo->set_active_language_id(lang_id, add_to_mru);
}

void onboard_osk_context_on_language_selection_closed(OnboardOskContext* ooc)
{
    OnboardOsk* oo;
    if (!SAFE_CAST(ooc, oo))
        return;
    oo->on_language_selection_closed();
}



void onboard_osk_view_draw(OnboardOskView* view, cairo_t* cr)
{
    OnboardOsk* oo = static_cast<OnboardOsk*>(view->ooc);
    ViewBase* v = static_cast<ViewBase*>(view);
    DrawingContext dc(cr);
    oo->draw(v, dc);
}

void onboard_osk_view_on_event(OnboardOskView* view, OnboardOskEvent* event)
{
    if (!view)
        return;
    OnboardOsk* oo = static_cast<OnboardOsk*>(view->ooc);
    ViewBase* v = static_cast<ViewBase*>(view);
    oo->on_event(v, event);
}

void onboard_osk_view_on_toplevel_geometry_changed(OnboardOskView* view)
{
    if (!view)
        return;
    OnboardOsk* oo = static_cast<OnboardOsk*>(view->ooc);
    ViewBase* v = static_cast<ViewBase*>(view);
    oo->on_toplevel_view_geometry_changed(v);
}


