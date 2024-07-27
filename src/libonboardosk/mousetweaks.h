#ifndef MOUSETWEAKS_H
#define MOUSETWEAKS_H

#include "clickgenerator.h"


class MouseTweaks : public ClickGenerator
{
    public:
        using Super = ClickGenerator;

        MouseTweaks(const ContextBase& context);
        virtual ~MouseTweaks();

        bool is_active()
        {
            //return this->dwell_click_enabled && bool(this->_iface);
            return false;
        }
        void on_hide_click_type_window_changed(bool hide) {(void)hide;}
        void enable_hover_click(bool enable) {(void)enable;}
};

#endif // MOUSETWEAKS_H
