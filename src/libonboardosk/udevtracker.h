#ifndef UDEVTRACKER_H
#define UDEVTRACKER_H

#include <ostream>
#include <vector>

#include "tools/noneable.h"

#include "signalling.h"


class UDevDevice
{
    public:
        std::string id;
        std::string name;
        int usb_interface_num{0};
};
std::ostream& operator<<(std::ostream& out, const UDevDevice& d);


class UDevTracker : public ContextBase
{
    public:
        using Super = ContextBase;
        UDevTracker(const ContextBase& context);
        virtual ~UDevTracker();

        static std::unique_ptr<UDevTracker> make(const ContextBase& context);

        // Return value:
        // true = one or more keyboard devices detected
        // false = no keyboard device detected
        // None = unknown
        bool is_keyboard_device_detected();

        std::vector<UDevDevice> get_keyboard_devices();

    private:
        void update_event_sources()
        {
            bool register_ = keyboard_detection_changed.has_listeners();
            register_udev_listeners(register_);
        }

        void register_listeners(bool register_)
        {
            register_udev_listeners(register_);
        }

        void register_udev_listeners(bool register_);

        // monitor udev events
        void update_keyboard_device_detected();

    private:
        void on_listeners_changed();
        void on_udev_event(const char* device_path);
        void detect_keyboard_devices();

        // For some devices osk_udev returns duplicate entries for different USB
        // interfaces. Filter those out and keep only USB interface 0.
        void append_unique(std::vector<UDevDevice>& devices, const UDevDevice& device);

    public:
        DEFINE_SIGNAL(<bool>, keyboard_detection_changed,
                      this, [&](){on_listeners_changed();});

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
};


#endif // UDEVTRACKER_H
