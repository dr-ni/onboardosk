
#include <map>

#include <libudev.h>
#include <glib.h>

#include "tools/container_helpers.h"
#include "tools/string_helpers.h"
#include "tools/logger.h"

#include "configuration.h"
#include "exception.h"
#include "udevtracker.h"


std::ostream& operator<<(std::ostream& out, const UDevDevice& d){
    out << "UDevDevice("
        << "id=" << d.id
        << ", name=" << d.name
        << ", usb_interface_num=" << d.usb_interface_num
        << ")";
    return out;
}

class UDevTracker::Impl
{
    public:

        Impl(UDevTracker* parent) :
            m_parent(parent)
        {
            m_udev = udev_new();
            if (m_udev == NULL)
                throw UDevException("failed to create UDev object (udev_new)");
        }

        ~Impl()
        {
            if (m_udev)
            {
                udev_unref(m_udev);
                m_udev = nullptr;
            }
        }

        void start_monitor()
        {
            m_input_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
            if (m_input_monitor)
            {
                GIOChannel *channel;
                int fd;

                udev_monitor_filter_add_match_subsystem_devtype(m_input_monitor,
                                                                "input", NULL);
                udev_monitor_enable_receiving(m_input_monitor);
                fd = udev_monitor_get_fd(m_input_monitor);

                // plug  udev fd into the glib mainloop machinery
                channel = g_io_channel_unix_new(fd);
                m_watch_source = g_io_create_watch(channel, G_IO_IN);
                g_io_channel_unref(channel);
                g_source_set_callback (m_watch_source,
                    reinterpret_cast<GSourceFunc>(on_udev_event), this, NULL);
                g_source_attach (m_watch_source,
                                 g_main_context_get_thread_default ());
                g_source_unref (m_watch_source);
            }
        }

        void stop_monitor()
        {
            if (m_watch_source)
            {
                g_source_destroy (m_watch_source);
                m_watch_source = NULL;
            }

            if (m_input_monitor)
            {
                udev_monitor_unref (m_input_monitor);
                m_input_monitor = NULL;
            }
        }


        static gboolean
        on_udev_event(GIOChannel* source, GIOCondition condition, gpointer data)
        {
            (void)source;
            (void)condition;
            Impl* this_ = reinterpret_cast<Impl*>(data);

            if (this_->m_input_monitor)
            {
                struct udev_device* device =
                    udev_monitor_receive_device (this_->m_input_monitor);
                if (device)
                {
                    const char* path = udev_device_get_devpath(device);
                    this_->m_parent->on_udev_event(path);
                }
            }

            return TRUE;
        }

        static const char* not_null(const char* c)
        {
            return c ? c : "";
        }

        using DeviceAttrs = std::vector<std::map<std::string, std::string>>;
        void get_keyboard_devices(DeviceAttrs& results)
        {

            struct udev_enumerate* enumerate = udev_enumerate_new(m_udev);
            udev_enumerate_add_match_subsystem(enumerate, "input");
            udev_enumerate_add_match_property(enumerate, "ID_INPUT_KEYBOARD", "1");
            udev_enumerate_scan_devices(enumerate);

            struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
            struct udev_list_entry *list_entry;
            udev_list_entry_foreach(list_entry, devices)
            {
                const char* path = udev_list_entry_get_name(list_entry);
                struct udev_device* device = udev_device_new_from_syspath(m_udev, path);
                if (udev_device_get_property_value(device, "PHYS"))
                {
                    auto it = results.emplace(results.end());
                    auto attrs = *it;

                    const char *str;
                    attrs["path"] = path;

                    str = not_null(udev_device_get_devnode(device));
                    attrs["devnode"] = str;

                    str = not_null(udev_device_get_sysname(device));
                    attrs["sysname"] = str;

                    str = not_null(udev_device_get_sysnum(device));
                    attrs["sysnum"] = str;

                    str = not_null(udev_device_get_syspath(device));
                    attrs["syspath"] = str;

                    str = not_null(udev_device_get_property_value(
                                         device, "NAME"));
                    attrs["NAME"] = str;

                    str = not_null(udev_device_get_property_value(
                                         device, "ID_BUS"));
                    attrs["ID_BUS"] = str;

                    str = not_null(udev_device_get_property_value(
                                         device, "ID_VENDOR_ID"));
                    attrs["ID_VENDOR_ID"] = str;

                    str = not_null(udev_device_get_property_value(
                                         device, "ID_MODEL_ID"));
                    attrs["ID_MODEL_ID"] = str;

                    str = not_null(udev_device_get_property_value(
                                         device, "ID_SERIAL"));
                    attrs["ID_SERIAL"] = str;

                    str = not_null(udev_device_get_property_value(
                                         device, "ID_USB_INTERFACE_NUM"));
                    attrs["ID_USB_INTERFACE_NUM"] = str;

                    str = not_null(udev_device_get_property_value(
                                         device, "ID_USB_INTERFACES"));
                    attrs["ID_USB_INTERFACES"] = str;
                }
            }
        }

    private:
        UDevTracker* m_parent{};
        struct udev* m_udev{};
        struct udev_monitor *m_input_monitor{};
        GSource* m_watch_source{};
        Noneable<bool> keyboard_device_detected;

        friend UDevTracker;
};



std::unique_ptr<UDevTracker> UDevTracker::make(const ContextBase& context)
{
    return std::make_unique<UDevTracker>(context);
}


UDevTracker::UDevTracker(const ContextBase& context) :
    Super(context),
    m_impl(std::make_unique<Impl>(this))
{
}

UDevTracker::~UDevTracker()
{
    register_listeners(false);
}

void UDevTracker::on_listeners_changed()
{
    update_event_sources();
    if (!keyboard_detection_changed.has_listeners())
        LOG_INFO << "all listeners disconnected";
}

void UDevTracker::register_udev_listeners(bool register_)
{
    if ((m_impl->m_udev != nullptr) != register_)
    {
        if (register_)
        {
            if (m_impl->m_udev)
            {
                m_impl->start_monitor();

                config()->auto_show->
                        keyboard_device_detection_exceptions.changed
                        .connect(this, [&](){detect_keyboard_devices();});

                update_keyboard_device_detected();
            }
        }
        else
        {
            config()->auto_show->
                    keyboard_device_detection_exceptions.changed
                    .disconnect(this);
            m_impl->stop_monitor();
            m_impl->m_udev = nullptr;
            m_impl->keyboard_device_detected.set_none();
        }
    }
}

void UDevTracker::on_udev_event(const char* device_path)
{
    LOG_DEBUG << "on_udev_event: " << device_path;
    detect_keyboard_devices();
}

void UDevTracker::detect_keyboard_devices()
{
    bool keyboard_detected_before = is_keyboard_device_detected();
    update_keyboard_device_detected();
    bool keyboard_detected = is_keyboard_device_detected();

    if (keyboard_detected != keyboard_detected_before)
    {
        LOG_DEBUG << "detect_keyboard_devices: " << keyboard_detected;
        keyboard_detection_changed.emit_async(keyboard_detected);
    }
}

void UDevTracker::update_keyboard_device_detected()
{
    Noneable<bool> detected;
    if (m_impl->m_udev)
    {
        auto keyboard_devices = get_keyboard_devices();
        LOG_DEBUG << "update_keyboard_device_detected: "
                  << "keyboard_devices=" << keyboard_devices;
        detected = false;
        for (auto& device : keyboard_devices)
        {
            if (!contains(config()->auto_show->keyboard_device_detection_exceptions.get(),
                          device.id))
            {
                detected = true;
                break;
            }
        }
    }

    LOG_DEBUG << "update_keyboard_device_detected: keyboard detected=" << repr(detected);
    m_impl->keyboard_device_detected = detected;
}

bool UDevTracker::is_keyboard_device_detected()
{
    return m_impl->keyboard_device_detected;
}

std::vector<UDevDevice> UDevTracker::get_keyboard_devices()
{
    if (m_impl->m_udev)
    {
        Impl::DeviceAttrs raw_devices;
        m_impl->get_keyboard_devices(raw_devices);
        LOG_DEBUG << "get_keyboard_devices:"
                  << " raw_devices=" << raw_devices;

        std::vector<UDevDevice> devices;
        for (auto& d : raw_devices)
        {
            UDevDevice device;
            std::string name = replace_all(d["NAME"], "\"", "");
            std::string serial = replace_all(d["ID_SERIAL"], "\"", "");

            device.id = d["ID_VENDOR_ID"] + ":" +
                        d["ID_MODEL_ID"] + ":" +
                        (serial.empty() ? serial :
                         replace_all(name, " ", "_"));
            device.name = name;
            device.usb_interface_num = to_int(d["ID_USB_INTERFACE_NUM"]);

            append_unique(devices, device);
        }

        return devices;
    }

    return {};
}

void UDevTracker::append_unique(std::vector<UDevDevice>& devices, const UDevDevice& device)
{
    for (size_t i=0; i<devices.size(); i++)
    {
        UDevDevice& d = devices[i];
        if (d.id == device.id)
        {
            if (device.usb_interface_num < d.usb_interface_num)
                devices[i] = device;
            return;
        }
    }

    devices.emplace_back(device);
}

