#ifndef ONBOARDOSK_INSTANCE_H
#define ONBOARDOSK_INSTANCE_H

#include <glib-object.h>

G_BEGIN_DECLS

struct _OnboardOskKeyboardView;
typedef struct _OnboardOskKeyboardView OnboardOskKeyboardView;

#define ONBOARD_OSK_TYPE_INSTANCE (onboard_osk_instance_get_type ())
G_DECLARE_DERIVABLE_TYPE (OnboardOskInstance, onboard_osk_instance, ONBOARDOSK, INSTANCE, GObject)

struct _OnboardOskInstanceClass
{
    GObjectClass parent_class;  // we are inheriting from this

    /* virtual methods */
    void (*on_toplevel_view_added) (OnboardOskInstance  *self, OnboardOskKeyboardView* view);
    void (*on_toplevel_view_remove) (OnboardOskInstance  *self, OnboardOskKeyboardView* view);
    guint (*get_current_group) (OnboardOskInstance  *self);

    /**
     * onboard_osk_instance_get_current_rules_names:
     * @names: (out callee-allocates) (array length=names_length) (transfer none): five strings
     * @names_length: (out): Length of @names
     */
    void (*get_current_rules_names)(OnboardOskInstance* self, gchar*** names, gint* names_length);

    void (*send_keyval_event)(OnboardOskInstance* self, gint keyval, gboolean press);
    void (*send_keycode_event)(OnboardOskInstance* self, gint keycode, gboolean press);

    void (*grab_pointer)(OnboardOskInstance* self);
    void (*ungrab_pointer)(OnboardOskInstance* self);

    guint (*get_n_monitors)(OnboardOskInstance* self);

    /**
     * onboard_osk_instance_get_monitor_geometry:
     * @monitor_index: (in): index of the monitor
     * @x: (out) (transfer none): monitor geometry x
     * @y: (out) (transfer none): monitor geometry y
     * @w: (out) (transfer none): monitor geometry width
     * @h: (out) (transfer none): monitor geometry height
     */
    void (*get_monitor_geometry)(OnboardOskInstance* self, gint monitor_index,
                                 gdouble* x, gdouble* y, gdouble* w, gdouble* h);

    /**
     * onboard_osk_instance_get_monitor_geometry_mm:
     * @monitor_index: (in): index of the monitor
     * @w: (out) (transfer none): monitor width in mm
     * @h: (out) (transfer none): monitor height in mm
     */
    void (*get_monitor_size_mm)(OnboardOskInstance* self, gint monitor_index,
                                gdouble* w, gdouble* h);
    /**
     * onboard_osk_instance_get_monitor_workarea:
     * @monitor_index: (in): index of the monitor
     * @x: (out) (transfer none): monitor work area x
     * @y: (out) (transfer none): monitor work area y
     * @w: (out) (transfer none): monitor work area width
     * @h: (out) (transfer none): monitor work area height
     */
    void (*get_monitor_workarea)(OnboardOskInstance* self, gint monitor_index,
                                 gdouble* x, gdouble* y, gdouble* w, gdouble* h);

    gint (*get_monitor_at_active_window)(OnboardOskInstance* self);
    gint (*get_monitor_at_actor)(OnboardOskInstance* self,
                                 GObject* actor);
    gint (*get_primary_monitor)(OnboardOskInstance* self);

    /**
     * onboard_osk_instance_show_language_selection:
     * @view: (in) (transfer none): toplevel view the language button resides in
     * @rect: (in) (array length=rect_len) (transfer none): x, y, w, h of button in the actor's coordinates
     * @rect_len: (in): Length of @rect
     * @active_lang_id: (in) (transfer none): id of the currently active language
     * @system_lang_id: (in) (transfer none): id of the user's system language
     * @mru_lang_ids: (in) (array length=mru_lang_ids_len) (transfer none): most recently used language ids
     * @mru_lang_ids_len: (in): Length of @mru_lang_ids
     * @other_lang_ids: (in) (array length=other_lang_ids_len) (transfer none): remaining language ids
     * @other_lang_ids_len: (in): Length of @other_lang_ids
     */
    void (*show_language_selection)(
            OnboardOskInstance* self,
            OnboardOskKeyboardView* view,
            const gdouble* rect, gint rect_len,
            const gchar* active_lang_id,
            const gchar* system_lang_id,
            const gchar** mru_lang_ids, gint mru_lang_ids_len,
            const gchar** other_lang_ids, gint other_lang_ids_len);
    gpointer padding[20];
};

/* public methods. */

OnboardOskInstance * onboard_osk_instance_new (void);   // constructor

gint onboard_osk_instance_start(OnboardOskInstance* self);
void onboard_osk_instance_destroy(OnboardOskInstance* self);

void onboard_osk_instance_toggle_visible(OnboardOskInstance* self);
void onboard_osk_instance_show(OnboardOskInstance* self);
void onboard_osk_instance_hide(OnboardOskInstance* self);
gboolean onboard_osk_instance_is_visible(OnboardOskInstance* self);

guint onboard_osk_instance_get_n_toplevel_views(OnboardOskInstance* self);
OnboardOskKeyboardView* onboard_osk_instance_get_toplevel_view_at(OnboardOskInstance* self, gint index);

guint onboard_osk_instance_get_current_group (OnboardOskInstance *self);
void onboard_osk_instance_get_current_rules_names(OnboardOskInstance* self, gchar*** names, gint* names_length);

void onboard_osk_instance_on_group_changed(OnboardOskInstance* self);

void onboard_osk_instance_get_monitor_geometry(
        OnboardOskInstance* self, gint monitor_index,
        gdouble* x, gdouble* y, gdouble* w, gdouble* h);
void onboard_osk_instance_get_monitor_size_mm(
        OnboardOskInstance* self, gint monitor_index,
        gdouble* w, gdouble* h);
void onboard_osk_instance_get_monitor_workarea(
        OnboardOskInstance* self, gint monitor_index,
        gdouble* x, gdouble* y, gdouble* w, gdouble* h);
gint onboard_osk_instance_get_monitor_at_active_window(
        OnboardOskInstance* self);
gint onboard_osk_instance_get_monitor_at_actor(
        OnboardOskInstance* self, GObject* actor);
gint onboard_osk_instance_get_primary_monitor(
        OnboardOskInstance* self);

const gchar* onboard_osk_instance_get_language_full_name(
        OnboardOskInstance* self,
        const gchar* lang_id);

void onboard_osk_instance_set_active_language_id(
        OnboardOskInstance* self,
        const gchar* lang_id, gboolean add_to_mru);

void onboard_osk_instance_show_language_selection(
        OnboardOskInstance* self,
        OnboardOskKeyboardView* view,
        const gdouble* rect, gint rect_len,
        const gchar* active_lang_id,
        const gchar* system_lang_id,
        const gchar** mru_lang_ids, gint mru_lang_ids_len,
        const gchar** other_lang_ids, gint other_lang_ids_len);

void onboard_osk_instance_on_language_selection_closed(
        OnboardOskInstance* self);

G_END_DECLS

#endif // ONBOARDOSK_INSTANCE_H
