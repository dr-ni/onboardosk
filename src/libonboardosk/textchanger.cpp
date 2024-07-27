
#include <vector>

#include "tools/logger.h"
#include "tools/iostream_helpers.h"
#include "tools/ustringmain.h"

#include "configuration.h"
#include "keyboard.h"
#include "keyboarddecls.h"
#include "keyboardkeylogic.h"
#include "keydefinitions.h"
#include "keysynth.h"
#include "textchanger.h"
#include "textcontext.h"
#include "timer.h"
#include "virtkey.h"

using std::vector;


TextChanger::TextChanger(const ContextBase& context) :
    ContextBase(context)
{}

TextChanger::~TextChanger()
{}


TextChangerKeyStroke::TextChangerKeyStroke(const ContextBase& context) :
    Super(context),
    m_key_synth_virtkey(std::make_unique<KeySynthVirtkey>(context)),
    m_key_synth_atspi(std::make_unique<KeySynthAtspi>(context)),
    m_key_synth_gnome_shell(std::make_unique<KeySynthGnomeShell>(context))
{
    update_key_synth();
    m_connections.connect(config()->keyboard->key_synth.changed,
                          [&]{update_key_synth();});
}

TextChangerKeyStroke::~TextChangerKeyStroke()
{}

void TextChangerKeyStroke::update_key_synth()
{
    vector<KeySynthEnum::Enum> key_synth_candidates;

    KeySynthEnum::Enum key_synth_id = config()->keyboard->key_synth;
    if (key_synth_id == KeySynthEnum::AUTO)
        key_synth_candidates = {
            KeySynthEnum::GNOME_SHELL,
            KeySynthEnum::XTEST,
            KeySynthEnum::UINPUT,
            KeySynthEnum::ATSPI,
        };
    else
        key_synth_candidates = {
            key_synth_id,
        };

    LOG_DEBUG << "KeySynth candidates: " << key_synth_candidates;

    key_synth_id = {};
    KeySynth* key_synth = nullptr;
    for (auto id : key_synth_candidates)
    {
        if (id == KeySynthEnum::ATSPI)
        {
            key_synth = m_key_synth_atspi.get();
            key_synth_id = id;
            break;
        }
        else if (id == KeySynthEnum::GNOME_SHELL)
        {
            if (m_key_synth_gnome_shell->probe())
            {
                key_synth = m_key_synth_gnome_shell.get();
                key_synth_id = id;
                break;
            }
        }
        else
        {
            auto vk = get_virtkey();
            if (!vk)
            {
                LOG_DEBUG << "KeySynth '" << id << "' unavailable: no virtkey";
            }
            else
            {
                key_synth = m_key_synth_virtkey.get();
                try
                {
                    if (id == KeySynthEnum::XTEST)
                        vk->select_backend(VirtKeyBackend::XTEST);
                    else if (id == KeySynthEnum::UINPUT)
                        vk->select_backend(VirtKeyBackend::UINPUT,
                                           UINPUT_DEVICE_NAME);
                    key_synth_id = id;
                    break;
                }
                catch (const VirtKeyException& ex)
                {
                    LOG_DEBUG << "KeySynth " << id
                              << " unavailable: " << ex.what();
                }
            }
        }
    }

    LOG_INFO << "using KeySynth " << key_synth_id;

    m_key_synth = key_synth;
}

void TextChangerKeyStroke::press_unicode(const std::string& c)
{
    m_key_synth->press_unicode(c);
}

void TextChangerKeyStroke::release_unicode(const std::string& c)
{
    m_key_synth->release_unicode(c);
}

void TextChangerKeyStroke::press_keysym(KeySym keysym)
{
    m_key_synth->press_keysym(keysym);
}

void TextChangerKeyStroke::release_keysym(KeySym keysym)
{
    m_key_synth->release_keysym(keysym);
}

void TextChangerKeyStroke::press_keycode(KeyCode keycode)
{
    m_key_synth->press_keycode(keycode);
}

void TextChangerKeyStroke::release_keycode(KeyCode keycode)
{
    m_key_synth->release_keycode(keycode);
}

int TextChangerKeyStroke::get_current_group()
{
    return m_key_synth->get_current_group();
}

void TextChangerKeyStroke::lock_group(int group)
{
    m_key_synth->lock_group(group);
}

void TextChangerKeyStroke::lock_mod(ModMask mod)
{
    m_key_synth->lock_mod(mod);
}

void TextChangerKeyStroke::unlock_mod(ModMask mod)
{
    m_key_synth->unlock_mod(mod);
}

void TextChangerKeyStroke::press_key_string(const UString& s)
{
    m_key_synth->press_key_string(s);
}

void TextChangerKeyStroke::press_keysyms(const std::string& key_name, size_t count)
{
    KeySym keysym = get_keysym_from_name(key_name);
    for (size_t i=0; i<count; i++)
    {
        press_keysym(keysym);
        release_keysym(keysym);
    }
}

void TextChangerKeyStroke::insert_string_at_caret(const UString& text)
{
    m_key_synth->press_key_string(text);
}

void TextChangerKeyStroke::delete_at_caret()
{
    SuppressModifiers sm(get_key_logic());
    this->press_keysyms("backspace");
}




TextChangerDirectInsert::TextChangerDirectInsert(const ContextBase& context,
                                                 TextChangerKeyStroke* tcks) :
    Super(context),
    m_text_changer_key_stroke(tcks),
    m_auto_repeat_delay_timer(std::make_unique<Timer>(context)),
    m_auto_repeat_interval_timer(std::make_unique<Timer>(context))
{
    auto virtkey = get_virtkey();
    unsigned int delay = 500;
    unsigned int interval = 30;

    if (virtkey)
        virtkey->get_auto_repeat_rate(delay, interval);

    m_auto_repeat_delay = std::chrono::milliseconds(delay);
    m_auto_repeat_interval = std::chrono::milliseconds(interval);

    LOG_DEBUG << "auto-repeat:"
              << " delay=" << m_auto_repeat_delay.count()
              << " interval=" << m_auto_repeat_interval.count();
}

TextChangerDirectInsert::~TextChangerDirectInsert()
{
    stop_auto_repeat();
}

TextContext*TextChangerDirectInsert::get_text_context()
{
    return get_keyboard()->get_text_context();
}

void TextChangerDirectInsert::insert_unicode(const std::string& s)
{
    auto text_context = get_text_context();
    if (text_context)
        text_context->insert_text_at_caret(s);
}

void TextChangerDirectInsert::start_auto_repeat(const std::string& s)
{
    m_auto_repeat_delay_timer->start(m_auto_repeat_delay,
                                     [s, this] () -> bool
    {on_auto_repeat_delay_timer(s); return false;});
}

void TextChangerDirectInsert::stop_auto_repeat()
{
    m_auto_repeat_delay_timer->stop();
    m_auto_repeat_interval_timer->stop();
}

void TextChangerDirectInsert::on_auto_repeat_delay_timer(const std::string& s)
{
    m_auto_repeat_interval_timer->start(m_auto_repeat_interval,
                                        [s, this] () -> bool
                                        {on_auto_repeat_interval_timer(s); return true;});
}

void TextChangerDirectInsert::on_auto_repeat_interval_timer(const std::string& s)
{
    insert_unicode(s);
}

void TextChangerDirectInsert::press_keycode(KeyCode keycode)
{
    stop_auto_repeat();
    m_text_changer_key_stroke->press_keycode(keycode);
}

void TextChangerDirectInsert::release_keycode(KeyCode keycode)
{
    m_text_changer_key_stroke->release_keycode(keycode);
}

void TextChangerDirectInsert::press_keysym(KeySym keysym)
{
    stop_auto_repeat();
    m_text_changer_key_stroke->press_keysym(keysym);
}

void TextChangerDirectInsert::release_keysym(KeySym keysym)
{
    m_text_changer_key_stroke->release_keysym(keysym);
}

void TextChangerDirectInsert::press_unicode(const std::string& s)
{
    insert_unicode(s);
    start_auto_repeat(s);
}

void TextChangerDirectInsert::release_unicode(const std::string& s)
{
    (void)s;
    stop_auto_repeat();
}

int TextChangerDirectInsert::get_current_group()
{
    return m_text_changer_key_stroke->get_current_group();
}

void TextChangerDirectInsert::lock_group(int group)
{
    m_text_changer_key_stroke->lock_group(group);
}

void TextChangerDirectInsert::lock_mod(ModMask mod)
{
    m_text_changer_key_stroke->lock_mod(mod);
}

void TextChangerDirectInsert::unlock_mod(ModMask mod)
{
    m_text_changer_key_stroke->unlock_mod(mod);
}

void TextChangerDirectInsert::press_keysyms(const std::string& key_name, size_t count)
{
    m_text_changer_key_stroke->press_keysyms(key_name, count);
}

void TextChangerDirectInsert::insert_string_at_caret(const UString& s)
{
    auto text_context = get_text_context();
    if (text_context)
    {
        auto text = replace_all(s, "\\n", "\n");
        text_context->insert_text_at_caret(text);
    }
}

void TextChangerDirectInsert::delete_at_caret()
{
    auto text_context = get_text_context();
    if (text_context)
        text_context->delete_text_before_caret(1);
}
