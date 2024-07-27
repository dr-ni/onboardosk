/*
 * Copyright © 2011-2012 Gerd Kohlberger <lowfi@chello.at>
 * Copyright © 2011-2017 marmuta <marmvta@gmail.com>
 *
 * This file is part of Onboard.
 *
 * Onboard is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Onboard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <iomanip>
#include <memory>
#include <string.h>
#include <cstring>

#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>

#include "tools/container_helpers.h"
#include "tools/glib_helpers.h"
#include "tools/logger.h"
#include "tools/string_helpers.h"
#include "tools/thread_helpers.h"

#include "configuration.h"
#include "exception.h"
#include "inputeventsourcexinput.h"
#include "onboardoskevent.h"
#include "layoutview.h"


#define XI_PROP_PRODUCT_ID "Device Product ID"

static unsigned int gdk_button_masks[] = {GDK_BUTTON1_MASK,
                                          GDK_BUTTON2_MASK,
                                          GDK_BUTTON3_MASK,
                                          GDK_BUTTON4_MASK,
                                          GDK_BUTTON5_MASK};

typedef int InputDeviceId;

// gdk_error_trap_push and friends don't seem to be thread safe, as
// Gnome-shell crashes a lot when our X message loop runs in a thread.
// This class does better.
class TrapError
{
    public:
        TrapError(Display* display) :
            m_display(display)
        {
            m_error_display = nullptr;
            std::memset(&m_error, 0, sizeof(m_error));
            //m_old_error_handler = XSetErrorHandler(error_handler);
        }
        ~TrapError()
        {
            //XSetErrorHandler(m_old_error_handler);
        }
        static int error_handler(Display* display, XErrorEvent* error)
        {
            m_error_display = display;
            m_error.error_code = error->error_code;
            return 0;
        }

        unsigned char get_error()
        {
            return {};
            if (m_error_display == m_display)
                return m_error.error_code;
            else
                return {};
        }

        static Display* m_error_display;
        static XErrorEvent m_error;

        int (*m_old_error_handler) (Display *, XErrorEvent *){};
        Display* m_display{};
};

Display* TrapError::m_error_display{};
XErrorEvent TrapError::m_error{};


enum InputDeviceTouchMode
{
    NoTouch = 0,
    DirectTouch = 1,     // = XIDirectTouch
    DependentTouch = 2   // = XIDependentTouch
};

class InputDevice
{
    public:
        using Ptr = std::shared_ptr<InputDevice>;

        // Get a configuration string for the device.
        // Format: VID:PID:USE
        std::string get_config_string()
        {
            using namespace std;
            return sstr()
                << setfill('0') << setw(4) << hex << this->vid << ":"
                << setfill('0') << setw(4) << hex << this->pid << ":"
                << this->use;
        }

        // Touch screen device?
         bool is_touch_screen()
         {
             return this->type == ONBOARD_OSK_TOUCHSCREEN_DEVICE;
         }

         // methods inherited from Gerd's scanner device.
         // Is this a master device?
         bool is_master()
         {
             return this->use == XIMasterPointer ||
                    this->use == XIMasterKeyboard;
         }

         // Is this device a pointer?
         bool is_pointer()
         {
             return this->use == XIMasterPointer ||
                    this->use == XISlavePointer;
         }

         // Is this device a keyboard?
         bool is_keyboard()
         {
             return this->use == XIMasterKeyboard ||
                    this->use == XISlaveKeyboard;
         }

         // Is this device detached?
         bool is_floating()
         {
             return this->use == XIFloatingSlave;
         }

    public:
        InputDeviceId device_id;
        std::string name;
        int use;
        int attachment;
        bool enabled;
        InputDeviceTouchMode touch_mode;
        unsigned int vid;
        unsigned int pid;
        OnboardOskDeviceType type;
};
typedef InputDevice::Ptr InputDevicePtr;


class InputEventSourceXInput : public InputEventSource
{
    public:
        using Super = InputEventSource;
        InputEventSourceXInput(const ContextBase& context, InputEventSourceSink* sink);
        virtual ~InputEventSourceXInput();

        bool select_events(Window win, InputDeviceId device_id,
                           unsigned long event_mask);
        bool unselect_events(Window win, InputDeviceId device_id);

        void get_devices(std::vector<InputDevicePtr>& devices);

        bool attach(InputDeviceId device_id, InputDeviceId master_id);
        bool detach(InputDeviceId device_id);

        bool grab_device_id(InputDeviceId device_id, Window win);
        bool ungrab_device_id(InputDeviceId device_id);
        bool is_grabbed(const InputDevicePtr& device);

        InputDeviceId get_client_pointer_id(Window win);

        virtual void lock() override;
        virtual void unlock() override;

        void select_slave_pointers_events(Window win=None);

    protected:
        virtual void on_input_source_event(OnboardOskEvent* event) override;

    private:
        bool get_device_info(InputDevice* device, InputDeviceId device_id);
        void run_message_loop();
        int select(Window win, int id,
                   unsigned char* mask, int mask_len);

        static GdkFilterReturn event_filter_func(GdkXEvent* gdk_xevent,
                                                 GdkEvent* gdk_event,
                                                 InputEventSourceXInput* this_);
        GdkFilterReturn event_filter(GdkXEvent *gdk_xevent,
                                     GdkEvent *gdk_event);

        bool get_product_id(int id, unsigned int* vendor_id, unsigned int* product_id);
        InputDeviceTouchMode get_touch_mode(XIAnyClassInfo** classes, int num_classes);

        OnboardOskEventType translate_event_type(int xi_type);
        OnboardOskDeviceType classify_source(const char* name, int use, int touch_mode);
        void init_source_device(OnboardOskEvent& ooe, InputDeviceId source_id);
        bool init_event(OnboardOskEvent& ooe, XIEvent* xievent);
        bool handle_event(XIEvent* xievent);
        unsigned long translate_keycode(int keycode, XIGroupState* group, XIModifierState* mods);
        OnboardOskStateMask get_master_state();
        OnboardOskStateMask get_current_state();
        void update_state(XIDeviceEvent* event);

        void update_devices();
        InputDevicePtr find_device_from_id(InputDeviceId device_id);
        InputDevicePtr get_client_pointer(Window win);

        // get the first master pointer
        InputDevicePtr get_master_pointer();

        // All slaves of the client pointer, with and without device grabs.
        void get_client_pointer_slaves(Window win, std::vector<InputDevicePtr>& devices);

        // Slaves that are currently attached to the client pointer.
        void get_client_pointer_attached_slaves(Window win, std::vector<InputDevicePtr>& devices);

        ViewBase* find_top_level(OnboardOskEvent* event);
    private:
        Display  *m_display{};
        int       m_xi2_opcode{};
        Atom      m_atom_product_id{};

        int       m_button_states[G_N_ELEMENTS(gdk_button_masks)]{};
        ThreadFd  m_thread;

        std::vector<InputDevicePtr> m_devices;
        std::vector<InputDevicePtr> m_slave_devices;
        InputDevicePtr m_master_device;
        std::vector<InputDeviceId> m_grabbed_device_ids;
};

std::unique_ptr<InputEventSource> InputEventSource::make_xinput(const ContextBase& context,
                                                                InputEventSourceSink* sink)
{
    return std::make_unique<InputEventSourceXInput>(context, sink);
}

InputEventSourceXInput::InputEventSourceXInput(const ContextBase& context,
                                               InputEventSourceSink* sink) :
    Super(context, sink)
{
    int event, error;
    int major = 2;
    int minor = 2;
    Status status;

    XInitThreads();

    /* set display before anything else! */
    //GdkDisplay* display = gdk_display_get_default ();
    //if (!GDK_IS_X11_DISPLAY (display)) // Wayland, MIR?
    //    throw Exception("not an X display");
    //m_display = GDK_DISPLAY_XDISPLAY (display);

    m_display = XOpenDisplay(nullptr);
    if (!m_display)
        throw Exception("not an X display");

    Locker<InputEventSource> locker(this);

    memset(m_button_states, 0, sizeof(m_button_states));

    if (!XQueryExtension (m_display, "XInputExtension",
                          &m_xi2_opcode, &event, &error))
        throw Exception("XQueryExtension failed: no XInputExtension");

    // XIQueryVersion fails with X error BadValue if this isn't
    // the client's very first call. Someone, probably GTK is
    // successfully calling it before us, so just ignore the
    // error and move on.
    {
        TrapError trap(m_display);
        status = XIQueryVersion (m_display, &major, &minor);
    }
    if (status == BadRequest)
    {
        throw Exception("XQueryVersion failed: no XInput2 available");
    }
    if (major * 1000 + minor < 2002)
    {
        throw Exception(sstr()
            << "XQueryVersion failed: XInput 2.2 is not supported "
            << "(found " << major << "." << minor << ")");
    }

    unsigned char mask[2] = { 0, 0 };
    XISetMask (mask, XI_HierarchyChanged);
    select (0, XIAllDevices, mask, sizeof (mask));

    //gdk_window_add_filter (NULL,
    //                       reinterpret_cast<GdkFilterFunc>(event_filter_func),
    //                       this);

    m_atom_product_id = XInternAtom(m_display, XI_PROP_PRODUCT_ID, False);

    //select_events(None, XIAllDevices, XI_ButtonPressMask|XI_ButtonReleaseMask|XI_EnterMask|XI_LeaveMask|XI_MotionMask);

    update_devices();

    int display_fd = ConnectionNumber(m_display);
    try
    {
        m_thread.start(display_fd, [this](){run_message_loop();});
        m_thread.set_name("InputEventSourceXInput");
    }
    catch (const std::runtime_error& ex)
    {
        throw VirtKeyException(std::string("failed to start thread: ") +
                               ex.what());
    }
}

InputEventSourceXInput::~InputEventSourceXInput()
{
    unsigned char mask[2] = { 0, 0 };

    select (0, XIAllDevices, mask, sizeof (mask));

    //gdk_window_remove_filter (NULL,
    //                          reinterpret_cast<GdkFilterFunc>(event_filter_func),
    //                          this);
    m_thread.stop();

    if (m_display)
        XCloseDisplay(m_display);
}

void InputEventSourceXInput::run_message_loop()
{
    LOG_INFO << "thread start";

    try
    {
        XFlush(m_display);
        while (!m_thread.is_stop_requested())
        {
            // Must process events before waiting on the
            // fd the first time. Else the fd never wakes up
            // select in wait_for_data().
            while(true)
            {
                Locker<InputEventSource> locker(this);
                //std::lock_guard<std::recursive_mutex> locker(m_mutex);
                if (!XPending(m_display))
                    break;

                XEvent event;
                XNextEvent(m_display, &event);
                event_filter(reinterpret_cast<GdkXEvent*>(&event), nullptr);
            }

            if (!m_thread.wait_for_data())  // throws runtime_error
                break;
        }
    }
    catch (const std::runtime_error& ex)
    {
        LOG_ERROR << "aborting thread: " << ex.what();
    }

    LOG_INFO << "thread exit";
}

// Selects XInput events for a device. The device will send the selected
// events to the event_handler.
bool InputEventSourceXInput::select_events(Window win, InputDeviceId device_id,
                                           unsigned long event_mask)
{
    unsigned char mask[4] = { 0, 0, 0, 0};
    int nbits = std::min(sizeof(event_mask), sizeof(mask)) * 8;
    for (int i = 0; i < nbits; i++)
    {
        if (event_mask & 1<<i)
            XISetMask (mask, i);
    }

    return select(win, device_id, mask, sizeof(mask)) == 0;
}

// "Closes" a device.
bool InputEventSourceXInput::unselect_events(Window win, InputDeviceId device_id)
{
    unsigned char mask[1] = { 0 };
    if (select(win, device_id, mask, sizeof(mask)) != 0)
        return false;
    return true;
}

int InputEventSourceXInput::select(Window win, int id,
                                   unsigned char *mask,
                                   int mask_len)
{
    Locker<InputEventSource> locker(this);
    //std::lock_guard<std::recursive_mutex> locker(m_mutex);

    XIEventMask events;

    events.deviceid = id;
    events.mask_len = mask_len;
    events.mask = mask;

    if (win == None)
        win = DefaultRootWindow (m_display);

    {
        TrapError trap(m_display);
        XISelectEvents (m_display, win, &events, 1);
        XFlush(m_display);

        return trap.get_error();
    }
    return 0;
}

GdkFilterReturn InputEventSourceXInput::event_filter_func(
    GdkXEvent *gdk_xevent,
    GdkEvent *gdk_event,
    InputEventSourceXInput* this_)
{
    return this_->event_filter(gdk_xevent, gdk_event);
}

GdkFilterReturn InputEventSourceXInput::event_filter(GdkXEvent* gdk_xevent,
                                                     GdkEvent* gdk_event)
{
    (void)gdk_event;

    XGenericEventCookie *cookie =
            &(reinterpret_cast<XEvent*>(gdk_xevent))->xcookie;

    if (cookie->type == GenericEvent &&
        cookie->extension == m_xi2_opcode)
    {
        if (XGetEventData(m_display, cookie))
        {
            auto event = reinterpret_cast<XIEvent*>(cookie->data);

            //printf("event %d\n", cookie->evtype);
            //XIDeviceEvent *e = cookie->data;
            //printf("device %d evtype %d type %d  detail %d win %d\n", event->deviceid, evtype, e->type, e->detail, (int)e->event);

            handle_event(event);
        }
    }

    return GDK_FILTER_CONTINUE;
}

bool InputEventSourceXInput::get_product_id (
    int           id,
    unsigned int *vendor_id,
    unsigned int *product_id)
{
    Status         rc;
    Atom           act_type;
    int            act_format;
    unsigned long  nitems, bytes;
    unsigned char *data;

    *vendor_id  = 0;
    *product_id = 0;

    {
        TrapError trap(m_display);
        rc = XIGetProperty (m_display, id, m_atom_product_id,
                            0, 2, False, XA_INTEGER,
                            &act_type, &act_format, &nitems, &bytes, &data);
    }

    if (rc == Success && nitems == 2 && act_format == 32)
    {
        guint32* data32 = reinterpret_cast<guint32*>(data);

        *vendor_id  = *data32;
        *product_id = *(data32 + 1);

        XFree (data);

        return True;
    }

    return False;
}

InputDeviceTouchMode InputEventSourceXInput::get_touch_mode (XIAnyClassInfo **classes,
                                            int num_classes)
{
    for (int i = 0; i < num_classes; i++)
    {
        XITouchClassInfo* class_ = reinterpret_cast<XITouchClassInfo*> (classes[i]);
        if (class_->type == XITouchClass)
        {
            if (class_->num_touches)
            {
                if (class_->mode == XIDirectTouch)
                    return InputDeviceTouchMode::DirectTouch;
                if (class_->mode == XIDependentTouch)
                    return InputDeviceTouchMode::DependentTouch;
            }
        }
    }

    return InputDeviceTouchMode::NoTouch;
}


// Get a list of all input devices of the system.
void InputEventSourceXInput::get_devices(std::vector<InputDevicePtr>& devices)
{
    Locker<InputEventSource> locker(this);
    //std::lock_guard<std::recursive_mutex> locker(m_mutex);
    int n_devices;

    std::unique_ptr<XIDeviceInfo, decltype (&XIFreeDeviceInfo)> device_infos =
        {XIQueryDevice(m_display, XIAllDevices, &n_devices),
         XIFreeDeviceInfo};

    for (int i=0; i < n_devices; i++)
    {
        XIDeviceInfo* device_info = device_infos.get() + i;

        unsigned int vid, pid;
        get_product_id(device_info->deviceid, &vid, &pid);

        auto device = std::make_shared<InputDevice>();
        device->name = device_info->name;
        device->device_id = device_info->deviceid;
        device->use = device_info->use;
        device->attachment = device_info->attachment;
        device->enabled = device_info->enabled;
        device->vid = vid;
        device->pid = pid;
        device->touch_mode = get_touch_mode(device_info->classes,
                                            device_info->num_classes);
        device->type = classify_source(device_info->name,
                                       device_info->use,
                                       device->touch_mode);
        devices.emplace_back(device);
    }
}

bool InputEventSourceXInput::get_device_info(InputDevice* device,
                                             InputDeviceId device_id)
{
    int n_devices;

    TrapError trap(m_display);
    std::unique_ptr<XIDeviceInfo, decltype (&XIFreeDeviceInfo)> device_infos =
        {XIQueryDevice(m_display, device_id, &n_devices),
         XIFreeDeviceInfo};
    XFlush(m_display);
    if (trap.get_error())
        return false;   // invalid device_id

    XIDeviceInfo* device_info = device_infos.get();

    unsigned int vid, pid;
    get_product_id(device_info->deviceid, &vid, &pid);

    device->name = device_info->name;
    device->device_id = device_info->deviceid;
    device->use = device_info->use;
    device->attachment = device_info->attachment;
    device->enabled = device_info->enabled;
    device->vid = vid;
    device->pid = pid;
    device->touch_mode = get_touch_mode(device_info->classes,
                                        device_info->num_classes);
    return true;
}

// Attaches the device <device_id> to master device <master_id>.
bool InputEventSourceXInput::attach(InputDeviceId device_id,
                                                InputDeviceId master_id)
{
    Locker<InputEventSource> locker(this);
    //std::lock_guard<std::recursive_mutex> locker(m_mutex);

    XIAttachSlaveInfo info;
    info.type = XIAttachSlave;
    info.deviceid = device_id;
    info.new_master = master_id;

    TrapError trap(m_display);
    XIChangeHierarchy (m_display,
                       reinterpret_cast<XIAnyHierarchyChangeInfo*>(&info), 1);
    XFlush(m_display);

    if (trap.get_error())
        return false;   // failed to attach device
    return true;
}

// Detaches an input device for its master. Detached devices
// stop sending "core events".
bool InputEventSourceXInput::detach(InputDeviceId device_id)
{
    Locker<InputEventSource> locker(this);
    //std::lock_guard<std::recursive_mutex> locker(m_mutex);

    XIDetachSlaveInfo info;
    info.type = XIDetachSlave;
    info.deviceid = device_id;

    TrapError trap(m_display);
    XIChangeHierarchy (m_display,
                       reinterpret_cast<XIAnyHierarchyChangeInfo*>(&info), 1);
    XFlush(m_display);

    if (trap.get_error())
        return false;   // failed to detach device
    return true;
}

// Grabs the device with <device_id>.
bool InputEventSourceXInput::grab_device_id(InputDeviceId device_id,
                                                     Window win)
{
    Locker<InputEventSource> locker(this);
    //std::lock_guard<std::recursive_mutex> locker(m_mutex);

    unsigned char mask[1] = {0};
    XIEventMask events;

    Status status;
    gint error;

    if (!win)
        win = DefaultRootWindow (m_display);

    events.deviceid = device_id;
    events.mask = mask;
    events.mask_len = sizeof(mask);

    TrapError trap(m_display);
    status = XIGrabDevice(m_display, device_id, win, CurrentTime, None,
                          XIGrabModeSync, XIGrabModeAsync,
                          True, &events);
    error = trap.get_error();
    if (status != Success || error)
    {
        // << "failed to grab device" << device_id
        // << "(" << status << ", " << error << ")";
        return false;
    }
    return true;
}

// Ungrab the device <device_id>.
bool InputEventSourceXInput::ungrab_device_id(InputDeviceId device_id)
{
    Locker<InputEventSource> locker(this);
    //std::lock_guard<std::recursive_mutex> locker(m_mutex);
    Status status;
    gint error{};

    TrapError trap(m_display);
    status = XIUngrabDevice(m_display, device_id, CurrentTime);
    error = trap.get_error();

    if (status != Success || error)
    {
        // << "failed to ungrab device" << device_id
        // << "(" << status << ", " << error << ")";
        return false;
    }
    return true;
}

bool InputEventSourceXInput::is_grabbed(const InputDevicePtr& device)
{
    return contains(m_grabbed_device_ids, device->device_id);
}

InputDeviceId InputEventSourceXInput::get_client_pointer_id (Window win)
{
    Locker<InputEventSource> locker(this);
    //std::lock_guard<std::recursive_mutex> locker(m_mutex);
    int device_id = 0;
    XIGetClientPointer(m_display, win, &device_id);
    return device_id;
}

void InputEventSourceXInput::lock()
{
    XLockDisplay(m_display);
}

void InputEventSourceXInput::unlock()
{
    XUnlockDisplay(m_display);
}

bool InputEventSourceXInput::handle_event(XIEvent* xievent)
{
    OnboardOskEvent ooe{};
    if (init_event(ooe, xievent))
    {
        queue_event(nullptr, &ooe);
        return true;
    }
    return false;
}

bool InputEventSourceXInput::init_event(OnboardOskEvent& ooe, XIEvent* xievent)
{
    bool handled = true;
    auto xitype = xievent->evtype;
    ooe.type = translate_event_type(xitype);
    switch (xitype)
    {
        case XI_Enter:
        case XI_Leave:
        {
            XIEnterEvent* event = reinterpret_cast<XIEnterEvent*>(xievent);

            ooe.type = translate_event_type(xitype);
            ooe.x = event->event_x;
            ooe.y = event->event_y;
            ooe.x_root = event->root_x;
            ooe.y_root = event->root_y;
            ooe.button = 0;
            ooe.state = get_master_state();
            ooe.sequence_id = 0;

            ooe.sequence_id = 0;
            init_source_device(ooe, event->sourceid);
            ooe.time = xievent->time;

            break;
        }

        case XI_Motion:
        case XI_ButtonPress:
        case XI_ButtonRelease:
        case XI_TouchBegin:
        case XI_TouchUpdate:
        case XI_TouchEnd:
        {
            XIDeviceEvent *event = reinterpret_cast<XIDeviceEvent*>(xievent);
            unsigned int button = 0;
            unsigned int sequence = 0;

            if (xitype == XI_Motion ||
                xitype == XI_ButtonPress ||
                xitype == XI_ButtonRelease)
                button = event->detail;

            if (xitype == XI_TouchBegin ||
                xitype == XI_TouchUpdate ||
                xitype == XI_TouchEnd)
                sequence = event->detail;

            update_state(event);

            ooe.type = translate_event_type(xitype);
            ooe.x = event->event_x;
            ooe.y = event->event_y;
            ooe.x_root = event->root_x;
            ooe.y_root = event->root_y;
            ooe.button = button;
            ooe.state = get_current_state();
            ooe.sequence_id = sequence;

            init_source_device(ooe, event->sourceid);
            ooe.time = xievent->time;

            break;
        }

        case XI_HierarchyChanged:
        {
            update_devices();
            auto event = reinterpret_cast<XIHierarchyEvent*>(xievent);

            if (event->flags & (XISlaveAdded |
                                XISlaveRemoved |
                                XISlaveAttached |
                                XISlaveDetached))
            {
                for (int i = 0; i < event->num_info; i++)
                {
                    XIHierarchyInfo* info = &event->info[i];

                    //ooe.display = display;
                    ooe.device_id = info->deviceid;

                    if (info->flags & XISlaveAdded)
                        ooe.type = ONBOARD_OSK_EVENT_DEVICE_ADDED;
                    else if (info->flags & XISlaveRemoved)
                        ooe.type = ONBOARD_OSK_EVENT_DEVICE_REMOVED;
                    else if (info->flags & XISlaveAttached)
                        ooe.type = ONBOARD_OSK_EVENT_SLAVE_ATTACHED;
                    else if (info->flags & XISlaveDetached)
                        ooe.type = ONBOARD_OSK_EVENT_SLAVE_DETACHED;
                }
            }
            break;
        }

        case XI_DeviceChanged:
        {
            update_devices();

            auto event = reinterpret_cast<XIDeviceChangedEvent*>(xievent);
            if (event->reason == XISlaveSwitch)
            {
                ooe.device_id = event->deviceid;
                init_source_device(ooe, event->sourceid);
                ooe.type = ONBOARD_OSK_EVENT_DEVICE_CHANGED;
            }
            break;
        }

        case XI_KeyPress:
        {
            auto event = reinterpret_cast<XIDeviceEvent*>(xievent);

            if (!(event->flags & XIKeyRepeat))
            {
                int keycode = event->detail;
                int keyval = translate_keycode(keycode,
                                               &event->group,
                                               &event->mods);
                if (keyval)
                {
                    // ooe.display = event->display;
                    ooe.type = ONBOARD_OSK_EVENT_KEY_PRESS;
                    ooe.device_id = event->deviceid;
                    init_source_device(ooe, event->sourceid);
                    ooe.keycode = keycode;
                    ooe.keysym = keyval;
                }
            }
            break;
        }

        case XI_KeyRelease:
        {
            auto event = reinterpret_cast<XIDeviceEvent*>(xievent);

            if (!(event->flags & XI_KeyRelease))
            {
                int keycode = event->detail;
                int keyval = translate_keycode(keycode,
                                               &event->group,
                                               &event->mods);
                if (keyval)
                {
                    // ooe.display = event->display;
                    ooe.type = ONBOARD_OSK_EVENT_KEY_RELEASE;
                    ooe.device_id = event->deviceid;
                    init_source_device(ooe, event->sourceid);
                    ooe.keycode = keycode;
                    ooe.keysym = keyval;
                }
            }
            break;
        }

        default:
            handled = false;
    }
    return handled;
}

void InputEventSourceXInput::init_source_device(OnboardOskEvent& ooe, InputDeviceId source_id)
{
    auto device = find_device_from_id(source_id);
    if (device)
    {
        ooe.source_device_id = source_id;
        std::strncpy(ooe.source_device_name,
                     device->name.c_str(),
                     sizeof(ooe.source_device_name));
        ooe.source_device_type = device->type;
    }
}

// Determine the source type (Gdk.InputSource) of the device.
// Logic taken from GDK, gdk/x11/gdkdevicemanager-xi2.c
OnboardOskDeviceType InputEventSourceXInput::classify_source(const char* name, int use, int touch_mode)
{
    OnboardOskDeviceType device_type{ONBOARD_OSK_UNKNOWN_DEVICE};

    if (use == XIMasterKeyboard ||
        use == XISlaveKeyboard)
    {
        device_type = ONBOARD_OSK_KEYBOARD_DEVICE;
    }
    else if (use == XISlavePointer &&
             touch_mode)
    {
        if (touch_mode == XIDirectTouch)
            device_type = ONBOARD_OSK_TOUCHSCREEN_DEVICE;
        else
            device_type = ONBOARD_OSK_TOUCHPAD_DEVICE;
    }
    else
    {
        auto nm = lower(name);
        if (contains(nm, "eraser"))
            device_type = ONBOARD_OSK_ERASER_DEVICE;
        else if (contains(nm, "cursor"))
            device_type = ONBOARD_OSK_CURSOR_DEVICE;
        else if (contains(nm, "wacom") ||
                 contains(nm, "pen"))   // uh oh, false positives?
            device_type = ONBOARD_OSK_PEN_DEVICE;
        else
            device_type = ONBOARD_OSK_POINTER_DEVICE;
    }
    return device_type;
}

// Translate XInput event type to GDK event type.
OnboardOskEventType InputEventSourceXInput::translate_event_type (int xi_type)
{
    OnboardOskEventType type;

    switch (xi_type)
    {
        case XI_Motion:
        case XI_RawMotion:
            type = ONBOARD_OSK_EVENT_MOTION; break;
        case XI_ButtonPress:
        case XI_RawButtonPress:
            type = ONBOARD_OSK_EVENT_BUTTON_PRESS; break;
        case XI_ButtonRelease:
        case XI_RawButtonRelease:
            type = ONBOARD_OSK_EVENT_BUTTON_RELEASE; break;
        case XI_Enter:
            type = ONBOARD_OSK_EVENT_ENTER; break;
        case XI_Leave:
            type = ONBOARD_OSK_EVENT_LEAVE; break;
        case XI_TouchBegin:
        case XI_RawTouchBegin:
            type = ONBOARD_OSK_EVENT_TOUCH_BEGIN; break;
        case XI_TouchUpdate:
        case XI_RawTouchUpdate:
            type = ONBOARD_OSK_EVENT_TOUCH_UPDATE; break;
        case XI_TouchEnd:
        case XI_RawTouchEnd:
            type = ONBOARD_OSK_EVENT_TOUCH_END; break;

        default: type = ONBOARD_OSK_EVENT_NONE; break;
    }
    return type;
}

unsigned long InputEventSourceXInput::translate_keycode(
    int keycode,
    XIGroupState* group,
    XIModifierState* mods)
{
    unsigned int keyval = 0;

    gdk_keymap_translate_keyboard_state (gdk_keymap_get_default (),
                                         keycode,
                                         static_cast<GdkModifierType>(mods->effective),
                                         group->effective,
                                         &keyval, NULL, NULL, NULL);
    return keyval;
}

// Translate XInput state into GDK event state.
static OnboardOskStateMask
translate_state (XIModifierState *mods_state,
                 XIButtonState   *button_state,
                 XIGroupState    *group_state)
{
    unsigned int state = 0;

    if (mods_state)
        state = mods_state->effective;

    if (button_state)
    {
        int n = MIN ((int)G_N_ELEMENTS(gdk_button_masks), button_state->mask_len * 8);
        for (int i = 0; i < n; i++)
            if (XIMaskIsSet (button_state->mask, i))
                state |= gdk_button_masks[i];
    }

    if (group_state)
        state |= (group_state->effective) << 13;
    return static_cast<OnboardOskStateMask>(state);
}

// Get Gdk event state of the master pointer.
//
// The master aggregates currently pressed buttons and key presses from all
// slave devices.
//
// Why we need aggregate button presses:
// Francesco uses one pointing device for button presses and a different one
// for motion events. The motion slave doesn't know about the button
// slave's state. We can either aggregate the button state over all slaves
// ourselves or rely on the master to do it for us.
OnboardOskStateMask InputEventSourceXInput::get_master_state()
{
    Window          win = DefaultRootWindow (m_display);
    Window          root;
    Window          child;
    double          root_x;
    double          root_y;
    double          win_x;
    double          win_y;
    XIButtonState   buttons;
    XIModifierState mods;
    XIGroupState    group;
    OnboardOskStateMask state{};

    int master_id = 0;
    XIGetClientPointer(m_display, None, &master_id);

    TrapError trap(m_display);
    XIQueryPointer(m_display,
                   master_id,
                   win,
                   &root,
                   &child,
                   &root_x,
                   &root_y,
                   &win_x,
                   &win_y,
                   &buttons,
                   &mods,
                   &group);
    if (!trap.get_error())
    {
        state = translate_state(&mods, &buttons, &group);
    }

    return state;
}

// Get current GDK event state.
OnboardOskStateMask InputEventSourceXInput::get_current_state()
{
    gsize i;

    // Get out-of-sync master state, for key state mainly.
    // Button state will be out-dated immediately before or after
    // button press/release events.
    unsigned int state = get_master_state();

    // override button state with what we collected in-sync
    // -> no spurious stuck keys due to erroneous state in
    // motion events.
    for (i = 0; i < G_N_ELEMENTS(gdk_button_masks); i++)
    {
        int mask = gdk_button_masks[i];
        state &= ~mask;
        if (m_button_states[i] > 0)
            state |= mask;
    }

    return static_cast<OnboardOskStateMask>(state);
}

// Keep track of button state changes in sync with the events we receive.
void InputEventSourceXInput::update_state(XIDeviceEvent* event)
{
    int button = event->detail;
    if (button >= 1 && button < (int)G_N_ELEMENTS(m_button_states))
    {
        int* count = m_button_states + (button-1);
        if (event->evtype == XI_ButtonPress)
            (*count)++;
        if (event->evtype == XI_ButtonRelease)
        {
            (*count)--;

            // some protection at least against initially pressed buttons
            if (*count < 0)
                *count = 0;
        }
    }
}

void InputEventSourceXInput::update_devices()
{
    m_devices.clear();
    get_devices(m_devices);
    select_slave_pointers_events();
}

InputDevicePtr InputEventSourceXInput::find_device_from_id(InputDeviceId device_id)
{
    for (auto& device : m_devices)
        if (device->device_id == device_id)
            return device;
    return {};
}

InputDevicePtr InputEventSourceXInput::get_client_pointer(Window win)
{
    InputDeviceId id = get_client_pointer_id(win);
    return find_device_from_id(id);
}

InputDevicePtr InputEventSourceXInput::get_master_pointer()
{
    for (auto& d : m_devices)
        if (d->is_master() &&
            d->is_pointer())
            return d;
    return {};
}

// All slaves of the client pointer, with and without device grabs.
void InputEventSourceXInput::get_client_pointer_slaves(
        Window win,
        std::vector<InputDevicePtr>& devices)
{
    auto client_pointer = get_client_pointer(win);
    if (!client_pointer)
        client_pointer = get_master_pointer();
    if (client_pointer)
    {
        for (auto& d : m_devices)
            if (d->is_pointer() &&
                !d->is_master() &&
                d->attachment == client_pointer->device_id)
                devices.emplace_back(d);
    }
}

void InputEventSourceXInput::get_client_pointer_attached_slaves(
        Window win,
        std::vector<InputDevicePtr>& devices)
{
    std::vector<InputDevicePtr> ds;
    get_client_pointer_slaves(win, ds);

    for (auto& device : ds)
        if (!device->is_floating() &&
            !is_grabbed(device))
            devices.emplace_back(device);
}

void InputEventSourceXInput::select_slave_pointers_events(Window win)
{
    // Select events of the master pointer.
    // Enter/leave events aren't supported by the slaves.
    int event_mask = XI_EnterMask |
                     XI_LeaveMask;
    InputDevicePtr device = get_client_pointer(win);
    if (!device)
    {
        LOG_INFO << "client pointer not found, trying master pointer";
        device = get_master_pointer();
    }
    if (!device)
        LOG_WARNING << "master pointer not found, typing will not be possible";
    else
    {
        LOG_INFO << "listening to XInput master:"
                 << " " << repr(device->name)
                 << " " << device->device_id
                 << " " << device->get_config_string();

        if (!select_events(win, device->device_id, event_mask))
            LOG_WARNING << "failed to select events for device"
                        << " " << device->device_id;
    }

    m_master_device = device;

    // Select events of all attached (non-floating) slave pointers.
    event_mask = XI_ButtonPressMask |
                 XI_ButtonReleaseMask |
                 XI_EnterMask |
                 XI_LeaveMask |
                 XI_MotionMask;
    if (config()->are_touch_events_enabled())
    {
        event_mask |= XI_TouchBeginMask |
                      XI_TouchEndMask |
                      XI_TouchUpdateMask;
    }

    std::vector<InputDevicePtr> devices;
    get_client_pointer_attached_slaves(win, devices);

    if (logger()->can_log(LogLevel::INFO))
    {
        std::vector<std::string> a;
        for (auto& d : devices)
            a.emplace_back(sstr()
                << d->name
                << " " << d->device_id
                << " " << d->get_config_string());
        LOG_INFO << "listening to XInput slaves: " << a;
    }

    for (auto& d : devices)
    {
        if (!select_events(win, d->device_id, event_mask))
            LOG_WARNING << "failed to select events for device"
                        << " " << d->device_id;
    }

    m_slave_devices = devices;
}

ViewBase* InputEventSourceXInput::find_top_level(OnboardOskEvent* event)
{
    for (auto view : reversed(get_toplevel_views()))
    {
        Rect r = view->canvas_to_root(view->get_canvas_rect());
        if (r.contains({event->x_root, event->y_root}))
            return view;
    }
    return {};
}

// Process event coming from the event queue.
void InputEventSourceXInput::on_input_source_event(OnboardOskEvent* event)
{
    ViewBase* toplevel;

    if (m_pointer_grab_view)
        toplevel = m_pointer_grab_view->find_toplevel_view_from_leaf();
    else
        toplevel = find_top_level(event);

    // transform from root window/stage to toplevel coordinates
    if (toplevel)
    {
        Rect r = toplevel->get_rect();
        event->x = event->x_root - r.x;
        event->y = event->y_root - r.y;
    }

    // new toplevel?
    if (m_last_entered_toplevel != toplevel)
    {
        // generate leave event?
        if (m_last_entered_toplevel)
        {
            OnboardOskEvent e = *event;
            e.type = ONBOARD_OSK_EVENT_LEAVE;
            on_toplevel_event(m_last_entered_toplevel, &e);
            m_last_entered_toplevel = nullptr;
        }
        // generate enter event?
        if (toplevel)
        {
            OnboardOskEvent e = *event;
            e.type = ONBOARD_OSK_EVENT_ENTER;
            on_toplevel_event(toplevel, &e);
            m_last_entered_toplevel = toplevel;
        }
    }

    if (toplevel)
        on_toplevel_event(toplevel, event);
}

