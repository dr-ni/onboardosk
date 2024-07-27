#include <math.h>
#include <cairo.h>
#include <string.h>

#include <clutter/clutter.h>

#include "onboardosk.h"
#include "onboardosk-keyboardview.h"
#include "onboardosk-instance.h"


typedef struct
{
    OnboardOskView* view;
    ClutterActor* actor;
    OnboardOskCursorType last_cursor_type;
    gint n_active_sequences;
} OnboardOskKeyboardViewPrivate;


G_DEFINE_TYPE_WITH_PRIVATE (OnboardOskKeyboardView, onboard_osk_keyboardview, G_TYPE_OBJECT)

#define ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ONBOARDOSK_TYPE_KEYBOARDVIEW, OnboardOskKeyboardViewPrivate))


// enum for class signals
enum
{
  SIG_CURSOR_CHANGE_REQUEST,
  N_SIGNALS
};

// array of class signal ids
static guint g_signals[N_SIGNALS];


static void
onboard_osk_keyboardview_finalize (GObject *gobject);

static gboolean
on_clutter_canvas_draw(ClutterCanvas *canvas,
                       cairo_t       *cr,
                       int            width,
                       int            height,
                       OnboardOskKeyboardView* self);

// class initialization
static void
onboard_osk_keyboardview_class_init (OnboardOskKeyboardViewClass *klass)
{
    GObjectClass *super_class = G_OBJECT_CLASS (klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    super_class->finalize = onboard_osk_keyboardview_finalize;

    g_signals[SIG_CURSOR_CHANGE_REQUEST] =
      g_signal_new ("cursor-change-request",
                   G_TYPE_FROM_CLASS (gobject_class),
                   (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS),
                   0 /* closure */,
                   NULL /* accumulator */,
                   NULL /* accumulator data */,
                   NULL /* C marshaller */,
                   G_TYPE_NONE /* return_type */,
                   1     /* n_params */,
                   G_TYPE_UINT  /* param_types */);
}

// instance initialization
OnboardOskKeyboardView *
onboard_osk_keyboardview_new (void)
{
  OnboardOskKeyboardView *instance;
  instance = ONBOARDOSK_KEYBOARDVIEW(g_object_new(ONBOARDOSK_TYPE_KEYBOARDVIEW, NULL));
  return instance;
}

static void
onboard_osk_keyboardview_init (OnboardOskKeyboardView *self)
{
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE (self);

    priv->last_cursor_type = ONBOARD_OSK_CURSOR_NONE;
}

static void
onboard_osk_keyboardview_finalize (GObject *gobject)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(gobject));
    OnboardOskKeyboardView* self = ONBOARDOSK_KEYBOARDVIEW(gobject);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE (self);

    g_object_unref(priv->actor);

    G_OBJECT_CLASS (onboard_osk_keyboardview_parent_class)->finalize (gobject);
}

static void
on_actor_resize (ClutterActor           *actor,
                 const ClutterActorBox  *allocation,
                 ClutterAllocationFlags  flags,
                 gpointer                user_data)
{
    g_return_if_fail (CLUTTER_IS_ACTOR(actor));
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(user_data));

    OnboardOskKeyboardView *self = ONBOARDOSK_KEYBOARDVIEW (user_data);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    gfloat width, height;
    clutter_actor_get_size (actor, &width, &height);
    //printf("on_actor_resize %f %f\n", width, height );

    onboard_osk_view_on_toplevel_geometry_changed(priv->view);
}

static void init_event(ClutterActor* actor, const ClutterEvent *event, OnboardOskEvent* ooe)
{
    gfloat stage_x, stage_y;
    gfloat actor_x, actor_y;

    ooe->sequence_id = 0;

    clutter_event_get_coords (event, &stage_x, &stage_y);
    clutter_actor_transform_stage_point (actor,
                                         stage_x, stage_y,
                                         &actor_x, &actor_y);
    ooe->x = (double)actor_x;
    ooe->y = (double)actor_y;
    ooe->x_root = (double)stage_x;
    ooe->y_root = (double)stage_y;

    if (event->type == CLUTTER_ENTER ||
        event->type == CLUTTER_LEAVE)
    {
        ooe->button = 0;
    }
    else
    {
        ClutterEventSequence* sequence =
                clutter_event_get_event_sequence (event);
        if (sequence)
            // "The #ClutterEventSequence structure is an opaque
            // type used to denote the event sequence of a touch event"
            ooe->sequence_id = (intptr_t)sequence;
        else
            ooe->sequence_id = 0;

        if (event->type == CLUTTER_BUTTON_PRESS ||
            event->type == CLUTTER_BUTTON_RELEASE)
            ooe->button = (int)clutter_event_get_button(event);
        else
            ooe->button = 1;
    }

    ooe->time = clutter_event_get_time (event);
    ooe->state = clutter_event_get_state (event);

    ClutterInputDevice* source_device = clutter_event_get_source_device (event);
    if (source_device)
    {
        ooe->source_device_id =
            (OnboardOskDeviceId)clutter_input_device_get_device_id(
                                    source_device);
        ooe->source_device_type =
            (OnboardOskDeviceType)clutter_input_device_get_device_type(
                                       source_device);
        const char* name = clutter_input_device_get_device_name(source_device);
        if (name)
            strncpy(ooe->source_device_name, name,
                    sizeof(ooe->source_device_name));
    }
    else
    {
        ooe->source_device_id = 0;
        ooe->source_device_type = ONBOARD_OSK_UNKNOWN_DEVICE;
    }
}

static OnboardOskInstance* get_instance_from_view(OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), NULL);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    g_return_val_if_fail(priv->view != NULL, NULL);
    g_return_val_if_fail(priv->view->ooc != NULL, NULL);

    g_return_val_if_fail (ONBOARDOSK_IS_INSTANCE (priv->view->ooc->user_data), NULL);
    return ONBOARDOSK_INSTANCE(priv->view->ooc->user_data);
}

static void count_active_sequences(OnboardOskKeyboardView* self, int increment)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self));
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);
    priv->n_active_sequences += increment;
    if (priv->n_active_sequences < 0)
        priv->n_active_sequences = 0;
    if (priv->n_active_sequences > 1)   // limit to 1, can't get out of sync
        priv->n_active_sequences = 1;
}

static gboolean on_button_press (ClutterActor *actor,
                                 ClutterEvent *event,
                                 OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (CLUTTER_IS_ACTOR(actor), CLUTTER_EVENT_STOP);
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), CLUTTER_EVENT_STOP);

    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);
    count_active_sequences(self, 1);

    OnboardOskEvent ooe = {ONBOARD_OSK_EVENT_BUTTON_PRESS};
    init_event(actor, event, &ooe);

    //printf("on_button_press %f %f\n", ooe.x, ooe.y);

    onboard_osk_view_on_event(priv->view, &ooe);

    return CLUTTER_EVENT_STOP;
}

static gboolean on_button_release (ClutterActor *actor,
                                   ClutterEvent *event,
                                   OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (CLUTTER_IS_ACTOR(actor), CLUTTER_EVENT_STOP);
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), CLUTTER_EVENT_STOP);

    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);
    count_active_sequences(self, -1);

    OnboardOskEvent ooe = {ONBOARD_OSK_EVENT_BUTTON_RELEASE};
    init_event(actor, event, &ooe);

    //printf("on_button_release %f %f\n", ooe.x, ooe.y);

    onboard_osk_view_on_event(priv->view, &ooe);

    return CLUTTER_EVENT_STOP;
}

static gboolean on_motion (ClutterActor *actor,
                           ClutterEvent *event,
                           OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (CLUTTER_IS_ACTOR(actor), CLUTTER_EVENT_STOP);
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), CLUTTER_EVENT_STOP);

    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    OnboardOskEvent ooe = {ONBOARD_OSK_EVENT_MOTION};
    init_event(actor, event, &ooe);

    //printf("on_motion %f %f\n", ooe.x, ooe.y);

    onboard_osk_view_on_event(priv->view, &ooe);

    return CLUTTER_EVENT_STOP;
}

static gboolean on_touch_event (ClutterActor *actor,
                                ClutterEvent *event,
                                OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (CLUTTER_IS_ACTOR(actor), CLUTTER_EVENT_STOP);
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), CLUTTER_EVENT_STOP);

    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    if (event->type == CLUTTER_TOUCH_BEGIN)
        count_active_sequences(self, 1);
    else if (event->type == CLUTTER_TOUCH_END ||
             event->type == CLUTTER_TOUCH_CANCEL)
        count_active_sequences(self, -1);

    OnboardOskEvent ooe = {};
    if (event->type == CLUTTER_TOUCH_BEGIN)
        ooe.type == ONBOARD_OSK_EVENT_TOUCH_BEGIN;
    else if (event->type == CLUTTER_TOUCH_UPDATE)
        ooe.type == ONBOARD_OSK_EVENT_TOUCH_UPDATE;
    else if (event->type == CLUTTER_TOUCH_END)
        ooe.type == ONBOARD_OSK_EVENT_TOUCH_END;
    else if (event->type == CLUTTER_TOUCH_CANCEL)
        ooe.type = ONBOARD_OSK_EVENT_TOUCH_CANCEL;
    else
        return CLUTTER_EVENT_STOP;

    init_event(actor, event, &ooe);

    //printf("on_touch_event %f %f\n", ooe.x, ooe.y);

    onboard_osk_view_on_event(priv->view, &ooe);

    return CLUTTER_EVENT_STOP;
}

static gboolean on_captured_event (ClutterActor *actor,
                                   ClutterEvent *event,
                                   OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (CLUTTER_IS_ACTOR(actor), CLUTTER_EVENT_STOP);
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), CLUTTER_EVENT_STOP);

    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    OnboardOskEvent ooe = {ONBOARD_OSK_EVENT_MOTION};
    init_event(actor, event, &ooe);

    ClutterActor * related =	clutter_event_get_related (event);
    printf("on_captured_event %f %f %d %d %p %d\n",
           ooe.x, ooe.y, event->type, clutter_event_get_flags (event),
           related, clutter_event_get_state (event));
    return CLUTTER_EVENT_PROPAGATE;

    if (event->type == CLUTTER_LEAVE)
        return on_button_release(actor, event, self);

    if (event->type == CLUTTER_TOUCH_END)
        return on_touch_event(actor, event, self);

    return CLUTTER_EVENT_PROPAGATE;
}

static gboolean on_enter_event (ClutterActor *actor,
                                ClutterEvent *event,
                                OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (CLUTTER_IS_ACTOR(actor), CLUTTER_EVENT_STOP);
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), CLUTTER_EVENT_STOP);

    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    OnboardOskEvent ooe = {ONBOARD_OSK_EVENT_ENTER};
    init_event(actor, event, &ooe);

    onboard_osk_view_on_event(priv->view, &ooe);

    return CLUTTER_EVENT_STOP;
}

static gboolean on_leave_event (ClutterActor *actor,
                                ClutterEvent *event,
                                OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (CLUTTER_IS_ACTOR(actor), CLUTTER_EVENT_STOP);
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), CLUTTER_EVENT_STOP);

    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    //printf("on_leave_event\n");

    OnboardOskEvent ooe = {ONBOARD_OSK_EVENT_LEAVE};
    init_event(actor, event, &ooe);

    onboard_osk_view_on_event(priv->view, &ooe);

    return CLUTTER_EVENT_STOP;
}

/**
 * onboard_osk_keyboardview_get_name:
 *
 * Returns: (transfer none): the name
 */
const char* onboard_osk_keyboardview_get_name(OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), NULL);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE (self);

    g_return_val_if_fail (priv->view != NULL, NULL);

    return priv->view->name;
}

gboolean onboard_osk_keyboardview_is_reactive_type(OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), TRUE);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE (self);

    g_return_val_if_fail (priv->view != NULL, TRUE);

    OnboardOskView* toplevel = priv->view;
    return toplevel->view_type == ONBOARD_OSK_VIEW_TYPE_KEYBOARD ||
           toplevel->view_type == ONBOARD_OSK_VIEW_TYPE_LAYOUT_POPUP;
}

void onboard_osk_keyboardview_set_internal_view(OnboardOskKeyboardView* self, OnboardOskView* view)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self));
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE (self);
    priv->view = view;
}

/**
 * onboard_osk_keyboardview_get_internal_view: (skip)
 */
OnboardOskView* onboard_osk_keyboardview_get_internal_view(OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), NULL);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE (self);
    return priv->view;
}

/**
 * onboard_osk_keyboardview_set_actor:
 * @actor: (transfer none): a #ClutterActor
 *
 * Set the @actor the will be displaying this view.
 */
void onboard_osk_keyboardview_set_actor(OnboardOskKeyboardView* self, GObject* actor)
{
    OnboardOskKeyboardViewPrivate* priv;
    ClutterContent* content;

    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self));
    g_return_if_fail (CLUTTER_IS_ACTOR(actor));

    priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE (self);
    if (priv->actor)
        g_object_unref(priv->actor);
    priv->actor = (ClutterActor*)actor;
    g_object_ref(priv->actor);

    g_signal_connect (priv->actor, "allocation-changed", G_CALLBACK (on_actor_resize), self);

    g_signal_connect (priv->actor, "enter-event", G_CALLBACK (on_enter_event), self);
    g_signal_connect (priv->actor, "leave-event", G_CALLBACK (on_leave_event), self);

    g_signal_connect (priv->actor, "button-press-event", G_CALLBACK (on_button_press), self);
    g_signal_connect (priv->actor, "button-release-event", G_CALLBACK (on_button_release), self);
    g_signal_connect (priv->actor, "motion-event", G_CALLBACK (on_motion), self);

    g_signal_connect (priv->actor, "touch-event", G_CALLBACK (on_touch_event), self);

    //g_signal_connect (priv->actor, "captured-event", G_CALLBACK (on_captured_event), self);

    content = clutter_actor_get_content(priv->actor);
    g_return_if_fail (CLUTTER_IS_CANVAS(content));
    g_signal_connect (content, "draw", G_CALLBACK (on_clutter_canvas_draw), self);
}

/**
 * onboard_osk_keyboardview_get_actor:
 *
 * Set the actor the will be displaying this view.
 * Returns: (transfer none): the #ClutterActor
 */
GObject* onboard_osk_keyboardview_get_actor(OnboardOskKeyboardView* self)
{
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self), NULL);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE (self);
    g_return_val_if_fail (CLUTTER_IS_ACTOR(priv->actor), NULL);
    return G_OBJECT(priv->actor);
}

static gboolean on_clutter_canvas_draw(ClutterCanvas *canvas,
                                   cairo_t       *cr,
                                   int            width,
                                   int            height,
                                   OnboardOskKeyboardView* self)
{
    g_return_val_if_fail(CLUTTER_IS_CANVAS(canvas), CLUTTER_EVENT_STOP);
    onboard_osk_keyboardview_draw(self, cr);
    return CLUTTER_EVENT_STOP;

}

/**
 * onboard_osk_keyboardview_draw:
 * @cr: (transfer none): cairo context
 *
 * Draw view into cairo context @cr.
 */
void onboard_osk_keyboardview_draw(OnboardOskKeyboardView* self, cairo_t* cr)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(self));
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    g_return_if_fail (CLUTTER_IS_ACTOR(priv->actor));
    ClutterActor* actor = priv->actor;

    #if 0
    gfloat x, y, w, h;
    clutter_actor_get_position(actor, &x, &y);
    clutter_actor_get_size(actor, &w, &h);
    printf("on_draw1 %f %f %f %f\n", x, y, w, h);
    #endif

    // clear canvas
    cairo_save (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    cairo_restore (cr);

    #if 0
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

    clutter_cairo_set_source_color (cr, CLUTTER_COLOR_Blue);
    cairo_rectangle (cr, 0, 0, width, height);
    cairo_fill (cr);

    clutter_cairo_set_source_color (cr, CLUTTER_COLOR_Red);
    cairo_move_to (cr, 0, 0);
    cairo_line_to (cr, width-1, height-1);
    cairo_move_to (cr, 0, height-1);
    cairo_line_to (cr, width, 0);
    cairo_stroke (cr);
    #endif

    onboard_osk_view_draw(priv->view, cr);
}



// Onboard toolkit dependent callbacks

static void view_get_geometry(OnboardOskView* view, OnboardOskViewGeometry* geometry)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(view->user_data));

    OnboardOskKeyboardView *self = ONBOARDOSK_KEYBOARDVIEW(view->user_data);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    g_return_if_fail (CLUTTER_IS_ACTOR(priv->actor));
    ClutterActor* actor = priv->actor;

    gfloat x, y, w, h;
    clutter_actor_get_position(actor, &x, &y);
    clutter_actor_get_size(actor, &w, &h);
    geometry->x = (double)x;
    geometry->y = (double)y;
    geometry->w = (double)w;
    geometry->h = (double)h;
}

static void view_move(OnboardOskView* view, double x, double y)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(view->user_data));

    OnboardOskKeyboardView *self = ONBOARDOSK_KEYBOARDVIEW(view->user_data);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    g_return_if_fail (CLUTTER_IS_ACTOR(priv->actor));
    ClutterActor* actor = priv->actor;

    x = floor(x);
    y = floor(y);
    clutter_actor_set_position(actor, (gfloat)x, (gfloat)y);
    //printf("view_move %f %f %p %p\n", x, y, view, actor);
}

static void view_move_resize(OnboardOskView* view, double x, double y, double w, double h)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(view->user_data));

    OnboardOskKeyboardView *self = ONBOARDOSK_KEYBOARDVIEW(view->user_data);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    g_return_if_fail (CLUTTER_IS_ACTOR(priv->actor));
    ClutterActor* actor = priv->actor;

    x = floor(x);
    y = floor(y);
    w = floor(w);
    h = floor(h);
    clutter_actor_set_position(actor, (gfloat)x, (gfloat)y);
    clutter_actor_set_size(actor, (gfloat)w, (gfloat)h);
    // printf("view_move_resize %f %f %f %f %p %p\n", x, y, w, h, view, actor);
}

static void view_set_visible(OnboardOskView* view, int visible)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(view->user_data));

    OnboardOskKeyboardView *self = ONBOARDOSK_KEYBOARDVIEW(view->user_data);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    g_return_if_fail (CLUTTER_IS_ACTOR(priv->actor));
    ClutterActor* actor = priv->actor;

    if (visible)
        clutter_actor_show(actor);
    else
        clutter_actor_hide(actor);
}

static void view_get_screen_rect(OnboardOskView* view, double* x, double* y, double* w, double* h)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(view->user_data));

    OnboardOskKeyboardView *self = ONBOARDOSK_KEYBOARDVIEW(view->user_data);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    g_return_if_fail (CLUTTER_IS_ACTOR(priv->actor));
    ClutterActor* actor = priv->actor;

    ClutterActor* stage = clutter_actor_get_stage(actor);
    if (stage)
    {
        gfloat x_, y_, w_, h_;
        clutter_actor_get_position(stage, &x_, &y_);
        clutter_actor_get_size(stage, &w_, &h_);
        *x = (double)x_;
        *y = (double)y_;
        *w = (double)w_;
        *h = (double)h_;
    }
    else
    {
        *x = 0.0;
        *y = 0.0;
        *w = 0.0;
        *h = 0.0;
    }
}

static void view_queue_draw(OnboardOskView* view)
{
    OnboardOskKeyboardView *self;
    OnboardOskKeyboardViewPrivate *priv;
    ClutterActor* actor;
    ClutterContent* content;

    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(view->user_data));
    self = ONBOARDOSK_KEYBOARDVIEW(view->user_data);
    priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    g_return_if_fail (CLUTTER_IS_ACTOR(priv->actor));
    actor = priv->actor;
    if (clutter_actor_is_visible(actor))
    {
        content = clutter_actor_get_content(actor);

        //printf("view_queue_draw1\n");
        clutter_content_invalidate (content);
        //printf("view_queue_draw2\n");
        //clutter_actor_queue_redraw(actor);
    }
}

static void view_queue_draw_area(OnboardOskView* view, double x, double y, double w, double h)
{
    view_queue_draw(view);

    //cairo_rectangle_int_t clip = {(int)x, (int)y, (int)w, (int)h};
    //clutter_actor_queue_redraw_with_clip(actor, &clip);
    //clutter_actor_queue_redraw(actor);
}

static void view_process_updates(OnboardOskView* view)
{
    (void)view;
    // how?
}

static PangoContext* view_get_pango_context(OnboardOskView* view)
{
    g_return_val_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(view->user_data), NULL);

    OnboardOskKeyboardView *self = ONBOARDOSK_KEYBOARDVIEW(view->user_data);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    g_return_val_if_fail (CLUTTER_IS_ACTOR(priv->actor), NULL);
    ClutterActor* actor = priv->actor;

    return clutter_actor_get_pango_context(actor);
}

void view_set_cursor_type(OnboardOskView* view, OnboardOskCursorType cursor_type)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(view->user_data));

    OnboardOskKeyboardView *self = ONBOARDOSK_KEYBOARDVIEW(view->user_data);
    OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);

    g_return_if_fail (CLUTTER_IS_ACTOR(priv->actor));
    ClutterActor* actor = priv->actor;

    // Mutter doesn't reset the cursor with META_CURSOR_NONE and
    // aborts instead. Set it to the default cursor instead.
    if (cursor_type == ONBOARD_OSK_CURSOR_NONE)
        cursor_type = ONBOARD_OSK_CURSOR_DEFAULT;

    // Also the cursor is global, so remember a single view-independent
    // last_cursor in order to reduce calling frequency.
    if (priv->last_cursor_type != cursor_type)
    {
        g_signal_emit(self, g_signals[SIG_CURSOR_CHANGE_REQUEST], 0, cursor_type);
        priv->last_cursor_type = cursor_type;
    }

    /*
    cursor = Gdk.Cursor(cursor_type);
    if (cursor)
    {
        window.set_cursor(cursor);
    }
    global.screen.set_cursor(Meta.Cursor.POINTING_HAND);
    */
}

static void view_grab_pointer(OnboardOskView* toplevel)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(toplevel->user_data));
    OnboardOskKeyboardView *self = ONBOARDOSK_KEYBOARDVIEW(toplevel->user_data);
    OnboardOskInstance* instance = get_instance_from_view(self);
    if (instance)
    {
        OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);
        ClutterActor* actor = priv->actor;
        g_return_if_fail (CLUTTER_IS_ACTOR(actor));

        OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(instance);
        g_return_if_fail (klass->grab_pointer != NULL);
        klass->grab_pointer(instance);   // call virtual function

        clutter_grab_pointer(actor);
    }
}

static void view_ungrab_pointer(OnboardOskView* toplevel)
{
    g_return_if_fail (ONBOARDOSK_IS_KEYBOARDVIEW(toplevel->user_data));
    OnboardOskKeyboardView *self = ONBOARDOSK_KEYBOARDVIEW(toplevel->user_data);

    clutter_ungrab_pointer();

    OnboardOskInstance* instance = get_instance_from_view(self);
    if (instance)
    {
        OnboardOskKeyboardViewPrivate *priv = ONBOARDOSK_KEYBOARDVIEW_GET_PRIVATE(self);
        ClutterActor* actor = priv->actor;
        g_return_if_fail (CLUTTER_IS_ACTOR(actor));

        OnboardOskInstanceClass* klass = ONBOARDOSK_INSTANCE_GET_CLASS(instance);
        g_return_if_fail (klass->ungrab_pointer != NULL);
        klass->ungrab_pointer(instance);   // call virtual function
    }
}

void _init_view_callbacks(struct _OnboardOskContext* ooc)
{
    struct OnboardOskCallbacks* callbacks = onboard_osk_context_get_callbacks(ooc);
    callbacks->view_get_geometry = view_get_geometry;
    callbacks->view_move = view_move;
    callbacks->view_move_resize = view_move_resize;
    callbacks->view_set_visible = view_set_visible;
    callbacks->view_get_screen_rect = view_get_screen_rect;
    callbacks->view_queue_draw = view_queue_draw;
    callbacks->view_queue_draw_area = view_queue_draw_area;
    callbacks->view_process_updates = view_process_updates;
    callbacks->view_get_pango_context = view_get_pango_context;
    callbacks->view_set_cursor_type = view_set_cursor_type;
    callbacks->view_grab_pointer = view_grab_pointer;
    callbacks->view_ungrab_pointer = view_ungrab_pointer;
}



