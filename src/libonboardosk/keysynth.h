#ifndef KEYSYNTH_H
#define KEYSYNTH_H

#include <chrono>

#include "tools/keydecls.h"
#include "tools/textdecls.h"

#include "onboardoskglobals.h"

class UString;

class KeySynth : public ContextBase
{
    public:
        using SteadyClock = std::chrono::steady_clock;
        using SteadyTimePoint = std::chrono::time_point<SteadyClock>;

        KeySynth(const ContextBase& context);
        virtual ~KeySynth();

        virtual void press_unicode(CodePoint cp) = 0;
        virtual void release_unicode(CodePoint cp) = 0;
        virtual void press_unicode(const std::string& s) = 0;
        virtual void release_unicode(const std::string& s) = 0;

        virtual void press_keysym(KeySym keysym) = 0;
        virtual void release_keysym(KeySym keysym) = 0;

        virtual void press_keycode(KeyCode keycode) = 0;
        virtual void release_keycode(KeyCode keycode) = 0;

        virtual int get_current_group() = 0;
        virtual void lock_group(int group) = 0;

        virtual void lock_mod(ModMask mod_mask) = 0;
        virtual void unlock_mod(ModMask mod_mask) = 0;

        // Send key presses for all characters in a unicode string.
        virtual void press_key_string(const UString& keystr) = 0;

    protected:
        // Pause between multiple key-strokes.
        // Firefox and Thunderbird may need this to not miss key-strokes.
        void delay_keypress();

    private:
        SteadyTimePoint m_last_press_time{};
};


class KeySynthVirtkey : public KeySynth
{
    public:
        using Super = KeySynth;

        KeySynthVirtkey(const ContextBase& context);
        virtual ~KeySynthVirtkey();

        virtual void press_unicode(CodePoint cp) override;
        virtual void release_unicode(CodePoint cp) override;
        virtual void press_unicode(const std::string& s) override;
        virtual void release_unicode(const std::string& s) override;

        virtual void press_keysym(KeySym keysym) override;
        virtual void release_keysym(KeySym keysym) override;

        virtual void press_keycode(KeyCode keycode) override;
        virtual void release_keycode(KeyCode keycode) override;

        virtual int get_current_group() override;
        virtual void lock_group(int group) override;

        virtual void lock_mod(ModMask mod_mask) override;
        virtual void unlock_mod(ModMask mod_mask) override;

        // Send key presses for all characters in a unicode string.
        virtual void press_key_string(const UString& keystr) override;
};


class KeySynthAtspi : public KeySynthVirtkey
{
    public:
        using Super = KeySynthVirtkey;

        KeySynthAtspi(const ContextBase& context);
        virtual ~KeySynthAtspi();

        virtual void press_keycode(KeyCode keycode) override;
        virtual void release_keycode(KeyCode keycode) override;
        virtual void press_key_string(const UString& s) override;
};


class KeySynthGnomeShell : public KeySynthVirtkey
{
    public:
        using Super = KeySynthVirtkey;

        KeySynthGnomeShell(const ContextBase& context);
        virtual ~KeySynthGnomeShell();

        virtual bool probe();

        virtual void press_keysym(KeySym keysym) override;
        virtual void release_keysym(KeySym keysym) override;

        virtual void press_keycode(KeyCode keycode) override;
        virtual void release_keycode(KeyCode keycode) override;
};


class NoKeySynthDelay
{
    public:
        NoKeySynthDelay(ContextBase* context) :
            m_context(context)
        {
            m_context->set_suppress_keypress_delay(true);
        }
        ~NoKeySynthDelay()
        {
            m_context->set_suppress_keypress_delay(false);
        }
    private:
        ContextBase* m_context;
};


#endif // KEYSYNTH_H
