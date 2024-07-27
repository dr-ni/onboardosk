#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <memory>

using namespace std;

#include "layoutitem.h"

GtkWidget *window;

int main (int   argc, char *argv[]) {

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Onboard");

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}

// salvaged functionality from python conversion
#if 0
static int get_n_monitors(OnboardOskContext* ooc)
{
    (void)ooc;
    GdkDisplay* display = gtk_widget_get_display(window);
    if (display)
        return gdk_display_get_n_monitors(display);
    return 0;
}

static void get_monitor_geometry(OnboardOskContext* ooc, int monitor_index,
                                 double* x, double* y, double* w, double* h)
{
    (void)ooc;
    (void)monitor_index;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    /*
    GdkRectangle dest = {};
    screen = this->get_screen();
    geom = gdk_screen_get_monitor_geometry(screen, monitor_index, &dest);
    *x = dest.x;
    *y = dest.y;
    *w = dest.width;
    *h = dest.height;
    *x = 0;
    *y = 0;
    *w = 1920;
    *h = 1200;
    */
}
static void get_monitor_size_mm(OnboardOskContext* ooc, int monitor_index, double* w, double* h)
{
    (void)monitor_index;
    /*
    GdkRectangle dest = {};
    screen = this->get_screen();
    size_mm = (screen.get_monitor_width_mm(monitor),
               screen.get_monitor_height_mm(monitor));
    */
    *w = 518;
    *h = 324;
}

static void get_monitor_workarea(OnboardOskContext* ooc, int monitor_index, double* x, double* y, double* w, double* h)
{
    *x = 0;
    *y = 0;
    *w = 1920;
    *h = 1200;
    /*
    OnboardOskInstance* instance = ONBOARDOSK_INSTANCE(ooc->user_data);

    *x = 0;
    *y = 0;
    *w = 0;
    *h = 0;

    if (instance->n_toplevel_views)
    {
        gfloat x_, y_, w_, h_;
        ClutterActor* actor = CLUTTER_ACTOR(instance->toplevel_views[0]);
        ClutterActor* stage = clutter_actor_get_stage(actor);
        clutter_actor_get_position(stage, &x_, &y_);
        clutter_actor_get_size(stage, &w_, &h_);
        *x = (double)x_;
        *y = (double)y_;
        *w = (double)w_;
        *h = (double)h_;
    }
    */
    /*
        display = Gdk.Display.get_default();
        try
        {
            // From Zesty, new in Gtk 3.22
            monitor = display.get_monitor(monitor_index);
            area = monitor.get_workarea();
        }
        except AttributeError:
            // Around Xenial and below, deprecated and returning unexpected values in Zesty
            screen = this->get_screen();
               area = screen.get_monitor_workarea(monitor_index);

        area = Rect(area.x, area.y, area.width, area.height);
        return area;
    *x = 0;
    *y = 0;
    *w = 1920;
    *h = 1200;
    */
}

static int get_monitor_at_active_window()
{
    int monitor_index = 0;
    /* j
    screen = this->get_screen();
    window = screen.get_active_window();
    if (window)
    {
        // Sometimes causes X error "BadDrawable" in Vivid for Francesco
        Gdk.error_trap_push();
        monitor_index = screen.get_monitor_at_window(window);
        if (Gdk.error_trap_pop())
        {
            monitor_index = screen.get_primary_monitor();
        }
    }
    else
    {
        monitor_index = screen.get_primary_monitor();
    }
    */
    return monitor_index;
}

static int get_monitor_at_view(OnboardOskView* view)
{
    int monitor_index = 0;
    /*
    gdk_win = window.get_window();
    screen = window.get_screen();
    if (gdk_win && screen)
        monitor = screen.get_monitor_at_window(gdk_win);
    */
    return monitor_index;
}

static int get_primary_monitor()
{
    int monitor_index = 0;
    /*
    screen = this->get_screen();
    monitor_index = screen.get_primary_monitor();
    */
    return monitor_index;
}

static unsigned int get_double_click_time()
{
    unsigned int time = 200;
    /*
    Gtk.Settings.get_default() .get_property("gtk-double-click-time");
    */
    return time;
}

static double get_drag_threshold()
{
    double val = 8;
    /*
    val Gtk.Settings.get_default() .get_property("gtk-dnd-drag-threshold");
    */
    return val;
}

static int is_alt_special()
{
    // Alt moves windows in mutter on X and wayland,
    // however, actors of shell extensions are not affected.
    return FALSE;
}

#endif
