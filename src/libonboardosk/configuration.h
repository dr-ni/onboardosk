#ifndef CONFIG_H
#define CONFIG_H

#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

using std::array;
using std::vector;
using std::string;

#include "tools/container_helpers.h"
#include "tools/noneable.h"
#include "tools/point_decl.h"
#include "tools/border.h"
#include "tools/string_helpers.h"

#include "configdecls.h"
#include "configobject.h"
#include "handle.h"
#include "layoutdecls.h"
#include "mousetweaks.h"
#include "keyboarddecls.h"
#include "onboardoskglobals.h"


// gsettings schemas
constexpr const char* SCHEMA_ONBOARD           = "org.onboardosk";
constexpr const char* SCHEMA_KEYBOARD          = "org.onboardosk.keyboard";
constexpr const char* SCHEMA_WINDOW            = "org.onboardosk.window";
constexpr const char* SCHEMA_WINDOW_LANDSCAPE  = "org.onboardosk.window.landscape";
constexpr const char* SCHEMA_WINDOW_PORTRAIT   = "org.onboardosk.window.portrait";
constexpr const char* SCHEMA_ICP               = "org.onboardosk.icon-palette";
constexpr const char* SCHEMA_ICP_LANDSCAPE     = "org.onboardosk.icon-palette.landscape";
constexpr const char* SCHEMA_ICP_PORTRAIT      = "org.onboardosk.icon-palette.portrait";
constexpr const char* SCHEMA_AUTO_SHOW         = "org.onboardosk.auto-show";
constexpr const char* SCHEMA_UNIVERSAL_ACCESS  = "org.onboardosk.universal-access";
constexpr const char* SCHEMA_THEME             = "org.onboardosk.theme-settings";
constexpr const char* SCHEMA_LOCKDOWN          = "org.onboardosk.lockdown";
constexpr const char* SCHEMA_SCANNER           = "org.onboardosk.scanner";
constexpr const char* SCHEMA_TYPING_ASSISTANCE = "org.onboardosk.typing-assistance";
constexpr const char* SCHEMA_WORD_SUGGESTIONS  = "org.onboardosk.typing-assistance.word-suggestions";

constexpr const char* SCHEMA_GSS               = "org.gnome.desktop.screensaver";
constexpr const char* SCHEMA_GDI               = "org.gnome.desktop.interface";
constexpr const char* SCHEMA_GDA               = "org.gnome.desktop.a11y.applications";
constexpr const char* SCHEMA_UNITY_GREETER     = "com.canonical.unity-greeter";
constexpr const char* SCHEMA_A11Y_MOUSE        = "org.gnome.desktop.a11y.mouse";

// constexpr const char* MODELESS_GKSU_KEY = "/apps/gksu/disable-grab";  // old gconf key, unused

// hard coded defaults
constexpr const double DEFAULT_X               = 100;   // Make sure these match the schema defaults,
constexpr const double DEFAULT_Y               = 50;    // else dconf data migration won't happen.
constexpr const double DEFAULT_WIDTH           = 700;
constexpr const double DEFAULT_HEIGHT          = 205;

static const std::vector<Handle::Enum> DEFAULT_WINDOW_HANDLES{Handle::RESIZE_MOVE.begin(),
                                                              Handle::RESIZE_MOVE.end()};

#define CLASS_CONSTANT_STRING static constexpr const char* const

// launched by ...
class Launcher
{
    public:
        enum Enum {
            NONE,
            GSS,
            UNITY_GREETER
        };
};

class ScanMode
{
    public:
        enum Enum {
            AUTOSCAN,
            OVERSCAN,
            STEPSCAN,
            DIRECTED
        };
};

class RepositionMethod
{
    public:
        enum Enum {
            NONE,
            PREVENT_OCCLUSION,
            REDUCE_POINTER_TRAVEL,
        };
};


class CommandLineOptions;
class ConfigAutoShow;
class ConfigA11yMouse;
class ConfigKeyboard;
class ConfigLockdown;
class ConfigScanner;
class ConfigTypingAssistance;
class ConfigUniversalAccess;
class ConfigWindow;
class COWindowOrientation;
class ConfigWordSuggestions;


class Config : public ConfigObject
{
    public:
        // Keep absolute paths variable for OSs that deviate from
        // the Filesystem Hierarchy Standard, e.g. NixOS.
        const string usr_lib_dir{"/usr/lib"};
        const string usr_share_dir{"/usr/share"};
        const string usr_local_share_dir{"/usr/local/share"};

        const string default_install_dir {fs::path(usr_share_dir) / "onboard"};
        const string default_local_install_dir {fs::path(usr_local_share_dir) / "onboard"};
        const string isocodes_dir{fs::path(usr_share_dir) / "xml/iso-codes"};

        const string user_sub_dir{"onboard"};

        const string default_layout{"Compact"};
        const string default_theme{"Classic Onboard"};
        const string default_color_scheme{"Classic Onboard"};

        const string layout_subdir{"layouts"};
        const string layout_file_extension{"onboard"};

        const string theme_subdir{"themes"};
        const string theme_file_extension{"theme"};

        const string color_scheme_subdir{"themes"};
        const string color_scheme_file_extension{"colors"};

        const string models_subdir{"models"};
        const string models_file_extension{"lm"};


        // A copy of snippets so that when the list changes in gsettings we can
        // tell which items have changed.
        //_last_snippets = None;

        // Margin to leave around labels
        const Size label_margin{1, 1};

        // Horizontal label alignment
        const double default_label_x_align{0.5};

        // Vertical label alignment
        const double default_label_y_align{0.5};

        // layout group for independently sized superkey labels
        const string superkey_size_group{"super"};

        // width of frame around onboard when window decoration is disabled
        const double undecorated_frame_width{6.0};

        // width of frame around popup windows
        const double popup_frame_width{5.0};

        // radius of the rounded window corners
        const double corner_radius{10};

        // y displacement of the key face of dish keys
        const double dish_key_y_offset{0.8};

        // raised border size of dish keys
        const Size dish_key_border{2.5, 2.5};

        // minimum time keys are drawn in pressed state
        const double unpress_delay{0.15};

        // Margin to leave around wordlist labels; smaller margins leave
        // more room for prediction choices
        const Size wordlist_label_margin{2, 2};

        // Gap between wordlist buttons
        const Size wordlist_button_spacing{0.5, 0.5};

        // Gap between wordlist predictions && correctios
        const Size wordlist_entry_spacing{1.0, 1.0};

        // threshold protect window move/resize
        const bool drag_protection{true};

        Launcher::Enum launched_by{Launcher::NONE};

        double window_scaling_factor{1.0};

    public:
        // persistent configuration keys
        GSKey<string> schema_version{this, "schema-version", ""};   // is assigned SCHEMA_VERSION on first start
        GSKey<bool> use_system_defaults{this, "use-system-defaults", false};
        GSKey<string> layout{this, "layout", default_layout};
        GSKey<string> theme{this, "theme", default_theme};
        GSKey<bool> system_theme_tracking_enabled{this, "system-theme-tracking-enabled", true};
        GSKey<std::map<string, string>> system_theme_associations{this, "system-theme-associations", {}, {"a{ss}"}};
        GSKey<SnippetsMap, vector<string>> snippets{this, "snippets", {},
        {
            "as",
            unpack_snippets,
            pack_snippets
        }};
        GSKey<bool> show_status_icon{this, "show-status-icon", true};
        GSKey<StatusIconProvider::Enum> status_icon_provider{this, "status-icon-provider", StatusIconProvider::APPINDICATOR, {}, {
                                                                {"auto", StatusIconProvider::AUTO},
                                                                {"GtkStatusIcon", StatusIconProvider::GTKSTATUSICON},
                                                                {"AppIndicator", StatusIconProvider::APPINDICATOR},
                                                            }};
        GSKey<bool> start_minimized{this, "start-minimized", false};
        GSKey<bool> show_tooltips{this, "show-tooltips", true};
        GSKey<string> key_label_font{this, "key-label-font", ""};   // default font for all themes

        GSKey<LabelOverrideMap, vector<string>> key_label_overrides{this, "key-label-overrides", {},  // default labels for all themes
        {
            "as",
            unpack_label_overrides,
            pack_label_overrides
        }};
        GSKey<int> current_settings_page{this, "current-settings-page", 0};

        GSKey<bool> xembed_onboard{this, "xembed-onboard", false};
        GSKey<AspectChangeRange, vector<double>>  xembed_aspect_change_range {this, "xembed-aspect-change-range", {0, 1.6},
        {
            "ad",
            [](const vector<double>& v) {
                return AspectChangeRange{v.at(0), v.at(1)};
            },
            [](const AspectChangeRange& r) {
                return vector<double>{r.min_aspect, r.max_aspect};
            },
        }};

        GSKey<string> xembed_background_color{this, "xembed-background-color", "#0000007F"};
        GSKey<bool> xembed_background_image_enabled{this, "xembed-background-image-enabled", true};
        GSKey<double> xembed_unity_greeter_offset_x{this, "xembed-unity-greeter-offset-x", 85.0};


        std::unique_ptr<ConfigKeyboard> keyboard;
        std::unique_ptr<ConfigWindow> window;
        std::unique_ptr<ConfigAutoShow> auto_show;
        std::unique_ptr<ConfigLockdown> lockdown;
        std::unique_ptr<ConfigScanner> scanner;
        std::unique_ptr<ConfigTypingAssistance> typing_assistance;
        std::unique_ptr<ConfigUniversalAccess> universal_access;
        std::unique_ptr<ConfigA11yMouse> a11y_mouse;

        // shortcuts for convenient access
        ConfigWordSuggestions* word_suggestions{};

    public:
        using Super = ConfigObject;
        using This = Config;

        Config(const ContextBase& context);
        ~Config();
        static std::unique_ptr<This> make(const ContextBase& context);

        bool parse_command_line(const std::vector<string>& args);
        void init()
        {
            read_all_keys();
        }

        const ThemePtr current_theme() const;
        void set_current_theme(const ThemePtr& theme);

        ColorSchemePtr current_color_scheme();
        void set_current_color_scheme(const ColorSchemePtr& color_scheme);

        string get_key_label_font() const;
        const LabelOverrideMap get_key_label_overrides() const;

        string get_layout_filename();
        string get_theme_filename();
        string get_color_scheme_filename();
        string get_image_filename(const string& fn);

        string find_layout_filename(const string& description,
                                         const string& filename,
                                         const string& extension={},
                                         const string& final_fallback={});

        // Layout file to fallback to when the initial layout won't load
        string get_fallback_layout_filename();

        string get_user_dir();
        string get_install_dir();
        string get_source_dir();
        bool is_running_from_source();

        string get_user_layout_dir();
        string get_system_layout_dir();
        string get_user_theme_dir();
        string get_system_theme_dir();
        string get_user_color_scheme_dir();
        string get_system_color_scheme_dir();
        string get_user_model_dir();
        string get_system_model_dir();

        vector<string> get_emojione_image_dirs();

        double get_window_scaling_factor() const {return this->window_scaling_factor;}

        bool is_xid_mode() {return false;}
        bool is_running_under_gdm() {return false;}

        bool has_window_decoration();
        bool get_sticky_state();

        bool is_visible_at_start();

        bool is_inactive_transparency_enabled();

        // Keep aspect ratio of the whole keyboard window?
        // Not recommended, no effect in MATE and elsewhere the
        // keyboard tends to shrink with each interaction.
        bool is_keep_window_aspect_ratio_enabled();

        // Keep aspect ratio of only the frame (keyboard area)
        // inside the keyboard window?
        bool is_keep_frame_aspect_ratio_enabled();

        bool is_keep_xembed_frame_aspect_ratio_enabled();

        bool is_keep_docking_frame_aspect_ratio_enabled();

        bool is_force_to_top();

        bool is_alt_special();

        bool is_docking_enabled();

        bool is_dock_expanded();
        Size get_dock_size();

        // Multiple toplevel views allowed (e.g. for tablet thumb keyboard)?
        bool can_have_multiple_toplevels();

        bool is_screen_landscape();
        void set_screen_landscape(bool is_landscape);
        COWindowOrientation* get_window_orientation_co();
        COWindowOrientation* get_window_orientation_co(bool is_landscape);

        bool is_auto_show_enabled();

        bool is_auto_hide_enabled();

        bool is_auto_hide_on_keypress_enabled();

        bool is_tablet_mode_detection_enabled();

        bool is_keyboard_device_detection_enabled();

        bool can_auto_show_reposition();

        RepositionMethod::Enum get_auto_show_reposition_method();

        // Allowed to change auto hide?
        bool can_set_auto_hide();

        bool are_word_suggestions_enabled();

        bool are_spelling_suggestions_enabled();

        bool is_spell_checker_enabled();

        bool is_auto_capitalization_enabled();

        bool is_typing_assistance_enabled();

        bool can_log_learning();

        bool is_event_source_gtk();

        bool is_event_source_xinput();

        double get_drag_threshold();

        bool are_touch_events_enabled() const;
        bool is_multi_touch_enabled() const;

        // Fall-back hover click only for Onboard's keys.
        bool is_local_hover_click_enabled();
        void enable_local_hover_click(bool activate);
        double get_local_hover_click_dwell_delay();
        double get_local_hover_click_dwell_threshold();

        void set_active_language_id(const std::string& lang_id,
                                    bool add_to_mru=false);
        void set_mru_lang_id(const std::string& lang_id);

        // String of handle ids to array of Handle enums
        static std::vector<Handle::Enum> string_to_handles(const string& s);

        // Array of handle enums to string of handle ids
        static string handles_to_string(std::vector<Handle::Enum> handles);

    private:
        static SnippetsMap unpack_snippets(const vector<string>& value);
        static vector<string> pack_snippets(const SnippetsMap& value);

        static vector<string> pack_label_overrides(const LabelOverrideMap& value);
        static LabelOverrideMap unpack_label_overrides(const vector<string>& value);

        // Find file, either the root layout or an include file.
        string find_filename(const string& description,
                                  const string& subdir,
                                  const string& filename,
                                  const string& extension={},
                                  const string& final_fallback={});

        using FilenameCallback = std::function<string(string)>;
        string get_user_sys_filename(const string filename,
                                          const string description,
                                          const string final_fallback = {},
                                          const FilenameCallback& user_filename_func = {},
                                          const FilenameCallback& system_filename_func = {});

        void invalidate_layout_resource_search_paths();
        std::vector<string> get_layout_resource_search_paths(const string& subdir);
        string get_resource_filename(const string& filename, const string& description, const vector<string>& paths, const string& final_fallback={});

    private:
        std::unique_ptr<CommandLineOptions> options;

        string m_source_dir;
        std::vector<string> m_image_search_paths;

        ThemePtr m_current_theme;
        ColorSchemePtr m_current_color_scheme;

        mutable Noneable<string> m_cached_key_label_font;
        bool m_is_screen_landscape{true};
};



typedef std::map<string, string> KeyPressModifiers;

class ConfigKeyboard : public ConfigObject
{
    public:
        GSKey<bool>                 show_click_buttons{this, "show-click-buttons", false};
        GSKey<double>               sticky_key_release_delay{this, "sticky-key-release-delay", 0.0};
        GSKey<double>               sticky_key_release_on_hide_delay{this, "sticky-key-release-on-hide-delay", 5.0};
        GSKey<std::map<string, string>>  sticky_key_behavior{this, "sticky-key-behavior", {{"all", "cycle"}}, {"a{ss}"}};
        GSKey<double>               long_press_delay{this, "long-press-delay", 0.5};
        //
        GSKey<KeyAction::Enum>      default_key_action {this, "default-key-action", KeyAction::DELAYED_STROKE, {}, {
            {"single-stroke", KeyAction::SINGLE_STROKE},
            {"delayed-stroke", KeyAction::DELAYED_STROKE},
        }};
        static_assert(KeyAction::SINGLE_STROKE == 0, "KeyAction must match DefaultKeyAction in gsettings schema");
        static_assert(KeyAction::DELAYED_STROKE == 1, "KeyAction must match DefaultKeyAction in gsettings schema");

        GSKey<KeySynthEnum::Enum>       key_synth {this, "key-synth", KeySynthEnum::AUTO, {}, {
            {"auto", KeySynthEnum::AUTO},
            {"XTest", KeySynthEnum::XTEST},
            {"uinput", KeySynthEnum::UINPUT},
            {"AT-SPI", KeySynthEnum::ATSPI},
            {"GNOME_SHELL", KeySynthEnum::GNOME_SHELL},
        }};
        GSKey<TouchInputMode::Enum> touch_input {this, "touch-input", TouchInputMode::MULTI, {}, {
            {"none", TouchInputMode::NONE},
            {"single", TouchInputMode::SINGLE},
            {"multi", TouchInputMode::MULTI},
        }};
        GSKey<InputEventSourceEnum::Enum> input_event_source {this, "input-event-source", InputEventSourceEnum::XINPUT, {}, {
            {"GTK", InputEventSourceEnum::GTK},
            {"XInput", InputEventSourceEnum::XINPUT},
        }};
        GSKey<bool>                 touch_feedback_enabled{this, "touch-feedback-enabled", false};
        GSKey<int>                  touch_feedback_size{this, "touch-feedback-size", 0};
        GSKey<bool>                 audio_feedback_enabled{this, "audio-feedback-enabled", false};
        GSKey<bool>                 audio_feedback_place_in_space{this, "audio-feedback-place-in-space", false};
        GSKey<bool>                 show_secondary_labels{this, "show-secondary-labels", false};
        GSKey<double>               inter_key_stroke_delay{this, "inter-key-stroke-delay", 0.0};
        GSKey<double>               modifier_update_delay{this, "modifier-update-delay", 1.0};
        GSKey<KeyPressModifiers>    key_press_modifiers{this, "key-press-modifiers", {{"button3", "SHIFT"}}, {"a{ss}"}};

    public:
        ConfigKeyboard(ConfigObject* parent) :
            ConfigObject(parent, SCHEMA_KEYBOARD, "keyboard")
        {}

        bool can_upper_case_on_button(MouseButton::Enum button)
        {
            auto& kpms = this->key_press_modifiers.get();
            string key = "button" + std::to_string(static_cast<int>(button));
            return get_value(kpms, key) == "SHIFT";
        }

        void set_upper_case_on_button(MouseButton::Enum button, bool on)
        {
            string key = "button" + std::to_string(static_cast<int>(button));
            KeyPressModifiers kpms = this->key_press_modifiers.get();  // must make a copy
            if (on)
                kpms[key] = "SHIFT";
            else
                remove(kpms, key);
            this->key_press_modifiers = kpms;
        }
};


class COWindowOrientation : public ConfigObject
{
    public:
        COWindowOrientation(ConfigObject* parent, const string& schema, const string& section) :
            ConfigObject(parent, schema, section)
        {}

    public:
        GSKey<double, int> x{this, "x", DEFAULT_X};
        GSKey<double, int> y{this, "y", DEFAULT_Y};
        GSKey<double, int> width{this, "width", DEFAULT_WIDTH};
        GSKey<double, int> height{this, "height", DEFAULT_HEIGHT};
        GSKey<double, int> dock_width{this, "dock-width", DEFAULT_WIDTH};
        GSKey<double, int> dock_height{this, "dock-height", DEFAULT_HEIGHT};
        GSKey<bool> dock_expand{this, "dock-expand", true};
};

// Window configuration
class ConfigWindow : public ConfigObject
{
    private:
        class Landscape : public COWindowOrientation
        {
            public:
                Landscape(ConfigObject* parent) :
                    COWindowOrientation(parent, SCHEMA_WINDOW_LANDSCAPE, "window.landscape")
                {}
        };

        class Portrait : public COWindowOrientation
        {
            public:
                Portrait(ConfigObject* parent) :
                    COWindowOrientation(parent, SCHEMA_WINDOW_PORTRAIT, "window.portrait")
                {}
        };

    public:
        GSKey<bool>                 window_state_sticky{this, "window-state-sticky", true};
        GSKey<bool>                 window_decoration{this, "window-decoration", false};
        GSKey<bool>                 force_to_top{this, "force-to-top", false};
        GSKey<bool>                 keep_aspect_ratio{this, "keep-aspect-ratio", false};
        GSKey<bool>                 transparent_background{this, "transparent-background", false};
        GSKey<double>               transparency{this, "transparency", 0.0};
        GSKey<double>               background_transparency {this, "background-transparency", 10.0};
        GSKey<bool>                 enable_inactive_transparency {this, "enable-inactive-transparency", false};
        GSKey<double>               inactive_transparency{this, "inactive-transparency", 50.0};
        GSKey<double>               inactive_transparency_delay{this, "inactive-transparency-delay", 1.0};

        GSKey<vector<Handle::Enum>, string>  window_handles{this, "window-handles", DEFAULT_WINDOW_HANDLES,
            {
                "s",
                [](const string& value) {return Config::string_to_handles(value);},
                [](const vector<Handle::Enum>& v) {return Config::handles_to_string(v);}
            }};

        GSKey<bool>                 docking_enabled {this, "docking-enabled", false};
        GSKey<DockingEdge::Enum>    docking_edge {this, "docking-edge", DockingEdge::BOTTOM, {}, {
            {"top", DockingEdge::TOP},
            {"bottom", DockingEdge::BOTTOM},
        }};
        GSKey<DockingMonitor::Enum> docking_monitor {this, "docking-monitor", DockingMonitor::ACTIVE, {}, {
            {"active", DockingMonitor::ACTIVE},
            {"primary", DockingMonitor::PRIMARY},
            {"monitor0", DockingMonitor::MONITOR0},
            {"monitor1", DockingMonitor::MONITOR1},
            {"monitor2", DockingMonitor::MONITOR2},
            {"monitor3", DockingMonitor::MONITOR3},
            {"monitor4", DockingMonitor::MONITOR4},
            {"monitor5", DockingMonitor::MONITOR5},
            {"monitor6", DockingMonitor::MONITOR6},
            {"monitor7", DockingMonitor::MONITOR7},
            {"monitor8", DockingMonitor::MONITOR8},
        }};
        GSKey<bool>                 docking_shrink_workarea {this, "docking-shrink-workarea", true};
        GSKey<AspectChangeRange, std::vector<double>>  docking_aspect_change_range {this, "docking-aspect-change-range", {0, 1.6},
        {
            "ad",
            [](const vector<double>& v) {
                return AspectChangeRange{v.at(0), v.at(1)};
            },
            [](const AspectChangeRange& r) {
                return vector<double>{r.min_aspect, r.max_aspect};
            },
        }};

        std::unique_ptr<ConfigWindow::Landscape> landscape{std::make_unique<ConfigWindow::Landscape>(this)};
        std::unique_ptr<ConfigWindow::Portrait>  portrait{std::make_unique<ConfigWindow::Portrait>(this)};

    public:
        ConfigWindow(ConfigObject* parent) :
            ConfigObject(parent, SCHEMA_WINDOW, "window")
        {
            m_children.emplace_back(this->landscape.get());
            m_children.emplace_back(this->portrait.get());
        }

        COWindowOrientation* get_orientation_co(bool landscape_)
        {
            if (landscape_)
                return this->landscape.get();
            return this->portrait.get();
        }
/*
    void position_notify_add(callback)
    {
        this->landscape.x_notify_add(callback);
        this->landscape.y_notify_add(callback);
        this->portrait.x_notify_add(callback);
        this->portrait.y_notify_add(callback);
    }

    void size_notify_add(callback)
    {
        this->landscape.width_notify_add(callback);
        this->landscape.height_notify_add(callback);
        this->portrait.width_notify_add(callback);
        this->portrait.height_notify_add(callback);
    }

    void dock_size_notify_add(callback)
    {
        this->landscape.dock_width_notify_add(callback);
        this->landscape.dock_height_notify_add(callback);
        this->portrait.dock_width_notify_add(callback);
        this->portrait.dock_height_notify_add(callback);
    }

    void docking_notify_add(callback)
    {
        this->docking_enabled_notify_add(callback);
        this->docking_edge_notify_add(callback);
        this->docking_monitor_notify_add(callback);
        this->docking_shrink_workarea_notify_add(callback);

        this->landscape.dock_expand_notify_add(callback);
        this->portrait.dock_expand_notify_add(callback);
    }
*/
        double get_active_opacity()
        {
            return 1.0 - this->transparency / 100.0;
        }

        double get_inactive_opacity()
        {
            return 1.0 - this->inactive_transparency / 100.0;
        }

        double get_minimal_opacity()
        {
            // Return the lowest opacity the window can have when visible.
            return std::min(this->get_active_opacity(), this->get_inactive_opacity());
        }

        double get_background_opacity()
        {
            return 1.0 - this->background_transparency / 100.0;
        }
};


// auto_show configuration
class ConfigAutoShow : public ConfigObject
{
    public:
        GSKey<bool>                  enabled{this, "enabled", false};

        GSKey<RepositionMethod::Enum>::EnumValues reposition_enum_values= {
            {"none", RepositionMethod::NONE},
            {"prevent-occlusion", RepositionMethod::PREVENT_OCCLUSION},
            {"reduce-travel", RepositionMethod::REDUCE_POINTER_TRAVEL},
        };
        GSKey<RepositionMethod::Enum> reposition_method_floating {this, "reposition-method-floating",
                                                                    RepositionMethod::PREVENT_OCCLUSION, {},
                                                                    reposition_enum_values};
        GSKey<RepositionMethod::Enum> reposition_method_docked {this, "reposition-method-docked",
                                                                    RepositionMethod::PREVENT_OCCLUSION, {},
                                                                    reposition_enum_values};

        GSKey<Border, vector<double>> widget_clearance{this, "widget-clearance", {25.0, 55.0, 25.0, 40.0},
        {
            "(dddd)",
            [](const vector<double>& a) {
             return Border{a.at(0), a.at(1), a.at(2), a.at(3)};
            },
            [](const Border& b) {
             return vector<double>{b.left, b.top, b.right, b.bottom};
            },
        }};
        GSKey<bool>                  hide_on_key_press{this, "hide-on-key-press", true};
        GSKey<double>                hide_on_key_press_pause{this, "hide-on-key-press-pause", 1800.0};
        GSKey<bool>                  tablet_mode_detection_enabled{this, "tablet-mode-detection-enabled", true};
        GSKey<int>                   tablet_mode_enter_key{this, "tablet-mode-enter-key", 0};
        GSKey<int>                   tablet_mode_leave_key{this, "tablet-mode-leave-key", 0};
        GSKey<string>                tablet_mode_state_file{this, "tablet-mode-state-file", ""};
        GSKey<string>                tablet_mode_state_file_pattern{this, "tablet-mode-state-file-pattern", "1"};

        GSKey<bool>                  keyboard_device_detection_enabled{this, "keyboard-device-detection-enabled", false};
        GSKey<vector<string>>        keyboard_device_detection_exceptions{this, "keyboard-device-detection-exceptions", {},
        {
            "as",
        }};

    public:
        ConfigAutoShow(ConfigObject* parent) :
            ConfigObject(parent, SCHEMA_AUTO_SHOW, "auto-show")
        {}

        /*
        void tablet_mode_detection_notify_add(callback)
        {
            this->tablet_mode_detection_enabled_notify_add(callback);
            this->tablet_mode_enter_key_notify_add(callback);
            this->tablet_mode_leave_key_notify_add(callback);
        }*/
};


// universal_access configuration
class ConfigUniversalAccess : public ConfigObject
{

    public:
        GSKey<double, int>          drag_threshold {this, "drag-threshold", -1.0};
        GSKey<bool>                 hide_click_type_window {this, "hide-click-type-window", true};
        GSKey<bool>                 enable_click_type_window_on_exit {this, "enable-click-type-window-on-exit", true};
        GSKey<bool>                 local_hover_click_enabled {this, "local-hover-click-enabled", false};

    public:
        ConfigUniversalAccess(ConfigObject* parent);

        // called when changed in gsettings (preferences window)
        void _post_notify_hide_click_type_window()
        {
            auto mousetweaks = get_mousetweaks();
            if (mousetweaks)
                mousetweaks->on_hide_click_type_window_changed(
                    this->hide_click_type_window);
        }
};


// Lockdown/Kiosk mode configuration
class ConfigLockdown : public ConfigObject
{
    public:
        GSKey<bool>                 disable_click_buttons {this, "disable-click-buttons", false};
        GSKey<bool>                 disable_hover_click {this, "disable-hover-click", false};
        GSKey<bool>                 disable_dwell_activation {this, "disable-dwell-activation", false};
        GSKey<bool>                 disable_preferences {this, "disable-preferences", false};
        GSKey<bool>                 disable_quit {this, "disable-quit", false};
        GSKey<bool>                 disable_touch_handles {this, "disable-touch-handles", false};
        GSKey<vector<vector<string>>> disable_keys {this, "disable-keys", {{"CTRL", "LALT", "F[0-9]+"}}, {"aas"}};

    public:
        ConfigLockdown(ConfigObject* parent) :
            ConfigObject(parent, SCHEMA_LOCKDOWN, "lockdown")
        {}

        /*
        void lockdown_notify_add(callback)
        {
            this->disable_click_buttons_notify_add(callback);
            this->disable_hover_click_notify_add(callback);
            this->disable_preferences_notify_add(callback);
            this->disable_quit_notify_add(callback);
        }
        */
};

// Scanner configuration
class ConfigScanner : public ConfigObject
{
    public:
        GSKey<bool>                 enabled{this, "enabled", false};
        GSKey<ScanMode::Enum>       mode {this, "mode", ScanMode::AUTOSCAN, {}, {
            {"Autoscan", ScanMode::AUTOSCAN},
            {"Overscan", ScanMode::OVERSCAN},
            {"Stepscan", ScanMode::STEPSCAN},
            {"Directed", ScanMode::DIRECTED},
        }};
        GSKey<double>               interval{this, "interval", 1.20};
        GSKey<double>               interval_fast{this, "interval-fast", 0.05};
        GSKey<int>                  cycles{this, "cycles", 2};
        GSKey<int>                  backtrack{this, "backtrack", 5};
        GSKey<bool>                 alternate{this, "alternate", false};
        GSKey<bool>                 user_scan{this, "user-scan", false};
        GSKey<string>               device_name{this, "device-name", "Default"};
        GSKey<bool>                 device_detach{this, "device-detach", false};
        GSKey<std::map<int, int>>   device_key_map {this, "device-key-map", {}, {"a{ii}"}};
        GSKey<std::map<int, int>>   device_button_map {this, "device-button-map", {{1, 0}, {3, 5}}, {"a{ii}"}};
        GSKey<bool>                 feedback_flash{this, "feedback-flash", true};

    public:
        ConfigScanner(ConfigObject* parent) :
            ConfigObject(parent, SCHEMA_SCANNER, "scanner")
        {
        }
};


// typing-assistance configuration keys
class ConfigTypingAssistance : public ConfigObject
{
    public:
        GSKey<string>               active_language{this, "active-language", ""};
        GSKey<vector<string>>       recent_languages{this, "recent-languages", {}, {"as"}};
        GSKey<size_t, int>          max_recent_languages{this, "max-recent-languages", 5};
        GSKey<SpellcheckBackend::Enum> spell_check_backend {this, "spell-check-backend", SpellcheckBackend::HUNSPELL, {}, {
            {"hunspell", SpellcheckBackend::HUNSPELL},
            {"aspell", SpellcheckBackend::ASPELL},
        }};
        GSKey<bool>                 auto_capitalization{this, "auto-capitalization", false};
        GSKey<bool>                 auto_correction{this, "auto-correction", false};

        std::unique_ptr<ConfigWordSuggestions> word_suggestions;

    public:
        ConfigTypingAssistance(ConfigObject* parent);
};


// word-suggestions configuration keys
class ConfigWordSuggestions : public ConfigObject
{
    public:
        // wordlist_buttons
        static constexpr const char* KEY_ID_PREVIOUS_PREDICTIONS = "previous-predictions";
        static constexpr const char* KEY_ID_NEXT_PREDICTIONS = "next-predictions";
        static constexpr const char* KEY_ID_LANGUAGE = "language";
        static constexpr const char* KEY_ID_PAUSE_LEARNING = "pause-learning";
        static constexpr const char* KEY_ID_HIDE = "hide";
        static constexpr const std::array<const char*, 5> KEY_ORDER = {
            KEY_ID_PREVIOUS_PREDICTIONS,
            KEY_ID_NEXT_PREDICTIONS,
            KEY_ID_PAUSE_LEARNING,
            KEY_ID_LANGUAGE,
            KEY_ID_HIDE,
        };

        GSKey<bool>                 enabled{this, "enabled", false};
        GSKey<bool>                 auto_learn{this, "auto-learn", false};
        GSKey<bool>                 punctuation_assistance{this, "punctuation-assistance", true};
        GSKey<bool>                 delayed_word_separators_enabled{this, "delayed-word-separators-enabled", false};
        GSKey<bool>                 accent_insensitive{this, "accent-insensitive", true};
        GSKey<int>                  max_word_choices{this, "max-word-choices", 5};
        GSKey<bool>                 spelling_suggestions_enabled{this, "spelling-suggestions-enabled", true};
        GSKey<vector<string>>       wordlist_buttons{this, "wordlist-buttons", {
            KEY_ID_PREVIOUS_PREDICTIONS,
            KEY_ID_NEXT_PREDICTIONS,
            KEY_ID_LANGUAGE,
            KEY_ID_HIDE}
        };
        GSKey<bool>                 pause_learning_locked{this, "pause-learning-locked", false};
        GSKey<LearningBehavior::Enum> learning_behavior_paused{this, "learning-behavior-paused", LearningBehavior::NOTHING, {}, {
            {"nothing", LearningBehavior::NOTHING},
            {"known-only", LearningBehavior::KNOWN_ONLY},
        }};

        // deprecated
        GSKey<bool>                 stealth_mode{this, "stealth-mode", false};
        GSKey<bool>                 show_context_line{this, "show-context-line", false};

    public:
        ConfigWordSuggestions(ConfigObject* parent);
        /*
        void word_prediction_notify_add(callback)
        {
            this->auto_learn_notify_add(callback);
            this->punctuation_assistance_notify_add(callback);
            this->stealth_mode_notify_add(callback);
        }
        */

        string get_can_auto_learn_debug_string();

        bool can_auto_learn();

        bool is_learning_paused();

        PauseLearning::Enum get_pause_learning();

        void set_pause_learning(PauseLearning::Enum);

        /*
        void post_notify_pause_learning_locked()
        {
            this->m_pause_learning = this->pause_learning_locked ?
                PauseLearning::LOCKED : PauseLearning::OFF;
        }
        */

        vector<string> get_shown_wordlist_button_ids();

        void show_wordlist_button(const string& key_id, bool show);

        bool can_show_language_button();

        bool can_show_pause_learning_button();

        bool can_show_more_predictions_button();

        bool can_learn_new_words();

    private:
        int get_button_priority(const string& key_id);

    private:
        PauseLearning::Enum m_pause_learning{PauseLearning::OFF};
};


// Lockdown/Kiosk mode configuration
class ConfigA11yMouse : public ConfigObject
{
    public:
        GSKey<bool>                 dwell_click_enabled {this, "dwell-click-enabled", false};
        GSKey<double>               dwell_time {this, "dwell-time", 1.2};
        GSKey<double, int>          dwell_threshold {this, "dwell-threshold", 10};
        GSKey<bool>                 click_type_window_visible {this, "click-type-window-visible", false};

    public:
        ConfigA11yMouse(ConfigObject* parent) :
            ConfigObject(parent, SCHEMA_A11Y_MOUSE, "a11y_mouse")
        {}
};


#endif // CONFIG_H
