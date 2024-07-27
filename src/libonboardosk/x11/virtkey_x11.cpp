#ifndef VIRTKEY_X11_CPP
#define VIRTKEY_X11_CPP

/*
 * Copyright © 2006-2008 Chris Jones <tortoise@tortuga>
 * Copyright © 2010, 2016 marmuta <marmvta@gmail.com>
 * Copyright © 2013 Gerd Kohlberger <lowfi@chello.at>
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

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/XKBrules.h>

#include "tools/string_helpers.h"

#include "../tools/logger.h"

#include "../exception.h"
#include "../virtkey.h"


#define N_MOD_INDICES (Mod5MapIndex + 1)


class VirtKeyX11Impl : public VirtKey
{
    public:
        using Super = VirtKey;

        VirtKeyX11Impl(const ContextBase& context) :
            Super(context)
        {
            init();
        }
        virtual ~VirtKeyX11Impl();

        // public interface
        virtual bool is_x11_display() override {return true;}
        virtual KeymapGroup get_current_group() override;
        virtual std::string get_current_group_name() override;
        virtual bool get_auto_repeat_rate(unsigned int& delay, unsigned int& interval) override;
        virtual KeyCode get_keycode_from_keysym(KeySym keysym, KeymapGroup group, ModMask& mod_mask_out) override;
        virtual KeySym get_keysym_from_keycode(KeyCode keycode, ModMask modmask, KeymapGroup group) override;
        virtual std::string get_label_from_keycode(KeyCode keycode, ModMask modmask, KeymapGroup group) override;
        virtual std::vector<std::string> get_current_rules_names() override;
        virtual std::string get_layout_as_string() override;
        virtual void set_group(KeymapGroup group, bool lock) override;
        virtual void set_modifiers(ModMask mod_mask, bool lock, bool press) override;
        virtual void reload() override;
        virtual void send_key_event(KeyCode keycode, bool press) override;

    private:
        void init();
        void init_keyboard();
        KeymapGroup get_effective_group(XkbDescPtr kbd, KeyCode keycode, KeymapGroup group);
        KeyCode keysym_to_keycode(XkbDescPtr kbd, KeySym keysym, KeymapGroup group, ModMask* mod_mask);
        KeyCode map_keysym_xkb(KeySym keysym, KeymapGroup group);
        std::vector<std::string> get_rules_names();

    private:
        Display   *m_xdisplay;

        int m_xkb_base_event;
        XkbDescPtr m_kbd;
};

std::unique_ptr<VirtKey> VirtKey::make_vk_x11(const ContextBase& context)
{
    return std::make_unique<VirtKeyX11Impl>(context);
}




VirtKeyX11Impl::~VirtKeyX11Impl()
{
    if (m_kbd)
        XkbFreeKeyboard (m_kbd, XkbAllComponentsMask, True);
}

KeymapGroup VirtKeyX11Impl::get_current_group()
{
    // Try toolkit-dependent code first for
    // easier debugging of the wayland code.
    auto group = Super::get_current_group();
    if (is_valid(group))
        return group;

    XkbStateRec state;
    if (XkbGetState (m_xdisplay, XkbUseCoreKbd, &state) != Success)
        return static_cast<KeymapGroup>(-1);
    return state.locked_group;
}

std::string VirtKeyX11Impl::get_current_group_name()
{
    std::string result;
    KeymapGroup group;

    if (!m_kbd->names)
        throw VirtKeyException("VirtKeyX11Impl::get_current_group_name: "
                               "no group names available");

    group = get_current_group();
    if (!is_valid(group))
        return {};

    if (m_kbd->names->groups[group] != None)
    {
        char *name = XGetAtomName (m_xdisplay,
                                   m_kbd->names->groups[group]);
        if (name)
        {
            result = name;
            XFree (name);
        }
    }

    return result;
}

// Read the contents of the root window property _XKB_RULES_NAMES.
std::vector<std::string> VirtKeyX11Impl::get_rules_names()
{
    std::vector<std::string> results;

    XkbRF_VarDefsRec vd;
    char *tmp = NULL;

    if (!XkbRF_GetNamesProp (m_xdisplay, &tmp, &vd))
        return {};

    if (tmp)
    {
        results.emplace_back(tmp);
        XFree (tmp);
    }
    else
        results.emplace_back("");

    if (vd.model)
    {
        results.emplace_back(vd.model);
        XFree (vd.model);
    }
    else
        results.emplace_back("");

    if (vd.layout)
    {
        results.emplace_back(vd.layout);
        XFree (vd.layout);
    }
    else
        results.emplace_back("");

    if (vd.variant)
    {
        results.emplace_back(vd.variant);
        XFree (vd.variant);
    }
    else
        results.emplace_back("");

    if (vd.options)
    {
        results.emplace_back(vd.options);
        XFree (vd.options);
    }
    else
        results.emplace_back("");

    assert(results.size() == 5);

    return results;
}

// Read the contents of the root window property _XKB_RULES_NAMES.
std::vector<std::string> VirtKeyX11Impl::get_current_rules_names()
{
    std::vector<std::string> results;

    // Try toolkit-dependent code first for
    // easier debugging of the wayland code.
    results = Super::get_current_rules_names();
    if (results.empty())
    {
        // layout and variant of get_rules_names() have entries per group
        std::vector<std::string> names = get_rules_names();
        if (!names.empty())
        {
            auto group = get_current_group();
            auto layouts  = split(names[2], ',');
            auto variants = split(names[3], ',');

            std::string layout = group < layouts.size() ? layouts[group] : "";
            std::string variant = group < variants.size() ? variants[group] : "";

            results = {"", "", layout, variant, ""};
        }
    }
    if (results.empty())
        results = {"", "", "", "", ""};

    assert(results.size() == 5);

    return results;
}

bool VirtKeyX11Impl::get_auto_repeat_rate(unsigned int& delay,
                                          unsigned int& interval)
{
    if (!XkbGetAutoRepeatRate(m_xdisplay, XkbUseCoreKbd, &delay, &interval))
    {
        delay = 0;
        interval = 0;
        return false;
    }
    return true;
}

// Return group for keycode with out-of-range action applied.
KeymapGroup VirtKeyX11Impl::get_effective_group(XkbDescPtr kbd, KeyCode keycode, KeymapGroup group)
{
    unsigned num_groups = XkbKeyNumGroups(kbd, keycode);
    KeymapGroup key_group = group;

    if (num_groups == 0)
    {
        key_group = static_cast<KeymapGroup>(-1);
    }
    else if (num_groups == 1)
    {
        key_group = 0;
    }
    else if (key_group >= num_groups)
    {
        unsigned int group_info = XkbKeyGroupInfo(kbd, keycode);

        switch (XkbOutOfRangeGroupAction(group_info))
        {
            case XkbClampIntoRange:
                key_group = num_groups - 1;
                break;

            case XkbRedirectIntoRange:
                key_group = XkbOutOfRangeGroupNumber(group_info);
                if (key_group >= num_groups)
                    key_group = 0;
                break;

            case XkbWrapIntoRange:
            default:
                key_group %= num_groups;
                break;
        }
    }

    return key_group;
}

KeyCode VirtKeyX11Impl::keysym_to_keycode(XkbDescPtr kbd, KeySym keysym, KeymapGroup group, ModMask* mod_mask)
{
    KeyCode result = 0;
    KeyCode keycode = 0;
    int key_group;
    int num_levels;
    int level;
    unsigned int new_mask = 0;

    for (keycode = kbd->min_key_code; keycode < kbd->max_key_code; keycode++)
    {
        key_group = get_effective_group(kbd, keycode, group);
        if (key_group >= 0)  // valid key, i.e. not unused?
        {
            num_levels = XkbKeyGroupWidth(kbd, keycode, key_group);
            for (level = 0; level < num_levels; level++)
            {
                KeySym ks = XkbKeySymEntry(kbd, keycode, level, key_group);
                if (ks == keysym)
                {
                    int i;
                    XkbKeyTypePtr key_type = XkbKeyKeyType(kbd, keycode,
                                                           key_group);
                    int map_count = key_type->map_count;

                    #ifdef DEBUG_OUTPUT
                    printf("kc %d level %d ks %ld key_type->num_levels %d, "
                           "key_type->map_count %d, mods m %d r %d v %d\n",
                           keycode, level, ks, key_type->num_levels,
                           key_type->map_count, key_type->mods.mask,
                           key_type->mods.real_mods, key_type->mods.vmods);
                    for (i = 0; i < map_count; i++)
                    {
                        XkbKTMapEntryPtr entry = key_type->map + i;
                        printf("xxx i %d: level %d entry->level %d "
                               "entry->modsmask %d \n",
                               i, level, entry->level, entry->mods.mask);
                    }
                    #endif

                    if (level == 0)
                    {
                        result = keycode;
                        new_mask = 0;
                    }
                    else
                    {
                        for (i = 0; i < map_count; i++)
                        {
                            XkbKTMapEntryPtr entry = key_type->map + i;
                            if (entry->level == level)
                            {
                                result = keycode;
                                new_mask = entry->mods.mask;
                                break;
                            }
                        }
                    }

                    break;
                }
            }
        }

        if (result)
            break;
    }

    if (mod_mask)
        *mod_mask = new_mask;

    return result;
}


#ifdef DEBUG_OUTPUT
void dump_xkb_state(VirtkeyX* this, int keycode, KeySym keysym, KeymapGroup group)
{
    unsigned int ma;
    int kca;
    int key_group = get_effective_group(m_kbd, keycode, group);

    for (int i=244; i<=m_kbd->max_key_code;i++)
    {
        KeySym* pks = &XkbKeySymEntry(m_kbd, i, 0, key_group);
        unsigned int g = XkbKeyGroupInfo(m_kbd, i);
        printf("%3d pkeysym 0x%p keysym %9ld group_info 0x%x "
               //"NumGroups %d "
               //"OutOfRangeGroupAction %d OutOfRangeGroupNumber "
               //"%d OutOfRangeGroupInfo %d "
               "num_groups %d KeyGroupsWidth %d kt_index [%d,%d,%d,%d] "
               "XkbCMKeySymsOffset %4d [",
               i, pks,
               XkbKeySymEntry(m_kbd, i, 0, key_group),
               g,
               //XkbNumGroups(g),
               //XkbOutOfRangeGroupAction(g),
               //XkbOutOfRangeGroupNumber(g),
               //XkbOutOfRangeGroupInfo(g),
               XkbKeyNumGroups(m_kbd, i),
               XkbKeyGroupsWidth(m_kbd, i),
               XkbCMKeyTypeIndex(m_kbd->map, i, 0),
               XkbCMKeyTypeIndex(m_kbd->map, i, 1),
               XkbCMKeyTypeIndex(m_kbd->map, i, 2),
               XkbCMKeyTypeIndex(m_kbd->map, i, 3),
               XkbCMKeySymsOffset(m_kbd->map, i)
               );
        {
            int n = XkbKeyNumGroups(m_kbd, i) *
                    XkbKeyGroupsWidth(m_kbd, i);
            for (int j=0; j<n; j++)
            {
                printf("%9ld", (XkbKeySymsPtr(m_kbd, i)[j]));
                if (j < n-1)
                    printf(",");
            }
            printf("]\n");
        }
    }

    {
        int start = XkbCMKeySymsOffset(m_kbd->map, 244) / 10 * 10;
        for (int r=0; r<5; r++)
        {
            int offset = start + r*10;
            printf("%4d: ", offset);

            for (int i=0; i<10; i++)
            {
                printf("%9ld ", (m_kbd->map->syms[offset+i]));
            }
            printf("\n");
        }
    }

    kca = keysym_to_keycode(m_kbd, keysym, group, &ma);
    printf("Remapping keysym %ld to keycode %d found keycode %d, "
           "min_key_code %d max_key_code %d XkbKeyGroupsWidth %d\n",
           keysym, keycode, kca, m_kbd->min_key_code,
           m_kbd->max_key_code, XkbKeyGroupsWidth(m_kbd, keycode));
}
#endif

KeyCode VirtKeyX11Impl::map_keysym_xkb(KeySym keysym, KeymapGroup group)
{
    int modified_key = 0;
    KeyCode keycode;
    Status status;
    int key_group;

    // Change one of the last 10 keysyms, remapping the keyboard map
    // on the fly. This assumes the last 10 aren't already used.
    const int n = 10;
    keycode = m_kbd->max_key_code - modified_key - 1;
    modified_key = (modified_key + 1) % n;

    #ifdef DEBUG_OUTPUT
    dump_xkb_state(this, keycode, keysym, group);
    #endif

    // Allocate space for the new symbol and init types.
    {
        int n_groups = 1;
        int new_types[XkbNumKbdGroups] = {XkbOneLevelIndex};
        XkbMapChangesRec changes;  // man XkbSetMap
        memset(&changes, 0, sizeof(changes));

        changes.changed = XkbKeySymsMask;
        changes.first_key_sym = keycode;
        changes.num_key_syms = 1;

        status = XkbChangeTypesOfKey (m_kbd, keycode,
                                      n_groups, XkbGroup1Mask,
                                      new_types, &changes);
        if (status != Success)
        {
            return 0;
        }
    }

    // Patch in our new symbol
    key_group = get_effective_group(m_kbd, keycode, group);
    XkbKeySymEntry(m_kbd, keycode, 0, key_group) = keysym;

    #ifdef DEBUG_OUTPUT
    dump_xkb_state(this, keycode, keysym, group);
    #endif


    // Tell the server
    {
        XkbMapChangesRec changes;  // man XkbSetMap
        changes.changed = XkbKeySymsMask;
        changes.first_key_sym = keycode;
        changes.num_key_syms = 1;

        if (!XkbChangeMap(m_xdisplay, m_kbd, &changes))
            return 0;

        XSync (m_xdisplay, False);
    }

    return keycode;
}

KeyCode VirtKeyX11Impl::get_keycode_from_keysym(KeySym keysym,
                                                KeymapGroup group,
                                                ModMask& mod_mask_out)
{
    KeyCode keycode;

    // Look keysym up in current group.
    keycode = keysym_to_keycode(m_kbd, keysym,
                                group, &mod_mask_out);
    if (!keycode)
        keycode = map_keysym_xkb(keysym, group);

    return keycode;
}

std::string VirtKeyX11Impl::get_label_from_keycode(KeyCode keycode,
                                                   ModMask modmask, KeymapGroup group)
{
    std::string result;
    unsigned int mods;
    int label_size;
    KeySym keysym;
    XKeyPressedEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = KeyPress;
    ev.display = m_xdisplay;

    mods = XkbBuildCoreState (modmask, group);

    ev.state = mods;
    ev.keycode = keycode;
    char buf[256];
    label_size = XLookupString(&ev, buf, sizeof(buf), &keysym, NULL);
    buf[label_size] = '\0';

    if (keysym)
        result = get_label_from_keysym(keysym);
    else
        result = buf;

    return result;
}

KeySym VirtKeyX11Impl::get_keysym_from_keycode(KeyCode keycode, ModMask modmask, KeymapGroup group)
{
    KeySym keysym;
    unsigned int mods_rtn;

    XkbTranslateKeyCode (m_kbd,
                         keycode,
                         XkbBuildCoreState (modmask, group),
                         &mods_rtn,
                         &keysym);
    return keysym;
}

/*
 * Return a string representative of the whole layout including all groups.
 * Caller takes ownership, call free() on the result.
 */
std::string VirtKeyX11Impl::get_layout_as_string()
{
    char* result = NULL;
    char* symbols;

    if (!m_kbd->names || !m_kbd->names->symbols)
        return {};

    symbols = XGetAtomName (m_xdisplay, m_kbd->names->symbols);
    if (symbols)
    {
        result = symbols;
        XFree (symbols);
    }

    return result;
}

void VirtKeyX11Impl::set_group (KeymapGroup group, bool lock)
{
    if (lock)
        XkbLockGroup (m_xdisplay, XkbUseCoreKbd, group);
    else
        XkbLatchGroup (m_xdisplay, XkbUseCoreKbd, group);

    XSync (m_xdisplay, False);
}

void VirtKeyX11Impl::set_modifiers (ModMask mod_mask, bool lock, bool press)
{
    // apply modifier change
    if (lock)
        XkbLockModifiers(m_xdisplay, XkbUseCoreKbd,
                         mod_mask, press ? mod_mask : 0);
    else
        XkbLatchModifiers(m_xdisplay, XkbUseCoreKbd,
                          mod_mask, press ? mod_mask : 0);

    XSync (m_xdisplay, False);
}

void VirtKeyX11Impl::init_keyboard()
{
    if (m_kbd)
    {
        XkbFreeKeyboard (m_kbd, XkbAllComponentsMask, True);
        m_kbd = NULL;
    }

    m_kbd = XkbGetKeyboard(m_xdisplay,
                         XkbCompatMapMask | XkbNamesMask | XkbGeometryMask,
                         XkbUseCoreKbd);
#ifndef NDEBUG
    /* test missing keyboard (LP:#526791) keyboard on/off every 10 seconds */
    if (getenv ("VIRTKEY_DEBUG"))
    {
        if (m_kbd && time(nullptr) % 20 < 10)
        {
            XkbFreeKeyboard (m_kbd, XkbAllComponentsMask, True);
            m_kbd = nullptr;
        }
    }
#endif
    if (!m_kbd)
        throw VirtKeyException("VirtKeyX11Impl::init_keyboard: XkbGetKeyboard failed.");
}

void VirtKeyX11Impl::init()
{
    GdkDisplay* display;
    gint xkb_major = XkbMajorVersion;
    gint xkb_minor = XkbMinorVersion;

    m_kbd = NULL;

    XInitThreads();

    display = gdk_display_get_default ();
    if (!GDK_IS_X11_DISPLAY (display))
        throw VirtKeyException("VirtKeyX11Impl::init: not an X display.");

    m_xdisplay = GDK_DISPLAY_XDISPLAY (display);

    // Init Xkb just in case, even though Gdk should have done so already.
    if (!XkbLibraryVersion (&xkb_major, &xkb_minor))
        throw VirtKeyException(sstr()
            << "XkbLibraryVersion failed:"
            << " compiled for v" << XkbMajorVersion << "." << XkbMinorVersion
            << " but found v" << xkb_major << "." << xkb_minor);

    xkb_major = XkbMajorVersion;
    xkb_minor = XkbMinorVersion;

    if (!XkbQueryExtension (m_xdisplay, NULL,
                            &m_xkb_base_event, NULL,
                            &xkb_major, &xkb_minor))
        throw VirtKeyException(sstr()
            << "XkbQueryExtension failed:"
            << " compiled for v" << XkbMajorVersion << "." << XkbMinorVersion
            << " but found v" << xkb_major << "." << xkb_minor);

    init_keyboard();
}

void VirtKeyX11Impl::reload()
{
    Super::reload();
    init_keyboard();
}

void VirtKeyX11Impl::send_key_event(KeyCode keycode, bool press)
{
    XTestFakeKeyEvent (m_xdisplay,
                       keycode, press, CurrentTime);
    XSync (m_xdisplay, False);
}

#endif // VIRTKEY_X11_CPP
