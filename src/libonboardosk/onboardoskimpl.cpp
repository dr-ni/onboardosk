#include "tools/logger.h"
#include "tools/xdgdirs.h"

#include "atspistatetracker.h"
#include "audioplayer.h"
#include "colorscheme.h"
#include "configuration.h"
#include "drawingcontext.h"
#include "exception.h"
#include "globalkeylistener.h"
#include "hardwaresensortracker.h"
#include "inputeventreceiver.h"
#include "inputeventsource.h"
#include "keyboard.h"
#include "keyboardview.h"
#include "languagedb.h"
#include "layoutroot.h"
#include "layoutloadersvg.h"
#include "layoutviewkeyboard.h"
#include "mousetweaks.h"
#include "onboardoskimpl.h"
#include "textrendererpangocairo.h"
#include "theme.h"
#include "timer.h"
#include "translation.h"
#include "udevtracker.h"
#include "virtkey.h"
#include "wordsuggestions.h"

using std::endl;
using std::string;
using std::vector;

OnboardOskImpl::OnboardOskImpl() :
    Super(nullptr)
{
    m_globals_ptr = std::make_unique<OnboardOskGlobals>();
    ContextBase::m_globals = m_globals_ptr.get();
    m_call_reload_layout = std::make_unique<CallOnce<>>(*this,
                             [this](){reload_layout();});
}

OnboardOskImpl::~OnboardOskImpl()
{
    notify_toplevels_remove();
}

bool OnboardOskImpl::startup(const std::vector<std::string>& args)
{
    m_connections.disconnect_all();

    auto log_level = LogLevel::WARNING;
    //auto log_level = LogLevel::TRACE;
    //auto log_level = LogLevel::DEBUG;
    //auto log_level = LogLevel::ATSPI;
    if (!init_essentials(args, log_level))
        return false;

    // load the initial layout
    LOG_INFO << "loading initial layout";
    m_call_reload_layout->enqueue();

    auto keyboard_view = get_keyboard_view();
    if (!keyboard_view)
        return false;

    keyboard_view->set_startup_visibility();

    return true;
}

bool OnboardOskImpl::init_essentials(const std::vector<std::string>& args,
                                     LogLevel log_level, bool test_mode)
{
    // init translations
    init_translation_for_library();

    m_globals->m_instance = this;

    // setup global toolkit-dependent callbacks
    m_globals->m_callbacks = &toolkit_callbacks;

    // setup logger
    const auto& _logger = Logger::get_default();
    _logger->set_level(log_level);
    m_globals->m_logger = _logger;

    LOG_DEBUG << "initializing";

    // create xdgdirs "singleton" (needs the logger)
    m_globals->m_xdgdirs = std::make_unique<XDGDirs>(
                               m_globals->m_logger.get());

    // create config
    m_globals->m_config = Config::make(*this);
    if (!config()->parse_command_line(args))
        return false;
    config()->init();

    if (!test_mode)
    {
        // create virtkey
        try {
            m_globals->m_virtkey = VirtKey::make_vk_wayland(*this);
            LOG_DEBUG << "VirtKeyWayland selected" << endl;
        }
        catch (const VirtKeyException& ex) {
            LOG_DEBUG << "VirtKeyWayland: " << ex.what();
        }
        if (!m_globals->m_virtkey)
        {
            try {
                m_globals->m_virtkey = VirtKey::make_vk_x11(*this);
                LOG_DEBUG << "VirtKeyX11 selected";
            }
            catch (const VirtKeyException& ex) {
                LOG_DEBUG << "VirtKeyX11: " << ex.what();
            }
        }
        if (m_globals->m_virtkey)
        {
            // In Wayland the first try to load the layout may fail due to
            // missing keymap. Once we get this event, we'll try again.
            m_connections.connect(m_globals->m_virtkey->keymap_changed,
                                  [&]{on_keymap_changed();});
            m_connections.connect(m_globals->m_virtkey->group_changed,
                                  [&]{on_group_changed();});
        }
        else
            LOG_WARNING << "failed to create virtkey: key-stroke generation unavailable";

        // create audio player
        try {
            m_globals->m_audio_player = AudioPlayer::make_ap_canberra(*this);
        }
        catch (const AudioException& ex) {
            LOG_WARNING << "AudioPlayer: " << ex.what();
        }

        // create AT-SPI state tracker
        try {
            m_globals->m_atspi_state_tracker = AtspiStateTracker::make(*this);
        }
        catch (const Exception& ex) {
            LOG_WARNING << "AtspiStateTracker: " << ex.what();
        }

        // create hardware sensor (tablet mode) tracker
        try {
            m_globals->m_hardware_sensor_tracker= HardwareSensorTracker::make(*this);
        }
        catch (const Exception& ex) {
            LOG_WARNING << "HardwareSensorTracker: " << ex.what();
        }

        // create UDev tracker for physical keyboard device detection
        try {
            m_globals->m_udev_tracker= UDevTracker::make(*this);
        }
        catch (const Exception& ex) {
            LOG_WARNING << "UDevTracker: " << ex.what();
        }

        // create listener for global physical key presses
        try {
            m_globals->m_global_key_listener= GlobalKeyListener::make(*this);
        }
        catch (const Exception& ex) {
            LOG_WARNING << "GlobalKeyListener: " << ex.what();
        }

        // create input event source
        update_input_event_source();
        m_connections.connect(config()->keyboard->input_event_source.changed,
                              [&]{update_input_event_source();});
    }

    // create language db with isocodes language information
    try {
        m_globals->m_language_db = LanguageDB::make(*this);
    }
    catch (const Exception& ex) {
        LOG_WARNING << "LanguageDB: " << ex.what();
    }

    // create the central keyboard instance
    m_globals->m_keyboard = Keyboard::make(*this);

    // and the central keyboard view
    m_globals->m_keyboard_view = KeyboardView::make(*this, "KeyboardView");

    return true;
}

// called on gsettings change
void OnboardOskImpl::update_input_event_source()
{
    std::vector<InputEventSourceEnum::Enum> candidates;

    switch(config()->keyboard->input_event_source)
    {
        case InputEventSourceEnum::GTK:
            candidates = {InputEventSourceEnum::GTK};
            break;

        case InputEventSourceEnum::XINPUT:
            candidates = {InputEventSourceEnum::XINPUT, InputEventSourceEnum::GTK};
            break;
    }

    m_globals->m_input_event_source.reset();

    for (auto candidate : candidates)
    {
        if (candidate == InputEventSourceEnum::GTK)
        {
            try {
                m_globals->m_input_event_source = InputEventSource::make_native(*this, {});
                LOG_DEBUG << "InputEventSourceNati selected";
                break;
            }
            catch (const Exception& ex) {
                LOG_WARNING << "InputEventSourceNative: " << ex.what();
            }
            break;
        }
        else if (candidate == InputEventSourceEnum::XINPUT)
        {
            try {
                m_globals->m_input_event_source = InputEventSource::make_xinput(*this, {});
                LOG_DEBUG << "InputEventSourceXInput selected";
                break;
            }
            catch (const Exception& ex) {
                LOG_WARNING << "InputEventSourceXInput: " << ex.what();
            }
            break;
        }
    }
}

void OnboardOskImpl::toggle_visible()
{
    get_keyboard_view()->request_visibility_toggle();
}

void OnboardOskImpl::show()
{
    get_keyboard_view()->request_visibility(true);
}

void OnboardOskImpl::hide()
{
    get_keyboard_view()->request_visibility(false);
}

bool OnboardOskImpl::is_visible()
{
    return get_keyboard_view()->is_visible();
}

void OnboardOskImpl::draw(ViewBase* view, DrawingContext& dc)
{
    if (is_toplevel_view(view))
        view->draw(dc);
    else
        LOG_WARNING << "0x" << std::hex << view << " is not a toplevel view"
                    << ", ignoring draw request";
}


void OnboardOskImpl::on_event(ViewBase* view, OnboardOskEvent* event)
{
    auto event_source = get_input_event_source();
    if (event_source && event_source->is_native())
        event_source->queue_event(view, event);
}

std::string OnboardOskImpl::get_language_full_name(const std::string& lang_id)
{
    auto language_db = get_language_db();
    if (language_db)
        return language_db->get_language_full_name(lang_id);
    return {};
}

void OnboardOskImpl::set_active_language_id(const std::string& lang_id,
                                            bool add_to_mru)
{
    auto language_db = get_language_db();
    auto ws = get_word_suggestions();
    if (language_db)
    {
        auto lang_id_ = lang_id;
        if (lang_id == language_db->get_system_default_lang_id())
            lang_id_ = "";

        ws->set_active_language_id(lang_id, add_to_mru);
    }
}

void OnboardOskImpl::on_language_selection_closed()
{
    auto ws = get_word_suggestions();
    if (ws)
        ws->on_language_selection_closed();

    // distinct time point: commit updates at the very end
    get_keyboard()->commit_ui_updates();
}

void OnboardOskImpl::on_keymap_changed()
{
    LOG_DEBUG << "keymap changed";
    m_call_reload_layout->enqueue();
}

void OnboardOskImpl::on_group_changed()
{
    auto vk = get_virtkey();
    if (vk)
        LOG_DEBUG << "keymap group changed to " << vk->get_current_group_name()
                  << " (" << vk->get_current_group() << ")";
    else
        LOG_DEBUG << "keymap group changed";

    m_call_reload_layout->enqueue();
}

std::vector<ViewBase*>& OnboardOskImpl::get_toplevel_views()
{
    return Super::get_toplevel_views();
}

void OnboardOskImpl::on_toplevel_view_geometry_changed(ViewBase* view)
{
    if (is_toplevel_view(view))
        view->on_toplevel_geometry_changed();
    else
        LOG_WARNING << "0x" << std::hex << view << " is not a toplevel view"
                    << ", ignoring";
}

// Check if the system keyboard layout has changed and
// (re)load Onboards layout accordingly.
void OnboardOskImpl::reload_layout(bool force_load)
{
    KeyboardState keyboard_state;

    auto vk = get_virtkey();
    if (vk)
    {
        try
        {
            vk->reload(); // reload keyboard names
            keyboard_state = {vk->get_layout_as_string(),
                              vk->get_current_group_name()};
        }
        catch (const VirtKeyException& ex) {
            LOG_WARNING << "Keyboard layout changed, but retrieving "
                        << "keyboard information failed: " << ex.what();
        }
    }

    if ((m_last_keyboard_state != keyboard_state) || force_load)
    {
        m_last_keyboard_state = keyboard_state;

        string layout_filename = config()->get_layout_filename();

        try
        {
            load_layout(layout_filename);
        }
        catch (const Exception& ex)
        {
            LOG_ERROR << "Layout error: " << ex.what()
                      << ". Falling back to default layout.";

            // last ditch effort to load the default layout
            load_layout(config()->get_fallback_layout_filename());
        }
    }
}

bool OnboardOskImpl::load_layout(const std::string& layout_filename)
{
    LOG_INFO << "loading keyboard layout " << repr(layout_filename);

    auto keyboard_view = get_keyboard_view();

    ThemePtr theme;
    string theme_filename = config()->get_theme_filename();
    if (!theme_filename.empty())
    {
        LOG_INFO << "loading theme " << repr(theme_filename);
        ThemeLoader loader(*this);
        theme = loader.load(theme_filename);
    }
    config()->set_current_theme(theme);

    ColorSchemePtr color_scheme;
    string color_scheme_filename = config()->get_color_scheme_filename();
    if (!color_scheme_filename.empty())
    {
        LOG_INFO << "loading color scheme " << repr(color_scheme_filename);
        ColorSchemeLoader loader(*this);
        color_scheme = loader.load(color_scheme_filename);
    }
    config()->set_current_color_scheme(color_scheme);

    LayoutViews& views = get_keyboard_layout_views();

    // remove old keyboard layout views
    notify_toplevels_remove();
    remove_any(get_toplevel_views(), [](const ViewBase* v)
        {return v->get_view_type() == ONBOARD_OSK_VIEW_TYPE_KEYBOARD;});
    views.clear();
    keyboard_view->get_children().clear();

    // load layout
    auto loader = LayoutLoaderSVG::make(*this);
    m_layout = loader->load(layout_filename, color_scheme);
    get_keyboard()->set_layout(m_layout);
    if (m_layout)
    {
        // create as many layout views as the layout needs
        vector<LayoutItemPtr> view_roots = m_layout->get_view_subtrees();
        for (size_t i=0; i<view_roots.size(); i++)
        {
            auto& item = view_roots[i];
            auto view = LayoutViewKeyboard::make(
                *this,
                "LayoutViewKeyboard" + std::to_string(i));

            view->set_layout_subtree(item);
            views.emplace_back(std::move(view));
        }

        update_view_graph();
        add_toplevel_keyboard_views();
    }

    // let toolkit-dependent code create actors
    notify_toplevels_added();

    // update view rects
    keyboard_view->restore_view_rect();

    // update ui
    auto keyboard = get_keyboard();
    keyboard->invalidate_ui();
    keyboard->commit_ui_updates();

    return m_layout != nullptr;
}

void OnboardOskImpl::update_view_graph()
{
    auto keyboard_view = get_keyboard_view();
    auto& layout_views = get_keyboard_layout_views();

    // The graph part: update parent pointers. Toplevels
    // have no parents.
    if (config()->can_have_multiple_toplevels())
    {
        for (auto& view : layout_views)
            view->set_parent(nullptr);
    }
    else
    {
        auto& children = keyboard_view->get_children();
        for (auto& view : layout_views)
        {
            view->set_parent(keyboard_view);
            children.emplace_back(view);
        }
    }

    bool b = config()->can_have_multiple_toplevels();
    for (auto& view : layout_views)
        view->set_parent(b ? nullptr : keyboard_view);
}

// find the views that should have toplevel windows/actors
void OnboardOskImpl::add_toplevel_keyboard_views()
{
    // are there toplevel LayoutViewKeyboards (split keyboard when docked)?
    bool has_toplevel_layout_views = false;
    for (auto& view : get_keyboard_layout_views())
        if (!view->get_parent())
        {
            has_toplevel_layout_views = true;
            add_toplevel_view(view.get());
        }

    // else KeyboardView must be the toplevel (single keyboard or not docked)!
    if (!has_toplevel_layout_views)
        add_toplevel_view(get_keyboard_view());
}

// notify toolkit-dependent code of the newly added views
void OnboardOskImpl::notify_toplevels_added()
{
    for (auto& view : ContextBase::get_toplevel_views())
        view->notify_toplevel_added();
}

// notify toolkit-dependent code of imminent view removal
void OnboardOskImpl::notify_toplevels_remove()
{
    for (auto& view : ContextBase::get_toplevel_views())
        view->notify_toplevel_remove();
}

