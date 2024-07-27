#include <set>

#include "tools/container_helpers.h"
#include "tools/logger.h"
#include "tools/stacktrace.h"

#include "audioplayer.h"
#include "clickgenerator.h"
#include "configuration.h"
#include "buttoncontroller.h"
#include "inputsequence.h"
#include "keyboard.h"
#include "keyboardkeylogic.h"
#include "keyboardscanner.h"
#include "keyboardview.h"
#include "layoutkey.h"
#include "layoutroot.h"
#include "layoutview.h"
#include "timer.h"
#include "wordsuggestions.h"


std::unique_ptr<Keyboard> Keyboard::make(const ContextBase& context)
{
    return std::make_unique<This>(context);
}

Keyboard::Keyboard(const ContextBase& context) :
    Super(context),
    m_key_logic(std::make_unique<KeyboardKeyLogic>(context)),
    m_scanner(std::make_unique<KeyboardScanner>(context)),
    m_word_suggestions(std::make_unique<WordSuggestions>(context)),
    m_click_generator(std::make_unique<ClickGenerator>(context))
{}

Keyboard::~Keyboard()
{  }

LayoutRootPtr Keyboard::get_layout_root()
{ return ::get_layout_root(m_layout);}

void Keyboard::set_layout(const LayoutItemPtr& layout)
{
    m_layout = layout;
    on_layout_loaded();
}

void Keyboard::on_layout_loaded()
{
    // hide all still visible feedback popups; keys have changed.
    //m_touch_feedback->hide();

    init_button_controllers();
    init_active_layers();

    m_word_suggestions->on_layout_loaded();

    /*
    this->update_modifiers();          // TODO: port modifier updates
    this->update_scanner_enabled();    // TODO: port scanner
    */

    // redraw everything
    this->invalidate_ui();
}

void Keyboard::init_button_controllers()
{
    m_button_controllers.clear();

    auto layout = get_layout();
    layout->for_each_global_key([&](const LayoutItemPtr& item)
    {
        auto key = dynamic_cast<LayoutKey*>(item.get());
        auto bc = ButtonController::make_for(item, *this);
        if (bc)
        {
            m_button_controllers[key] = std::move(bc);
        }
        else
        {
            if (key && key->is_button())
                LOG_WARNING << "missing ButtonController for key "
                            << repr(key->get_id());
        }
    });
}

ButtonController* Keyboard::get_button_controller(const LayoutKeyPtr& key)
{
    //LOG_DEBUG << m_button_controllers;
    auto it = m_button_controllers.find(key.get());
    if (it != m_button_controllers.end())
        return it->second.get();
    return {};
}

void Keyboard::on_focusable_gui_opening()
{
    if (auto ws = get_word_suggestions())
        ws->on_focusable_gui_opening();
}

void Keyboard::on_focusable_gui_closed()
{
    if (auto ws = get_word_suggestions())
        ws->on_focusable_gui_closed();
}

void Keyboard::on_activity_detected()
{
    if (m_word_suggestions)
        m_word_suggestions->on_activity_detected();
}

void Keyboard::on_outside_click(MouseButton::Enum button)
{
    m_key_logic->release_latched_sticky_keys();
    m_click_generator->end_mapped_click();
    if (m_word_suggestions)
        m_word_suggestions->on_outside_click(button);
}

void Keyboard::on_cancel_outside_click()
{
    if (m_word_suggestions)
        m_word_suggestions->on_cancel_outside_click();
}

// pressed state of a key instance was set
void Keyboard::on_key_pressed(const LayoutKeyPtr& key, const LayoutView* view, const InputSequencePtr& sequence, bool action)
{
    if (sequence)   // Not a simulated key press, scanner?
    {
        auto word_suggestions = get_word_suggestions();
        bool feedback = word_suggestions ? word_suggestions->can_give_keypress_feedback() : true;

        // audio feedback
        if (action &&
            config()->keyboard->audio_feedback_enabled)
        {
            Point pt(-1, -1);   // keep passwords privat
            if (feedback)
                pt = sequence->root_point;

            Point pts(-1, -1);
            if (config()->keyboard->audio_feedback_place_in_space)
                pts = pt;

            auto player = get_audio_player();
            if (player)
                player->play(AudioPlayer::key_feedback, pt, pts);
        }

        // key label popup
        if (!config()->is_xid_mode() &&
           config()->keyboard->touch_feedback_enabled &&
           sequence->event_type != EventType::DWELL &&
           key->can_show_label_popup() &&
           feedback)
        {
            get_keyboard_view()->show_touch_feedback(key, view);
        }
    }
}

void Keyboard::on_key_unpressed(const LayoutKeyPtr& key)
{
    auto key_logic = get_keyboard()->get_key_logic();
    auto keyboard_view = get_keyboard_view();

    key_logic->on_key_unpressed(key);

    keyboard_view->redraw_item(key);
    keyboard_view->hide_touch_feedback(key);
}

void Keyboard::on_all_keys_up()
{
    if (m_word_suggestions)
        m_word_suggestions->on_all_keys_up();
}

void Keyboard::invalidate_ui()
{
    m_invalidated_ui |= UIMask::ALL;
}

void Keyboard::invalidate_ui_no_resize()
{
    m_invalidated_ui |= UIMask::ALL & ~UIMask::VIEW_SIZE;
}

void Keyboard::invalidate_context_ui()
{
    m_invalidated_ui |= (UIMask::CONTROLLERS |
                         UIMask::SUGGESTIONS |
                         UIMask::LAYOUT);
}

void Keyboard::invalidate_key(const LayoutKeyPtr& key)
{
    m_invalidated_ui |= UIMask::ITEMS;
    LayoutItemPtr item = std::dynamic_pointer_cast<LayoutItem>(key);
    if (item)
        m_items_to_invalidate.emplace(item);
}

void Keyboard::invalidate_keys(const std::vector<LayoutKeyPtr>& keys)
{
    m_invalidated_ui |= UIMask::ITEMS;
    for (auto& key : keys)
    {
        LayoutItemPtr item = std::dynamic_pointer_cast<LayoutItem>(key);
        if (item)
            m_items_to_invalidate.emplace(item);
    }
}

void Keyboard::invalidate_item(const LayoutItemPtr& item)
{
    m_invalidated_ui |= UIMask::ITEMS;
    if (item)
        m_items_to_invalidate.emplace(item);
}

void Keyboard::invalidate_labels()
{
    m_invalidated_ui |= UIMask::LABEL_TEXT | UIMask::LABEL_SIZE;
}

void Keyboard::invalidate_labels_text()
{
    m_invalidated_ui |= UIMask::LABEL_TEXT;
}

void Keyboard::invalidate_size()
{
    invalidate_ui();
    m_invalidated_ui |= UIMask::LAYOUT |
                        UIMask::VIEW_SIZE |
                        UIMask::LABEL_SIZE;
}

void Keyboard::invalidate_layout()
{
    m_invalidated_ui |= UIMask::LAYOUT |
                        UIMask::LABEL_SIZE;
}

void Keyboard::invalidate_visible_layers()
{
    m_invalidated_ui |= UIMask::LAYERS;
}

void Keyboard::invalidate_canvas()
{
    m_invalidated_ui |= UIMask::REDRAW_ALL;
}

// Call this at most once for each event distinct in time
// and only after (almost) all processing of the event handler
// has finished.
void Keyboard::commit_ui_updates(bool allow_redraw)
{
    if (!m_invalidated_ui)
        return;

    if (m_ui_updates_recursion_count)
        return;
    m_ui_updates_recursion_count++;

    //LOG_DEBUG << "2: "  << m_invalidated_ui << " " << allow_redraw;
    //print_stack_trace(0, 12);

    ItemsToInvalidate& items = m_items_to_invalidate;
    UIMask::Enum mask = m_invalidated_ui;
    if (mask & UIMask::CONTROLLERS)
    {
        // update buttons
        for (auto& it : m_button_controllers)
        {
            auto& controller = it.second;
            controller->set_items_to_invalidate_container(items);
            controller->update();
        }
        mask = m_invalidated_ui;  // may have been changed by controllers
    }

    if (mask & UIMask::SUGGESTIONS)
    {
        m_word_suggestions->update_suggestions_ui(items);

        // update buttons that depend on suggestions
        for (auto& it : m_button_controllers)
        {
            auto& controller = it.second;
            controller->set_items_to_invalidate_container(items);
            controller->update_late();
        }
    }

    if (mask & UIMask::LAYERS)
    {
        this->update_visible_layers();
    }

    if (mask & UIMask::VIEW_SIZE)
    {
        if (auto layout = get_layout_root())
            layout->invalidate_tree();  // clear cached hit rects

        for (auto& view : get_keyboard_layout_views())
            view->invalidate_for_resize();
    }

    if (mask & UIMask::LAYOUT)
    {
        if (auto layout = get_layout_root())
            layout->invalidate_tree();  // clear cached hit rects

        update_layout();   // after suggestions!
    }

    if (mask & UIMask::ITEMS)
    {
        for (auto& key : items)
            key->invalidate();
    }

    if (mask & UIMask::LABEL_TEXT)
    {
        update_label_text(get_layout_root(), items);
    }

    if (mask & (UIMask::SUGGESTIONS | UIMask::LAYERS))
    {
        this->update_scanner();
    }

    if (mask & UIMask::LABEL_SIZE)
    {
        auto layout = get_layout_root();
        if (layout)
            layout->invalidate_tree();  // clear cached hit rects

        clear_text_renderers();   // try to keep PangoLayout leaking down
        update_label_font_sizes(layout, items);
    }

    if (allow_redraw)
    {
        if (mask & UIMask::REDRAW_ALL)
        {
            for (auto& view : get_keyboard_layout_views())
                view->redraw();
        }
        else
        {
            for (auto& view : get_layout_views())
                view->redraw_items(get_keys(items));
        }
    }

    m_invalidated_ui = {};
    m_items_to_invalidate.clear();
    m_ui_updates_recursion_count--;
}

void Keyboard::update_layout()
{
    for (auto& view : get_keyboard_layout_views())
        view->update_layout();
}

void Keyboard::update_visible_layers()
{
    auto layers = this->get_layer_ids();
    if (!layers.empty())
    {
        vector<string> v{"", layers[0]};
        extend(v, get_values(m_active_layer_ids));
        get_layout()->set_visible_layers(v);
    }
}

void Keyboard::update_scanner()
{
    // notify the scanner about layer changes
    if (m_scanner)
    {
        auto layout = get_layout();
        if (layout)
            m_scanner->update_layer(layout,
                                    get_active_layer_id(), true);
    }
}

vector<string> Keyboard::get_layer_ids(const string& parent_layer_id)
{
    auto layout = get_layout();
    if (layout)
        return layout->get_layer_ids(parent_layer_id);
    return {};
}

bool Keyboard::is_first_layer_id(const string& layer_id, const string& parent_layer_id)
{
    auto layers = this->get_layer_ids(parent_layer_id);
    if (layers.empty())
        return false;
    return layers[0] == layer_id;
}

string Keyboard::get_active_layer_id(const string& parent_layer_id)
{
    auto layer_ids = this->get_layer_ids(parent_layer_id);
    if (layer_ids.empty())
        return {};

    string active_id = get_value(m_active_layer_ids, parent_layer_id);
    if (active_id.empty() ||
        !contains(layer_ids, active_id))
    {
        return layer_ids[0];
    }

    return active_id;
}

vector<string> Keyboard::get_active_layer_ids()
{
    return get_values(m_active_layer_ids);
}

void Keyboard::set_active_layer_id(const string& layer_id, const string& parent_layer_id)
{
    auto layers = get_layer_ids(parent_layer_id);
    if (!layers.empty())
    {
        if (layer_id.empty() ||
            !contains(layers, layer_id))
        {
            m_active_layer_ids[parent_layer_id] = layers[0];
        }
        else
        {
            m_active_layer_ids[parent_layer_id] = layer_id;
        }
    }
}

bool Keyboard::is_first_layer_active(const string& parent_layer_id)
{
    return is_first_layer_id(get_active_layer_id(parent_layer_id), parent_layer_id);
}

void Keyboard::maybe_switch_to_first_layer(const LayoutKeyPtr& key)
{
    if (!is_first_layer_active() != 0 &&
        !is_layer_locked())
    {
        auto unlatch = key->can_unlatch_layer();
        if (unlatch.is_none())
        {
            // for backwards compatibility with Onboard <0.99
            unlatch = !key->is_layer_button() and
                      !contains(array<string, 2>{"move", "showclick"},
                                key->get_id());
        }

        if (unlatch)
        {
            set_active_layer_id({});
            invalidate_visible_layers();
            invalidate_canvas();
            invalidate_context_ui();  // update layer button state
        }
    }
}

void Keyboard::init_active_layers()
{
    auto layout = get_layout();
    if (!layout)
    {
        m_active_layer_ids.clear();
        return;
    }

    // Remove entries that don't exist in the current layout.
    // Helps testing new layouts by not resetting all layers
    // when a layout is loaded.
    auto layer_ids = this->get_layer_ids();
    for (auto parent_layer_id : get_keys(m_active_layer_ids))
    {
        if (!parent_layer_id.empty() &&
            !contains(layer_ids, parent_layer_id))
        {
            remove(m_active_layer_ids, parent_layer_id);
        }
    }

    // Set valid layer_ids for all parent_layers. Create entries
    // for all existing layer_parent_ids for correct initial visibility of
    // sublayers.
    for (auto lid : get_layer_ids())
    {
        string parent_layer_id = layout->layer_to_parent_id(lid);
        layer_ids = this->get_layer_ids(parent_layer_id);

        if (!layer_ids.empty())
        {
            string active_layer_id = get_value(m_active_layer_ids,
                                               parent_layer_id);
            if (!contains(layer_ids, active_layer_id))
                m_active_layer_ids[parent_layer_id] = layer_ids[0];
        }
        else
        {
                m_active_layer_ids[parent_layer_id] = {};
        }
    }
}

// Iterate through all key groups and set each key's
// label font size to the maximum possible for that group.
void Keyboard::update_labels(const LayoutItemPtr& layout, LOD lod)
{
    std::set<LayoutItemPtr> changed_keys;

    // no label changes necessary while dragging
    if (lod == LOD::FULL)
        update_label_text(layout, changed_keys);

    update_label_font_sizes(layout, changed_keys);
}

void Keyboard::update_label_text(const LayoutItemPtr& layout,
                                 std::set<LayoutItemPtr>& changed_keys)
{
    ModMask mod_mask = m_key_logic->get_mod_mask();

    if (layout)
    {
        // update label text
        layout->for_each_key([&](const LayoutItemPtr& item)
        {
            auto key = std::dynamic_pointer_cast<LayoutKey>(item);
            std::string old_label = key->get_label();
            std::string old_secondary_label = key->get_secondary_label();
            key->configure_labels(mod_mask);
            if (key->get_label() != old_label ||
                key->get_secondary_label() != old_secondary_label)
            {
                changed_keys.insert(key);
            }
        });
    }
}

void Keyboard::update_label_font_sizes(const LayoutItemPtr& layout,
                                       std::set<LayoutItemPtr>& changed_keys)
{
    if (!layout)
        return;

    if (get_keyboard_layout_views().empty())   // get_text_renderer() needs a view
        return;

    ModMask mod_mask = m_key_logic->get_mod_mask();

    // update font sizes
    KeyGroups key_groups;
    layout->get_key_groups(key_groups);
    for (auto& it : key_groups)
    {
        auto& keys = it.second;
        double max_size = 0.0;

        for (auto item : keys)
        {
            auto key = std::dynamic_pointer_cast<LayoutKey>(item);

            double best_size = key->get_best_font_size(mod_mask);
            if (best_size > 0.0)
            {
                if (key->ignore_group)
                {
                    if (key->font_size != best_size)
                    {
                        key->font_size = best_size;
                        changed_keys.insert(key);
                    }
                }
                else
                {
                    if (max_size == 0.0 || best_size < max_size)
                        max_size = best_size;
                }
            }
        }

        for (auto item : keys)
        {
            auto key = std::dynamic_pointer_cast<LayoutKey>(item);

            if (key->font_size != max_size &&
               !key->ignore_group)
            {
                key->font_size = max_size;
                changed_keys.insert(key);
            }
        }
    }
}

vector<LayoutItemPtr> Keyboard::find_items_from_ids(const vector<string>& ids)
{
    auto layout = get_layout();
    if (!layout)
        return {};
    return layout->find_ids(ids);
}

vector<LayoutItemPtr> Keyboard::find_items_from_classes(const vector<string>& item_classes)
{
    auto layout = get_layout();
    if (!layout)
        return {};
    return layout->find_classes(item_classes);
}

LayoutKeyPtr Keyboard::find_key_from_id(const string& id)
{
    auto layout = get_layout();
    if (!layout)
        return {};
    layout->find_key_if([&id](const LayoutItemPtr& item) -> bool
    {
        LayoutKeyPtr key = std::dynamic_pointer_cast<LayoutKey>(item);
        if (key->theme_id == id)
            return true;
        else if (key->id == id)
            return true;
        return false;
    });
    return {};
}


