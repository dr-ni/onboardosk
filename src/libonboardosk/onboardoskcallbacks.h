#ifndef ONBOARDOSKCALLBACKS_H
#define ONBOARDOSKCALLBACKS_H

#include "tools/keydecls.h"

#include "onboardoskevent.h"
#include "onboardoskview.h"

typedef struct _OnboardOskRect OnboardOskRect;

struct _PangoContext;
typedef struct _PangoContext PangoContext;

struct _OnboardOskContext;
typedef struct _OnboardOskContext OnboardOskContext;

typedef int (*TimerCallback) (void* user_data);
typedef int (*StartTimerFunc) (int milliseconds, TimerCallback callback, void* user_data);
typedef void (*StopTimerFunc) (int timer_id);

typedef int (*IdleRunCallback) (void* user_data);  // returns bool, FALSE for on-shot
typedef int (*IdleRunFunc) (IdleRunCallback callback, void* user_data);

typedef int (*IsCompositedFunc) ();

typedef void (*GetCurrentPointerStateFunc) (double* x, double* y, OnboardOskStateMask* mask);
typedef void (*MoveToCurrentDesktopFunc) ();

typedef void (*GetScreenGeometryFunc) (OnboardOskContext* ooc, double* x, double* y, double* w, double* h);

typedef int (*GetNMonitorsFunc) (OnboardOskContext* ooc);
typedef void (*GetMonitorGeometryFunc) (OnboardOskContext* ooc, int monitor_index, double* x, double* y, double* w, double* h);
typedef void (*GetMonitorSizeMMFunc) (OnboardOskContext* ooc, int monitor_index, double* w, double* h);
typedef void (*GetMonitorWorkAreaFunc) (OnboardOskContext* ooc, int monitor_index, double* x, double* y, double* w, double* h);
typedef int (*GetMonitorAtActiveWindowFunc) (OnboardOskContext* ooc);
typedef int (*GetMonitorAtViewFunc) (OnboardOskContext* ooc, const OnboardOskView* view);
typedef int (*GetPrimaryMonitorFunc) (OnboardOskContext* ooc);

typedef unsigned int (*GetDoubleClickTimeFunc) ();
typedef double (*GetDragThresholdFunc) ();

typedef KeymapGroup (*GetCurrentGroupFunc) (OnboardOskContext* ooc);
typedef void (*GetRulesNamesFunc) (OnboardOskContext* ooc, char*** names, int* names_length);
typedef void (*SendKeyValEventFunc) (OnboardOskContext* ooc, int keyval, int press);
typedef void (*SendKeyCodeEventFunc) (OnboardOskContext* ooc, int keycode, int press);

typedef void (*ShowLanguageSelectionFunc) (
        OnboardOskContext* ooc, OnboardOskView* toplevel,
        const double* rect,  int rect_len,
        const char* active_lang_id,
        const char* system_lang_id,
        const char** mru_lang_ids, int mru_lang_ids_len,
        const char** other_lang_ids, int other_lang_ids_len);

typedef int (*ShowDialogWPErrorRecoveryFunc) (OnboardOskContext* ooc,
                                              const char* markup,
                                              const char* message_type,
                                              const char* buttons);

// former WMQuirks
typedef int (*IsAltSpecialFunc) ();  // used to depend on is_override_redirect()
typedef int (*MustFixConfigureEvent) ();

// signals
typedef void (*OnToplevelViewAddedFunc) (OnboardOskContext* ooc, OnboardOskView* view);
typedef void (*OnToplevelViewRemoveFunc) (OnboardOskContext* ooc, OnboardOskView* view);

// View functions
typedef void (*ViewGetGeometryFunc) (OnboardOskView* view, OnboardOskViewGeometry* geometry);
typedef void (*ViewMoveFunc) (OnboardOskView* view, double x, double y);   // root coordinates
typedef void (*ViewMoveResizeFunc) (OnboardOskView* view, double x, double y, double w, double h);   // root coordinates
typedef void (*ViewSetVisibleFunc) (OnboardOskView* view, int visible);  // visible==0: hide, visible!=0: show window/actor

typedef void (*ViewGetScreenRectFunc) (OnboardOskView* view, double* x, double* y, double* w, double* h);

typedef void (*ViewQueueDrawFunc) (OnboardOskView* view);
typedef void (*ViewQueueDrawAreaFunc) (OnboardOskView* view, double x, double y, double w, double h);
typedef void (*ViewProcessUpdatesFunc) (OnboardOskView* view);

typedef PangoContext* (*ViewGetPangoContextFunc) (OnboardOskView* view);
typedef void (*ViewSetCursorTypeFunc) (OnboardOskView* view, OnboardOskCursorType cursor_type);

typedef void (*ViewGrabPointerFunc) (OnboardOskView* view);
typedef void (*ViewUngrabPointerFunc) (OnboardOskView* view);


struct OnboardOskCallbacks
{
    StartTimerFunc start_timer;
    StopTimerFunc stop_timer;
    IdleRunFunc idle_run;

    IsCompositedFunc is_composited;

    GetCurrentPointerStateFunc get_current_pointer_state;
    MoveToCurrentDesktopFunc move_to_current_desktop;

    GetScreenGeometryFunc get_screen_geometry;

    GetMonitorGeometryFunc get_monitor_geometry;
    GetMonitorSizeMMFunc get_monitor_size_mm;
    GetMonitorWorkAreaFunc get_monitor_workarea;
    GetNMonitorsFunc get_n_monitors;
    GetMonitorAtActiveWindowFunc get_monitor_at_active_window;   // at active desktop window
    GetMonitorAtViewFunc get_monitor_at_view;      // at one of our views
    GetPrimaryMonitorFunc get_primary_monitor;

    GetDoubleClickTimeFunc get_double_click_time;
    GetDragThresholdFunc get_drag_threshold;

    GetCurrentGroupFunc get_current_group;
    GetRulesNamesFunc get_current_rules_names;
    SendKeyValEventFunc send_keyval_event;
    SendKeyCodeEventFunc send_keycode_event;

    ShowLanguageSelectionFunc show_language_selection;
    ShowDialogWPErrorRecoveryFunc show_dialog_wp_error_recovery;

    // former Quirks
    IsAltSpecialFunc is_alt_special;
    MustFixConfigureEvent must_fix_configure_event;

    // signals
    OnToplevelViewAddedFunc on_top_level_view_added;
    OnToplevelViewRemoveFunc on_top_level_view_remove;

    // View
    ViewGetGeometryFunc view_get_geometry;
    ViewMoveFunc view_move;
    ViewMoveResizeFunc view_move_resize;
    ViewSetVisibleFunc view_set_visible;

    ViewGetScreenRectFunc view_get_screen_rect;

    ViewQueueDrawFunc view_queue_draw;
    ViewQueueDrawAreaFunc view_queue_draw_area;
    ViewProcessUpdatesFunc view_process_updates;

    ViewGetPangoContextFunc view_get_pango_context;
    ViewSetCursorTypeFunc view_set_cursor_type;

    ViewGrabPointerFunc view_grab_pointer;
    ViewUngrabPointerFunc view_ungrab_pointer;
};


#endif // ONBOARDOSKCALLBACKS_H
