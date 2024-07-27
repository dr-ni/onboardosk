#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "tools/noneable.h"

#include "inputsequencedecls.h"
#include "keyboarddecls.h"
#include "keydefinitions.h"
#include "layoutdecls.h"
#include "onboardoskglobals.h"
#include "stickybehavior.h"

using std::string;
using std::vector;


// enum of keyboard UI update flags
class UIMask
{
    public:
        enum Enum
        {
            NONE        = 0,
            CONTROLLERS = 1<<0,
            SUGGESTIONS = 1<<1,
            LABEL_TEXT  = 1<<2,
            LABEL_SIZE  = 1<<3,
            ITEMS        = 1<<4,
            LAYOUT      = 1<<5,
            LAYERS      = 1<<6,
            VIEW_SIZE   = 1<<7,
            REDRAW_KEYS = 1<<8,
            REDRAW_ALL  = 1<<9,
            ALL         = -1,
        };
};

inline UIMask::Enum operator ~(UIMask::Enum a)
{ return static_cast<UIMask::Enum>(~static_cast<int>(a)); }
inline UIMask::Enum operator | (UIMask::Enum a, UIMask::Enum b)
{ return static_cast<UIMask::Enum>(static_cast<int>(a) | static_cast<int>(b)); }
inline UIMask::Enum operator & (UIMask::Enum a, UIMask::Enum b)
{ return static_cast<UIMask::Enum>(static_cast<int>(a) & static_cast<int>(b)); }
inline UIMask::Enum& operator |= (UIMask::Enum& a, UIMask::Enum b)
{ a = a | b; return a;}


class ButtonController;
class ClickGenerator;
class InputSequence;
class KeyboardAnimator;
class KeyboardKeyLogic;
class KeyboardScanner;
class TextContext;
class WordSuggestions;


typedef std::map<LayoutKey*, std::unique_ptr<ButtonController>> ButtonControllerMap;

class Keyboard : public ContextBase
{
    public:
        using Super = ContextBase;
        using This = Keyboard;

        Keyboard(const ContextBase& context);
        virtual ~Keyboard();

        static std::unique_ptr<This> make(const ContextBase& context);

        LayoutItemPtr& get_layout() { return m_layout;}
        LayoutRootPtr get_layout_root();
        void set_layout(const LayoutItemPtr& layout);

        KeyboardKeyLogic* get_key_logic() {return m_key_logic.get();}
        WordSuggestions* get_word_suggestions() {return m_word_suggestions.get();}
        TextContext* get_text_context() {return nullptr;}

        void on_activity_detected();
        // Called by outside click polling.
        // Keep this as Francesco likes to have modifiers
        // reset when clicking outside of onboard.
        void on_outside_click(MouseButton::Enum button);

        // Called when outside click polling times out.
        void on_cancel_outside_click();

        void on_key_pressed(const LayoutKeyPtr& key, const LayoutView* view,
                            const InputSequencePtr& sequence, bool action);

        // pressed state of a key instance was cleard
        void on_key_unpressed(const LayoutKeyPtr& key);

        // all keys have been released
        void on_all_keys_up();

        // Update everything.
        // Quite expensive, don't call this while typing.
        void invalidate_ui();

        // Update everything assuming key sizes don't change.
        // Doesn't invalidate cached surfaces.
        void invalidate_ui_no_resize();

        // Update text-context dependent ui
        void invalidate_context_ui();

        // Update visibility of layers in the layout tree,
        // e.g. when the active layer changed.
        void invalidate_key(const LayoutKeyPtr& key);
        void invalidate_keys(const std::vector<LayoutKeyPtr>& keys);
        void invalidate_item(const LayoutItemPtr& item);

        // Update label text and invalidate caches for resizing
        void invalidate_labels();

        // Update label text
        void invalidate_labels_text();

        // view size(s) changed
        void invalidate_size();

        // Recalculate item rectangles.
        void invalidate_layout();

        // Update visibility of layers in the layout tree,
        // e.g. when the active layer changed.
        void invalidate_visible_layers();

        // Just redraw everything
        void invalidate_canvas();

        void commit_ui_updates(bool allow_redraw=true);

        bool is_layer_locked() {return m_layer_locked;}
        void set_layer_locked(bool b) {m_layer_locked = b;}

        bool is_first_layer_id(const string& layer_id, const string& parent_layer_id={});

        // Return currently active layer id.
        string get_active_layer_id(const string& parent_layer_id={});

        vector<string> get_active_layer_ids();

        // Set currently active layer id.
        // Empty id = make first layer active
        void set_active_layer_id(const string& layer_id={}, const string& parent_layer_id={});

        bool is_first_layer_active(const string& parent_layer_id={});

        // Activate the first layer if key allows it.
        void maybe_switch_to_first_layer(const LayoutKeyPtr& key);

        vector<LayoutItemPtr> find_items_from_ids(const vector<string>& ids);
        vector<LayoutItemPtr> find_items_from_classes(const vector<string>& item_classes);

        ButtonController* get_button_controller(const LayoutKeyPtr& key);

        // Turn off AT-SPI listeners while there is a dialog open.
        // Onboard and occasionally the whole desktop tend to lock up otherwise.
        // Call this before dialog/popop menus are opened by Onboard itthis->
        void on_focusable_gui_opening();

        // Call this after dialogs/menus have been closed.
        void on_focusable_gui_closed();

        // Iterate through all key groups && set each key's
        // label font size to the maximum possible for that group.
        void update_labels(const LayoutItemPtr& layout, LOD lod=LOD::FULL);
        void update_label_text(const LayoutItemPtr& layout, std::set<LayoutItemPtr>& changed_keys);
        void update_label_font_sizes(const LayoutItemPtr& layout, std::set<LayoutItemPtr>& changed_keys);

    private:
        void on_layout_loaded();

        // Update layout, key sizes are probably changing.
        void update_layout();

        // show/hide layers
        void update_visible_layers();

        // tell scanner to update on layout changes
        void update_scanner();

        // Reset layer index if it is out of range. e.g. due to
        // loading a layout with fewer layers.
        void init_active_layers();
        void init_button_controllers();

        vector<string> get_layer_ids(const string& parent_layer_id={});

        // Find the first key matching the given id. Id may be a complete
        // theme_id (key.theme_id) or just the regular item id (key.id).
        LayoutKeyPtr find_key_from_id(const string& id);

    private:
        LayoutItemPtr m_layout;
        std::unique_ptr<KeyboardKeyLogic> m_key_logic;
        std::unique_ptr<KeyboardScanner> m_scanner;
        std::unique_ptr<WordSuggestions> m_word_suggestions;
        std::unique_ptr<ClickGenerator> m_click_generator;

        UIMask::Enum m_invalidated_ui{};
        ItemsToInvalidate m_items_to_invalidate;
        int m_ui_updates_recursion_count{};

        ButtonControllerMap m_button_controllers;

        using ActiveLayerIdsMap = std::map<string, string>;
        ActiveLayerIdsMap m_active_layer_ids;
        bool m_layer_locked{false};


};

#endif // KEYBOARD_H
