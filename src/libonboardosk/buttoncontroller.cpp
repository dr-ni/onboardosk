
#include <memory>
#include <tuple>

#include "tools/logger.h"
#include "tools/process_helpers.h"

#include "configuration.h"
#include "buttoncontroller.h"
#include "keyboard.h"
#include "keyboardkeylogic.h"
#include "keyboardview.h"
#include "languagedb.h"
#include "layoutkey.h"
#include "layoutroot.h"
#include "layoutview.h"
#include "layoutwordlist.h"
#include "mousetweaks.h"
#include "wordsuggestions.h"


ButtonController::ButtonController(const ContextBase& context, const LayoutItemPtr& key) :
    ContextBase(context),
    m_item(key)
{}

LayoutItemPtr ButtonController::get_item()
{
    return m_item.lock();   // lock weak_ptr
}

LayoutKeyPtr ButtonController::get_key()
{
    return std::dynamic_pointer_cast<LayoutKey>(get_item());
}

void ButtonController::press(LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
{
    (void)view;
    (void)button;
    (void)event_type;
}

void ButtonController::long_press(LayoutView* view, MouseButton::Enum button)
{
    (void)view;
    (void)button;
}

void ButtonController::release(LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
{
    (void)view;
    (void)button;
    (void)event_type;
}

void ButtonController::update()
{
}

void ButtonController::update_late()
{
}

bool ButtonController::can_dwell()
{
    return false;
}

bool ButtonController::is_activated_on_press()
{
    return false;
}

void ButtonController::set_items_to_invalidate_container(ItemsToInvalidate& items)
{
    m_items_to_invalidate = &items;
}

void ButtonController::set_visible(bool visible)
{
    auto key = get_key();
    if (key->visible != visible)
    {
        LOG_DEBUG << "ButtonController: " << *key
                  << ".visible = " << repr(visible);
        auto layout = get_keyboard()->get_layout_root();
        layout->set_item_visible(key, visible);
        if (m_items_to_invalidate)
            m_items_to_invalidate->insert(key);
    }
}

void ButtonController::set_sensitive(bool sensitive)
{
    auto key = get_key();
    if (key->sensitive != sensitive)
    {
        LOG_DEBUG << "ButtonController: " << *key
                  << ".sensitive = " << repr(sensitive);
        key->sensitive = sensitive;
        if (m_items_to_invalidate)
            m_items_to_invalidate->insert(key);
    }
}

void ButtonController::set_active(bool active)
{
    auto key = get_key();
    if (key->active != active)
    {
        LOG_DEBUG << "ButtonController: " << *key
                  << ".active = " << repr(active);
        key->active = active;
        if (m_items_to_invalidate)
            m_items_to_invalidate->insert(key);
    }
}

void ButtonController::set_locked(bool locked)
{
    auto key = get_key();
    if (key->locked != locked)
    {
        LOG_DEBUG << "ButtonController: " << *key
                  << ".locked = " << repr(locked);
        key->active = locked;
        key->locked = locked;
        if (m_items_to_invalidate)
            m_items_to_invalidate->insert(key);
    }
}


#if 0
// Controller for click buttons
class BCClick : public ButtonController
{
    void release(LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
    {
        cs = this->keyboard.get_click_simulator();
        if (!cs)
        {
            return;
        }
        if (this->is_active())
        {
            // stop click mapping, reset to primary button && single click
            cs.map_primary_click(view,
                                 ClickSimulator.PRIMARY_BUTTON,
                                 ClickSimulator.CLICK_TYPE_SINGLE);
        }
        else
        {
            // Exclude click type buttons from the click mapping
            // to be able to reliably cancel the click.
            // -> They will receive only single left clicks.
            rects = view->get_click_type_button_rects();
            this->keyboard._click_sim.set_exclusion_rects(rects);

            // start click mapping
            cs.map_primary_click(view, this->button, this->click_type);
        }

        // Mark current event handled to stop ClickMapper from receiving it.
        view->set_xi_event_handled(true);
    }

    void update()
    {
        cs = this->keyboard.get_click_simulator();
        if (cs)
        {  // gone on exit
            this->set_active(this->is_active());
            this->set_sensitive(
                cs.supports_click_params(this->button, this->click_type));
    }

    void is_active()
    {
        cs = this->keyboard.get_click_simulator();
        return (cs and;
                cs.get_click_button() == this->button and;
                cs.get_click_type() == this->click_type);
    }
}


class BCSingleClick : public BCClick
{
    id = "singleclick";
    button = ClickSimulator.PRIMARY_BUTTON;
    click_type = ClickSimulator.CLICK_TYPE_SINGLE;
}


class BCMiddleClick : public BCClick
{
    id = "middleclick";
    button = ClickSimulator.MIDDLE_BUTTON;
    click_type = ClickSimulator.CLICK_TYPE_SINGLE;
}


class BCSecondaryClick : public BCClick
{
    id = "secondaryclick";
    button = ClickSimulator.SECONDARY_BUTTON;
    click_type = ClickSimulator.CLICK_TYPE_SINGLE;
}


class BCDoubleClick : public BCClick
{
    id = "doubleclick";
    button = ClickSimulator.PRIMARY_BUTTON;
    click_type = ClickSimulator.CLICK_TYPE_DOUBLE;
}


class BCDragClick : public BCClick
{
    id = "dragclick";
    button = ClickSimulator.PRIMARY_BUTTON;
    click_type = ClickSimulator.CLICK_TYPE_DRAG;

    void release(LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
    {
        BCClick.release(self, view, button, event_type);
        this->keyboard.show_touch_handles(show=this->_can_show_handles(),
                                         auto_hide=false);
    }

    void update()
    {
        active_before = this->key.active;
        BCClick.update(self);
        active_now = this->key.active;

        if (active_before && !active_now)
        {
            // hide the touch handles
            this->keyboard.show_touch_handles(this->_can_show_handles());
        }
    }

    void _can_show_handles()
    {
        return (this->is_active() and;
                config()->is_mousetweaks_active() and;
                !config()->xid_mode);
    }
}
#endif

class BCHoverClick : public ButtonController
{
    public:
        static constexpr const char* id = "hoverclick";

        BCHoverClick(const ContextBase& context, const LayoutItemPtr& item) :
            ButtonController(context, item)
        {}

        void release(LayoutView*, MouseButton::Enum, EventType::Enum)
        {
            auto mousetweaks = get_mousetweaks();
            if (mousetweaks)
                mousetweaks->enable_hover_click(!mousetweaks->is_active());
            else
                config()->enable_local_hover_click(
                            !config()->is_local_hover_click_enabled());
        }

        void update()
        {
            auto mousetweaks = get_mousetweaks();
            bool available = true;
            bool active = (mousetweaks && mousetweaks->is_active()) ||
                          config()->is_local_hover_click_enabled();

            this->set_sensitive(available &&
                               !config()->lockdown->disable_hover_click);
            // force locked color for better visibility
            this->set_locked(active);
        }

        bool can_dwell()
        {
            auto mousetweaks = get_mousetweaks();
            return !(mousetweaks && mousetweaks->is_active());
        }
};
constexpr const char* BCHoverClick::id;


class BCShowClick : public ButtonController
{
    public:
        static constexpr const char* id = "showclick";

        BCShowClick(const ContextBase& context, const LayoutItemPtr& item) :
            ButtonController(context, item)
        {}

        void release(LayoutView*, MouseButton::Enum, EventType::Enum)
        {
            config()->keyboard->show_click_buttons =
                !config()->keyboard->show_click_buttons;
        }

        void update()
        {
            bool allowed = !config()->lockdown->disable_click_buttons;

            set_visible(allowed);

            // Don't show active state. Toggling the click column
            // should be enough feedback.
            // set_active(config()->keyboard->show_click_buttons)

            // show/hide click buttons
            bool show_click = config()->keyboard->show_click_buttons && allowed;
            auto layout = get_keyboard()->get_layout_root();
            if (layout)
            {
                layout->for_each([&layout, &show_click](const LayoutItemPtr& item) -> void
                {
                    if (item->group == "click")
                    {
                        layout->set_item_visible(item, show_click);
                    }
                    else
                    if (item->group == "noclick")
                    {
                        layout->set_item_visible(item, !show_click);
                    }
                });
            }
        }

        bool can_dwell()
        {
            return true;
            return get_mousetweaks() && get_mousetweaks()->is_active();
        }
};
constexpr const char* BCShowClick::id;


class BCHide : public ButtonController
{
    public:
        static constexpr const char* id = "hide";

        BCHide(const ContextBase& context, const LayoutItemPtr& item) :
            ButtonController(context, item)
        {}

        void release(LayoutView*, MouseButton::Enum, EventType::Enum)
        {
            // No request_keyboard_visible() here, so hide button can
            // unlock_visibility in case of stuck keys.
            get_keyboard_view()->request_visibility(false);
        }

        void update()
        {
            // insensitive in XEmbed mode except in unity-greeter
            this->set_sensitive(!config()->is_xid_mode());
        }
};
constexpr const char* BCHide::id;


// Layer switch button, switches to layer <this->_layer_id> when released.
class BCLayer : public ButtonController
{
    public:
        BCLayer(const ContextBase& context, const LayoutItemPtr& item) :
            ButtonController(context, item)
        {
            auto key = dynamic_cast<LayoutKey*>(item.get());
            m_layer_id = key->get_target_layer_id();
            m_parent_layer_id = key->get_target_layer_parent_id();
        }

        void release(LayoutView*, MouseButton::Enum button, EventType::Enum event_type)
        {
            auto keyboard = get_keyboard();
            auto key_logic = keyboard->get_key_logic();
            auto key = get_key();

            bool active_before =
                keyboard->get_active_layer_id(m_parent_layer_id) == m_layer_id;
            bool locked_before = active_before && keyboard->is_layer_locked();

            bool active;
            bool locked;
            std::tie(active, locked) = key_logic->step_sticky_key_state(key,
                                                  active_before, locked_before,
                                                  button, event_type);

            // push buttons switch layers even though they don't activate the key
            if (!key_logic->can_activate_key(key))
                active = true;

            keyboard->set_active_layer_id(active ? m_layer_id : "",
                                          m_parent_layer_id);

            // sublayers don't lock at all for now
            if (m_parent_layer_id.empty())
            {
                keyboard->set_layer_locked(
                    keyboard->is_first_layer_id(m_layer_id) ? false : locked);
            }

            if (active_before != active)
            {
                keyboard->invalidate_visible_layers();
                keyboard->invalidate_canvas();
            }
        }

        void update()
        {
            auto keyboard = get_keyboard();
            auto key_logic = keyboard->get_key_logic();
            auto key = get_key();

            // don't show active state for layer 0, it'd be visible all the time
            string active_layer_id =
                keyboard->get_active_layer_id(m_parent_layer_id);
            bool active = key->show_active && m_layer_id == active_layer_id;
            if (active)
                active = key_logic->can_activate_key(key);

            this->set_active(active);
            this->set_locked(active &&
                             m_parent_layer_id.empty() &&
                             keyboard->is_layer_locked());
        }

    private:
        string m_layer_id;
        string m_parent_layer_id;
};

class BCMove : public ButtonController
{
    public:
        static constexpr const char* id = "move";

        BCMove(const ContextBase& context, const LayoutItemPtr& item) :
            ButtonController(context, item)
        {}

        virtual void press(LayoutView* view, MouseButton::Enum , EventType::Enum ) override
        {
            if (!config()->is_xid_mode() &&
                view == view->get_originating_view())  // not popup?
                view->start_move_view();
        }

        virtual void long_press(LayoutView*, MouseButton::Enum) override
        {
            if (!config()->is_xid_mode())
                get_keyboard_view()->show_touch_handles(true);
        }

        virtual void release(LayoutView* view, MouseButton::Enum, EventType::Enum) override
        {
            if (!config()->is_xid_mode() &&
                view == view->get_originating_view())  // not popup?
                view->stop_move_view();
            else
                get_keyboard_view()->show_touch_handles(true);
        }

        virtual void update() override
        {
            set_visible(!config()->has_window_decoration() &&
                        !config()->is_xid_mode() &&
                        contains(config()->window->window_handles.get(), Handle::MOVE));
        }

        virtual bool is_activated_on_press() override
        {
            return true;  // cannot undo on press, dragging is already in progress
        }
};
constexpr const char* BCMove::id;


class BCPreferences : public ButtonController
{
    public:
        static constexpr const char* id = "settings";

        BCPreferences(const ContextBase& context, const LayoutItemPtr& item) :
            ButtonController(context, item)
        {}

        void release(LayoutView*, MouseButton::Enum, EventType::Enum)
        {
            run_cmd({"onboard-settings"});
        }

        void update()
        {
            set_visible(!config()->is_xid_mode() &&
                        !config()->is_running_under_gdm() &&
                        !config()->lockdown->disable_preferences);
        }
};
constexpr const char* BCPreferences::id;


#if 0
class BCQuit : public ButtonController
{

    id = "quit";

    void release(LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
    {
        app = this->keyboard.get_application();
        if (app)
        {
            // finish current key processing then quit
            GLib.idle_add(app.do_quit_onboard);
        }
    }

    void update()
    {
        this->set_visible(!config()->xid_mode and;
                         !config()->lockdown.disable_quit);
    }
}
#endif

class BCExpandCorrections : public ButtonController
{
    public:
        static constexpr const char* id = "expand-corrections";

        BCExpandCorrections(const ContextBase& context, const LayoutItemPtr& item) :
            ButtonController(context, item)
        {}

        void release(LayoutView*, MouseButton::Enum, EventType::Enum)
        {
            auto wordlist = std::dynamic_pointer_cast<LayoutPanelWordList>(
                                get_key()->get_parent());
            if (wordlist)
                wordlist->expand_corrections(!wordlist->are_corrections_expanded());
        }
};
constexpr const char* BCExpandCorrections::id;


class BCPreviousPredictions : public ButtonController
{
    public:
        static constexpr const char* id = "previous-predictions";

        BCPreviousPredictions(const ContextBase& context, const LayoutItemPtr& item) :
            ButtonController(context, item)
        {}

        void release(LayoutView*, MouseButton::Enum, EventType::Enum)
        {
            auto wordlist = std::dynamic_pointer_cast<LayoutPanelWordList>(
                                get_key()->get_parent());
            if (wordlist)
            {
                wordlist->goto_previous_predictions();
                get_keyboard()->invalidate_context_ui();
            }
        }

        void update_late()
        {
            auto wordlist = std::dynamic_pointer_cast<LayoutPanelWordList>(
                                get_key()->get_parent());
            if (wordlist)
                set_sensitive(wordlist->can_goto_previous_predictions());
        }
};
constexpr const char* BCPreviousPredictions::id;


class BCNextPredictions : public ButtonController
{
    public:
        static constexpr const char* id = "next-predictions";

        BCNextPredictions(const ContextBase& context, const LayoutItemPtr& item) :
            ButtonController(context, item)
        {}

        void release(LayoutView*, MouseButton::Enum, EventType::Enum)
        {
            auto wordlist = std::dynamic_pointer_cast<LayoutPanelWordList>(
                                get_key()->get_parent());
            if (wordlist)
            {
                wordlist->goto_next_predictions();
                get_keyboard()->invalidate_context_ui();
            }
        }

        void update_late()
        {
            auto wordlist = std::dynamic_pointer_cast<LayoutPanelWordList>(
                                get_key()->get_parent());
            if (wordlist)
                set_sensitive(wordlist->can_goto_next_predictions());
        }
};
constexpr const char* BCNextPredictions::id;


class BCPauseLearning : public ButtonController
{
    public:
        static constexpr const char* id = "pause-learning";

        BCPauseLearning(const ContextBase& context, const LayoutItemPtr& item) :
            ButtonController(context, item)
        {}

        void release(LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
        {
            (void)view;

            auto key_logic = get_key_logic();
            auto ws = get_word_suggestions();
            auto key = get_key();

            bool active, locked;
            std::tie(active, locked) = key_logic->step_sticky_key_state(
                                            key,
                                            key->active, key->locked,
                                            button, event_type);
            key->active  = active;
            key->locked  = locked;

            PauseLearning::Enum pause = PauseLearning::OFF;
            if (active)
                pause = PauseLearning::step(pause);
            if (locked)
                pause = PauseLearning::step(pause);

            bool pause_started = (config()->word_suggestions->get_pause_learning()
                                  == PauseLearning::OFF &&
                                  pause != PauseLearning::OFF);

            config()->word_suggestions->set_pause_learning(pause);

            // immediately forget changes
            if (pause_started)
            {
                if (ws)
                    ws->discard_changes();
            }
        }

        void update()
        {
            auto& co = config()->word_suggestions;
            set_active(co->get_pause_learning() != PauseLearning::OFF);
            set_locked(co->get_pause_learning() == PauseLearning::LOCKED);
        }
};
constexpr const char* BCPauseLearning::id;


class BCLanguage : public ButtonController
{
    public:
        static constexpr const char* id = "language";

        BCLanguage(const ContextBase& context, const LayoutItemPtr& item) :
            ButtonController(context, item)
        {}

        using Clock = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        void release(LayoutView* view, MouseButton::Enum button, EventType::Enum event_type)
        {
            (void) event_type;
            using namespace std::chrono;
            auto key = get_key();

            if (Clock::now() - m_menu_close_time > 0.5s)
            {
                set_active(!key->active);
                if (key->active)
                {
                    show_menu(view, key, button);
                }
            }
            m_menu_close_time = {};
        }

        void update()
        {
            if (config()->are_word_suggestions_enabled())
            {
                auto key = get_key();
                auto keyboard = get_keyboard();
                auto ws = get_word_suggestions();
                auto language_db = get_language_db();
                if (!ws || !language_db)
                    return;

                auto lang_id = ws->get_lang_id();
                auto label = capitalize(language_db->get_language_code(lang_id));

                if (label != key->get_label() ||
                    key->tooltip.empty())
                {
                    key->labels = {{0, label}};
                    key->tooltip = language_db->get_language_full_name(lang_id);
                    keyboard->invalidate_ui();
                }
            }
        }

    private:
        void show_menu(LayoutView* view, const LayoutKeyPtr& key, MouseButton::Enum button)
        {
            auto ws = get_word_suggestions();
            if (ws)
                ws->show_language_selection(view, key, button, [&]{on_menu_closed();});
        }

        void on_menu_closed()
        {
            auto keyboard = get_keyboard();

            set_active(false);
            keyboard->invalidate_key(get_key());
            m_menu_close_time = Clock::now();
        }

    private:
        TimePoint m_menu_close_time;
};
constexpr const char* BCLanguage::id;


using MakeFunc = std::unique_ptr<ButtonController>(*)(const ContextBase& context,
                                                      const LayoutItemPtr& item);
template <class T>
std::pair<string, MakeFunc> make_map_entry()
{
    return  {T::id,
        [](const ContextBase& context,
           const LayoutItemPtr& item) -> std::unique_ptr<ButtonController>
        {return std::make_unique<T>(context, item);}};
}

std::unique_ptr<ButtonController> ButtonController::make_for(const LayoutItemPtr& item, const ContextBase& context)
{
    static std::map<string, MakeFunc> m = {
        make_map_entry<BCExpandCorrections>(),
        make_map_entry<BCHide>(),
        make_map_entry<BCHoverClick>(),
        make_map_entry<BCLanguage>(),
        make_map_entry<BCMove>(),
        make_map_entry<BCNextPredictions>(),
        make_map_entry<BCPauseLearning>(),
        make_map_entry<BCPreferences>(),
        make_map_entry<BCPreviousPredictions>(),
        make_map_entry<BCShowClick>(),
    };

    auto key = dynamic_cast<LayoutKey*>(item.get());
    if (key && key->is_layer_button())
        return std::make_unique<BCLayer>(context, item);

    auto func = get_value(m, item->get_id());
    if (func)
        return func(context, item);

    return {};
}

