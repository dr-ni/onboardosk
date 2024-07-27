#ifndef ONBOARDOSKGLOBALS_H
#define ONBOARDOSKGLOBALS_H

#include <map>
#include <memory>
#include <vector>

class AtspiStateTracker;
class AudioPlayer;
class Config;
class GlobalKeyListener;
class MouseTweaks;
class HardwareSensorTracker;
class InputEventSource;
class Keyboard;
class KeyboardKeyLogic;
class KeyboardView;
class LanguageDB;
class Logger;
class OnboardOsk;
class TextContext;
class UDevTracker;
class ViewBase;
class VirtKey;
class WordSuggestions;
class WPEngine;
class XDGDirs;

class LayoutView;
typedef std::shared_ptr<LayoutView> LayoutViewPtr;

using LayoutViews = std::vector<LayoutViewPtr>;

enum class TextRendererSlot
{
    DEFAULT,
    LABEL = DEFAULT,
    SECONDARY,
    POPUP_INDICATOR
};

typedef int FontSizeInt;

using TextRenderers = std::map<std::pair<TextRendererSlot, FontSizeInt>,
                               std::unique_ptr<class TextRendererPangoCairo>>;


class Toolkit
{
    public:
        enum Enum
        {
            CAIRO,
        };
};

struct _OnboardOskContext;
typedef struct _OnboardOskContext OnboardOskContext;

struct OnboardOskCallbacks;

class OnboardOskGlobals
{
    private:
        OnboardOskCallbacks* m_callbacks;  // callbacks for toolkit dependent functionality

        std::shared_ptr<Logger> m_logger;
        std::unique_ptr<Config> m_config;

        std::unique_ptr<XDGDirs> m_xdgdirs;
        std::unique_ptr<GlobalKeyListener> m_global_key_listener;  // impossible in wayland?
        std::unique_ptr<UDevTracker> m_udev_tracker;
        std::unique_ptr<HardwareSensorTracker> m_hardware_sensor_tracker;
        std::unique_ptr<AtspiStateTracker> m_atspi_state_tracker;
        std::unique_ptr<VirtKey> m_virtkey;
        std::unique_ptr<AudioPlayer> m_audio_player;
        std::unique_ptr<MouseTweaks> m_mousetweaks;
        std::unique_ptr<LanguageDB> m_language_db;
        std::unique_ptr<InputEventSource> m_input_event_source;

        OnboardOsk* m_instance;
        std::unique_ptr<Keyboard> m_keyboard;
        std::shared_ptr<KeyboardView> m_keyboard_view;
        LayoutViews m_layout_views;
        std::vector<ViewBase*> m_toplevel_views;

        Toolkit::Enum m_toolkit;
        bool m_supports_alpha{true};

        // PangoLayout leaks memory as usual. Try to mitigate this by keeping a
        // limited number of layouts and reuse them wherever text drawing is needed.
        TextRenderers m_text_renderers;

        // suppress delays between key-presses for all KeySynths
        bool m_suppress_keypress_delay{false};


        friend class OnboardOskImpl;
        friend class ContextBase;
};

// Base class for transporting global settings deep into the class hierarchy
class ContextBase
{
    public:
        ContextBase(OnboardOskGlobals* globals) :
            m_globals(globals)
        {}

        const OnboardOskCallbacks* get_global_callbacks() const {return m_globals->m_callbacks;}
        bool is_composited();

        Logger* logger();
        const Logger* logger() const;

        Config* config() {return m_globals->m_config.get();}
        const Config* config() const {return m_globals->m_config.get();}

        OnboardOsk* get_instance() {return m_globals->m_instance;}
        OnboardOskContext* get_cinstance();  // for C-callbacks

        Keyboard* get_keyboard() const {return m_globals->m_keyboard.get();}
        KeyboardKeyLogic* get_key_logic() const;

        WordSuggestions* get_word_suggestions() const;
        WPEngine* get_wp_engine() const;
        TextContext* get_text_context() const;

        KeyboardView* get_keyboard_view() {return m_globals->m_keyboard_view.get();}
        LayoutViews& get_keyboard_layout_views();
        std::vector<LayoutView*> get_layout_views();
        std::vector<ViewBase*>& get_toplevel_views() {return m_globals->m_toplevel_views;}

        bool is_toplevel_view(ViewBase* view);
        bool is_leaf_view(ViewBase* view);
        ViewBase* get_first_toplevel_view();
        void add_toplevel_view(ViewBase* view);
        void remove_toplevel_view(ViewBase* view);

        MouseTweaks* get_mousetweaks() {return m_globals->m_mousetweaks.get();}

        XDGDirs* get_xdg_dirs() {return m_globals->m_xdgdirs.get();}
        GlobalKeyListener* get_global_key_listener() {return m_globals->m_global_key_listener.get();}
        VirtKey* get_virtkey() {return m_globals->m_virtkey.get();}
        AudioPlayer* get_audio_player() {return m_globals->m_audio_player.get();}
        AtspiStateTracker* get_atspi_state_tracker() {return m_globals->m_atspi_state_tracker.get();}
        HardwareSensorTracker* get_hardware_sensor_tracker() {return m_globals->m_hardware_sensor_tracker.get();}
        UDevTracker* get_udev_tracker() {return m_globals->m_udev_tracker.get();}
        LanguageDB* get_language_db() {return m_globals->m_language_db.get();}
        InputEventSource* get_input_event_source() {return m_globals->m_input_event_source.get();}

        Toolkit::Enum get_toolkit() const {return m_globals->m_toolkit;}
        bool supports_alpha() const {return m_globals->m_supports_alpha;}

        TextRenderers& get_text_renderers() {return  m_globals->m_text_renderers;}
        void clear_text_renderers();
        TextRendererPangoCairo* get_text_renderer(TextRendererSlot slot,
                                                  FontSizeInt font_size_int);

        bool get_suppress_keypress_delay()  {return  m_globals->m_suppress_keypress_delay;}
        void set_suppress_keypress_delay(bool suppress)  {m_globals->m_suppress_keypress_delay = suppress;}

    public:
        OnboardOskGlobals* m_globals{};     // weak pointer to the one held by OnboardOsk
};

#endif // ONBOARDOSKGLOBALS_H
