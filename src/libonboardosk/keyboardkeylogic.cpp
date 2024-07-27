
#include <array>

#include "tools/container_helpers.h"
#include "tools/iostream_helpers.h"
#include "tools/string_helpers.h"
#include "tools/logger.h"
#include "tools/noneable.h"
#include "tools/process_helpers.h"

#include "buttoncontroller.h"
#include "canonicalequivalents.h"
#include "configuration.h"
#include "inputsequence.h"
#include "keyboard.h"
#include "keyboardview.h"
#include "keyboardkeylogic.h"
#include "keysynth.h"
#include "layoutkey.h"
#include "layoutview.h"
#include "timer.h"
#include "textchanger.h"
#include "wordsuggestions.h"


// Releases latched and locked modifiers after a period of inactivity.
// Inactivity here means no keys are pressed.
class AutoReleaseTimer : public Timer
{
    public:
        using Super = Timer;
        AutoReleaseTimer(const ContextBase& context) :
            Super(context)
        {}

        void start(const Noneable<bool>& visibility_change={})
        {
            stop();
            double delay = config()->keyboard->sticky_key_release_delay;
            if (visibility_change == false)
            {
                double hide_delay = config()->keyboard->sticky_key_release_on_hide_delay;
                if (hide_delay != 0.0)
                {
                    if (delay != 0.0)
                        delay = std::min(delay, hide_delay);
                    else
                        delay = hide_delay;
                }
            }
            if (delay != 0.0)
            {
                Super::start(delay);
            }
        }

        virtual bool on_timer() override
        {
            // When sticky_key_release_delay is set, release NumLock too.
            // We then assume Onboard is used in a kiosk setting, and
            // everything has to be reset for the next customer.
            bool release_all_keys = config()->keyboard->sticky_key_release_delay != 0.0;
            if (release_all_keys)
                config()->word_suggestions->set_pause_learning(
                            PauseLearning::OFF);

            auto keyboard = get_keyboard();
            keyboard->get_key_logic()->release_latched_sticky_keys();
            keyboard->get_key_logic()->release_locked_sticky_keys(release_all_keys);
            keyboard->set_active_layer_id({});
            keyboard->invalidate_ui_no_resize();
            keyboard->commit_ui_updates();

            return false;  // one-shot
        }
};

// Redraw keys unpressed after a short while.
// There are multiple timers to suppurt multi-touch.
class UnpressTimers : public ContextBase
{
    public:
        using Super = ContextBase;

        UnpressTimers(const ContextBase& context) :
            Super(context)
        {}

        void start(const LayoutKeyPtr& key)
        {
            // Make sure to stop timers when keys are destroyed
            m_connections.connect(key->destroy_notify,
                                  [this, key]{finish(key);});

            auto it = m_timers.find(key);
            if (it == m_timers.end())
            {
                auto pair = m_timers.emplace(key, *this);
                it = pair.first;
            }
            Timer& timer = it->second;
            timer.start(config()->unpress_delay, [this, key]()
                                                 {return on_timer(key);});
        }

        void stop(const LayoutKeyPtr& key)
        {
            m_connections.disconnect(key->destroy_notify);

            auto it = m_timers.find(key);
            if (it != m_timers.end())
            {
                Timer& timer = it->second;
                timer.stop();
                m_timers.erase(it);
            }
        }

        void cancel_all()
        {
            for (auto& it : m_timers)
            {
                LayoutKeyPtr key = it.first;
                Timer& timer = it.second;
                timer.stop();
                key->pressed = false;
            }
            m_timers.clear();
        }

        void finish_all()
        {
            for (auto& it : m_timers)
            {
                LayoutKeyPtr key = it.first;
                Timer& timer = it.second;
                timer.stop();
                unpress(key);
            }
            m_timers.clear();
        }

        void finish(const LayoutKeyPtr& key)
        {
            auto it = m_timers.find(key);
            if (it != m_timers.end())
            {
                Timer& timer = it->second;
                timer.stop();
                unpress(key);
                m_timers.erase(it);
            }
        }

        bool on_timer(const LayoutKeyPtr& key)
        {
            unpress(key);
            stop(key);
            return false;
        }

        void unpress(const LayoutKeyPtr& key)
        {
            get_key_logic()->key_unpress(key);
        }

    private:
        std::map<LayoutKeyPtr, Timer, std::owner_less<LayoutKeyPtr>> m_timers;
        SignalConnections m_connections;
};


KeyboardKeyLogic::KeyboardKeyLogic(const ContextBase& context) :
    Super(context),
    m_auto_release_timer(std::make_unique<AutoReleaseTimer>(context)),
    m_long_press_timer(std::make_unique<Timer>(context)),
    m_unpress_timers(std::make_unique<UnpressTimers>(context)),
    m_text_changer_key_stroke(std::make_unique<TextChangerKeyStroke>(context)),
    m_text_changer_direct_insert(std::make_unique<TextChangerDirectInsert>(context,
                                                                           m_text_changer_key_stroke.get()))
{}

KeyboardKeyLogic::~KeyboardKeyLogic()
{}

TextChanger* KeyboardKeyLogic::get_text_changer()
{
    TextChanger* tc = get_text_changer_keystroke();

    auto ws = get_word_suggestions();
    if (!ws && ws->can_direct_insert_text())
        tc = m_text_changer_direct_insert.get();

    return tc;
}

TextChangerKeyStroke* KeyboardKeyLogic::get_text_changer_keystroke()
{
    return m_text_changer_key_stroke.get();
}

TextChangerDirectInsert* KeyboardKeyLogic::get_text_changer_direct_insert()
{
    return m_text_changer_direct_insert.get();
}

ModMask KeyboardKeyLogic::get_mod_mask()
{
    return m_mods.get_mod_mask();
}

// Outer layer of key functions. Call on user interaction only.
void KeyboardKeyLogic::interactive_key_down(LayoutView* view,
                                            const InputSequencePtr& sequence)
{
    key_down(sequence->active_key, view, sequence);
    m_auto_release_timer->start();
}

void KeyboardKeyLogic::interactive_key_down_update(LayoutView* view,
                                                   const InputSequencePtr& sequence,
                                                   const LayoutKeyPtr& old_key)
{
    assert(!old_key || !old_key->activated); // old_key must be undoable
    key_up(old_key, view, sequence, false);
    key_down(sequence->active_key, view, sequence, false);
}

void KeyboardKeyLogic::interactive_key_up(LayoutView* view,
                                          const InputSequencePtr& sequence)
{
    key_up(sequence->active_key, view, sequence,
           !sequence->cancel_key_action);
}

// Second layer of key functions. These may be called e.g. by the scanner.
void KeyboardKeyLogic::key_down(const LayoutKeyPtr& key,
                                LayoutView* view,
                                const InputSequencePtr& sequence,
                                bool action)
{
    auto keyboard = get_keyboard();
    auto keyboard_view = get_keyboard_view();
    auto word_suggestion = keyboard->get_word_suggestions();

    LOG_DEBUG << "key_down " << sequence;

    word_suggestion->on_any_key_down();

    MouseButton::Enum button;
    EventType::Enum event_type;
    if (sequence)
    {
        button = sequence->button;
        event_type = sequence->event_type;
    }
    else
    {
        button = MouseButton::LEFT;
        event_type = EventType::CLICK;
    }

    if (key &&
        key->sensitive)
    {
        // Stop hiding the keyboard until all keys have been released.
        keyboard_view->lock_visibility();

        // stop timed redrawing for this key
        m_unpress_timers->stop(key);

        // announce temporary modifiers
        ModMask temp_mod_mask = 0;
        if (config()->keyboard->can_upper_case_on_button(button))
            temp_mod_mask = Modifier::SHIFT;

        set_temporary_modifiers(temp_mod_mask);
        update_temporary_key_label(key, temp_mod_mask);

        // mark key pressed
        key->pressed = true;
        keyboard->on_key_pressed(key, view, sequence, action);

        // Get drawing behind us now, so it can't delay processing key_up()
        // and cause unwanted key repeats on slow systems.
        keyboard_view->redraw_item(key);
        if (view)
            view->process_updates();

        // perform key action (!just dragging)?
        if (action)
        {
            do_key_down_action(key, view, button, event_type);

            // Make note that it was us who just sent text
            // (vs. at-spi update due to scrolling, physical typing, ...).
            // -> disables set_modifiers() for the case that virtkey
            // just locked temporary modifiers.
            if (is_text_insertion_key(key))
                set_currently_typing();
        }

        // remember as pressed key
        if (!contains(m_pressed_keys, key))
            m_pressed_keys.emplace_back(key);
    }
}

void KeyboardKeyLogic::key_up(const LayoutKeyPtr& key,
                              LayoutView* view,
                              const InputSequencePtr& sequence,
                              bool action)
{
    auto keyboard = get_keyboard();
    auto keyboard_view = get_keyboard_view();

    LOG_DEBUG << "key_up " << sequence;

    MouseButton::Enum button;
    EventType::Enum event_type;
    if (sequence)
    {
        button = sequence->button;
        event_type = sequence->event_type;
    }
    else
    {
        button = MouseButton::LEFT;
        event_type = EventType::CLICK;
    }

    if (key &&
       key->sensitive)
    {
        // Was the key nothing but pressed before?
        bool extend_pressed_state = key->is_pressed_only();

        // perform key action?
        // (not just dragging or canceled due to long press)
        if (action)
        {
            // If there was no down action yet (dragging), catch up now
            if (!key->activated)
                do_key_down_action(key, view, button, event_type);

            do_key_up_action(key, view, button, event_type);

            // Skip context && button controller updates for the common
            // letter press to improve responsiveness on slow systems.
            if (key->type == KeyType::BUTTON)
                keyboard->invalidate_context_ui();
        }

        // no action but key was activated: must have been a long press
        else if (key->activated)
        {
            // switch to layer 0 after long pressing snippet buttons
            if (key->type == KeyType::MACRO)
                keyboard->maybe_switch_to_first_layer(key);
        }

        // Is the key still nothing but pressed?
        extend_pressed_state = extend_pressed_state &&
                               key->is_pressed_only() &&
                               action;

        // Draw key unpressed to remove the visual feedback.
        if (extend_pressed_state &&
           !config()->scanner->enabled)
        {
            // Keep key pressed for a little longer for clear user feedback.
            m_unpress_timers->start(key);
        }
        else
        {
            // Unpress now to avoid flickering of the
            // pressed color after key release.
            key_unpress(key);
            keyboard->invalidate_key(key);
        }

        // no more actions left to finish
        key->activated = false;

        // remove from list of pressed keys
        remove(m_pressed_keys, key);
        auto& v = m_pressed_keys;
        auto it = std::find(v.begin(), v.end(), key);
        if (it != v.end())
            v.erase(it);

        // Make note that it was us who just sent text
        // (vs. at-spi update due to scrolling, physical typing, ...).
        if (is_text_insertion_key(key))
            set_currently_typing();
    }

    // Was this the final touch sequence?
    if (!keyboard_view->has_input_sequences())
    {
        m_non_modifier_released = false;
        m_pressed_keys.clear();
        keyboard->on_all_keys_up();

        // Allow hiding the keyboard again (LP //1648543).
        keyboard_view->unlock_and_apply_visibility();
    }

    // Process pending UI updates
    //keyboard->commit_ui_updates();
}

bool KeyboardKeyLogic::key_long_press(const LayoutKeyPtr& key,
                                      LayoutView* view,
                                      MouseButton::Enum button)
{
    auto keyboard = get_keyboard();
    auto keyboard_view = get_keyboard_view();

    LOG_DEBUG << "key_long_press " << key;

    bool long_pressed = false;
    KeyType key_type = key->type;

    if (!config()->is_xid_mode())
    {
        // Is there a popup definition in the layout?
        auto sublayout = key->get_popup_layout();
        if (sublayout)
        {
            view->show_popup_layout(key, sublayout);
            long_pressed = true;
        }

        else if (key_type == KeyType::BUTTON)
        {
            // Buttons decide for themselves what is to happen.
            auto controller = keyboard->get_button_controller(key);
            if (controller)
                controller->long_press(view, button);
        }

        else
        if (key->is_prediction_key())
        {
            view->show_prediction_menu(key, button);
            long_pressed = true;
        }

        else
        if (key_type == KeyType::MACRO)
        {
            int snippet_id = to_int(key->macro_id);
            view->edit_snippet(snippet_id);
            long_pressed = true;
        }
        else
        {
            // All other keys get hard-coded long press menus
            // (where available).
            KeyAction::Enum action = get_key_action(key);
            if (action == KeyAction::DELAYED_STROKE &&
               !key->is_word_suggestion())
            {
                string label = key->get_label();
                auto alternatives = find_canonical_equivalents(label);
                if (!alternatives.empty())
                {
                    keyboard_view->hide_touch_feedback(key);
                    view->show_popup_alternative_chars(key, alternatives);
                    long_pressed = true;
                }
            }
        }
    }
    return long_pressed;
}

void KeyboardKeyLogic::key_unpress(const LayoutKeyPtr& key)
{
    if (key->pressed)
    {
        key->pressed = false;
        get_keyboard()->on_key_unpressed(key);
    }
}

void KeyboardKeyLogic::do_key_down_action(const LayoutKeyPtr& key,
                                          LayoutView* view,
                                          MouseButton::Enum button,
                                          EventType::Enum event_type)
{
    auto keyboard = get_keyboard();

    // generate key-stroke
    KeyAction::Enum action = get_key_action(key);
    bool can_send_key = ((!key->sticky || !key->active) &&
                         action != KeyAction::DELAYED_STROKE);
    if (can_send_key)
        send_key_down(key, view, button, event_type);

    // Modifier keys may change multiple keys
    // -> redraw all dependent keys
    // no danger of key repeats due to delays
    // -> redraw asynchronously
    if (can_send_key && key->is_modifier())
        keyboard->invalidate_labels_text();

    if (key->is_button())
    {
        auto controller = keyboard->get_button_controller(key);
        if (controller)
            key->activated = controller->is_activated_on_press();
    }
}

void KeyboardKeyLogic::do_key_up_action(const LayoutKeyPtr& key,
                                        LayoutView* view,
                                        MouseButton::Enum button,
                                        EventType::Enum event_type)
{
    auto keyboard = get_keyboard();

    if (key->sticky)
    {
        bool can_send_key;

        // Multi-touch release?
        if (key->is_modifier() &&
            !can_cycle_modifiers())
        {
            can_send_key = true;
        }
        else // single touch/click
        {
            can_send_key = step_sticky_key(key, button, event_type);
        }

        if (can_send_key)
        {
            send_key_up(key, view);
            if (key->is_modifier())
                keyboard->invalidate_labels_text();
        }
    }
    else
    {
        release_non_sticky_key(key, view, button, event_type);
    }

    // Multi-touch: temporarily stop cycling modifiers if
    // a non-modifier key was pressed. This way we get both,
    // cycling latched && locked state with single presses
    // && press-only action for multi-touch modifer + key press.
    if (!key->is_modifier())
        m_non_modifier_released = true;
}

void KeyboardKeyLogic::send_key_down(const LayoutKeyPtr& key,
                                     LayoutView* view,
                                     MouseButton::Enum button,
                                     EventType::Enum event_type)
{
    auto keyboard = get_keyboard();
    auto word_suggestions = keyboard->get_word_suggestions();

    if (is_key_disabled(key))
    {
        LOG_DEBUG << "rejecting blacklisted key action for "
                  << repr(key->get_id());
        return;
    }

    auto modifier = key->modifier;

    if (modifier == Modifier::ALT &&
        is_alt_special())
    {
        m_last_alt_key = key;
    }
    else
    {
        KeyAction::Enum action = get_key_action(key);
        if (action != KeyAction::DELAYED_STROKE)
        {
            if (word_suggestions)
                word_suggestions->on_before_key_press(key);  // for word suggestions

            maybe_send_alt_press_for_key(key, view, button, event_type);
            maybe_lock_temporary_modifiers_for_key(key);

            send_key_press(key, view, button, event_type);
        }

        if (action == KeyAction::DOUBLE_STROKE)    // e.g. CAPS
            send_key_release(key, view, button, event_type);
    }

    if (modifier)
    {
        do_lock_modifiers(modifier);

        // Update word suggestions on shift press.
        keyboard->invalidate_context_ui();

        key->activated = true;  // modifiers set -> can't undo press anymore
    }
}

void KeyboardKeyLogic::send_key_up(const LayoutKeyPtr& key, LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
{
    auto keyboard = get_keyboard();
    auto word_suggestions = keyboard->get_word_suggestions();
    auto text_changer = get_text_changer();

    if (is_key_disabled(key))
    {
        LOG_DEBUG << "send_key_up: rejecting blacklisted key action for "
                  << repr(key->get_id());
        return;
    }

    auto modifier = key->modifier;
    auto action = get_key_action(key);

    // Unlock most modifiers before key release, otherwise Compiz wall
    // plugin's viewport switcher window doesn't close after
    // Alt+Ctrl+Up/Down (LP: #1532254).
    if (modifier &&
        action != KeyAction::DOUBLE_STROKE)  // not NumLock, CAPS
    {
        do_unlock_modifiers(modifier);

        // Update word suggestions on shift unlatch || release.
        keyboard->invalidate_context_ui();
    }

    // generate key event(s)
    if (modifier == Modifier::ALT &&
        is_alt_special())
    {
        // do nothing
    }
    else
    {
        if (action == KeyAction::DOUBLE_STROKE ||
            action == KeyAction::DELAYED_STROKE)
        {
            if (word_suggestions)
                word_suggestions->on_before_key_press(key);

            maybe_send_alt_press_for_key(key, view, button, event_type);
            maybe_lock_temporary_modifiers_for_key(key);

            if (key->type == KeyType::CHAR)
            {
                // allow direct text insertion by AT-SPI for char keys
                text_changer->insert_string_at_caret(key->keystr);
            }
            else
            {
                send_key_press(key, view, button, event_type);
                send_key_release(key, view, button, event_type);
            }
        }
        else
        {
            send_key_release(key, view, button, event_type);
        }
    }

    // Unlock NumLock, CAPS, etc. after key events were sent,
    // else they are toggled right back on.
    if (modifier &&
        action == KeyAction::DOUBLE_STROKE)
    {
        do_unlock_modifiers(modifier);

        // Update word suggestions on shift unlatch or release.
        keyboard->invalidate_context_ui();
    }

    maybe_unlock_temporary_modifiers();
    maybe_send_alt_release_for_key(key, view, button, event_type);

    // Check modifier counts for plausibility.
    // There might be a bug lurking that gets certain modifers stuck
    // with negative counts. Work around this && be verbose about it
    // so we can fix it eventually.
    // Seems fixed in 0.99, but keep the check just in case.
    // Happens again since 1.1.0 when using physical keyboards in
    // parallel with Onboard. Occasionally we fail to detect where a
    // modifier change originated from.
    for (size_t i=0; i<m_mods.size(); i++)
    {
        auto mod = m_mods.mod_at(i);
        auto nkeys = m_mods[mod];
        if (nkeys < 0)
        {
            LOG_WARNING << "Negative count " << m_mods[mod]
                        << " for modifier " << mod << ", reset.";
            m_mods[mod] = 0;

            // Reset this too, else unlatching won't happen until restart.
            m_external_mod_changes[mod] = 0;
        }
    }
}

void KeyboardKeyLogic::update_temporary_key_label(const LayoutKeyPtr& key, ModMask temp_mod_mask)
{
    ModMask mod_mask = get_mod_mask();
    temp_mod_mask |= mod_mask;
    if (key->m_mod_mask != temp_mod_mask)
        key->configure_labels(temp_mod_mask);
}

void KeyboardKeyLogic::set_temporary_modifiers(ModMask mod_mask)
{
    // only some single modifiers supported at this time
    if (!mod_mask ||
        contains(array_of<Modifier::Enum>(Modifier::SHIFT, Modifier::CAPS, Modifier::CTRL,
                 Modifier::SUPER, Modifier::ALTGR),
                 static_cast<Modifier::Enum>(mod_mask)))
    {
        m_temporary_modifiers = mod_mask;
    }
}

void KeyboardKeyLogic::maybe_lock_temporary_modifiers_for_key(const LayoutKeyPtr& key)
{
    ModMask modifier = m_temporary_modifiers;
    if (modifier &&
        key->modifier != modifier &&
        !key->is_button())
    {
        lock_temporary_modifiers(ModSource::KEYBOARD, modifier);
    }
}

void KeyboardKeyLogic::maybe_unlock_temporary_modifiers()
{
    unlock_all_temporary_modifiers();
}

void KeyboardKeyLogic::lock_temporary_modifiers(ModSource::Enum mod_source_id, ModMask mod_mask)
{
    //ModMaskStack* stack = set_default(m_locked_temporary_modifiers, mod_source_id, ModMaskStack{});
    auto& stack = set_default(m_locked_temporary_modifiers, mod_source_id, ModMaskStack{});
    stack.emplace_back(mod_mask);

    LOG_DEBUG << "lock_temporary_modifiers(" << repr(mod_source_id)
              << ", " << mod_mask
              << ") " << m_locked_temporary_modifiers;

    do_lock_modifiers(mod_mask);
}

void KeyboardKeyLogic::unlock_temporary_modifiers(ModSource::Enum mod_source_id)
{
    auto it = m_locked_temporary_modifiers.find(mod_source_id);
    if (it != m_locked_temporary_modifiers.end())
    {
        auto& stack = it->second;
        //assert(!stack.empty());
        if (stack.empty())
        {
            LOG_ERROR << "stack empty " << repr(mod_source_id)
                      << " " << m_locked_temporary_modifiers;
        }
        else
        {
            ModMask mod_mask = stack.back();
            stack.pop_back();

            LOG_DEBUG << "unlock_temporary_modifiers(" << repr(mod_source_id)
                      << ", " << mod_mask
                      << ") " << m_locked_temporary_modifiers;

            do_unlock_modifiers(mod_mask);
        }
    }
}

void KeyboardKeyLogic::unlock_all_temporary_modifiers()
{
    if (!m_locked_temporary_modifiers.empty())
    {
        ModifierCounts mod_counts;
        for (auto it : m_locked_temporary_modifiers)
        {
            ModMaskStack& stack = it.second;

            for (ModMask mod_mask : stack)
            {
                for (size_t i=0; i<mod_counts.size(); i++)
                {
                    Modifier::Enum mod = Modifier::bit_index_to_modifier(i);
                    if (mod_mask & mod)
                        mod_counts[mod]++;
                }
            }
        }

        m_locked_temporary_modifiers.clear();

        LOG_DEBUG << "unlock_all_temporary_modifiers() "
                  << m_locked_temporary_modifiers;

        do_unlock_modifier_counts(mod_counts);
    }
}

void KeyboardKeyLogic::do_lock_modifiers(ModMask mod_mask)
{
    ModMask mods_to_lock = 0;
    for (size_t i=0; i<m_mods.size(); i++)
    {
        Modifier::Enum mod = Modifier::bit_index_to_modifier(i);
        if (mod_mask & mod)
        {
            if (!m_mods[mod])
            {
                // Alt is special because it activates the
                // window manager's move mode.
                if (mod != Modifier::ALT ||
                    !is_alt_special())
                {
                    mods_to_lock |= mod;
                }

                m_mods[mod]++;
            }
        }

        if (mods_to_lock)
        {
            LOG_DEBUG << "do_lock_modifiers(" << mod_mask
                      << ") " << m_mods << " " << mods_to_lock;

            get_text_changer()->lock_mod(mods_to_lock);
        }
    }
}

void KeyboardKeyLogic::do_unlock_modifiers(ModMask mod_mask)
{
    ModifierCounts mod_counts;
    for (size_t i=0; i<mod_counts.size(); i++)
    {
        Modifier::Enum mod = Modifier::bit_index_to_modifier(i);
        if (mod_mask & mod)
            mod_counts[mod] = 1;
    }

    if (!mod_counts.empty())
    {
        do_unlock_modifier_counts(mod_counts);
    }
}

void KeyboardKeyLogic::do_unlock_modifier_counts(const ModifierCounts& counts)
{
    ModMask mods_to_unlock = 0;
    for (size_t i=0; i<counts.size(); i++)
    {
        Modifier::Enum mod = Modifier::bit_index_to_modifier(i);
        auto count = counts[mod];
        m_mods[mod] -= count;

        if (m_mods[mod] <= 0)
        {
            // Alt is special because it activates the
            // window manager's move mode.
            if (mod != Modifier::ALT ||
                !is_alt_special())
            {
                mods_to_unlock |= mod;
            }
        }

        if (m_mods[mod] < 0)
        {
            LOG_ERROR << "negative modifier count " << m_mods[mod]
                      << " for modifier " << mod
                      << ", correcting to 0";
            m_mods[mod] = 0;
        }

    }

    if (mods_to_unlock)
    {
        LOG_DEBUG << "do_unlock_modifier_counts(" << counts
                  << ") " << m_mods << " " << mods_to_unlock;

        get_text_changer()->unlock_mod(mods_to_unlock);
    }
}

bool KeyboardKeyLogic::is_alt_special()
{
    return config()->is_alt_special();
}

void KeyboardKeyLogic::maybe_send_alt_press_for_key(const LayoutKeyPtr& key, LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
{
    if (m_mods[Modifier::ALT] &&
        is_alt_special() &&
        !key->active &&
        !key->is_button() &&
        !is_key_disabled(key))
    {
        maybe_send_alt_press(view, button, event_type);
    }
}

void KeyboardKeyLogic::maybe_send_alt_release_for_key(const LayoutKeyPtr& key, LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
{
    (void)key;
    if (m_alt_locked)
        maybe_send_alt_release(view, button, event_type);
}

void KeyboardKeyLogic::maybe_send_alt_press(LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
{
    if (m_mods[Modifier::ALT] &&
        !m_alt_locked)
    {
        m_alt_locked = true;
        if (m_last_alt_key)
        {
            send_key_press(m_last_alt_key, view,
                           button, event_type);
        }
        get_text_changer()->lock_mod(Modifier::ALT);
    }
}

void KeyboardKeyLogic::maybe_send_alt_release(LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
{
    if (m_alt_locked)
    {
        m_alt_locked = false;
        if (m_last_alt_key)
        {
            this->send_key_release(m_last_alt_key,
                                   view, button, event_type);
        }
        get_text_changer()->unlock_mod(Modifier::ALT);
    }
}

void KeyboardKeyLogic::send_key_press(const LayoutKeyPtr& key, LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
{
    auto keyboard = get_keyboard();
    auto text_changer = get_text_changer();
    if (!text_changer)  // shouldn't happen ever
        return;

    bool activated = true;

    switch (key->type)
    {
        case KeyType::KEYCODE:
        {
            NoKeySynthDelay nd(this);
            text_changer->press_keycode(key->keycode);
            break;
        }

        case KeyType::KEYSYM:
        {
            NoKeySynthDelay nd(this);
            text_changer->press_keysym(key->keysym);
            break;
        }

        case KeyType::CHAR:
        {
            if (count_codepoints(key->keystr) == 1)
            {
                NoKeySynthDelay nd(this);
                text_changer->press_unicode(key->keystr);
            }
            break;
        }

        case KeyType::KEYPRESS_NAME:
        {
            NoKeySynthDelay nd(this);
            text_changer->press_keysym(
                        get_keysym_from_name(key->keystr));
            break;
        }

        case KeyType::BUTTON:
        {
            activated = false;
            key->on_press(view, button, event_type);
            auto controller = keyboard->get_button_controller(key);
            if (controller)
            {
                activated = controller->is_activated_on_press();
                controller->press(view, button, event_type);
            }
            break;
        }

        case KeyType::MACRO:
        {
            activated = false;
            break;
        }

        case KeyType::SCRIPT:
        {
            activated = false;
            break;
        }

        case KeyType::PREDICTION:
        {
            activated = false;
            break;
        }

        case KeyType::CORRECTION:
        {
            activated = false;
            break;
        }

        default:
            LOG_WARNING << "invalid KeyType " << key->type;
            break;
    }

    key->activated = activated;
}

void KeyboardKeyLogic::send_key_release(const LayoutKeyPtr& key, LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
{
    auto keyboard = get_keyboard();
    auto text_changer = get_text_changer();
    if (!text_changer)  // shouldn't happen ever
        return;

    switch (key->type)
    {
        case KeyType::CHAR:
        {
            if (count_codepoints(key->keystr) == 1)
                text_changer->release_unicode(key->keystr);
            else
                text_changer->insert_string_at_caret(key->keystr);
            break;
        }

        case KeyType::KEYSYM:
        {
            text_changer->release_keysym(key->keysym);
            break;
        }

        case KeyType::KEYPRESS_NAME:
        {
            text_changer->release_keysym(
                        get_keysym_from_name(key->keystr));
            break;
        }

        case KeyType::KEYCODE:
        {
            text_changer->release_keycode(key->keycode);
            break;
        }

        case KeyType::BUTTON:
        {
            key->on_release(view, button, event_type);
            auto controller = keyboard->get_button_controller(key);
            if (controller)
                controller->release(view, button, event_type);
            break;
        }

        case KeyType::MACRO:
        {
            auto snippet_id = to_int(key->macro_id);
            if (insert_snippet(snippet_id))
            {
            }

            // Block dialog in xembed mode.
            // Don't allow to open multiple dialogs in force-to-top mode.
            else
            {
                view->edit_snippet(snippet_id);
            }
            break;
        }

        case KeyType::SCRIPT:
        {
            // block settings dialog in xembed mode
            if (!config()->is_xid_mode())
            {
                if (!key->script_name.empty())
                    run_script(key->script_name);
            }
            break;
        }

        default:
            LOG_WARNING << "invalid KeyType " << key->type;
    }
}

void KeyboardKeyLogic::release_non_sticky_key(const LayoutKeyPtr& key, LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
{
    auto keyboard = get_keyboard();
    auto word_suggestions = keyboard->get_word_suggestions();

    // Request capitalization before keys are unlatched, so we can
    // prevent modifiers from toggling more than once && confuse
    // set_modifiers().
    if (word_suggestions)
        word_suggestions->on_before_key_release(key);

    // release key
    send_key_up(key, view, button, event_type);

    // Don't release latched modifiers for click buttons yet,
    // keep them unchanged until the actual click happens.
    // -> allow clicks with modifiers
    if (!key->is_layer_button() &&
        !(key->is_button() &&
          key->is_click_type_key()))
    {
        // Don't release SHIFT if we're going to enable
        // capitalization anyway.
        vector<LayoutKeyPtr> except_keys;
        if (m_capitalization_requested)
        {
            for (auto& k : m_latched_sticky_keys)
                if (k->modifier == Modifier::SHIFT)
                    except_keys.emplace_back(k);
        }

        // release latched modifiers
        release_latched_sticky_keys(except_keys,
                                    true);  // only_unpressed
    }

    set_last_typed_was_separator(key->is_separator());

    // Insert words on button release to avoid having the wordlist
    // change between button press && release.
    // Make sure latched modifiers have been released, else they will
    // affect the whole inserted string.
    if (word_suggestions)
        word_suggestions->send_key_up(key, view, button, event_type);

    // switch to layer 0 on (almost) any key release
    keyboard->maybe_switch_to_first_layer(key);

    // punctuation assistance && collapse corrections
    if (word_suggestions)
        word_suggestions->on_after_key_release(key);

    // capitalization requested by punctuator?
    if (m_capitalization_requested)
    {
        m_capitalization_requested = false;
        if (!m_mods[Modifier::SHIFT])   // SHIFT not active yet?
            enter_caps_mode();
    }
}

bool KeyboardKeyLogic::is_capitalization_requested()
{
    return m_capitalization_requested;
}

void KeyboardKeyLogic::request_capitalization(bool capitalize)
{
    m_capitalization_requested = capitalize;
}

void KeyboardKeyLogic::push_and_clear_modifiers(ModMask modifiers)
{
    ModifierCounts mods;
    mods.assign_filtered(m_mods, modifiers);
    m_suppress_modifiers_stack.emplace_back(mods);
    for (size_t i=0; i<mods.size(); i++)
    {
        auto mod = mods.mod_at(i);
        int count = mods[mod];
        if (count)
        {
            m_mods[mod] = 0;
            get_text_changer()->unlock_mod(mod);
        }
    }
}

void KeyboardKeyLogic::pop_and_restore_modifiers()
{
    assert(!m_suppress_modifiers_stack.empty());
    if (m_suppress_modifiers_stack.empty())
    {
        LOG_WARNING << "stack empty, unbalanced pop_and_restore_modifiers";
        return;
    }

    auto mods = m_suppress_modifiers_stack.back();
    m_suppress_modifiers_stack.pop_back();

    for (size_t i=0; i<mods.size(); i++)
    {
        auto mod = mods.mod_at(i);
        int count = mods[mod];
        if (count)
        {
            m_mods[mod] = count;
            get_text_changer()->lock_mod(mod);
        }
    }
}

void KeyboardKeyLogic::enter_caps_mode()
{
    auto keyboard = get_keyboard();

    auto lfsh_keys = keyboard->find_items_from_ids({"LFSH"});
    auto rtsh_keys = keyboard->find_items_from_ids({"RTSH"});

    // unlatch all shift keys
    for (auto& item : concat(lfsh_keys, rtsh_keys))
    {
        auto key = std::dynamic_pointer_cast<LayoutKey>(item);
        if (key->active)
        {
            key->active = false;
            key->locked = false;
            remove(m_latched_sticky_keys, key);
            remove(m_locked_sticky_keys, key);
        }
        keyboard->invalidate_key(key);
    }

    // Latch right shift for capitalization,
    // if there is no right shift, latch left shift instead.
    auto& shift_keys = rtsh_keys.empty() ? lfsh_keys : rtsh_keys;
    for (auto item : shift_keys)
    {
        auto key = std::dynamic_pointer_cast<LayoutKey>(item);
        if (!key->active)
        {
            key->active = true;
            if (!contains(m_latched_sticky_keys, key))
                m_latched_sticky_keys.emplace_back(key);
            keyboard->invalidate_key(key);
        }
    }

    m_mods[Modifier::SHIFT] = 1;
    get_text_changer()->lock_mod(Modifier::SHIFT);
    keyboard->invalidate_labels();
}

void KeyboardKeyLogic::set_currently_typing()
{
    m_last_typing_time = SteadyClock::now();
}

bool KeyboardKeyLogic::is_typing()
{
    bool text_key_pressed =
            std::any_of(m_pressed_keys.begin(), m_pressed_keys.end(),
                        [&](const LayoutKeyPtr& key) -> bool
    {return is_text_insertion_key(key);});
    return text_key_pressed ||
            SteadyClock::now() - m_last_typing_time <= std::chrono::milliseconds(500);
}

void KeyboardKeyLogic::set_last_typed_was_separator(bool value)
{
    m_last_typed_was_separator = value;
}

bool KeyboardKeyLogic::get_last_typed_was_separator()
{
    return m_last_typed_was_separator;
}

bool KeyboardKeyLogic::is_text_insertion_key(const LayoutKeyPtr& key)
{
    return key && key->is_text_changing();
}

const std::vector<std::string> KeyboardKeyLogic::find_canonical_equivalents(const std::string& label)
{
    CanonicalEquivalentsMap section;
    if (get_value(g_canonical_equivalents, "all", section))
        return get_value(section, label);
    return {};
}

bool KeyboardKeyLogic::insert_snippet(int snippet_id)
{
    SnippetsValue value = get_value(config()->snippets.get(), snippet_id);
    if (!value.text.empty())
    {
        get_text_changer()->insert_string_at_caret(value.text);
        return true;
    }
    return false;
}

void KeyboardKeyLogic::run_script(const std::string& script)
{
    LOG_INFO << "running script " << repr(script);
    run_cmd({"python",
             "-cfrom Onboard.settings import Settings\ns = Settings(False)"});
}

bool KeyboardKeyLogic::step_sticky_key(const LayoutKeyPtr& key,
                                       MouseButton::Enum button,
                                       EventType::Enum event_type)
{
    bool active;
    bool locked;

    std::tie(active, locked) = step_sticky_key_state(key,
                                                     key->active, key->locked,
                                                     button, event_type);
    // apply the new states
    bool was_active  = key->active;
    bool deactivated = false;
    key->active  = active;
    key->locked  = locked;
    if (active)
    {
        if (locked)
        {
            remove(m_latched_sticky_keys, key);
            if (!contains(m_locked_sticky_keys, key))
                m_locked_sticky_keys.emplace_back(key);
        }
        else
        {
            if (!contains(m_latched_sticky_keys, key))
                m_latched_sticky_keys.emplace_back(key);
            remove(m_locked_sticky_keys, key);
        }
    }
    else
    {
        remove(m_latched_sticky_keys, key);
        remove(m_locked_sticky_keys, key);

        deactivated = was_active ||
                      !can_activate_key(key);  // push-button
    }

    return deactivated;
}

bool KeyboardKeyLogic::can_activate_key(const LayoutKeyPtr& key)
{
    auto behavior = get_sticky_key_behavior(key);
    return (StickyBehavior::can_latch(behavior) ||
            StickyBehavior::can_lock(behavior));
}

bool KeyboardKeyLogic::can_cycle_modifiers()
{
    // Any non-modifier currently held down?
    for (auto key : m_pressed_keys)
        if (!key->is_modifier())
            return false;

    // Any non-modifier released before?
    if (m_non_modifier_released)
        return false;

    return true;
}

std::tuple<bool, bool> KeyboardKeyLogic::step_sticky_key_state(const LayoutKeyPtr& key,
                                                               bool active, bool locked,
                                                               MouseButton::Enum button,
                                                               EventType::Enum event_type)
{
    (void)button;

    auto behavior = get_sticky_key_behavior(key);
    bool double_click = event_type == EventType::DOUBLE_CLICK;

    // double click usable?
    if (double_click &&
        StickyBehavior::can_lock_on_double_click(behavior))
    {
        // any state -> locked
        active = true;
        locked = true;
    }

    // single click or unused double click
    else
    {
        // off -> latched or locked
        if (!active)
        {
            if (StickyBehavior::can_latch(behavior))
            {
                active = true;
            }
            else
                if (StickyBehavior::can_lock_on_single_click(behavior))
                {
                    active = true;
                    locked = true;
                }
        }

        // latched -> locked
        else if (!key->locked &&
                 StickyBehavior::can_lock_on_single_click(behavior))
        {
            locked = true;
        }

        // latched || locked -> off
        else if (StickyBehavior::can_cycle(behavior))
        {
            active = false;
            locked = false;
        }
    }

    return {active, locked};
}

StickyBehavior::Enum KeyboardKeyLogic::get_sticky_key_behavior(const LayoutKeyPtr& key)
{
    // try the individual key id
    auto behavior = get_sticky_behavior_for(key->get_id());

    // default to the layout's behavior
    // CAPS was hard-coded here to LOCK_ONLY until v0.98.
    if (behavior == StickyBehavior::NONE &&
        key->sticky_behavior != StickyBehavior::NONE)
    {
        behavior = key->sticky_behavior;
    }

    // try the key group
    if (behavior == StickyBehavior::NONE)
    {
        if (key->is_modifier())
            behavior = get_sticky_behavior_for("modifiers");

        if (key->is_layer_button())
            behavior = this->get_sticky_behavior_for("layers");
    }

    // try the 'all' group
    if (behavior == StickyBehavior::NONE)
        behavior = get_sticky_behavior_for("all");

    // else fall back to hard coded default
    if (!StickyBehavior::is_valid(behavior))
    {
        behavior = StickyBehavior::CYCLE;
    }

    return behavior;
}

StickyBehavior::Enum KeyboardKeyLogic::get_sticky_behavior_for(const std::string& group)
{
    auto behavior = StickyBehavior::NONE;
    string value;

    if (get_value(config()->keyboard->sticky_key_behavior.get(), group, value))
    {
        behavior = StickyBehavior::to_behavior(value);
        if (behavior == StickyBehavior::NONE)
            LOG_WARNING << "invalid sticky behavior " << repr(value)
                        << " for group " << repr(group);
    }
    return behavior;
}

bool KeyboardKeyLogic::is_key_disabled(const LayoutKeyPtr& key)
{
    if (m_disabled_keys.empty())
    {
        create_disabled_keys_set(m_disabled_keys);
        LOG_DEBUG << "disabled keys: " << m_disabled_keys;
    }

    KeyIDModEntry entry{key->get_id(), this->get_mod_mask()};
    return m_disabled_keys.count(entry);
}

void KeyboardKeyLogic::create_disabled_keys_set(KeyboardKeyLogic::DisabledKeysSet& disabled_keys)
{
    auto keyboard = get_keyboard();
    auto layout = keyboard->get_layout();
    if (!layout)
        return;

    std::vector<std::string> available_key_ids;
    layout->for_each_key([&](const LayoutItemPtr& item)
        {available_key_ids.emplace_back(item->get_id());});

    for (auto& combo : config()->lockdown->disable_keys.get())
    {
        try {
            auto results = parse_key_combination(combo, available_key_ids);
            for (const auto& e : results)
                disabled_keys.emplace(e);
        }
        catch (const KeyDefsException& ex)
        {
            LOG_WARNING << "ignoring unrecognized key combination"
                        << " '" << combo << "' "
                        << "in lockdown.disable-keys: " << ex.what();
        }
    }
}

bool KeyboardKeyLogic::has_latched_sticky_keys()
{
    return !m_latched_sticky_keys.empty();
}

void KeyboardKeyLogic::release_latched_sticky_keys(const vector<LayoutKeyPtr>& except_keys,
                                                   bool only_unpressed, bool skip_externally_set_modifiers)
{
    LOG_DEBUG << "except_keys=" << except_keys
              << " only_unpressed=" << only_unpressed
              << " skip_externally_set_modifiers=" << skip_externally_set_modifiers;

    if (!m_latched_sticky_keys.empty())
    {
        auto latched_sticky_keys = m_latched_sticky_keys;
        for (auto& key : latched_sticky_keys)
        {
            if (except_keys.empty() || !contains(except_keys, key))
            {
                // Don't release still pressed modifiers, they may be
                // part of a multi-touch key combination.
                if (!only_unpressed || !key->pressed)
                {
                    // Don't release modifiers that where latched by
                    // set_modifiers due to external (physical keyboard)
                    // action.
                    // Else the latched modifiers go out of sync in
                    // on_outside_click() while an external tool like
                    // xte holds them down (LP: #1331549).
                    if (!skip_externally_set_modifiers ||
                        !key->is_modifier() ||
                        !m_external_mod_changes[key->modifier])
                    {
                        // Keep shift pressed if we're going to continue
                        // upper-case anyway. Else multiple locks and
                        // unlocks of SHIFT may happen when the punctuator
                        // is active. We can change the modifier state at
                        // most once per key release, else we can't
                        // distinguish our changes from physical
                        // keyboard actions in set_modifiers.
                        if (key->modifier != Modifier::SHIFT ||
                            !is_capitalization_requested())
                        {
                            send_key_up(key);
                            remove(m_latched_sticky_keys, key);
                            key->active = false;
                            get_keyboard()->invalidate_key(key);
                        }
                    }
                }
            }
        }

        // modifiers may change many key labels -> redraw all
        get_keyboard()->invalidate_labels();
    }
}

void KeyboardKeyLogic::release_locked_sticky_keys(bool release_all)
{
    if (!m_locked_sticky_keys.empty())
    {
        auto latched_sticky_keys = m_latched_sticky_keys;
        for (auto& key : latched_sticky_keys)
        {
            // NumLock is special, keep its state on exit
            // if not told otherwise.
            if (release_all ||
                key->modifier != Modifier::NUMLK)
            {
                send_key_up(key);
                remove(m_locked_sticky_keys, key);
                key->active = false;
                key->locked = false;
                key->pressed = false;
                get_keyboard()->invalidate_key(key);
            }
        }

        // modifiers may change many key labels -> redraw everything
        get_keyboard()->invalidate_labels();
    }
}

void KeyboardKeyLogic::start_auto_release_timer(bool visible)
{
    m_auto_release_timer->start(visible);
}

void KeyboardKeyLogic::start_long_press_timer(std::chrono::milliseconds delay, LayoutView* view, const InputSequencePtr& sequence)
{
    m_long_press_timer->start(delay, [this, view, sequence]() -> bool
                                     {on_long_press(view, sequence); return false;});
}

void KeyboardKeyLogic::on_long_press(LayoutView* view, const InputSequencePtr sequence)
{
    bool long_pressed = key_long_press(sequence->active_key, view, sequence->button);
    sequence->cancel_key_action = long_pressed; // cancel generating key-stroke
}

void KeyboardKeyLogic::stop_long_press()
{
    m_long_press_timer->stop();
}

void KeyboardKeyLogic::on_key_unpressed(const LayoutKeyPtr& key)
{
    set_temporary_modifiers(0);
    update_temporary_key_label(key, 0);
}

void KeyboardKeyLogic::finish_unpress_timers()
{
    m_unpress_timers->finish_all();
}

KeyAction::Enum KeyboardKeyLogic::get_key_action(const LayoutKeyPtr& key)
{
    auto keyboard = get_keyboard();

    KeyAction::Enum action = key->action;
    if (action == KeyAction::NONE)
    {
        if (key->is_button())
        {
            action = KeyAction::DELAYED_STROKE;
            auto controller = keyboard->get_button_controller(key);
            if (controller &&
                controller->is_activated_on_press())
            {
                action = KeyAction::SINGLE_STROKE;
            }
        }

        else if (key->type != KeyType::PREDICTION &&
                 key->type != KeyType::CORRECTION)
        {
            string label = key->get_label();
            auto alternatives = find_canonical_equivalents(label);
            if ((count_codepoints(label) == 1 && isalnum_str(label)) ||
                key->get_id() == "SPCE" ||
                !alternatives.empty())
            {
                action = config()->keyboard->default_key_action;
            }
            else
            {
                action = KeyAction::SINGLE_STROKE;
            }
        }
    }

    // Is there a popup defined for this key?
    if (action != KeyAction::DELAYED_STROKE)
    {
        if (key->get_popup_layout())
            action = KeyAction::DELAYED_STROKE;

        // Unity-greeter gets return stuck on login again in Zesty.
        // -> sent key-down/key-up on release
        if (config()->launched_by == Launcher::UNITY_GREETER)
            action = KeyAction::DELAYED_STROKE;
    }

    return action;
}


