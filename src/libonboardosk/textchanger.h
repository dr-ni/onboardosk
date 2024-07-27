#ifndef TEXTCHANGER_H
#define TEXTCHANGER_H

#include <chrono>
#include <memory>
#include <string>

#include "tools/keydecls.h"

#include "onboardoskglobals.h"
#include "signalling.h"

class KeySynth;
class KeySynthVirtkey;
class KeySynthAtspi;
class KeySynthGnomeShell;
class TextContext;
class Timer;
class UString;

// Abstract base class of TextChangers.
class TextChanger : public ContextBase
{
    public:
        TextChanger(const ContextBase& context);
        virtual ~TextChanger();

        // KeySynth interface
        virtual void press_unicode(const std::string& c) = 0;
        virtual void release_unicode(const std::string& c) = 0;
        virtual void press_keysym(KeySym keysym) = 0;
        virtual void release_keysym(KeySym keysym) = 0;
        virtual void press_keycode(KeyCode keycode) = 0;
        virtual void release_keycode(KeyCode keycode) = 0;
        virtual int get_current_group() = 0;
        virtual void lock_group(int group) = 0;
        virtual void lock_mod(ModMask mod) = 0;
        virtual void unlock_mod(ModMask mod) = 0;

        // Higher-level functions
        virtual void press_key_string(const UString& s) = 0;

        // Generate any number of full key-strokes for the given named key symbol.
        virtual void press_keysyms(const std::string& key_name, size_t count=1) = 0;

        // Insert text at the caret position.
        virtual void insert_string_at_caret(const UString& text) = 0;

        virtual void delete_at_caret() = 0;
};


// Insert and delete text with key-strokes.
// - KeySynthVirtkey
// - KeySynthAtspi (not used by default)
class TextChangerKeyStroke : public TextChanger
{
    public:
        using Super = TextChanger;

        TextChangerKeyStroke(const ContextBase& context);
        virtual ~TextChangerKeyStroke();

        void update_key_synth();

        // KeySynth interface
        virtual void press_unicode(const std::string& c) override;
        virtual void release_unicode(const std::string& c) override;
        virtual void press_keysym(KeySym keysym) override;
        virtual void release_keysym(KeySym keysym) override;
        virtual void press_keycode(KeyCode keycode) override;
        virtual void release_keycode(KeyCode keycode) override;
        virtual int get_current_group() override;
        virtual void lock_group(int group) override;
        virtual void lock_mod(ModMask mod) override;
        virtual void unlock_mod(ModMask mod) override;

        // Higher-level functions
        virtual void press_key_string(const UString& s) override;

        // Generate any number of full key-strokes for the given named key symbol.
        virtual void press_keysyms(const std::string& key_name, size_t count=1) override;

        // Insert text at the caret position.
        virtual void insert_string_at_caret(const UString& text) override;

        virtual void delete_at_caret() override;

    private:
        std::unique_ptr<KeySynthVirtkey> m_key_synth_virtkey;
        std::unique_ptr<KeySynthAtspi> m_key_synth_atspi;
        std::unique_ptr<KeySynthGnomeShell> m_key_synth_gnome_shell;
        KeySynth* m_key_synth{};

        SignalConnections m_connections;
};


// Insert and delete text by direct insertion/deletion.
// - Direct insertion/deletion via AtspiTextContext
class TextChangerDirectInsert : public TextChanger
{
    public:
        using Super = TextChanger;

        TextChangerDirectInsert(const ContextBase& context, TextChangerKeyStroke* tcks);
        virtual ~TextChangerDirectInsert();

        TextContext* get_text_context();

        void insert_unicode(const std::string& s);

        void start_auto_repeat(const std::string& s);
        void stop_auto_repeat();

        void on_auto_repeat_delay_timer(const std::string& s);

        void on_auto_repeat_interval_timer(const std::string& s);

        // KeySynth interface
        // Use key-strokes because of dead keys, hot keys and editing keys.
        void press_keycode(KeyCode keycode) override;

        void release_keycode(KeyCode keycode) override;

        // Use key-strokes because of dead keys, hot keys and editing keys.
        virtual void press_keysym(KeySym keysym) override;

        virtual void release_keysym(KeySym keysym) override;

        virtual void press_unicode(const std::string& s) override;

        virtual void release_unicode(const std::string& s) override;

        virtual int get_current_group() override;

        virtual void lock_group(int group) override;

        // We still have to lock mods for pointer clicks with modifiers
        // and hot-keys.
        virtual void lock_mod(ModMask mod) override;

        virtual void unlock_mod(ModMask mod) override;

        // Higher-level functions
        virtual void press_key_string(const UString& s) override
        {
            (void)s;
        }

        // Generate any number of full key-strokes for the given named key symbol.
        virtual void press_keysyms(const std::string& key_name, size_t count=1) override;

        // Insert text at the caret position.
        virtual void insert_string_at_caret(const UString& s) override;

        virtual void delete_at_caret() override;
    private:
        TextChangerKeyStroke* m_text_changer_key_stroke{};

        std::unique_ptr<Timer> m_auto_repeat_delay_timer;
        std::unique_ptr<Timer> m_auto_repeat_interval_timer;
        std::chrono::milliseconds m_auto_repeat_delay;
        std::chrono::milliseconds m_auto_repeat_interval;
};



#endif // TEXTCHANGER_H
