#ifndef HARDWARESENSORTRACKER_H
#define HARDWARESENSORTRACKER_H

#include "tools/noneable.h"

#include "signalling.h"

struct _OnboardOskEvent;
typedef struct _OnboardOskEvent OnboardOskEvent;

class AcpidListener;
class GlobalKeyListener;


class HardwareSensorTracker : public ContextBase
{
        friend AcpidListener;

    public:
        using Super = ContextBase;
        HardwareSensorTracker(const ContextBase& context);
        virtual ~HardwareSensorTracker();

        static std::unique_ptr<HardwareSensorTracker> make(const ContextBase& context);

        // Return value:
        // true = convertible is in tablet-mode
        // false = convertible is not in tablet-mode
        // None = mode unknown
        Noneable<bool> get_tablet_mode();

        void update_sensor_sources();

    private:
        void register_listeners(bool register_);
        void register_acpid_listeners(bool register_);
        void register_hotkey_listeners(bool register_);
        void set_tablet_mode(bool activ);

        void on_listeners_changed();
        bool has_listeners();

        // Read the state from known system files, if available.
        // Else return None.
        // "sysfs" files are read from kernel memory, shouldn't be
        // too expensive to do repeatedly.
        Noneable<bool> get_tablet_mode_state();

        // Global hotkey press received
        void on_key_press(const OnboardOskEvent* event);

    public:
        DEFINE_SIGNAL(<bool>, tablet_mode_changed,
                      this, [&](){on_listeners_changed();});
        DEFINE_SIGNAL(<bool>, power_button_pressed,
                      this, [&](){on_listeners_changed();});

    private:
        std::unique_ptr<AcpidListener> m_acpid_listener;
        GlobalKeyListener* m_key_listener{};
        Noneable<bool> m_tablet_mode;
};

#endif // HARDWARESENSORTRACKER_H
