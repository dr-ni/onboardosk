
#include <iomanip>
#include <thread>

#include <atspi/atspi.h>

#include "tools/logger.h"
#include "tools/string_helpers.h"
#include "tools/ustringmain.h"

#include "configuration.h"
#include "keyboardkeylogic.h"
#include "keysynth.h"
#include "onboardoskcallbacks.h"
#include "virtkey.h"


KeySynth::KeySynth(const ContextBase& context) :
    ContextBase(context)
{}

KeySynth::~KeySynth()
{}

void KeySynth::delay_keypress()
{
    using namespace std::chrono;

    duration<double> delay(config()->keyboard->inter_key_stroke_delay);
    if (delay > 0s)
    {
        auto now = SteadyClock::now();

        // not just single presses?
        if (!get_suppress_keypress_delay())
        {
            auto elapsed = now - m_last_press_time;
            auto remaining = delay - elapsed;
            if (remaining > 0s)
                std::this_thread::sleep_for(remaining);
        }

        m_last_press_time = now;
    }
}


KeySynthVirtkey::KeySynthVirtkey(const ContextBase& context) :
    Super(context)
{}

KeySynthVirtkey::~KeySynthVirtkey()
{}

void KeySynthVirtkey::press_unicode(CodePoint cp)
{
    LOG_DEBUG << "U+" << std::setfill('0') << std::setw(5)
              << std::hex << cp;
    auto virtkey = get_virtkey();
    if (virtkey)
    {
        KeySym keysym = virtkey->get_keysym_from_cp(cp);
        press_keysym(keysym);
    }
}

void KeySynthVirtkey::release_unicode(CodePoint cp)
{
    LOG_DEBUG << "U+" << std::setfill('0') << std::setw(5)
              << std::hex << cp;
    auto virtkey = get_virtkey();
    if (virtkey)
    {
        KeySym keysym = virtkey->get_keysym_from_cp(cp);
        release_keysym(keysym);
    }
}

void KeySynthVirtkey::press_unicode(const std::string& s)
{
    LOG_DEBUG << repr(s);
    auto virtkey = get_virtkey();
    if (virtkey)
    {
        KeySym keysym = virtkey->get_keysym_from_utf8(s);
        press_keysym(keysym);
    }
}

void KeySynthVirtkey::release_unicode(const std::string& s)
{
    LOG_DEBUG << repr(s);
    auto virtkey = get_virtkey();
    if (virtkey)
    {
        KeySym keysym = virtkey->get_keysym_from_utf8(s);
        release_keysym(keysym);
    }
}

void KeySynthVirtkey::press_keysym(KeySym keysym)
{
    LOG_DEBUG << keysym;
    auto virtkey = get_virtkey();
    if (virtkey)
    {
        ModMask mod_mask;
        int group = virtkey->get_current_group();
        KeyCode keycode = virtkey->get_keycode_from_keysym(keysym, group, mod_mask);

        // need modifiers for this keysym?
        if (mod_mask)
        {
            auto key_logic = get_key_logic();
            key_logic->lock_temporary_modifiers(
                ModSource::KEYSYNTH, mod_mask);
        }

        this->press_keycode(keycode);
    }
}

void KeySynthVirtkey::release_keysym(KeySym keysym)
{
    LOG_DEBUG << keysym;
    auto virtkey = get_virtkey();
    if (virtkey)
    {
        ModMask mod_mask;
        int group = virtkey->get_current_group();
        KeyCode keycode = virtkey->get_keycode_from_keysym(keysym, group, mod_mask);

        this->release_keycode(keycode);

        auto key_logic = get_key_logic();
        key_logic->unlock_temporary_modifiers(ModSource::KEYSYNTH);
    }
}

void KeySynthVirtkey::press_keycode(KeyCode keycode)
{
    LOG_DEBUG << static_cast<unsigned>(keycode);
    auto virtkey = get_virtkey();
    if (virtkey)
    {
        delay_keypress();
        virtkey->press_keycode(keycode);
    }
}

void KeySynthVirtkey::release_keycode(KeyCode keycode)
{
    LOG_DEBUG << static_cast<unsigned>(keycode);
    auto virtkey = get_virtkey();
    if (virtkey)
    {
        virtkey->release_keycode(keycode);
    }
}

int KeySynthVirtkey::get_current_group()
{
    auto virtkey = get_virtkey();
    if (virtkey)
        return virtkey->get_current_group();
    return 0;
}

void KeySynthVirtkey::lock_group(int group)
{
    auto virtkey = get_virtkey();
    if (virtkey)
        virtkey->lock_group(group);
}

void KeySynthVirtkey::lock_mod(ModMask mod_mask)
{
    auto virtkey = get_virtkey();
    if (virtkey)
        virtkey->lock_mod(mod_mask);
}

void KeySynthVirtkey::unlock_mod(ModMask mod_mask)
{
    auto virtkey = get_virtkey();
    if (virtkey)
        virtkey->unlock_mod(mod_mask);
}

void KeySynthVirtkey::press_key_string(const UString& keystr)
{
    UString str = replace_all(keystr, "\\n", "\n");  // for new lines in snippets

    auto virtkey = get_virtkey();
    if (virtkey)   // may be None in the last call before exiting
    {
        for (auto cp : str)
        {
            if (cp == '\b')  // backspace?
            {
                KeySym keysym = get_keysym_from_name("backspace");
                press_keysym(keysym);
                release_keysym(keysym);
            }
            else if (cp == '\n')
            {
                // press_unicode("\n") fails in gedit.
                // -> explicitely send the key symbol instead
                KeySym keysym = get_keysym_from_name("return");
                press_keysym(keysym);
                release_keysym(keysym);
            }
            else
            {   // any other printable key
                press_unicode(cp);
                release_unicode(cp);
            }
        }
    }
}


KeySynthAtspi::KeySynthAtspi(const ContextBase& context) :
    Super(context)
{}

KeySynthAtspi::~KeySynthAtspi()
{}

void KeySynthAtspi::press_keycode(KeyCode keycode)
{
    delay_keypress();
    atspi_generate_keyboard_event(keycode, "", AtspiKeySynthType::ATSPI_KEY_PRESS, nullptr);
}

void KeySynthAtspi::release_keycode(KeyCode keycode)
{
    atspi_generate_keyboard_event(keycode, "", AtspiKeySynthType::ATSPI_KEY_RELEASE, nullptr);
}

void KeySynthAtspi::press_key_string(const UString& s)
{
    atspi_generate_keyboard_event(0, s.to_utf8().c_str(), AtspiKeySynthType::ATSPI_KEY_STRING, nullptr);
}



KeySynthGnomeShell::KeySynthGnomeShell(const ContextBase& context) :
    Super(context)
{
}

KeySynthGnomeShell::~KeySynthGnomeShell()
{
}

bool KeySynthGnomeShell::probe()
{
    auto callbacks = get_global_callbacks();
    return callbacks->send_keyval_event &&
           callbacks->send_keycode_event;
}

void KeySynthGnomeShell::press_keysym(KeySym keysym)
{
    LOG_DEBUG << keysym;
    auto callbacks = get_global_callbacks();
    if (callbacks->send_keyval_event)
        callbacks->send_keyval_event(get_cinstance(),
                                     static_cast<int>(keysym), true);
}

void KeySynthGnomeShell::release_keysym(KeySym keysym)
{
    LOG_DEBUG << keysym;
    auto callbacks = get_global_callbacks();
    if (callbacks->send_keyval_event)
        callbacks->send_keyval_event(get_cinstance(),
                                     static_cast<int>(keysym), false);
}

void KeySynthGnomeShell::press_keycode(KeyCode keycode)
{
    LOG_DEBUG << keycode;
    auto callbacks = get_global_callbacks();
    if (callbacks->send_keycode_event)
        callbacks->send_keycode_event(get_cinstance(),
                                      static_cast<uint32_t>(keycode), true);
}

void KeySynthGnomeShell::release_keycode(KeyCode keycode)
{
    LOG_DEBUG << keycode;
    auto callbacks = get_global_callbacks();
    if (callbacks->send_keycode_event)
        callbacks->send_keycode_event(get_cinstance(),
                                      static_cast<uint32_t>(keycode), false);
}

