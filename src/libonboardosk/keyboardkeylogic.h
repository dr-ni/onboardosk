#ifndef KEYBOARDKEYLOGIC_H
#define KEYBOARDKEYLOGIC_H

#include <chrono>
#include <string>
#include <set>
#include <vector>

#include "inputsequencedecls.h"
#include "layoutdecls.h"
#include "keydefinitions.h"
#include "keyboarddecls.h"
#include "onboardoskglobals.h"
#include "stickybehavior.h"

using std::string;
using std::vector;

class AutoReleaseTimer;
class TextChanger;
class TextChangerKeyStroke;
class TextChangerDirectInsert;
class Timer;
class UnpressTimers;

// sources of modifier changes
class ModSource
{
    public:
        enum Enum {
            KEYBOARD,
            KEYSYNTH,
        };
};


class KeyboardKeyLogic : public ContextBase
{
    public:
        using Super = ContextBase;

        using SteadyClock = std::chrono::steady_clock;
        using SteadyTimePoint = std::chrono::time_point<SteadyClock>;

        using DisabledKeysSet = std::set<KeyIDModEntry>;

        KeyboardKeyLogic(const ContextBase& context);
        ~KeyboardKeyLogic();

        TextChanger* get_text_changer();
        TextChangerKeyStroke* get_text_changer_keystroke();
        TextChangerDirectInsert* get_text_changer_direct_insert();

        // Bit-mask of curently active modifers.
        ModMask get_mod_mask();

        // Key functions called from views. They support drag activation of keys.
        void interactive_key_down(LayoutView* view, const InputSequencePtr& sequence);
        void interactive_key_down_update(LayoutView* view, const InputSequencePtr& sequence,
                             const LayoutKeyPtr& old_key);
        void interactive_key_up(LayoutView* view, const InputSequencePtr& sequence);

        // Press down on one of Onboard's key representations.
        // This may be either an initial press, or a switch of the active_key
        // due to dragging.
        void key_down(const LayoutKeyPtr& key, LayoutView* view={}, const InputSequencePtr& sequence={}, bool action=true);

        // Release one of Onboard's key representations.
        void key_up(const LayoutKeyPtr& key, LayoutView* view={}, const InputSequencePtr& sequence={}, bool action=true);

        // Long press of one of Onboard's key representations.
        bool key_long_press(const LayoutKeyPtr& key, LayoutView* view,
                            MouseButton::Enum button=MouseButton::LEFT);

        void key_unpress(const LayoutKeyPtr& key);

        std::tuple<bool, bool> step_sticky_key_state(
            const LayoutKeyPtr& key,
            bool active, bool locked,
            MouseButton::Enum button, EventType::Enum event_type);

        // Can key be latched or locked?
        bool can_activate_key(const LayoutKeyPtr& key);

        // Modifier cycling enabled?
        // Not enabled for multi-touch with at least one pressed non-modifier key.
        bool can_cycle_modifiers();

        // any sticky keys latched?
        bool has_latched_sticky_keys();

        // release latched sticky (modifier) keys
        void release_latched_sticky_keys(const vector<LayoutKeyPtr>& except_keys={},
                                         bool only_unpressed=false,
                                         bool skip_externally_set_modifiers=true);

        // release locked sticky (modifier) keys
        void release_locked_sticky_keys(bool release_all=false);

        void start_auto_release_timer(bool visible);

        void start_long_press_timer(std::chrono::milliseconds delay, LayoutView* view,
                                    const InputSequencePtr& sequence);

        void on_long_press(LayoutView* view, const InputSequencePtr sequence);

        void stop_long_press();

        // pressed state of a key instance was cleard
        void on_key_unpressed(const LayoutKeyPtr& key);
        void finish_unpress_timers();

        bool is_capitalization_requested();
        void request_capitalization(bool capitalize);

        void push_and_clear_modifiers(ModMask modifiers);

        void pop_and_restore_modifiers();

        // Lock temporary modifiers
        void lock_temporary_modifiers(ModSource::Enum mod_source_id, ModMask mod_mask);

        // Unlock temporary modifiers
        void unlock_temporary_modifiers(ModSource::Enum mod_source_id);

        bool get_last_typed_was_separator();

        // Is Onboard currently or was it just recently sending any text?
        bool is_typing();

        void set_last_typed_was_separator(bool value);

    private:
        void do_key_down_action(const LayoutKeyPtr& key,
                                LayoutView* view,
                                MouseButton::Enum button,
                                EventType::Enum event_type);

        void do_key_up_action(const LayoutKeyPtr& key,
                              LayoutView* view,
                              MouseButton::Enum button,
                              EventType::Enum event_type);
        void send_key_down(const LayoutKeyPtr& key,
                           LayoutView* view,
                           MouseButton::Enum button,
                           EventType::Enum event_type);

        void send_key_up(const LayoutKeyPtr& key, LayoutView* view={},
                         MouseButton::Enum button=MouseButton::LEFT,
                         EventType::Enum event_type=EventType::CLICK);

        // update label for temporary modifiers
        void update_temporary_key_label(const LayoutKeyPtr& key, ModMask temp_mod_mask);

        // Announce the intention to lock these modifiers on key-press.
        void set_temporary_modifiers(ModMask mod_mask);

        // Lock modifier before a single key-press
        void maybe_lock_temporary_modifiers_for_key(const LayoutKeyPtr& key);

        // Unlock modifier after a single key-press
        void maybe_unlock_temporary_modifiers();

        // Unlock all temporary modifiers
        void unlock_all_temporary_modifiers();

        // Lock modifiers and track their state.
        void do_lock_modifiers(ModMask mod_mask);

        // Unlock modifier in response to modifier releases.
        void do_unlock_modifiers(ModMask mod_mask);

        // Unlock modifier in response to modifier releases.
        void do_unlock_modifier_counts(const ModifierCounts& counts);

        // Does the ALT key need special treatment due to it
        bool is_alt_special();

        // handle delayed Alt press
        void maybe_send_alt_press_for_key(const LayoutKeyPtr& key,
                                          LayoutView* view,
                                          MouseButton::Enum button,
                                          EventType::Enum event_type);

        // handle delayed Alt release
        void maybe_send_alt_release_for_key(const LayoutKeyPtr& key,
                                            LayoutView* view,
                                            MouseButton::Enum button,
                                            EventType::Enum event_type);

        void maybe_send_alt_press(LayoutView* view,
                                  MouseButton::Enum button,
                                  EventType::Enum event_type);

        void maybe_send_alt_release(LayoutView* view,
                                    MouseButton::Enum button,
                                    EventType::Enum event_type);

        // Actually generate a key press
        void send_key_press(const LayoutKeyPtr& key,
                            LayoutView* view,
                            MouseButton::Enum button,
                            EventType::Enum event_type);

        // Actually generate a key release
        void send_key_release(const LayoutKeyPtr& key,
                              LayoutView* view,
                              MouseButton::Enum button=MouseButton::LEFT,
                              EventType::Enum event_type=EventType::CLICK);

        void release_non_sticky_key(const LayoutKeyPtr& key,
                                    LayoutView* view,
                                    MouseButton::Enum button,
                                    EventType::Enum event_type);

        // One cycle step when pressing a sticky (latchabe/lockable)
        // modifier key (all sticky keys except layer buttons).
        bool step_sticky_key(const LayoutKeyPtr& key,
                             MouseButton::Enum button,
                             EventType::Enum event_type);

        // Return sticky behavior for the given key
        StickyBehavior::Enum get_sticky_key_behavior(const LayoutKeyPtr& key);

        StickyBehavior::Enum get_sticky_behavior_for(const string& group);

        // Check for blacklisted key combinations
        bool is_key_disabled(const LayoutKeyPtr& key);

        // Precompute a set of (modmask, key_id) tuples for fast
        // testing against the key blacklist.
        void create_disabled_keys_set(DisabledKeysSet& s);

        KeyAction::Enum get_key_action(const LayoutKeyPtr& key);

        // Do what has to be done for the next pressed
        // character to be capitalized.
        // Don't call key_down+up for this, because modifiers may be
        // configured not to latch.
        void enter_caps_mode();

        // Remember it was us that just typed text.
        void set_currently_typing();

        // Does key actually insert any characters (not a navigation key)?
        bool is_text_insertion_key(const LayoutKeyPtr& key);

        const std::vector<std::string> find_canonical_equivalents(const string& label);

        bool insert_snippet(int snippet_id);
        void run_script(const string& script);

    private:
        std::unique_ptr<AutoReleaseTimer> m_auto_release_timer;
        std::unique_ptr<Timer> m_long_press_timer;
        std::unique_ptr<UnpressTimers> m_unpress_timers;
        std::unique_ptr<TextChangerKeyStroke> m_text_changer_key_stroke;
        std::unique_ptr<TextChangerDirectInsert> m_text_changer_direct_insert;

        // The number of pressed keys per modifier
        ModifierCounts m_mods;

        // Same to keep track of modifier changes triggered from the outside.
        // Doesn't include modifier changes caused by Onboard itself, so this is
        // !a complete representation of the modifier state.
        ModifierCounts m_external_mod_changes;

        ModMask m_temporary_modifiers;
        using ModMaskStack = std::vector<ModMask>;
        using TempLockedModifierMap = std::map<ModSource::Enum, ModMaskStack>;
        TempLockedModifierMap m_locked_temporary_modifiers;

        std::vector<ModifierCounts> m_suppress_modifiers_stack;

        bool m_alt_locked{false};
        LayoutKeyPtr m_last_alt_key;

        bool m_capitalization_requested{false};
        bool m_last_typed_was_separator{false};

        vector<LayoutKeyPtr> m_pressed_keys;
        vector<LayoutKeyPtr> m_latched_sticky_keys;
        vector<LayoutKeyPtr> m_locked_sticky_keys;

        bool m_non_modifier_released{false};

        SteadyTimePoint m_last_typing_time{};

        DisabledKeysSet m_disabled_keys;
};


// Turn modifiers off temporarily. May be nested.
class SuppressModifiers
{
  public:
      SuppressModifiers(KeyboardKeyLogic* instance, ModMask modifiers=Modifier::LABEL_MODIFIERS) :
          m_instance(instance)
      {
          m_instance->push_and_clear_modifiers(modifiers);
      }
      ~SuppressModifiers()
      {
          m_instance->pop_and_restore_modifiers();
      }

  private:
      KeyboardKeyLogic* m_instance{};
};


#endif // KEYBOARDKEYLOGIC_H
