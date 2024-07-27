#ifndef VIRTKEY_H
#define VIRTKEY_H

#include <memory>
#include <string>
#include <vector>

#include "tools/keydecls.h"
#include "tools/textdecls.h"

#include "signalling.h"
#include "onboardoskglobals.h"


class VirtKeyBackend
{
    public:
        enum Enum {
            NONE,
            XTEST,
            UINPUT,
        };
};

class UInput;

class VirtKey : public ContextBase
{
    public:
        using Super = ContextBase;

        VirtKey(const ContextBase& context);
        virtual ~VirtKey();

        static std::unique_ptr<VirtKey> make_vk_wayland(const ContextBase& context);
        static std::unique_ptr<VirtKey> make_vk_x11(const ContextBase& context);

        virtual bool is_x11_display() {return false;}
        virtual KeymapGroup get_current_group();
        virtual std::string get_current_group_name() = 0;
        virtual bool get_auto_repeat_rate(unsigned int& delay, unsigned int& interval) = 0;
        virtual KeyCode get_keycode_from_keysym(KeySym keysym, KeymapGroup group, ModMask& mod_mask_out) = 0;
        virtual KeySym get_keysym_from_keycode(KeyCode keycode, ModMask modmask, KeymapGroup group) = 0;
        virtual std::string get_label_from_keycode(KeyCode keycode, ModMask modmask, KeymapGroup group) = 0;
        virtual std::vector<std::string> get_current_rules_names();
        virtual std::string get_layout_as_string() = 0;
        virtual void reload();

        virtual void set_group(KeymapGroup group, bool lock) = 0;
        virtual void set_modifiers(ModMask mod_mask, bool lock, bool press) = 0;
        virtual void send_key_event(KeyCode keycode, bool press) = 0;

        std::vector<std::string> get_labels_from_keycode(KeyCode keycode, std::vector<ModMask> modmasks);
        std::vector<KeySym> get_keysyms_from_keycode(KeyCode keycode, std::vector<ModMask> modmasks);
        KeySym get_keysym_from_cp(CodePoint cp);
        KeySym get_keysym_from_utf8(const std::string& s);

        void select_backend (VirtKeyBackend::Enum backend,
                             const std::string& device_name={});
        void close_backend();

        void press_keycode(KeyCode c);
        void release_keycode(KeyCode c);
        void lock_group(KeymapGroup group);
        void lock_mod(ModMask mask);
        void unlock_mod(ModMask mask);
        void latch_mod(ModMask mask);
        void unlatch_mod(ModMask mask);

    protected:
        std::string get_label_from_keysym(KeySym keysym);

    private:
        void set_backend_group(KeymapGroup group, bool lock);
        void set_backend_modifiers(ModMask mod_mask, bool lock, bool press);
        void send_backend_key_event(KeyCode keycode, bool press);

    public:
        DEFINE_SIGNAL(<>, keymap_changed, this);
        DEFINE_SIGNAL(<>, group_changed, this);

    private:
        VirtKeyBackend::Enum m_backend_id;
        std::unique_ptr<UInput> m_uinput;

        KeymapGroup m_current_group{INVALID_GROUP};
        std::vector<std::string> m_current_rules_names;  // layout, variant, etc. of the current group
};

#endif // VIRTKEY_H
