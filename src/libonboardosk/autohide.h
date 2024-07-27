#ifndef AUTOHIDE_H
#define AUTOHIDE_H

#include <memory>

#include "onboardoskglobals.h"

class GlobalKeyListener;
class UDevDevice;

struct _OnboardOskEvent;
typedef struct _OnboardOskEvent OnboardOskEvent;

class AutoHide : public ContextBase
{
    public:
        AutoHide(const ContextBase& context);
        virtual ~AutoHide();

        bool is_enabled()
        {
            return m_key_listener != nullptr;
        }

        void enable(bool enable)
        {
            register_events(enable);
        }

        bool is_auto_show_locked();

        void auto_show_unlock();

    private:
        void register_events(bool register_);
        void on_devices_updated();
        void on_key_press(const OnboardOskEvent* event);
        bool is_real_keyboard_event(const OnboardOskEvent* event);

        std::vector<std::string> get_keyboard_device_names();

    private:
        static constexpr const char* LOCK_REASON = "hide-on-key-press";

        GlobalKeyListener* m_key_listener{};
        std::vector<UDevDevice> m_udev_keyboard_devices;
};

#endif // AUTOHIDE_H
