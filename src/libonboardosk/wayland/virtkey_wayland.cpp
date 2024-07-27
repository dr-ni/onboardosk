
/*
 * Copyright © 2016 marmuta <marmvta@gmail.com>
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

#include <assert.h>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <sys/mman.h>

#include <glib.h>
#include <glib-object.h>

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#include <gdk/gdkkeysyms.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "tools/logger.h"
#include "tools/thread_helpers.h"

#include "exception.h"
#include "timer.h"
#include "virtkey.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#define N_MOD_INDICES (Mod5MapIndex + 1)


typedef struct
{
  GObject     parent_instance;
  GdkDisplay *display;

  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;

  PangoDirection *direction;
  gboolean bidi;
} _GdkWaylandKeymap;


class VirtKeyWaylandImpl : public VirtKey
{
    public:
        using Super = VirtKey;

        VirtKeyWaylandImpl(const ContextBase& context) :
            Super(context)
        {
            init();
        }
        virtual ~VirtKeyWaylandImpl();

        // public interface
        virtual KeymapGroup get_current_group() override;
        virtual std::string get_current_group_name() override;
        virtual bool get_auto_repeat_rate(unsigned int& delay, unsigned int& interval) override;
        virtual KeyCode get_keycode_from_keysym(KeySym keysym, KeymapGroup group, ModMask& mod_mask_out) override;
        virtual KeySym get_keysym_from_keycode(KeyCode keycode, ModMask modmask, KeymapGroup group) override;
        virtual std::string get_label_from_keycode(KeyCode keycode, ModMask modmask, KeymapGroup group) override;
        virtual std::vector<std::string> get_current_rules_names() override;
        virtual std::string get_layout_as_string() override;
        virtual void reload() override;

        virtual void set_group(KeymapGroup group, bool lock) override;
        virtual void set_modifiers(ModMask mod_mask, bool lock, bool press) override;
        virtual void send_key_event(KeyCode keycode, bool press) override;

    public:
        void init();
        GdkKeymap* get_gdk_keymap();
        struct xkb_keymap* get_gdk_xkb_keymap();
        struct xkb_state* get_gdk_xkb_state();
        struct xkb_keymap* get_xkb_keymap();
        struct xkb_state* get_xkb_state();

        // wl_registry_listener interface
        void on_registry_global_annoncement(wl_registry* registry, uint32_t id, const char* interface, uint32_t version);
        void on_registry_global_removal(wl_registry* registry, uint32_t id);

        // wl_seat_listener interface
        void on_seat_capabilities(struct wl_seat *seat,
                                  uint32_t caps);  // enum wl_seat_capability

        // wl_keyboard_listener interface
        void on_keyboard_keymap(struct wl_keyboard *keyboard,
                                uint32_t format, int fd, uint32_t size);
        void on_keyboard_enter(struct wl_keyboard *keyboard, uint32_t serial,
                               struct wl_surface *surface, struct wl_array *keys);
        void on_keyboard_leave(wl_keyboard* keyboard, uint32_t serial,
                               wl_surface* surface);
        void on_keyboard_repeat_info(wl_keyboard* keyboard, int32_t rate, int32_t delay);
        void on_keyboard_modifiers_changed(wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
        void on_keyboard_key_event(wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);

    private:
        KeymapGroup get_current_group_from_xkb();
        void run_message_loop();

    public:
        ThreadFd m_thread;

        std::unique_ptr<struct wl_display, decltype(&wl_display_disconnect)>
            m_display{nullptr, wl_display_disconnect};
        std::unique_ptr<struct wl_registry, decltype(&wl_registry_destroy)>
            m_registry{nullptr, wl_registry_destroy};
        std::unique_ptr<struct wl_seat, decltype(&wl_seat_destroy)>
            m_seat{nullptr, wl_seat_destroy};
        std::unique_ptr<struct wl_keyboard, decltype(&wl_keyboard_destroy)>
            m_keyboard{nullptr, wl_keyboard_destroy};

        std::unique_ptr<struct xkb_keymap, decltype(&xkb_keymap_unref)>
            m_xkb_keymap{nullptr, xkb_keymap_unref};
        std::unique_ptr<struct xkb_state, decltype(&xkb_state_unref)>
            m_xkb_state{nullptr, xkb_state_unref};

        KeymapGroup m_current_group{static_cast<KeymapGroup>(-1)};
};

std::unique_ptr<VirtKey> VirtKey::make_vk_wayland(const ContextBase& context)
{
    return std::make_unique<VirtKeyWaylandImpl>(context);
}


VirtKeyWaylandImpl::~VirtKeyWaylandImpl()
{
    m_thread.stop();
}

GdkKeymap* VirtKeyWaylandImpl::get_gdk_keymap()
{
    return gdk_keymap_get_default();
}

struct xkb_keymap* VirtKeyWaylandImpl::get_gdk_xkb_keymap ()
{
    GdkKeymap* gdk_keymap = get_gdk_keymap();
    struct xkb_keymap* xkb_keymap =
            reinterpret_cast<_GdkWaylandKeymap*>(gdk_keymap)->xkb_keymap;
    return xkb_keymap;
}

struct xkb_state* VirtKeyWaylandImpl::get_gdk_xkb_state ()
{
    GdkKeymap* gdk_keymap = get_gdk_keymap();
    //struct xkb_state* state = GDK_WAYLAND_KEYMAP (keymap)->xkb_state;
    struct xkb_state* xkb_state =
        reinterpret_cast<_GdkWaylandKeymap*>(gdk_keymap)->xkb_state;
    return xkb_state;
}

struct xkb_keymap* VirtKeyWaylandImpl::get_xkb_keymap ()
{
    return m_xkb_keymap.get();
}

struct xkb_state* VirtKeyWaylandImpl::get_xkb_state ()
{
    return m_xkb_state.get();
}

KeymapGroup VirtKeyWaylandImpl::get_current_group()
{
    return Super::get_current_group();
}

// unused
KeymapGroup VirtKeyWaylandImpl::get_current_group_from_xkb()
{
    // Gdk's xkb_state doesn't know the currently active layout (group)
    // (Xenial). Use our own xkb_keymap instead.
    struct xkb_keymap* xkb_keymap = get_xkb_keymap();
    struct xkb_state* xkb_state = get_xkb_state();
    if (xkb_state)
    {
        unsigned int i;
        for (i = 0; i < xkb_keymap_num_layouts(xkb_keymap); i++)
        {
            if (xkb_state_layout_index_is_active(xkb_state, i,
                                                 XKB_STATE_LAYOUT_EFFECTIVE))
                return i;
        }
    }
    return 0;
}

std::string VirtKeyWaylandImpl::get_current_group_name()
{
    std::string name;
    struct xkb_keymap* xkb_keymap = get_xkb_keymap();
    KeymapGroup group = get_current_group();
    if (xkb_keymap && is_valid(group))
        name = xkb_keymap_layout_get_name(xkb_keymap, group);
    return name;
}

// Remnant of reading the contents of the root window
// property _XKB_RULES_NAMES under X11.
// Gnome-shell gets this information from org.freedesktop.locale1 on the system bus.
std::vector<std::string> VirtKeyWaylandImpl::get_current_rules_names()
{
    std::vector<std::string> results;

    // Must be provided by toolkit-dependent code.
    // No way to get this from xkb or Wayland, apparently.
    results = Super::get_current_rules_names();
    if (results.empty())
        results = {"", "", "", "", ""};

    assert(results.size() == 5);

    return results;
}

bool VirtKeyWaylandImpl::get_auto_repeat_rate (unsigned int& delay,
                                               unsigned int& interval)
{
    delay = 500;
    interval = 30;
    return true;
}

KeyCode VirtKeyWaylandImpl::get_keycode_from_keysym(KeySym keysym, KeymapGroup group,
                                                 ModMask& mod_mask_out)
{
    KeyCode keycode = 0;
    GdkKeymap* gdk_keymap = get_gdk_keymap();
    GdkKeymapKey* keys;
    gint n_keys;

    LOG_TRACE << "virtkey_wayland_get_keycode_from_keysym:"
              << " keysym=" << keysym
              << " group=" << group;

    if (gdk_keymap_get_entries_for_keyval(gdk_keymap, keysym, &keys, &n_keys))
    {
        int i;
        for (i=0; i<n_keys; i++)
        {
            GdkKeymapKey* key = keys + i;
            LOG_TRACE << "    candidate keycode=" << key->keycode
                      << " group=" << key->group
                      << " level=" << key->level;
        }
        for (i=0; i<n_keys; i++)
        {
            GdkKeymapKey* key = keys + i;
            if (key->group == static_cast<gint>(group))
            {
                guint keysym_;
                gint effective_group;
                gint level;
                GdkModifierType consumed_modifiers;

                if (!gdk_keymap_translate_keyboard_state(
                            gdk_keymap, key->keycode,
                            static_cast<GdkModifierType>(0), group,
                            &keysym_, &effective_group, &level,
                            &consumed_modifiers))
                {
                    /* try shift modifier */
                    gdk_keymap_translate_keyboard_state(
                                gdk_keymap, key->keycode, GDK_SHIFT_MASK, group,
                                &keysym_, &effective_group, &level,
                                &consumed_modifiers);
                }
                if (key->level == level)
                {
                    keycode = key->keycode;
                    LOG_TRACE << "    selected keycode=" << key->keycode
                              << " group=" << key->group
                              << " level=" << key->level;
                    break;
                }
            }
        }
        g_free(keys);
    }
    LOG_TRACE << "    final     keycode: " << keycode;

    mod_mask_out = 0;

    return keycode;
}

KeySym VirtKeyWaylandImpl::get_keysym_from_keycode(KeyCode keycode, ModMask modmask, KeymapGroup group)
{
    GdkKeymap* gdk_keymap = get_gdk_keymap();
    guint keysym = 0;
    gint effective_group;
    gint level;
    GdkModifierType consumed_modifiers;
//xkb_keymap_key_get_syms_by_level
    gdk_keymap_translate_keyboard_state(gdk_keymap, keycode,
                                        static_cast<GdkModifierType>(modmask), group,
                                        &keysym, &effective_group, &level,
                                        &consumed_modifiers);
    //g_debug("virtkey_wayland_get_keysym_from_keycode: keycode %d, modmask %d, group %d, keysym %d\n", keycode, modmask, group, keysym);

    return keysym;
}

std::string VirtKeyWaylandImpl::get_label_from_keycode(KeyCode keycode, ModMask modmask, KeymapGroup group)
{
    KeySym keysym = get_keysym_from_keycode(keycode, modmask, group);
    return get_label_from_keysym(keysym);
}

// Return a string representative of the whole layout including all groups.
std::string VirtKeyWaylandImpl::get_layout_as_string()
{
    struct xkb_keymap* xkb_keymap = get_xkb_keymap();
    if (xkb_keymap)
        return xkb_keymap_get_as_string(xkb_keymap,
                                        XKB_KEYMAP_USE_ORIGINAL_FORMAT);
    return {};
}

void VirtKeyWaylandImpl::on_keyboard_keymap(struct wl_keyboard *keyboard,
                                            uint32_t format, int fd, uint32_t size)
{
    char *map_str;

    LOG_DEBUG << "on_keyboard_keymap:"
              << " format=" << format
              << " fd=" << fd
              << " size=" << size;

    std::unique_ptr<struct xkb_context, decltype(&xkb_context_unref)> context =
        {xkb_context_new (XKB_CONTEXT_NO_FLAGS), xkb_context_unref};

    map_str = static_cast<char*>(mmap (NULL, size, PROT_READ, MAP_SHARED, fd, 0));
    if (map_str == MAP_FAILED)
    {
        close(fd);
        return;
    }

    std::unique_ptr<struct xkb_keymap, decltype(&xkb_keymap_unref)> keymap =
        {xkb_keymap_new_from_string (context.get(), map_str,
                                                 static_cast<xkb_keymap_format>(format),
                                                 XKB_KEYMAP_COMPILE_NO_FLAGS),
         xkb_keymap_unref};

    munmap (map_str, size);
    close (fd);

    if (!keymap)
    {
        LOG_WARNING << "got invalid keymap from compositor, keeping previous/default one";
        return;
    }

    std::unique_ptr<struct xkb_state, decltype(&xkb_state_unref)> state =
        {xkb_state_new (keymap.get()), xkb_state_unref};

    m_xkb_keymap = std::move(keymap);
    m_xkb_state = std::move(state);

    {
        unsigned int i;
        for (i = 0; i < xkb_keymap_num_layouts (m_xkb_keymap.get()); i++)
        {
            LOG_DEBUG << "   layout index=" << i
                      << " active=" <<
                xkb_state_layout_index_is_active (m_xkb_state.get(), i, XKB_STATE_LAYOUT_EFFECTIVE);
        }
    }

    keymap_changed.emit();
    wl_display_flush(m_display.get());
}

void VirtKeyWaylandImpl::on_keyboard_enter(struct wl_keyboard *keyboard,
                                               uint32_t serial,
                                               struct wl_surface *surface,
                                               struct wl_array *keys)
{
    (void)keyboard;
    (void)surface;
    (void)keys;
    LOG_DEBUG << serial;
}

void VirtKeyWaylandImpl::on_keyboard_leave(struct wl_keyboard *keyboard,
                                           uint32_t serial,
                                           struct wl_surface *surface)
{
    (void)keyboard;
    (void)surface;
    LOG_DEBUG << serial;
}

void VirtKeyWaylandImpl::on_keyboard_key_event(struct wl_keyboard *keyboard,
                                               uint32_t serial, uint32_t time,
                                               uint32_t key, uint32_t state)
{
    (void)keyboard;
    LOG_DEBUG << "serial=" << serial
              << " time=" << time
              << " key=" << key
              << " state=" << state;
}

void VirtKeyWaylandImpl::on_keyboard_modifiers_changed(
    struct wl_keyboard *keyboard, uint32_t serial,
    uint32_t mods_depressed, uint32_t mods_latched,
    uint32_t mods_locked, uint32_t group)
{
    LOG_DEBUG << "mods_depressed=" << mods_depressed
              << " mods_latched=" << mods_latched
              << " mods_locked=" << mods_locked
              << " group=" << group;

    xkb_state_update_mask (m_xkb_state.get(), mods_depressed, mods_latched, mods_locked, group, 0, 0);

    {
        struct xkb_keymap* xkb_keymap = get_gdk_xkb_keymap();
        struct xkb_state* xkb_state = get_gdk_xkb_state();
        unsigned int i;
        for (i = 0; i < xkb_keymap_num_layouts (xkb_keymap); i++)
        {
            LOG_DEBUG << "   gdk layout index=" << i
                      << " active="
                      << xkb_state_layout_index_is_active (xkb_state, i, XKB_STATE_LAYOUT_EFFECTIVE)
                      << " name="
                      << xkb_keymap_layout_get_name(xkb_keymap, i);
        }
    }

    {
        struct xkb_keymap* xkb_keymap = get_xkb_keymap();
        struct xkb_state* xkb_state = get_xkb_state();
        unsigned int i;
        for (i = 0; i < xkb_keymap_num_layouts (xkb_keymap); i++)
        {
            LOG_DEBUG << "   wl layout index=" << i
                      << " active="
                      << xkb_state_layout_index_is_active (xkb_state, i, XKB_STATE_LAYOUT_EFFECTIVE)
                      << " name="
                      << xkb_keymap_layout_get_name(xkb_keymap, i);
        }
        LOG_DEBUG << "   current group=" << get_current_group();
    }

    // This doesn't really work as the client has to be focused for state updates
    if (m_current_group != group)
    {
        m_current_group = group;
        group_changed.emit();
    }
}

void VirtKeyWaylandImpl::on_keyboard_repeat_info(struct wl_keyboard *keyboard,
                                                 int32_t rate, int32_t delay)
{
    LOG_DEBUG << "keyboard_handle_repeat_info: "
              << " rate=" << rate
              << " delay=" << delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
    [](void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
        {reinterpret_cast<VirtKeyWaylandImpl*>(data)->
            on_keyboard_keymap(keyboard, format, fd, size);},

    [](void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
        {reinterpret_cast<VirtKeyWaylandImpl*>(data)->
            on_keyboard_enter(keyboard, serial, surface, keys);},

    [](void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
        {reinterpret_cast<VirtKeyWaylandImpl*>(data)->
            on_keyboard_leave(keyboard, serial, surface);},

    [](void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
        {reinterpret_cast<VirtKeyWaylandImpl*>(data)->
            on_keyboard_key_event(keyboard, serial, time, key, state);},

    [](void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
        {reinterpret_cast<VirtKeyWaylandImpl*>(data)->
            on_keyboard_modifiers_changed(keyboard, serial, mods_depressed, mods_latched, mods_locked, group);},

    [](void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay)
        {reinterpret_cast<VirtKeyWaylandImpl*>(data)->
            on_keyboard_repeat_info(keyboard, rate, delay);},
};

void VirtKeyWaylandImpl::on_seat_capabilities(struct wl_seat *seat,
                                              uint32_t caps)  // enum wl_seat_capability
{
    LOG_DEBUG << "caps=" <<  caps;

    if (caps & WL_SEAT_CAPABILITY_POINTER) {
        LOG_DEBUG << "seat has a pointer";
    }

    if (caps & WL_SEAT_CAPABILITY_KEYBOARD)
    {
        m_keyboard = {wl_seat_get_keyboard(seat), wl_keyboard_destroy};
        LOG_DEBUG << "seat has a keyboard: " << m_keyboard.get();
        if (m_keyboard)
        {
            wl_keyboard_set_user_data(m_keyboard.get(), this);
            wl_keyboard_add_listener(m_keyboard.get(), &keyboard_listener, this);
        }
    }
    else
    {
        m_keyboard = nullptr;
    }

    if (caps & WL_SEAT_CAPABILITY_TOUCH) {
        LOG_DEBUG << "seat has a touch screen";
    }
}

static const struct wl_seat_listener seat_listener =
{
    [](void *data, struct wl_seat *seat, uint32_t caps)
        {reinterpret_cast<VirtKeyWaylandImpl*>(data)->
            on_seat_capabilities(seat, caps);},
    nullptr,
};

void VirtKeyWaylandImpl::on_registry_global_annoncement(struct wl_registry *registry, uint32_t id,
                        const char *interface, uint32_t version)
{
    LOG_DEBUG << "interface=" << interface
              << " id=" << id
              << " version=" << version;

    if (strcmp(interface, "wl_seat") == 0)
    {
        m_seat = {reinterpret_cast<wl_seat*>(
                      wl_registry_bind(registry, id, &wl_seat_interface, 1)),
                  wl_seat_destroy};
        wl_seat_add_listener(m_seat.get(), &seat_listener, this);
    }
}

void VirtKeyWaylandImpl::on_registry_global_removal(struct wl_registry *registry, uint32_t id)
{
    LOG_DEBUG << "registry lost for " << id;
}

static const struct wl_registry_listener registry_listener = {
    [](void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
        {reinterpret_cast<VirtKeyWaylandImpl*>(data)->
            on_registry_global_annoncement(registry, id, interface, version);},

    [](void *data, struct wl_registry *registry, uint32_t name)
        {reinterpret_cast<VirtKeyWaylandImpl*>(data)->
            on_registry_global_removal(registry, name);},
};

void VirtKeyWaylandImpl::init ()
{
#if 0
    GdkDisplay* display = gdk_display_get_default ();
    if (!GDK_IS_WAYLAND_DISPLAY (display))
        throw VirtKeyException("not a wayland display");
    m_display = {gdk_wayland_display_get_wl_display (display), wl_display_disconnect};
#else
    m_display = {wl_display_connect(NULL), wl_display_disconnect};
#endif
    if (!m_display)
        throw VirtKeyException("wl_display_connect failed");

    int display_fd = wl_display_get_fd(m_display.get());
    if (!display_fd)
        throw VirtKeyException("wl_display_get_fd failed");

    m_registry = {wl_display_get_registry(m_display.get()),
                  wl_registry_destroy};
    if (!m_registry)
        throw VirtKeyException("wl_display_get_registry failed");
    wl_registry_add_listener(m_registry.get(), &registry_listener, this);
    // Get everything started.
    // All subsequent flushes are initiated by the message loop.
    wl_display_flush (m_display.get());
    wl_display_dispatch(m_display.get());
    wl_display_roundtrip(m_display.get());
    wl_display_roundtrip(m_display.get());
    wl_display_roundtrip(m_display.get());
    wl_display_roundtrip(m_display.get());

    try
    {
        m_thread.start(display_fd, [this](){run_message_loop();});
    }
    catch (const std::runtime_error& ex)
    {
        throw VirtKeyException(std::string("failed to start thread: ") +
                               ex.what());
    }

}

void VirtKeyWaylandImpl::run_message_loop()
{
    LOG_INFO << "thread start";

    try
    {
        while (!m_thread.is_stop_requested())
        {
            struct wl_display* display = m_display.get();
            if (wl_display_prepare_read(display) != 0)
            {
                // wait for the main thread to dispatch the message queue
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                LOG_DEBUG << "wl_display_prepare_read not ready";
                continue;
            }

            if (!m_thread.wait_for_data())
                break;

            wl_display_read_events(display);

            idle_call<VirtKeyWaylandImpl>(this, [](VirtKeyWaylandImpl* this_) {
                struct wl_display* d = this_->m_display.get();
                wl_display_dispatch_pending(d);
                wl_display_flush(d);
                return 0;  // one-shot
            });
        }
    }
    catch (const std::runtime_error& ex)
    {
        LOG_ERROR << "aborting thread: " << ex.what();
    }

    LOG_INFO << "thread exit";
}

void VirtKeyWaylandImpl::reload()
{
    Super::reload();
}

void VirtKeyWaylandImpl::set_group(KeymapGroup group, bool lock)
{
    (void)group;
    (void)lock;
    // We don't need this, currently.
}

void VirtKeyWaylandImpl::set_modifiers (ModMask mod_mask, bool lock, bool press)
{
    (void) mod_mask;
    (void) lock;
    (void) press;
    // No way to do this in Wayland?
}

void VirtKeyWaylandImpl::send_key_event(KeyCode keycode, bool press)
{
    (void) keycode;
    (void) press;
    // No way to do this in Wayland?
}

#pragma GCC diagnostic pop

#endif  /* GDK_WINDOWING_WAYLAND */

