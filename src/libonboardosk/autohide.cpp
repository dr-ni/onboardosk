
#include "tools/logger.h"
#include "tools/string_helpers.h"

#include "autohide.h"
#include "configuration.h"
#include "globalkeylistener.h"
#include "keyboardview.h"
#include "onboardoskevent.h"
#include "udevtracker.h"


AutoHide::AutoHide(const ContextBase& context) :
    ContextBase(context)
{

}

AutoHide::~AutoHide()
{
    register_events(false);
}

void AutoHide::register_events(bool register_)
{
    if (register_)
    {
        if (!m_key_listener)
        {
            m_key_listener = get_global_key_listener();
            if (m_key_listener)
            {
                m_key_listener->key_press.connect(
                    this, [&](const OnboardOskEvent* event){on_key_press(event);});
                m_key_listener->devices_updated.connect(
                    this, [&](){on_devices_updated();});
            }
        }
    }
    else
    {
        if (m_key_listener)
        {
            m_key_listener->key_press.disconnect(this);
            m_key_listener->devices_updated.disconnect(this);
        }
        m_key_listener = nullptr;
    }
}

void AutoHide::on_devices_updated()
{
    if (config()->is_tablet_mode_detection_enabled())
    {
        auto udev_tracker = get_udev_tracker();
        m_udev_keyboard_devices = udev_tracker->get_keyboard_devices();
    }
    else
    {
        m_udev_keyboard_devices.clear();
    }

    LOG_DEBUG << "AutoHide._on_devices_updated(): "
              << get_keyboard_device_names();
}

void AutoHide::on_key_press(const OnboardOskEvent* event)
{
    if (config()->is_auto_hide_on_keypress_enabled())
    {

        if (logger()->can_log(LogLevel::INFO))
        {
            std::string s = m_key_listener->get_key_event_string(event);
            LOG_INFO << "on_key_press(): " << repr(s);
        }

        // Only react to "real" keyboard devices when tablet-mode detection
        // is enabled. Kernel drivers like ideapad-laptop can send hotkeys
        // when switching to/from tablet-mode. We want to leave these to the
        // tablet-mode detection in HardwareSensorTracker && !have them
        // interfere with auto-hide-on-keypress.
        if (!config()->is_tablet_mode_detection_enabled() ||
            is_real_keyboard_event(event))
        {
            // no auto-hide for hotkeys configured for tablet-mode detection
            int enter_keycode = config()->auto_show->tablet_mode_enter_key;
            int leave_keycode = config()->auto_show->tablet_mode_leave_key;
            if (event->keycode != enter_keycode &&
                event->keycode != leave_keycode)
            {
                double duration = config()->auto_show->hide_on_key_press_pause;
                get_keyboard_view()->auto_show_lock_and_hide(this->LOCK_REASON,
                                                             duration);
            }
        }
    }
}

bool AutoHide::is_auto_show_locked()
{
    return get_keyboard_view()->is_auto_show_locked(this->LOCK_REASON);
}

void AutoHide::auto_show_unlock()
{
    get_keyboard_view()->auto_show_unlock(this->LOCK_REASON);
}

bool AutoHide::is_real_keyboard_event(const OnboardOskEvent* event)
{
    bool result = true;

    std::string source_device_name = lower(event->source_device_name);

    if (!m_udev_keyboard_devices.empty())
    {
        result = false;
        for (const auto& udevice : m_udev_keyboard_devices)
        {
            if (source_device_name == lower(udevice.name))
            {
                result = true;
                break;
            }
        }
    }

    LOG_DEBUG << "is_real_keyboard_event(): "
              << "xidevice=" << repr(source_device_name)
              << ", udevdevices=" << get_keyboard_device_names()
              << ", result=" << repr(result);

    return result;
}

std::vector<std::string> AutoHide::get_keyboard_device_names()
{
    std::vector<std::string> names;
    for (auto& device : m_udev_keyboard_devices)
        names.emplace_back(device.name);
    return names;
}

