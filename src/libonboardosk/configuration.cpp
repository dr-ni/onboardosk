
#include <string>
#include <assert.h>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "tools/container_helpers.h"
#include "tools/file_helpers.h"
#include "tools/iostream_helpers.h"
#include "tools/logger.h"
#include "tools/string_helpers.h"
#include "tools/xdgdirs.h"

#include "commandlineoptions.h"
#include "configuration.h"
#include "colorscheme.h"
#include "exception.h"
#include "onboardoskcallbacks.h"
#include "theme.h"

using namespace std;


Config::Config(const ContextBase& context) :
    Super(context, SCHEMA_ONBOARD, "main"),
    keyboard(std::make_unique<ConfigKeyboard>(this)),
    window(std::make_unique<ConfigWindow>(this)),
    auto_show(std::make_unique<ConfigAutoShow>(this)),
    lockdown(std::make_unique<ConfigLockdown>(this)),
    scanner(std::make_unique<ConfigScanner>(this)),
    typing_assistance(std::make_unique<ConfigTypingAssistance>(this)),
    universal_access(std::make_unique<ConfigUniversalAccess>(this)),
    a11y_mouse(std::make_unique<ConfigA11yMouse>(this)),

    options(std::make_unique<CommandLineOptions>())
{
    m_children.emplace_back(this->keyboard.get());
    m_children.emplace_back(this->window.get());
    m_children.emplace_back(this->auto_show.get());
    m_children.emplace_back(this->lockdown.get());
    m_children.emplace_back(this->scanner.get());
    m_children.emplace_back(this->typing_assistance.get());
    m_children.emplace_back(this->universal_access.get());
    m_children.emplace_back(this->a11y_mouse.get());

    this->word_suggestions = typing_assistance->word_suggestions.get();
}

Config::~Config()
{

}

std::unique_ptr<Config> Config::make(const ContextBase& context)
{
    return std::make_unique<This>(context);
}

bool Config::parse_command_line(const vector<string>& args)
{
    return options->parse(args);
}

const ThemePtr Config::current_theme() const
{
    return m_current_theme;
}

void Config::set_current_theme(const ThemePtr& theme_)
{
    m_current_theme = theme_;
}

ColorSchemePtr Config::current_color_scheme()
{
    return m_current_color_scheme;
}

void Config::set_current_color_scheme(const ColorSchemePtr& color_scheme)
{
    m_current_color_scheme = color_scheme;
}

string Config::get_key_label_font() const
{
    static const std::array<string, 3> font_attributes =
    {
        "bold",
        "italic",
        "condensed",
    };

    if (m_cached_key_label_font.is_none())
    {
        string value = current_theme()->key_label_font;
        if (!value.empty())
        {
            auto items = split(value);
            if (!items.empty())
            {
                // If no font family was provided merge in system font family
                if (contains(font_attributes, items[0]))
                {
                    auto system_items = split(key_label_font);
                    if (!system_items.empty() &&
                        !contains(font_attributes, system_items[0]))
                    {
                        items.insert(items.begin(), system_items[0]);
                        value = join(items, " ");
                    }
                }
            }
        }
        else
        {
            // get default value from onboard base config instead
            value = key_label_font;
        }

        m_cached_key_label_font = value;
    }

    return m_cached_key_label_font;
}

const LabelOverrideMap Config::get_key_label_overrides() const
{
    // merge with default value from onboard base config
    LabelOverrideMap result = this->key_label_overrides;
    update_map(result, current_theme()->key_label_overrides);
    return result;
}


string Config::get_layout_filename()
{
    auto& gskey = this->layout;
    return find_layout_filename(
                gskey.get_key_name(), gskey.get(),
                layout_file_extension,
                fs::path(get_install_dir()) / this->layout_subdir /
                default_layout += ("." + this->layout_file_extension));
}

string Config::get_theme_filename()
{
    auto& gskey = this->theme;
    return find_filename(
                gskey.get_key_name(), theme_subdir, gskey.get(),
                theme_file_extension,
                fs::path(get_install_dir()) / this->theme_subdir /
                default_color_scheme += ("." + theme_file_extension));
}

string Config::get_color_scheme_filename()
{
    auto name = current_theme()->color_scheme_basename;
    return find_filename(
                "color scheme", color_scheme_subdir, name,
                color_scheme_file_extension,
                fs::path(get_install_dir()) / this->color_scheme_subdir /
                default_color_scheme += ("." + color_scheme_file_extension));
}

// Returns an absolute path for a label image.
// This function isn't linked to any gsettings key.
string Config::get_image_filename(const string& fn)
{
    if (m_image_search_paths.empty())
    {
        m_image_search_paths = get_layout_resource_search_paths("images");
        extend(m_image_search_paths, get_emojione_image_dirs());
    }

    return get_resource_filename(fn, "image", m_image_search_paths);
}

// Force re-generation of search paths at next opportunity.
void Config::invalidate_layout_resource_search_paths()
{
    m_image_search_paths.clear();
}

// Look for files in directory of current layout, user- and
// system directory, in that order.
vector<string> Config::get_layout_resource_search_paths(const string& subdir)
{
    string user_dir = get_user_layout_dir();
    string sys_dir = get_system_layout_dir();
    if (!subdir.empty())
    {
        user_dir = fs::path(user_dir) / subdir;
        sys_dir = fs::path(sys_dir) / subdir;
    }

    vector<string> paths = {user_dir, sys_dir};

    // In case a full path was given for the layout file on command line,
    // look relative to there first.
    if (is_valid_filename(this->layout))
    {
        auto dir = fs::path(this->layout.get()).remove_filename();
        if (!subdir.empty())
            dir /= subdir;

        bool same = true;
        try
        {
            // make sure it isn't one of those we already know
            same = (fs::is_directory(sys_dir) &&
                    fs::equivalent(dir, sys_dir)) ||
                   (fs::is_directory(user_dir) &&
                    fs::equivalent(dir, user_dir));
        }
        catch (const fs::filesystem_error& ex)
        {
            LOG_ERROR << "fs::equivalent failed for " << repr(dir)
                      << ": " << ex.what();
            same = true;
        }

        if (!same)
            paths.insert(paths.begin(), dir);
    }

    return paths;
}


string Config::get_fallback_layout_filename()
{
    return this->find_layout_filename("layout", this->default_layout,
                                      this->layout_file_extension);
}

string Config::find_layout_filename(const string& description,
                                         const string& filename,
                                         const string& extension,
                                         const string& final_fallback)
{
    return find_filename(description, this->layout_subdir, filename, extension, final_fallback);
}

string Config::find_filename(const string& description,
                                  const string& subdir,
                                  const string& filename,
                                  const string& extension,
                                  const string& final_fallback)
{
    assert(!contains(extension, "."));  // must not already contain an extension dot

    FilenameCallback user_filename_func = [&](const string& fn) -> string {
        auto p = fs::path(get_user_dir()) / subdir / fn;
        if (!extension.empty())
            p += ("." + extension);
        return p;
    };
    FilenameCallback system_filename_func = [&](const string& fn) {
        auto p = fs::path(get_install_dir()) / subdir / fn;
        if (!extension.empty())
            p += ("." + extension);
        return p;
    };
    return get_user_sys_filename(filename, description, final_fallback,
                                 user_filename_func, system_filename_func);
}

string Config::get_user_dir()
{
    return get_xdg_dirs()->get_data_home(user_sub_dir);
}

string Config::get_user_layout_dir()
{
    return fs::path(get_user_dir()) / this->layout_subdir;
}

string Config::get_system_layout_dir()
{
    return fs::path(get_install_dir()) / this->layout_subdir;
}

string Config::get_install_dir()
{
    static string install_dir;
    string result = install_dir;
    if (result.empty())
    {
        // when run from source
        if (is_running_from_source())
        {
            /*
            // Add the data directory to the icon search path
            icon_theme = Gtk.IconTheme.get_default();
            src_icon_path = os.path.join(src_path, "icons");
            icon_theme.append_search_path(src_icon_path);
            */
            string src_path = this->get_source_dir();
            result = src_path;
        }
        // when installed to /usr/local
        else
        if (fs::is_directory(default_local_install_dir))
        {
            result = default_local_install_dir;
        }
        // when installed to /usr
        else
        if (fs::is_directory(default_install_dir))
        {
            result = default_install_dir;
        }
        install_dir = result;
    }

    assert(!result.empty());  // warn early when the installation dir wasn't found
    return result;
}

bool Config::is_running_from_source()
{
    return !get_source_dir().empty();
}

string Config::get_user_theme_dir()
{
    return fs::path(get_user_dir()) / this->theme_subdir;

}

string Config::get_system_theme_dir()
{
    return fs::path(get_install_dir()) / this->theme_subdir;

}

string Config::get_user_color_scheme_dir()
{
    return fs::path(get_user_dir()) / this->theme_subdir;
}

string Config::get_system_color_scheme_dir()
{
    return fs::path(get_install_dir()) / this->theme_subdir;
}

string Config::get_user_model_dir()
{
    return fs::path(get_user_dir()) / this->models_subdir;
}

string Config::get_system_model_dir()
{
    return fs::path(get_install_dir()) / this->models_subdir;
}

vector<string> Config::get_emojione_image_dirs()
{
    return {fs::path(get_install_dir()) / "emojione"};
}

bool Config::has_window_decoration()
{
    return this->window->window_decoration && !is_force_to_top();
}

bool Config::get_sticky_state()
{
    return !is_xid_mode() &&
            (this->window->window_state_sticky || is_force_to_top());
}

bool Config::is_visible_at_start()
{
    return is_xid_mode() ||
           (!this->start_minimized &&
            !this->auto_show->enabled);
}

bool Config::is_inactive_transparency_enabled()
{
    return this->window->enable_inactive_transparency &&
            !this->scanner->enabled;
}

bool Config::is_keep_window_aspect_ratio_enabled()
{
    return ((this->window->keep_aspect_ratio ||
            this->options->keep_aspect_ratio) &&
            !this->is_xid_mode() &&
            !this->is_docking_enabled());
}

bool Config::is_keep_frame_aspect_ratio_enabled()
{
    return is_keep_xembed_frame_aspect_ratio_enabled() ||
           is_keep_docking_frame_aspect_ratio_enabled();
}

bool Config::is_keep_xembed_frame_aspect_ratio_enabled()
{
    return this->is_xid_mode() && this->launched_by != Launcher::NONE;
}

bool Config::is_keep_docking_frame_aspect_ratio_enabled()
{
    return (!is_xid_mode() &&
            is_docking_enabled() &&
            is_dock_expanded());
}

bool Config::is_force_to_top()
{
    return this->window->force_to_top || is_docking_enabled();
}

bool Config::is_alt_special()
{
    auto callbacks = get_global_callbacks();
    if (callbacks->is_alt_special)
        return callbacks->is_alt_special();
    return true;
}

bool Config::is_docking_enabled()
{
    return this->window->docking_enabled;
}

bool Config::is_dock_expanded()
{
    auto orientation_co = window->get_orientation_co(is_screen_landscape());
    return this->window->docking_enabled && orientation_co->dock_expand;
}

bool Config::can_have_multiple_toplevels()
{
    return is_docking_enabled() && is_dock_expanded();
}

Size Config::get_dock_size()
{
    auto orientation_co = window->get_orientation_co(is_screen_landscape());
    return {orientation_co->dock_width, orientation_co->dock_height};
}

bool Config::is_screen_landscape()
{
    return m_is_screen_landscape;
}

void Config::set_screen_landscape(bool is_landscape)
{
    m_is_screen_landscape = is_landscape;
}

COWindowOrientation* Config::get_window_orientation_co()
{
    return get_window_orientation_co(is_screen_landscape());
}

COWindowOrientation* Config::get_window_orientation_co(bool is_landscape)
{
    return this->window->get_orientation_co(is_landscape);
}

bool Config::is_auto_show_enabled()
{
    return !is_xid_mode() &&
            auto_show->enabled;
}

bool Config::is_auto_hide_enabled()
{
    return is_auto_hide_on_keypress_enabled() ||
            is_tablet_mode_detection_enabled() ||
            is_keyboard_device_detection_enabled();
}

bool Config::is_auto_hide_on_keypress_enabled()
{
    return can_set_auto_hide() &&
            auto_show->hide_on_key_press;
}

bool Config::is_tablet_mode_detection_enabled()
{
    return can_set_auto_hide() &&
            auto_show->tablet_mode_detection_enabled;
}

bool Config::is_keyboard_device_detection_enabled()
{
    return can_set_auto_hide() &&
            auto_show->keyboard_device_detection_enabled;
}

bool Config::can_auto_show_reposition()
{
    return is_auto_show_enabled() &&
            get_auto_show_reposition_method() != RepositionMethod::NONE;
}

RepositionMethod::Enum Config::get_auto_show_reposition_method()
{
    if (is_docking_enabled())
        return auto_show->reposition_method_docked;
    else
        return auto_show->reposition_method_floating;
}

bool Config::can_set_auto_hide()
{
    return is_auto_show_enabled() &&
           is_event_source_xinput();
}

bool Config::are_word_suggestions_enabled()
{
    return this->word_suggestions->enabled && !is_xid_mode();
}

bool Config::are_spelling_suggestions_enabled()
{
    return are_word_suggestions_enabled() &&
            this->word_suggestions->spelling_suggestions_enabled;
}

bool Config::is_spell_checker_enabled()
{
    return are_spelling_suggestions_enabled() ||
            this->typing_assistance->auto_capitalization ||
            this->typing_assistance->auto_correction;
}

bool Config::is_auto_capitalization_enabled()
{
    return this->typing_assistance->auto_capitalization && !is_xid_mode();
}

bool Config::is_typing_assistance_enabled()
{
    return are_word_suggestions_enabled() ||
            is_auto_capitalization_enabled();
}

bool Config::can_log_learning()
{
    return this->options->log_learning;
}

bool Config::is_event_source_gtk()
{
    return this->keyboard->input_event_source == InputEventSourceEnum::GTK;
}

bool Config::is_event_source_xinput()
{
    return this->keyboard->input_event_source == InputEventSourceEnum::XINPUT;
}

double Config::get_drag_threshold()
{
    double threshold = this->universal_access->drag_threshold;
    if (threshold == -1.0)
    {
        auto callbacks = get_global_callbacks();
        if (callbacks->get_drag_threshold)
            threshold = callbacks->get_drag_threshold();
        else
            threshold = 8;
    }
    return threshold;
}

bool Config::are_touch_events_enabled() const
{
    return this->keyboard->touch_input != TouchInputMode::NONE;
}

bool Config::is_multi_touch_enabled() const
{
    return this->keyboard->touch_input == TouchInputMode::MULTI;
}

bool Config::is_local_hover_click_enabled()
{
    return universal_access->local_hover_click_enabled;
}

void Config::enable_local_hover_click(bool activate)
{
    universal_access->local_hover_click_enabled = activate;
}

double Config::get_local_hover_click_dwell_delay()
{
    return a11y_mouse->dwell_time;
}

double Config::get_local_hover_click_dwell_threshold()
{
    return a11y_mouse->dwell_threshold;
}

void Config::set_active_language_id(const std::string& lang_id, bool add_to_mru)
{
    if (add_to_mru)
        set_mru_lang_id(lang_id);
    this->typing_assistance->active_language = lang_id;
}

void Config::set_mru_lang_id(const std::string& lang_id)
{
    size_t max_recent_languages = this->typing_assistance->max_recent_languages;
    vector<string> recent_languages = this->typing_assistance->recent_languages;

    remove(recent_languages, lang_id);

    recent_languages.emplace(recent_languages.begin(), lang_id);
    this->typing_assistance->recent_languages =
        slice(recent_languages, 0, max_recent_languages);
}

std::vector<Handle::Enum> Config::string_to_handles(const string& s)
{
    auto ids = split(s, ' ');
    std::vector<Handle::Enum> handles;
    for (auto id  : ids)
    {
        auto handle = Handle::id_to_handle(id);
        if (handle != Handle::NONE)
            handles.emplace_back(handle);
    }
    return handles;
}

string Config::handles_to_string(std::vector<Handle::Enum> handles)
{
    string s;
    for (auto handle : handles)
        s += Handle::handle_to_id(handle) + " ";
    return strip(s);
}

string Config::get_source_dir()
{
    if (m_source_dir.empty())
    {
        vector<fs::path> candidates = {
            get_current_directory(),
            //os.path.dirname(os.path.dirname(os.path.abspath(__file__)));
        };

        for (auto& path : candidates)
        {
            auto fn =  path / "data" / "org.onboardosk.gschema.xml";
            if (is_file(fn))
            {
                m_source_dir = path;
                break;
            }
        }
    }

    return m_source_dir;
}

vector<string> escape_colon_strings(const vector<string>& v)
{
    vector<string> result;
    string s;
    for (auto& e : v)
    {
        s = replace_all(e, R"(\)", R"(\\)"); // escape backslash
        s = replace_all(s, R"(:)", R"(\:)"); // escape separator
        result.emplace_back(s);
    }
    return result;
}

vector<string> unescape_colon_strings(const vector<string>& v)
{
    vector<string> result;
    string s;
    for (auto& e : v)
    {
        s = replace_all(e, R"(\\)", R"(\)"); // unescape backslash
        s = replace_all(s, R"(\:)", R"(:)"); // unescape separator
        result.emplace_back(s);
    }
    return result;
}

SnippetsMap Config::unpack_snippets(const vector<string>& value)
{
    SnippetsMap result;
    for (auto& e : value)
    {
        auto fields = unescape_colon_strings(split(e, ':'));
        if (fields.size() == 3)
            result[to_int(fields[0])] = {fields[1], fields[2]};
    }
    return result;
}

vector<string> Config::pack_snippets(const SnippetsMap& value)
{
    vector<string> result;
    for (auto& e : value)
    {
        vector<string> v = escape_colon_strings({to_string(e.first),
                                                 e.second.label,
                                                 e.second.text});
        result.emplace_back(join(v, ":"));
    }
    return result;
}

LabelOverrideMap Config::unpack_label_overrides(const vector<string>& value)
{
    LabelOverrideMap result;
    for (auto& e : value)
    {
        auto fields = unescape_colon_strings(split(e, ':'));
        if (fields.size() == 3)
            result[fields[0]] = {fields[1], fields[2]};
    }
    return result;
}

vector<string> Config::pack_label_overrides(const LabelOverrideMap& value)
{
    vector<string> result;
    for (auto& e : value)
    {
        vector<string> v = escape_colon_strings({e.first,
                                                 e.second.label,
                                                 e.second.group});
        result.emplace_back(join(v, ":"));
    }
    return result;
}

// Checks a filename's validity && if necessary expands it to a
// fully qualified path pointing to either the user or system directory.
// User directory has precedence over the system one.
string Config::get_user_sys_filename(const string filename, const string description,
                                  const string final_fallback,
                                  const FilenameCallback& user_filename_func,
                                  const FilenameCallback& system_filename_func)
{
    string fullpath = filename;

    if (!filename.empty() && !is_valid_filename(filename))
    {
        // assume filename is just a basename instead of a full file path
        LOG_TRACE << description << " '" << filename << "' not found yet, "
                  << "retrying in default paths";

        fullpath = expand_user_sys_filename(filename,
                                            user_filename_func,
                                            system_filename_func);
        if (fullpath.empty())
        {
            LOG_INFO << "unable to locate '" << filename << "', "
                     << "loading default " << description << " instead";
        }
    }

    if (fullpath.empty() && !final_fallback.empty())
    {
        fullpath = final_fallback;
    }

    if (!is_file(fullpath))
    {
        LOG_ERROR << "failed to find " << description << " '" << filename << "'";
        fullpath = {};
    }
    else
    {
        LOG_TRACE << description << " '" << fullpath << "' found.";
    }

    return fullpath;
}

// Find the full path of a valid resource file, e.g. an image file.
// If the filename is already valid, it is returned unchanged. Else
// all given paths are searched for the filename.
string Config::get_resource_filename(const string& filename,
                                     const string& description,
                                     const vector<string>& paths,
                                     const string& final_fallback)
{
    string result;

    if (!filename.empty() && !is_valid_filename(filename))
    {
        // assume filename is just a basename instead of a full file path
        LOG_TRACE << description << " '" << filename << "' not found yet, "
                  << "searching in paths " << paths;

        for (auto dir : paths)
        {
            if (!dir.empty())
            {
                string fn = fs::path(dir) / filename;
                if (fs::is_regular_file(fn))
                {
                    result = fn;
                    break;
                }
            }
        }
    }

    if (result.empty())
        result = final_fallback;

    if (!fs::is_regular_file(result))
    {
        LOG_ERROR << "failed to find " << description << " '" << filename << "'";
        result.clear();
    }
    else
    {
        LOG_TRACE << description << " '" << result << "' found.";
    }

    return result;
}



ConfigTypingAssistance::ConfigTypingAssistance(ConfigObject* parent) :
    ConfigObject(parent, SCHEMA_TYPING_ASSISTANCE, "typing-assistance"),
    word_suggestions(std::make_unique<ConfigWordSuggestions>(this))
{
    m_children.emplace_back(this->word_suggestions.get());
}



constexpr const std::array<const char*, 5> ConfigWordSuggestions::KEY_ORDER;

ConfigWordSuggestions::ConfigWordSuggestions(ConfigObject* parent) :
    ConfigObject(parent, SCHEMA_WORD_SUGGESTIONS, "word-suggestions")
{
}

string ConfigWordSuggestions::get_can_auto_learn_debug_string()
{
    return sstr()
            << "enabled=" << this->enabled.get()
            << " auto_learn=" << this->auto_learn.get()
            << " is_learning_paused=" << is_learning_paused()
            << " learning_behavior_paused=" << this->learning_behavior_paused.get()
            << " pause_learning_locked=" << this->pause_learning_locked.get()
            << " m_pause_learning=" << m_pause_learning
            << " stealth_mode=" << this->stealth_mode.get();
}

bool ConfigWordSuggestions::can_auto_learn()
{
    return (this->enabled &&
            this->auto_learn &&
            (!is_learning_paused() ||
             this->learning_behavior_paused != LearningBehavior::NOTHING) &&
            !this->stealth_mode);
}

bool ConfigWordSuggestions::is_learning_paused()
{
    return get_pause_learning() != PauseLearning::OFF;
}

PauseLearning::Enum ConfigWordSuggestions::get_pause_learning()
{
    if (this->pause_learning_locked)
        return PauseLearning::LOCKED;
    else
        return m_pause_learning;
}

void ConfigWordSuggestions::set_pause_learning(PauseLearning::Enum value)
{
    m_pause_learning = value;
    this->pause_learning_locked = value == PauseLearning::LOCKED;
}

vector<string> ConfigWordSuggestions::get_shown_wordlist_button_ids()
{
    vector<string> result;
    for (auto& button_id : this->wordlist_buttons.get())
    {
        if (button_id != KEY_ID_PAUSE_LEARNING ||
            can_show_pause_learning_button())
        {
            result.emplace_back(button_id);
        }
    }
    return result;
}

void ConfigWordSuggestions::show_wordlist_button(const string& key_id,
                                                 bool show)
{
    vector<string> buttons = this->wordlist_buttons;  // make a copy
    if (show)
    {
        if (!contains(buttons, key_id))
        {
            auto priority = get_button_priority(key_id);
            if (priority >= 0)
            {
                bool inserted = false;
                for (size_t i=0; i<buttons.size(); i++)
                {
                    auto button = buttons[i];
                    auto p = get_button_priority(button);
                    if (priority < p)
                    {
                        buttons.emplace(
                                    std::next(buttons.begin(),
                                              static_cast<long>(i)), key_id);
                        inserted = true;
                        break;
                    }
                }
                if (!inserted)
                    buttons.emplace_back(key_id);
            }

            this->wordlist_buttons = buttons;
        }
    }
    else
    {
        if (contains(buttons, key_id))
        {
            remove(buttons, key_id);
            this->wordlist_buttons = buttons;
        }
    }
}

bool ConfigWordSuggestions::can_show_language_button()
{
    return contains(this->wordlist_buttons.get(), string(KEY_ID_LANGUAGE));
}

bool ConfigWordSuggestions::can_show_pause_learning_button()
{
    return this->auto_learn &&
           contains(this->wordlist_buttons.get(), string(KEY_ID_PAUSE_LEARNING));
}

bool ConfigWordSuggestions::can_show_more_predictions_button()
{
    return contains(this->wordlist_buttons.get(), string(KEY_ID_NEXT_PREDICTIONS));
}

bool ConfigWordSuggestions::can_learn_new_words()
{
    return !is_learning_paused();
}

int ConfigWordSuggestions::get_button_priority(const string& key_id)
{
    return static_cast<int>(find_index(KEY_ORDER, key_id));
}



ConfigUniversalAccess::ConfigUniversalAccess(ConfigObject* parent) :
    ConfigObject(parent, SCHEMA_UNIVERSAL_ACCESS, "universal-access")
{}

