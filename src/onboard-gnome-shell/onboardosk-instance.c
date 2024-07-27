#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <clutter/clutter.h>

#include "onboardosk-instance.h"
#include "onboardosk-keyboardview.h"
#include "onboardosk.h"


typedef struct
{
  OnboardOskContext *ooc;
} OnboardOskInstancePrivate;


G_DEFINE_TYPE_WITH_PRIVATE (OnboardOskInstance, onboard_osk_instance, G_TYPE_OBJECT)

#define ONBOARD_OSK_INSTANCE_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ONBOARD_OSK_TYPE_INSTANCE, OnboardOskInstancePrivate))

// enum for class signals
enum
{
  SIG_TOPLEVEL_VIEWS_CHANGED,
  N_SIGNALS
};

// array of class signal ids
static guint g_signals[N_SIGNALS];


static void
onboard_osk_instance_dispose (GObject *gobject);
static void
onboard_osk_instance_finalize (GObject *gobject);

static void
init_onboard_callbacks(OnboardOskContext* ooc);

extern void
_init_view_callbacks(struct _OnboardOskContext* ooc);

static void
on_toplevel_actor_destroy (ClutterActor* actor, gpointer user_data);
static void
free_toplevel_view (OnboardOskInstance* self, OnboardOskKeyboardView* view);


// class initialization
static void
onboard_osk_instance_class_init (OnboardOskInstanceClass *klass)
{
    GObjectClass *super_class = G_OBJECT_CLASS (klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    // When receiving SIGSEGV, etc., print back trace.
    onboard_osk_init_stack_trace();

    super_class->dispose = onboard_osk_instance_dispose;
    super_class->finalize = onboard_osk_instance_finalize;

    g_signals[SIG_TOPLEVEL_VIEWS_CHANGED] =
      g_signal_new ("toplevel-views-changed",
                   G_TYPE_FROM_CLASS (gobject_class),
                   (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS),
                   0 /* closure */,
                   NULL /* accumulator */,
                   NULL /* accumulator data */,
                   NULL /* C marshaller */,
                   G_TYPE_NONE /* return_type */,
                   0     /* n_params */,
                   G_TYPE_NONE  /* param_types */);
}

// instance initialization
OnboardOskInstance *
onboard_osk_instance_new (void)
{
  OnboardOskInstance *instance;
  instance = ONBOARDOSK_INSTANCE(g_object_new(ONBOARD_OSK_TYPE_INSTANCE, NULL));
  return instance;
}

static void
onboard_osk_instance_init (OnboardOskInstance *self)
{
}

static void
onboard_osk_instance_dispose (GObject *gobject)
{
    printf("onboard_osk_instance_dispose\n");

    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (gobject));

    G_OBJECT_CLASS (onboard_osk_instance_parent_class)->dispose (gobject);
}

static void
onboard_osk_instance_finalize (GObject *gobject)
{
    printf("onboard_osk_instance_finalize\n");

    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (gobject));
    OnboardOskInstance* self = ONBOARDOSK_INSTANCE(gobject);
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    if (priv->ooc)
    {
        onboard_osk_context_destroy(priv->ooc);
        priv->ooc = NULL;
    }

    G_OBJECT_CLASS (onboard_osk_instance_parent_class)->finalize (gobject);
}

gint onboard_osk_instance_start(OnboardOskInstance* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (self), 0);
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);

    priv->ooc = onboard_osk_context_new();
    if (priv->ooc)
    {
        priv->ooc->user_data = self;
        init_onboard_callbacks(priv->ooc);

        onboard_osk_context_startup(priv->ooc, 0, NULL);

        return TRUE;
    }
    return FALSE;
}

/*
 * When assigning null to the onboardosk instance variable in JavaScript,
 * finalize isn't being called. The reference count at this point is 1,
 * that should be fine, but maybe garbage collection delays freeing
 * the object.
 * -> Explicitely destroy the keyboard here. The actors should be
 * taken care off automatically when their views are removed.
 */
void onboard_osk_instance_destroy(OnboardOskInstance* self)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));

    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    if (priv->ooc)
    {
        onboard_osk_context_destroy(priv->ooc);
        priv->ooc = NULL;
    }
}

/**
 * onboard_osk_instance_toggle_visible:
 *
 * Toggle visibility of the keyboard, including all
 * toplevel views.
 * When showing, the behaviour is the same as
 * onboard_osk_instance_show().
 * When hiding, the behaviour is the same as
 * onboard_osk_instance_hide().
 */
void onboard_osk_instance_toggle_visible(OnboardOskInstance* self)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    onboard_osk_context_toggle_visible(priv->ooc);
}

/**
 * onboard_osk_instance_show:
 *
 * Show all toplevel views.
 * The show request is delayed until after all the OSK's
 * keys have been released. Afterwards the keyboard will
 * be locked visible, i.e. auto-show will not be able to
 * hide it.
 * Call onboard_osk_instance_hide to resume auto-show.
 */
void onboard_osk_instance_show(OnboardOskInstance* self)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    onboard_osk_context_show(priv->ooc);
}

/**
 * onboard_osk_instance_hide:
 *
 * Hide all toplevel views unconditionally.
 * The hide request is delayed until after all
 * of the OSK's keys have been released.
 * If the keyboard was previously locked visible,
 * it is unlocked and auto-show resumes.
 */
void onboard_osk_instance_hide(OnboardOskInstance* self)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    onboard_osk_context_hide(priv->ooc);
}

/**
 * onboard_osk_instance_get_visible:
 *
 * Returns the current state of visibility.
 * Auto-show, being locked visible, etc. don't influence
 * the result.
 *
 * Returns: TRUE if the keyboard is currently visible, else FALSE
 */
gboolean onboard_osk_instance_is_visible(OnboardOskInstance* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (self), FALSE);
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    return onboard_osk_context_is_visible(priv->ooc);
}

/**
 * onboard_osk_instance_get_n_toplevel_views:
 *
 * Get number of toplevel views.
 * Create this many toplevel windows/actors
 * in the extension.
 *
 * Returns: number of toplevel views
 */
guint onboard_osk_instance_get_n_toplevel_views(OnboardOskInstance* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (self), 0);
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    return 0;
}

/**
 * onboard_osk_instance_get_toplevel_view_at:
 *
 * Get toplevel actor at @index.
 *
 * Returns: (transfer none): an #OnboardOskKeyboardView
 */
OnboardOskKeyboardView* onboard_osk_instance_get_toplevel_view_at(OnboardOskInstance* self, gint index)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (self), NULL);
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    int n = 0;
    assert(index >= 0 && index < n);
    return NULL;
}

/**
 * on_top_level_view_added:
 *
 * A toplevel view has been added, e.g. after a layout was loaded
 * or when creating a popup.
 * Create an actor for it here.
 */
static void on_top_level_view_added(OnboardOskContext* ooc, OnboardOskView* toplevel)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data));
    OnboardOskInstance *self = ONBOARDOSK_INSTANCE(ooc->user_data);
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(self);

    OnboardOskKeyboardView* view = onboard_osk_keyboardview_new();
    onboard_osk_keyboardview_set_internal_view(view, toplevel);
    toplevel->user_data = view;

    g_return_if_fail (klass->on_toplevel_view_added != NULL);
    klass->on_toplevel_view_added(self, view);   // call virtual function

    printf("on_top_level_view_added %p %p %s\n", toplevel, view, toplevel->name);
    ClutterActor* actor = (ClutterActor*)
        onboard_osk_keyboardview_get_actor(view);
    g_return_if_fail (CLUTTER_IS_ACTOR (actor));

    g_signal_connect (actor, "destroy", G_CALLBACK (on_toplevel_actor_destroy), self);
}

/**
 * on_top_level_view_remove:
 *
 * A toplevel view has been removed, e.g. when a view is about to be deleted
 * before loading a new layout or when a popup is destroyed.
 * Destroy actors linked to the view here.
 */
static void on_top_level_view_remove(OnboardOskContext* ooc, OnboardOskView* toplevel)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data));
    OnboardOskInstance *self = ONBOARDOSK_INSTANCE(ooc->user_data);
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(self);

    OnboardOskKeyboardView* view = (OnboardOskKeyboardView*)toplevel->user_data;
    printf("on_top_level_view_remove %p %p %s\n", toplevel, view, toplevel->name);
    if (view)   // might have been removed by on_toplevel_actor_destroy already
    {
        g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW (view));

        ClutterActor* actor = (ClutterActor*)
            onboard_osk_keyboardview_get_actor(view);
        g_return_if_fail (CLUTTER_IS_ACTOR (actor));

        // Make sure on_toplevel_actor_destroy() is not called anymore
        g_signal_handlers_disconnect_by_func(
                    actor, G_CALLBACK (on_toplevel_actor_destroy), self);

        // Callee created the actor and has to destroy it as well.
        // We release the view in on_toplevel_actor_destroy().
        g_return_if_fail (klass->on_toplevel_view_remove != NULL);
        klass->on_toplevel_view_remove(self, view);

        // We created the view and have to release it as well.
        onboard_osk_keyboardview_set_internal_view(view, NULL);  // just to be sure
        g_object_unref(view);

        toplevel->user_data = NULL;
    }
}

// Actor is being destroyed from the outside, e.g. when the stage is destroyed.
static void on_toplevel_actor_destroy (ClutterActor* actor, gpointer user_data)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (user_data));
    OnboardOskInstance *self = ONBOARDOSK_INSTANCE(user_data);
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    OnboardOskContext *ooc = priv->ooc;

    int n = onboard_osk_context_get_n_toplevel_views(ooc);
    printf("on_toplevel_actor_destroy %d\n", n);
    for (int i=0; i<n; i++)
    {
        OnboardOskView* toplevel =
                onboard_osk_context_get_toplevel_view_at(ooc, i);
        OnboardOskKeyboardView* view =
                (OnboardOskKeyboardView*)toplevel->user_data;

        // Has free_toplevel_view() not been called yet for this toplevel?
        if (view)
        {
            g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW (view));

            printf("on_toplevel_actor_destroy %p %p %s\n", toplevel, view, toplevel->name);
            ClutterActor* a = (ClutterActor*)
                onboard_osk_keyboardview_get_actor(view);
            g_return_if_fail (CLUTTER_IS_ACTOR (a));

            if (actor == a)
            {
                free_toplevel_view(self, view);
                break;
            }
        }
    }
}

static void free_toplevel_view (OnboardOskInstance* self,
                                OnboardOskKeyboardView* view)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));

    ClutterActor* actor = (ClutterActor*)
        onboard_osk_keyboardview_get_actor(view);
    g_return_if_fail (CLUTTER_IS_ACTOR (actor));

    g_signal_handlers_disconnect_by_func(actor,
                                         G_CALLBACK (on_toplevel_actor_destroy),
                                         self);

    // We created the view and have to release it as well.
    OnboardOskView* toplevel = onboard_osk_keyboardview_get_internal_view(view);
    printf("free_toplevel_view1 %p %p %s\n", toplevel, view, toplevel->name);
    toplevel->user_data = NULL;
    onboard_osk_keyboardview_set_internal_view(view, NULL);  // just to be sure

    g_object_unref(view);
    printf("free_toplevel_view2\n");
}

guint onboard_osk_instance_get_current_group(OnboardOskInstance* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (self), 0);
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(self);

    g_return_val_if_fail (klass->get_current_group != NULL, 0);
    return klass->get_current_group(self);   // call virtual function
}

void onboard_osk_instance_get_current_rules_names(OnboardOskInstance* self,
                                                  gchar*** names,
                                                  gint* names_length)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(self);

    g_return_if_fail (klass->get_current_rules_names != NULL);
    klass->get_current_rules_names(self, names, names_length);   // call virtual function
}

void onboard_osk_instance_on_group_changed(OnboardOskInstance* self)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    onboard_osk_context_on_group_changed(priv->ooc);
}

void onboard_osk_instance_get_monitor_geometry(OnboardOskInstance* self,
                                               gint monitor_index,
                                               gdouble* x, gdouble* y,
                                               gdouble* w, gdouble* h)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(self);

    g_return_if_fail (klass->get_monitor_geometry != NULL);
    klass->get_monitor_geometry(self, monitor_index, x, y, w, h);   // call virtual function
}

void onboard_osk_instance_get_monitor_size_mm(OnboardOskInstance* self,
                                              gint monitor_index,
                                              gdouble* w, gdouble* h)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(self);

    g_return_if_fail (klass->get_monitor_size_mm != NULL);
    klass->get_monitor_size_mm(self, monitor_index, w, h);   // call virtual function
}

void onboard_osk_instance_get_monitor_workarea(OnboardOskInstance* self,
                                               gint monitor_index,
                                               gdouble* x, gdouble* y,
                                               gdouble* w, gdouble* h)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(self);

    g_return_if_fail (klass->get_monitor_workarea != NULL);
    klass->get_monitor_workarea(self, monitor_index, x, y, w, h);   // call virtual function
}

gint onboard_osk_instance_get_monitor_at_active_window(OnboardOskInstance* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (self), 0);
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(self);

    g_return_val_if_fail (klass->get_monitor_at_active_window != NULL, 0);
    klass->get_monitor_at_active_window(self);   // call virtual function
}

gint onboard_osk_instance_get_monitor_at_actor(OnboardOskInstance* self, GObject* actor)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (self), 0);
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(self);

    g_return_val_if_fail (klass->get_monitor_at_actor != NULL, 0);
    klass->get_monitor_at_actor(self, actor);   // call virtual function
}

gint onboard_osk_instance_get_primary_monitor(OnboardOskInstance* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (self), 0);
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(self);

    g_return_val_if_fail (klass->get_primary_monitor != NULL, 0);
    klass->get_primary_monitor(self);   // call virtual function
}

const gchar* onboard_osk_instance_get_language_full_name(
        OnboardOskInstance* self,
        const gchar* lang_id)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (self), NULL);
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);

    g_return_val_if_fail (priv->ooc != NULL, NULL);
    return onboard_osk_context_get_language_full_name(priv->ooc, lang_id);
}

void onboard_osk_instance_set_active_language_id(
        OnboardOskInstance* self, const gchar* lang_id, gboolean add_to_mru)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);

    g_return_if_fail (priv->ooc != NULL);
    onboard_osk_context_set_active_language_id(priv->ooc, lang_id, add_to_mru);
}

void onboard_osk_instance_show_language_selection(OnboardOskInstance* self,
        OnboardOskKeyboardView* view,
        const gdouble* rect,  gint rect_len,
        const gchar* active_lang_id,
        const gchar* system_lang_id,
        const gchar** mru_lang_ids, gint mru_lang_ids_len,
        const gchar** other_lang_ids, gint other_lang_ids_len)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(self);

    g_return_if_fail (klass->show_language_selection != NULL);
    klass->show_language_selection(self, view,
                                   rect, rect_len,
                                   active_lang_id, system_lang_id,
                                   mru_lang_ids, mru_lang_ids_len,
                                   other_lang_ids, other_lang_ids_len);   // call virtual function
}

void onboard_osk_instance_on_language_selection_closed(OnboardOskInstance* self)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (self));
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);

    g_return_if_fail (priv->ooc != NULL);
    onboard_osk_context_on_language_selection_closed(priv->ooc);
}

// toolkit-dependent Onboard callbacks

// toolkit-dependent timer
typedef struct
{
    TimerCallback callback;
    void* user_data;
    gint timer_id;
} TimerClosure;

static gboolean on_timer(gpointer user_data)
{
    TimerClosure* closure = (TimerClosure*) user_data;
    return closure->callback(closure->user_data);
}

static void on_timer_destroyed(gpointer user_data)
{
    TimerClosure* closure = (TimerClosure*) user_data;
    g_free (closure);
}

static int start_timer(int milliseconds, TimerCallback callback, void* user_data)
{
    TimerClosure *closure = g_new0 (TimerClosure, 1);
    closure->callback = callback;
    closure->user_data = user_data;
    closure->timer_id =
        clutter_threads_add_timeout_full(G_PRIORITY_DEFAULT, milliseconds, on_timer,
                                         closure, on_timer_destroyed);
    return closure->timer_id;
}

static void stop_timer(int timer_id)
{
    g_source_remove(timer_id);
}

static int idle_run(IdleRunCallback callback, void* user_data)
{
    clutter_threads_add_idle(callback, user_data);
    return FALSE;
}

static int is_composited()
{
    return TRUE;   // clutter is always composited, apparently
}

static void get_current_pointer_state(double* x, double* y,
                                      OnboardOskStateMask* mask)
{
    (void)x;
    (void)y;
    (void)mask;

    /*
    rootwin = Gdk.get_default_root_window();
    dunno, x, y, state = rootwin.get_pointer();
    */
}

static void move_to_current_desktop()
{
    /*
    win = get_window();
    if (win)
        win.move_to_current_desktop();
    */
}

static ClutterActor* get_clutter_stage(OnboardOskInstance* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (self), NULL);
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    int n = onboard_osk_context_get_n_toplevel_views(priv->ooc);
    if (n > 0)
    {
        OnboardOskView* toplevel =
                onboard_osk_context_get_toplevel_view_at(priv->ooc, 0);
        g_return_val_if_fail (toplevel, NULL);
        g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(toplevel->user_data),
                              NULL);
        OnboardOskKeyboardView* view =
                ONBOARDOSK_KEYBOARDVIEW(toplevel->user_data);

        ClutterActor* actor = (ClutterActor*)
                onboard_osk_keyboardview_get_actor(view);
        g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

        ClutterActor* stage = clutter_actor_get_stage(actor);
        g_return_val_if_fail (CLUTTER_IS_ACTOR (stage), NULL);

        return stage;
    }
    return NULL;
}

static void get_screen_geometry(OnboardOskContext* ooc, double* x, double* y, double* w, double* h)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data));
    OnboardOskInstance* self = ONBOARDOSK_INSTANCE(ooc->user_data);

    *x = 0;
    *y = 0;
    *w = 0;
    *h = 0;

    ClutterActor* stage = get_clutter_stage(self);
    g_return_if_fail(CLUTTER_IS_ACTOR(stage));

    gfloat x_, y_, w_, h_;
    clutter_actor_get_position(stage, &x_, &y_);
    clutter_actor_get_size(stage, &w_, &h_);
    *x = (double)x_;
    *y = (double)y_;
    *w = (double)w_;
    *h = (double)h_;
}

static int get_n_monitors(OnboardOskContext* ooc)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data), 0);
    OnboardOskInstance* self = ONBOARDOSK_INSTANCE(ooc->user_data);
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(ooc->user_data);

    g_return_val_if_fail (klass->get_n_monitors != NULL, 0);
    return klass->get_n_monitors(self);   // call virtual function
}

static void get_monitor_geometry(OnboardOskContext* ooc, int monitor_index,
                                 double* x, double* y, double* w, double* h)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data));
    OnboardOskInstance* self = ONBOARDOSK_INSTANCE(ooc->user_data);
    onboard_osk_instance_get_monitor_geometry(self, monitor_index, x, y, w, h);
}

static void get_monitor_size_mm(OnboardOskContext* ooc,
                                int monitor_index,
                                double* w, double* h)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data));
    OnboardOskInstance* self = ONBOARDOSK_INSTANCE(ooc->user_data);
    onboard_osk_instance_get_monitor_size_mm(self, monitor_index, w, h);
}

static void get_monitor_workarea(OnboardOskContext* ooc, int monitor_index,
                                 double* x, double* y, double* w, double* h)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data));
    OnboardOskInstance* self = ONBOARDOSK_INSTANCE(ooc->user_data);
    onboard_osk_instance_get_monitor_workarea(self, monitor_index, x, y, w, h);
}

static int get_monitor_at_active_window(OnboardOskContext* ooc)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data), 0);
    OnboardOskInstance* self = ONBOARDOSK_INSTANCE(ooc->user_data);
    return onboard_osk_instance_get_monitor_at_active_window(self);
}

static int get_monitor_at_view(OnboardOskContext* ooc, const OnboardOskView* toplevel)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data), 0);
    OnboardOskInstance* self = ONBOARDOSK_INSTANCE(ooc->user_data);

    OnboardOskKeyboardView* view = (OnboardOskKeyboardView*)toplevel->user_data;
    if (view)
    {
        g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW (view), 0);

        ClutterActor* actor = (ClutterActor*)
            onboard_osk_keyboardview_get_actor(view);
        g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), 0);

        return onboard_osk_instance_get_monitor_at_actor(self, G_OBJECT(actor));
    }
    return 0;
}

static int get_primary_monitor(OnboardOskContext* ooc)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data), 0);
    OnboardOskInstance* self = ONBOARDOSK_INSTANCE(ooc->user_data);
    return onboard_osk_instance_get_primary_monitor(self);
}

static void show_language_selection(
        OnboardOskContext* ooc, OnboardOskView* toplevel,
        const double* rect,  int rect_len,
        const char* active_lang_id,
        const char* system_lang_id,
        const char** mru_lang_ids, int mru_lang_ids_len,
        const char** other_lang_ids, int other_lang_ids_len)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data));
    OnboardOskInstance *self = ONBOARDOSK_INSTANCE(ooc->user_data);

    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(toplevel->user_data));
    OnboardOskKeyboardView* view =
            ONBOARDOSK_KEYBOARDVIEW (toplevel->user_data);

    onboard_osk_instance_show_language_selection(self, view,
                                   rect, rect_len,
                                   active_lang_id, system_lang_id,
                                   mru_lang_ids, mru_lang_ids_len,
                                   other_lang_ids, other_lang_ids_len);
}

static unsigned int get_double_click_time()
{
    unsigned int value = 200;
    ClutterSettings* settings = clutter_settings_get_default();
    if (settings)
        g_object_get (settings, "double-click-time", &value, NULL);
    return value;
}

static double get_drag_threshold()
{
    unsigned int value = 8;
    ClutterSettings* settings = clutter_settings_get_default();
    if (settings)
        g_object_get (settings, "dnd-drag-threshold", &value, NULL);
    return value;
}

/*
static double get_window_scaling_factor()
{
    unsigned int value = 1;
    ClutterSettings* settings = clutter_settings_get_default();
    if (settings)
        g_object_get (settings, "window-scaling-factor", &value, NULL); // gint, this can't be good
    return value;
}
*/

static int is_alt_special()
{
    // Alt moves windows in mutter on X and wayland,
    // however, actors of shell extensions are not affected.
    return FALSE;
}

static int must_fix_configure_event()
{
    // That was a quirk of Compiz, no need to fix in mutter.
    return FALSE;
}

static KeymapGroup
get_current_group(OnboardOskContext* ooc)
{
    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data), 0);

    OnboardOskInstance *self = ONBOARDOSK_INSTANCE(ooc->user_data);
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    return onboard_osk_instance_get_current_group(self);
}

static void
get_current_rules_names(OnboardOskContext* ooc, char*** names, int* names_length)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data));

    OnboardOskInstance *self = ONBOARDOSK_INSTANCE(ooc->user_data);
    OnboardOskInstancePrivate *priv = ONBOARD_OSK_INSTANCE_GET_PRIVATE (self);
    onboard_osk_instance_get_current_rules_names(self, names, names_length);
}

static void
send_keyval_event(OnboardOskContext* ooc, gint keyval, gboolean press)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data));
    OnboardOskInstance* self = ONBOARDOSK_INSTANCE(ooc->user_data);
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(ooc->user_data);

    g_return_if_fail (klass->send_keyval_event != NULL);
    klass->send_keyval_event(self, keyval, press);   // call virtual function
}

static void
send_keycode_event(OnboardOskContext* ooc, gint keycode, gboolean press)
{
    g_return_if_fail (ONBOARDOSK_IS_INSTANCE (ooc->user_data));
    OnboardOskInstance* self = ONBOARDOSK_INSTANCE(ooc->user_data);
    OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(ooc->user_data);

    g_return_if_fail (klass->send_keyval_event != NULL);
    klass->send_keycode_event(self, keycode, press);   // call virtual function
}


int show_dialog_wp_error_recovery(OnboardOskContext* ooc,
                                  const char* markup,
                                  const char* message_type,
                                  const char* buttons)
{
    (void)ooc;
    (void)markup;
    (void)message_type;
    (void)buttons;
#if 0
    if (strcmp(message_type, "question") == 0)
    {
        message_type = Gtk.MessageType.QUESTION;
    }
    else
    {
        message_type = Gtk.MessageType.ERROR;
    }

    if (buttons == "yes_no")
    {
        buttons = Gtk.ButtonsType.YES_NO;
    }
    else
    {
        buttons = Gtk.ButtonsType.OK;
    }

    message_type = Gtk.MessageType.QUESTION;
    dialog = Gtk.MessageDialog(message_type=message_type,
                               buttons=buttons);
    dialog.set_markup(markup);
    if (parent)
    {
        dialog.set_transient_for(parent);
    }

    // Don't hide dialog behind the keyboard in force-to-top mode.
    if (config.is_force_to_top())
    {
        dialog.set_position(Gtk.WindowPosition.CENTER);
    }

    response = dialog.run();
    dialog.destroy();

    return response == Gtk.ResponseType.YES;
#endif
    return FALSE;
}

// setup toolkit-dependent functions
static void init_onboard_callbacks(OnboardOskContext* ooc)
{
    struct OnboardOskCallbacks* callbacks = onboard_osk_context_get_callbacks(ooc);

    callbacks->start_timer = start_timer;
    callbacks->stop_timer = stop_timer;
    callbacks->idle_run = idle_run;
    callbacks->is_composited = is_composited;
    callbacks->get_current_pointer_state = get_current_pointer_state;
    callbacks->move_to_current_desktop = move_to_current_desktop;

    callbacks->get_screen_geometry = get_screen_geometry;

    callbacks->get_monitor_geometry = get_monitor_geometry;
    callbacks->get_monitor_size_mm = get_monitor_size_mm;
    callbacks->get_monitor_workarea = get_monitor_workarea;
    callbacks->get_n_monitors = get_n_monitors;
    callbacks->get_monitor_at_active_window = get_monitor_at_active_window;
    callbacks->get_monitor_at_view = get_monitor_at_view;
    callbacks->get_primary_monitor = get_primary_monitor;

    callbacks->get_double_click_time = get_double_click_time;
    callbacks->get_drag_threshold = get_drag_threshold;

    callbacks->get_current_group = get_current_group;
    callbacks->get_current_rules_names = get_current_rules_names;
    callbacks->send_keyval_event = send_keyval_event;
    callbacks->send_keycode_event = send_keycode_event;

    callbacks->show_language_selection = show_language_selection;
    callbacks->show_dialog_wp_error_recovery = show_dialog_wp_error_recovery;

    callbacks->is_alt_special = is_alt_special;
    callbacks->must_fix_configure_event = must_fix_configure_event;

    callbacks->on_top_level_view_added = on_top_level_view_added;
    callbacks->on_top_level_view_remove = on_top_level_view_remove;

    // finish view callbacks where the actor is defined
    _init_view_callbacks(ooc);
}
