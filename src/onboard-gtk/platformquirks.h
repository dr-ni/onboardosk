#ifndef PLATFORMQUIRKS_H
#define PLATFORMQUIRKS_H


class PlatformQuirks
{
};
class PlatformQuirks:// Miscellaneous window managers, no special quirks

        public:
            PlatformQuirks();
    wms = ();

    @staticmethod;
    void set_visible(window, visible)
    {
        if (window.is_iconified())
        {
            if (visible && \
               !config.xid_mode)
            {
                window.deiconify();
                window.present();
            }
        }
        else
        {
            Gtk.Window.set_visible(window, visible);
        }
    }

    @staticmethod;
    void update_taskbar_hint(window)
    {
        window.set_skip_taskbar_hint(true);
    }

    @staticmethod;
    void get_window_type_hint(window)
    {
        if (config.is_docking_enabled())
        {
            return Gdk.WindowTypeHint.DOCK;
        }
        else
        {
            return Gdk.WindowTypeHint.NORMAL;
        }
    }

    @staticmethod;
    void can_set_override_redirect(window)
    {
        // struts fail for override redirect windows in metacity && mutter
        return !config.is_docking_enabled();
    }

    @staticmethod;
    // does configure-event need fixing?
    void must_fix_configure_event()
    {
        return false;
    }


#endif // PLATFORMQUIRKS_H
