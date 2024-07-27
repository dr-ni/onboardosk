#include <array>
#include <map>
#include <memory>
#include <string>

#include <atspi/atspi.h>

#include "tools/container_helpers.h"
#include "tools/glib_helpers.h"
#include "tools/logger.h"
#include "tools/noneable.h"
#include "tools/process_helpers.h"
#include "tools/rect.h"
#include "tools/string_helpers.h"

#include "atspistatetracker.h"
#include "configuration.h"
#include "exception.h"
#include "textchanges.h"
#include "timer.h"
#include "uielement.h"

using std::string;
using std::vector;


#define CACHED_GET(state_value, func_name) \
cached_get(state_value, \
           [](AtspiAccessible* accessible, GError** error) \
               {return func_name(accessible, error);}, \
           "\"" #func_name "\"")  \

typedef std::shared_ptr<AtspiAccessible> AtspiAccessiblePtr;

typedef std::unique_ptr<AtspiComponent, decltype(&g_object_unref)> AtspiIFaceComponentPtr;
typedef std::unique_ptr<AtspiEditableText, decltype(&g_object_unref)> AtspiIFaceEditableTextPtr;
typedef std::unique_ptr<AtspiText, decltype(&g_object_unref)> AtspiIFaceTextPtr;

typedef std::unique_ptr<AtspiRange, decltype(&g_free)> AtspiRangePtr;
typedef std::unique_ptr<AtspiRect, decltype(&g_free)> AtspiRectPtr;
typedef std::unique_ptr<AtspiStateSet, decltype(&g_object_unref)> AtspiStateSetPtr;
typedef std::unique_ptr<AtspiTextRange, decltype(&g_free)> AtspiTextRangePtr;


std::string to_string(AtspiRole role)
{
    static std::array<std::pair<AtspiRole, const char*>, 121> a
    {{
        {ATSPI_ROLE_INVALID, "INVALID"},
        {ATSPI_ROLE_ACCELERATOR_LABEL, "ACCELERATOR_LABEL"},
        {ATSPI_ROLE_ALERT, "ALERT"},
        {ATSPI_ROLE_ANIMATION, "ANIMATION"},
        {ATSPI_ROLE_ARROW, "ARROW"},
        {ATSPI_ROLE_CALENDAR, "CALENDAR"},
        {ATSPI_ROLE_CANVAS, "CANVAS"},
        {ATSPI_ROLE_CHECK_BOX, "CHECK_BOX"},
        {ATSPI_ROLE_CHECK_MENU_ITEM, "CHECK_MENU_ITEM"},
        {ATSPI_ROLE_COLOR_CHOOSER, "COLOR_CHOOSER"},
        {ATSPI_ROLE_COLUMN_HEADER, "COLUMN_HEADER"},
        {ATSPI_ROLE_COMBO_BOX, "COMBO_BOX"},
        {ATSPI_ROLE_DATE_EDITOR, "DATE_EDITOR"},
        {ATSPI_ROLE_DESKTOP_ICON, "DESKTOP_ICON"},
        {ATSPI_ROLE_DESKTOP_FRAME, "DESKTOP_FRAME"},
        {ATSPI_ROLE_DIAL, "DIAL"},
        {ATSPI_ROLE_DIALOG, "DIALOG"},
        {ATSPI_ROLE_DIRECTORY_PANE, "DIRECTORY_PANE"},
        {ATSPI_ROLE_DRAWING_AREA, "DRAWING_AREA"},
        {ATSPI_ROLE_FILE_CHOOSER, "FILE_CHOOSER"},
        {ATSPI_ROLE_FILLER, "FILLER"},
        {ATSPI_ROLE_FOCUS_TRAVERSABLE, "FOCUS_TRAVERSABLE"},
        {ATSPI_ROLE_FONT_CHOOSER, "FONT_CHOOSER"},
        {ATSPI_ROLE_FRAME, "FRAME"},
        {ATSPI_ROLE_GLASS_PANE, "GLASS_PANE"},
        {ATSPI_ROLE_HTML_CONTAINER, "HTML_CONTAINER"},
        {ATSPI_ROLE_ICON, "ICON"},
        {ATSPI_ROLE_IMAGE, "IMAGE"},
        {ATSPI_ROLE_INTERNAL_FRAME, "INTERNAL_FRAME"},
        {ATSPI_ROLE_LABEL, "LABEL"},
        {ATSPI_ROLE_LAYERED_PANE, "LAYERED_PANE"},
        {ATSPI_ROLE_LIST, "LIST"},
        {ATSPI_ROLE_LIST_ITEM, "LIST_ITEM"},
        {ATSPI_ROLE_MENU, "MENU"},
        {ATSPI_ROLE_MENU_BAR, "MENU_BAR"},
        {ATSPI_ROLE_MENU_ITEM, "MENU_ITEM"},
        {ATSPI_ROLE_OPTION_PANE, "OPTION_PANE"},
        {ATSPI_ROLE_PAGE_TAB, "PAGE_TAB"},
        {ATSPI_ROLE_PAGE_TAB_LIST, "PAGE_TAB_LIST"},
        {ATSPI_ROLE_PANEL, "PANEL"},
        {ATSPI_ROLE_PASSWORD_TEXT, "PASSWORD_TEXT"},
        {ATSPI_ROLE_POPUP_MENU, "POPUP_MENU"},
        {ATSPI_ROLE_PROGRESS_BAR, "PROGRESS_BAR"},
        {ATSPI_ROLE_PUSH_BUTTON, "PUSH_BUTTON"},
        {ATSPI_ROLE_RADIO_BUTTON, "RADIO_BUTTON"},
        {ATSPI_ROLE_RADIO_MENU_ITEM, "RADIO_MENU_ITEM"},
        {ATSPI_ROLE_ROOT_PANE, "ROOT_PANE"},
        {ATSPI_ROLE_ROW_HEADER, "ROW_HEADER"},
        {ATSPI_ROLE_SCROLL_BAR, "SCROLL_BAR"},
        {ATSPI_ROLE_SCROLL_PANE, "SCROLL_PANE"},
        {ATSPI_ROLE_SEPARATOR, "SEPARATOR"},
        {ATSPI_ROLE_SLIDER, "SLIDER"},
        {ATSPI_ROLE_SPIN_BUTTON, "SPIN_BUTTON"},
        {ATSPI_ROLE_SPLIT_PANE, "SPLIT_PANE"},
        {ATSPI_ROLE_STATUS_BAR, "STATUS_BAR"},
        {ATSPI_ROLE_TABLE, "TABLE"},
        {ATSPI_ROLE_TABLE_CELL, "TABLE_CELL"},
        {ATSPI_ROLE_TABLE_COLUMN_HEADER, "TABLE_COLUMN_HEADER"},
        {ATSPI_ROLE_TABLE_ROW_HEADER, "TABLE_ROW_HEADER"},
        {ATSPI_ROLE_TEAROFF_MENU_ITEM, "TEAROFF_MENU_ITEM"},
        {ATSPI_ROLE_TERMINAL, "TERMINAL"},
        {ATSPI_ROLE_TEXT, "TEXT"},
        {ATSPI_ROLE_TOGGLE_BUTTON, "TOGGLE_BUTTON"},
        {ATSPI_ROLE_TOOL_BAR, "TOOL_BAR"},
        {ATSPI_ROLE_TOOL_TIP, "TOOL_TIP"},
        {ATSPI_ROLE_TREE, "TREE"},
        {ATSPI_ROLE_TREE_TABLE, "TREE_TABLE"},
        {ATSPI_ROLE_UNKNOWN, "UNKNOWN"},
        {ATSPI_ROLE_VIEWPORT, "VIEWPORT"},
        {ATSPI_ROLE_WINDOW, "WINDOW"},
        {ATSPI_ROLE_EXTENDED, "EXTENDED"},
        {ATSPI_ROLE_HEADER, "HEADER"},
        {ATSPI_ROLE_FOOTER, "FOOTER"},
        {ATSPI_ROLE_PARAGRAPH, "PARAGRAPH"},
        {ATSPI_ROLE_RULER, "RULER"},
        {ATSPI_ROLE_APPLICATION, "APPLICATION"},
        {ATSPI_ROLE_AUTOCOMPLETE, "AUTOCOMPLETE"},
        {ATSPI_ROLE_EDITBAR, "EDITBAR"},
        {ATSPI_ROLE_EMBEDDED, "EMBEDDED"},
        {ATSPI_ROLE_ENTRY, "ENTRY"},
        {ATSPI_ROLE_CHART, "CHART"},
        {ATSPI_ROLE_CAPTION, "CAPTION"},
        {ATSPI_ROLE_DOCUMENT_FRAME, "DOCUMENT_FRAME"},
        {ATSPI_ROLE_HEADING, "HEADING"},
        {ATSPI_ROLE_PAGE, "PAGE"},
        {ATSPI_ROLE_SECTION, "SECTION"},
        {ATSPI_ROLE_REDUNDANT_OBJECT, "REDUNDANT_OBJECT"},
        {ATSPI_ROLE_FORM, "FORM"},
        {ATSPI_ROLE_LINK, "LINK"},
        {ATSPI_ROLE_INPUT_METHOD_WINDOW, "INPUT_METHOD_WINDOW"},
        {ATSPI_ROLE_TABLE_ROW, "TABLE_ROW"},
        {ATSPI_ROLE_TREE_ITEM, "TREE_ITEM"},
        {ATSPI_ROLE_DOCUMENT_SPREADSHEET, "DOCUMENT_SPREADSHEET"},
        {ATSPI_ROLE_DOCUMENT_PRESENTATION, "DOCUMENT_PRESENTATION"},
        {ATSPI_ROLE_DOCUMENT_TEXT, "DOCUMENT_TEXT"},
        {ATSPI_ROLE_DOCUMENT_WEB, "DOCUMENT_WEB"},
        {ATSPI_ROLE_DOCUMENT_EMAIL, "DOCUMENT_EMAIL"},
        {ATSPI_ROLE_COMMENT, "COMMENT"},
        {ATSPI_ROLE_LIST_BOX, "LIST_BOX"},
        {ATSPI_ROLE_GROUPING, "GROUPING"},
        {ATSPI_ROLE_IMAGE_MAP, "IMAGE_MAP"},
        {ATSPI_ROLE_NOTIFICATION, "NOTIFICATION"},
        {ATSPI_ROLE_INFO_BAR, "INFO_BAR"},
        {ATSPI_ROLE_LEVEL_BAR, "LEVEL_BAR"},
        {ATSPI_ROLE_TITLE_BAR, "TITLE_BAR"},
        {ATSPI_ROLE_BLOCK_QUOTE, "BLOCK_QUOTE"},
        {ATSPI_ROLE_AUDIO, "AUDIO"},
        {ATSPI_ROLE_VIDEO, "VIDEO"},
        {ATSPI_ROLE_DEFINITION, "DEFINITION"},
        {ATSPI_ROLE_ARTICLE, "ARTICLE"},
        {ATSPI_ROLE_LANDMARK, "LANDMARK"},
        {ATSPI_ROLE_LOG, "LOG"},
        {ATSPI_ROLE_MARQUEE, "MARQUEE"},
        {ATSPI_ROLE_MATH, "MATH"},
        {ATSPI_ROLE_RATING, "RATING"},
        {ATSPI_ROLE_TIMER, "TIMER"},
        {ATSPI_ROLE_STATIC, "STATIC"},
        {ATSPI_ROLE_MATH_FRACTION, "MATH_FRACTION"},
        {ATSPI_ROLE_MATH_ROOT, "MATH_ROOT"},
        {ATSPI_ROLE_SUBSCRIPT, "SUBSCRIPT"},
        {ATSPI_ROLE_SUPERSCRIPT, "SUPERSCRIPT"},
    }};

    for (auto& e : a)
        if (e.first == role)
            return e.second;
    return {};
}

std::string to_string(AtspiStateType type)
{
    static std::array<std::pair<AtspiStateType, const char*>, 44> a
    {{
        {ATSPI_STATE_INVALID, "INVALID"},
        {ATSPI_STATE_ACTIVE, "ACTIVE"},
        {ATSPI_STATE_ARMED, "ARMED"},
        {ATSPI_STATE_BUSY, "BUSY"},
        {ATSPI_STATE_CHECKED, "CHECKED"},
        {ATSPI_STATE_COLLAPSED, "COLLAPSED"},
        {ATSPI_STATE_DEFUNCT, "DEFUNCT"},
        {ATSPI_STATE_EDITABLE, "EDITABLE"},
        {ATSPI_STATE_ENABLED, "ENABLED"},
        {ATSPI_STATE_EXPANDABLE, "EXPANDABLE"},
        {ATSPI_STATE_EXPANDED, "EXPANDED"},
        {ATSPI_STATE_FOCUSABLE, "FOCUSABLE"},
        {ATSPI_STATE_FOCUSED, "FOCUSED"},
        {ATSPI_STATE_HAS_TOOLTIP, "HAS_TOOLTIP"},
        {ATSPI_STATE_HORIZONTAL, "HORIZONTAL"},
        {ATSPI_STATE_ICONIFIED, "ICONIFIED"},
        {ATSPI_STATE_MODAL, "MODA"},
        {ATSPI_STATE_MULTI_LINE, "MULTI_LINE"},
        {ATSPI_STATE_MULTISELECTABLE, "MULTISELECTABLE"},
        {ATSPI_STATE_OPAQUE, "OPAQUE"},
        {ATSPI_STATE_PRESSED, "PRESSED"},
        {ATSPI_STATE_RESIZABLE, "RESIZABLE"},
        {ATSPI_STATE_SELECTABLE, "SELECTABLE"},
        {ATSPI_STATE_SELECTED, "SELECTED"},
        {ATSPI_STATE_SENSITIVE, "SENSITIVE"},
        {ATSPI_STATE_SHOWING, "SHOWING"},
        {ATSPI_STATE_SINGLE_LINE, "SINGLE_LINE"},
        {ATSPI_STATE_STALE, "STALE"},
        {ATSPI_STATE_TRANSIENT, "TRANSIENT"},
        {ATSPI_STATE_VERTICAL, "VERTICAL"},
        {ATSPI_STATE_VISIBLE, "VISIBLE"},
        {ATSPI_STATE_MANAGES_DESCENDANTS, "MANAGES_DESCENDANTS"},
        {ATSPI_STATE_INDETERMINATE, "INDETERMINATE"},
        {ATSPI_STATE_REQUIRED, "REQUIRED"},
        {ATSPI_STATE_TRUNCATED, "TRUNCATED"},
        {ATSPI_STATE_ANIMATED, "ANIMATED"},
        {ATSPI_STATE_INVALID_ENTRY, "INVALID_ENTRY"},
        {ATSPI_STATE_SUPPORTS_AUTOCOMPLETION, "SUPPORTS_AUTOCOMPLETION"},
        {ATSPI_STATE_SELECTABLE_TEXT, "SELECTABLE_TEXT"},
        {ATSPI_STATE_IS_DEFAULT, "IS_DEFAULT"},
        {ATSPI_STATE_VISITED, "VISITED"},
        {ATSPI_STATE_CHECKABLE, "CHECKABLE"},
        {ATSPI_STATE_HAS_POPUP, "HAS_POPUP"},
        {ATSPI_STATE_READ_ONLY, "READ_ONLY"},
    }};

    for (auto& e : a)
        if (e.first == type)
            return e.second;
    return {};
}

template<typename T>
class GWeakRefPtr
{
    public:
        GWeakRefPtr<T>()
        {
            g_weak_ref_init (&m_weak_ref, nullptr);
        }
        GWeakRefPtr<T>(T* object)
        {
            g_weak_ref_init (&m_weak_ref, object);
        }
        ~GWeakRefPtr<T>()
        {
            g_weak_ref_clear(&m_weak_ref);
        }

        GWeakRefPtr<T>& operator=(T* object)
        {
            g_weak_ref_set (&m_weak_ref, object);
            return *this;
        }

        // return a temporary strong reference or nullptr
        std::shared_ptr<T> get() const
        {
            auto object = g_weak_ref_get(&m_weak_ref);
            if (!G_IS_OBJECT(object))
                object = nullptr;

            return {reinterpret_cast<T*>(object),
                    g_object_unref};
        }

        mutable GWeakRef m_weak_ref;
};


class CachedAccessible : public UIElement,  public std::enable_shared_from_this<CachedAccessible>
{
    public:
        using Ptr = std::shared_ptr<CachedAccessible>;
        using StringMap = std::map<std::string, std::string>;
        using StringVector = std::vector<std::string>;

        //using StateEntry = Noneable<std::variant<AtspiRole, Rect>>;
        //using State = std::map<std::string, StateEntry>;
        struct State
        {
            Noneable<AtspiRole> role;
            Noneable<std::string> role_name;
            Noneable<std::string> name;
            AtspiStateSetPtr state_set{nullptr, g_object_unref};
            StringVector states;
            Noneable<int> id;
            StringMap attributes;
            StringVector interfaces;
            Noneable<std::string> description;
            Noneable<pid_t> pid;
            Noneable<std::string> process_name;
            Noneable<std::string> toolkit_name;
            Noneable<std::string> toolkit_version;
            AtspiEditableText* editable_text_iface;
            Ptr application;
            Noneable<Rect> extents;
            Ptr frame;
        };

    private:
        //AtspiAccessiblePtr m_accessible;
        GWeakRefPtr<AtspiAccessible> m_accessible;
        mutable State m_state;       // cache of various accessible properties

    public:
        using Super = UIElement;
        CachedAccessible(const ContextBase& context, AtspiAccessible* accessible) :
            Super(context)
        {
            //g_object_ref(accessible);
            //m_accessible = {accessible, g_object_unref};
            m_accessible = accessible;   // set weak reference, no need for g_object_ref here
        }

        CachedAccessible(const ContextBase& context, AtspiAccessiblePtr accessible) :
            Super(context),
            m_accessible(accessible.get())
        {
        }
        virtual ~CachedAccessible();

        Ptr getptr() {
            return std::enable_shared_from_this<CachedAccessible>::shared_from_this();
        }
        virtual std::ostream& dump(std::ostream& s,
                          const std::string& prefix,
                          const std::string& suffix) const override
        {
            s << prefix << "role=" << get_role() << suffix
              << prefix << "role_name=" << get_role_name() << suffix
              << prefix << "get_states=" << get_states() << suffix
              << prefix << "get_id=" << get_id() << suffix
              << prefix << "get_attributes=" << get_attributes() << suffix
              << prefix << "get_interfaces=" << get_interfaces() << suffix
              << prefix << "get_description=" << get_description() << suffix
              << prefix << "get_pid=" << get_pid() << suffix
              << prefix << "get_process_name=" << get_process_name() << suffix
              << prefix << "get_toolkit_name=" << get_toolkit_name() << suffix
              << prefix << "get_toolkit_version=" << get_toolkit_version() << suffix
              << prefix << "get_extents=" << get_extents() << suffix
              << prefix << "get_app_name=" << get_app_name() << suffix
              << prefix << "get_app_description=" << get_app_description() << suffix
              << prefix << "get_frame=" << get_frame() << suffix
              << prefix << "get_frame_extents=" << get_frame_extents() << suffix
              << prefix << "is_byobu=" << is_byobu() << suffix
              << prefix << "is_password_entry=" << is_password_entry() << suffix
              << prefix << "is_single_line=" << is_single_line() << suffix
              << prefix << "is_terminal=" << is_terminal() << suffix
              << prefix << "is_text_entry=" << is_text_entry() << suffix
              << prefix << "is_toolkit_gtk3=" << is_toolkit_gtk3() << suffix
              << prefix << "is_urlbar=" << is_urlbar() << suffix;
            return s;
        }

        bool operator!=(const CachedAccessible& other)
        {
            return !operator==(other);
        }
        bool operator==(const CachedAccessible& other)
        {
            return m_accessible.get().get() == other.m_accessible.get().get();
        }

    private:
        virtual bool is_same(const UIElementPtr& other) const override
        {
            if (other)
            {
                auto o = dynamic_cast<CachedAccessible*>(other.get());
                return m_accessible.get().get() == o->m_accessible.get().get();
            }
            return false;
        }

    public:
        // All cached state of the accessible
        State& get_state()
        {
            return m_state;
        }

        bool check_ok(GError*& error, const char* func_name) const
        {
            if (error)
            {
                LogStream(logger(), LogLevel::ATSPI, LOG_SRC_LOCATION);
                LOG_ATSPI << func_name
                          << ": " << error->domain << ": " << error->message
                          << " (" << error->code << ")";
                g_error_free(error);
                error = nullptr;
                return false;
            }
            return true;
        }

        void throw_on_error(GError* error, const char* func_name) const
        {
            if (error)
            {
                std::string msg = sstr()
                    << func_name
                    << ": " << error->domain << ": " << error->message
                    << " (" << error->code << ")";
                g_error_free(error);
                throw AtspiException(msg);
            }
        }

        template<typename T, typename F>
        Noneable<T>& cached_get(Noneable<T>& state_value,
                                const F& func, const char* func_name) const
        {
            if (state_value.is_none())
            {
                GError *error = nullptr;
                auto accptr = m_accessible.get();
                AtspiAccessible* acc = accptr.get();
                if (acc)
                {
                    auto value = func(acc, &error);
                    if(check_ok(error, func_name))
                    {
                        if constexpr(std::is_pointer_v<decltype(value)>)
                        {
                            if (value)
                                state_value = value;
                            else
                                state_value = decltype(state_value.value){};
                        }
                        else
                        {
                            state_value = value;
                        }

                    }
                }
                else
                    LOG_WARNING << func_name << ": invalid accessible " << acc;
            }
            return state_value;
        }

        void to_std_container(GHashTable* table, StringMap& m) const
        {
            g_hash_table_foreach (table,
                [](gpointer key, gpointer value, gpointer user_data)
                {
                    StringMap* m_ = reinterpret_cast<StringMap*>(user_data);
                    gchar* k = reinterpret_cast<gchar*>(key);
                    gchar* v = reinterpret_cast<gchar*>(value);
                    m_->emplace(k, v);
                },
                &m);
        }

        void to_std_container(GArray* garray, StringVector& v) const
        {
            gchar** data = reinterpret_cast<gchar**>(garray->data);
            for (guint i=0; i<garray->len; i++)
                v.emplace_back(data[i]);
        }

        void to_std_container(AtspiStateSet* state_set, StringVector& v) const
        {
            GArrayPtr ap = {atspi_state_set_get_states(state_set),
                           [](GArray* a){g_array_free(a, TRUE);} };
            if (ap)
            {
                AtspiStateType* data = reinterpret_cast<AtspiStateType*>(ap->data);
                for (guint i=0; i<ap->len; i++)
                    v.emplace_back(to_string(data[i]));
            }
        }

        // ////// Cached, exception-safe accessor functions //////

        Noneable<AtspiRole>& get_role() const
        {
            return CACHED_GET(m_state.role, atspi_accessible_get_role);
        }

        const Noneable<std::string>& get_role_name() const
        {
            return CACHED_GET(m_state.role_name, atspi_accessible_get_role_name);
        }

        const Noneable<std::string>& get_name() const
        {
            return CACHED_GET(m_state.name, atspi_accessible_get_name);
        }

        const Noneable<int>& get_id() const
        {
            return CACHED_GET(m_state.id, atspi_accessible_get_id);
        }

        void invalidate_state_set() const
        {
            m_state.state_set.reset();
        }

        AtspiStateSet* get_state_set() const
        {
            if (!m_state.state_set)
            {
                auto accptr = m_accessible.get();
                if (accptr)
                {
                    auto value = atspi_accessible_get_state_set(accptr.get());
                    if(value)
                        m_state.state_set = AtspiStateSetPtr{value, g_object_unref};
                }
            }
            return m_state.state_set.get();
        }

        bool contains_state(AtspiStateType type) const
        {
            AtspiStateSet* state_set = get_state_set();
            if(state_set)
                return atspi_state_set_contains(state_set, type);
            return false;
        }

        const StringVector& get_states() const
        {
            if (m_state.states.empty())
            {
                AtspiStateSet* state_set = get_state_set();
                if(state_set)
                    to_std_container(state_set, m_state.states);
            }
            return m_state.states;
        }

        const StringMap& get_attributes() const
        {
            if (m_state.attributes.empty())
            {
                auto accptr = m_accessible.get();
                if (accptr)
                {
                    GError* error = nullptr;
                    GHashTablePtr table = {atspi_accessible_get_attributes(accptr.get(), &error),
                                           g_hash_table_destroy};
                    if (check_ok(error, "atspi_accessible_get_attributes"))
                        to_std_container(table.get(), m_state.attributes);
                 }
            }
            return m_state.attributes;
        }

        const StringVector& get_interfaces() const
        {
            if (m_state.interfaces.empty())
            {
                auto accptr = m_accessible.get();
                if (accptr)
                {
                    GArrayPtr ap = {atspi_accessible_get_interfaces(accptr.get()),
                                   [](GArray* a){g_array_free(a, TRUE);} };
                    if (ap)
                        to_std_container(ap.get(), m_state.interfaces);
                }
            }
            return m_state.interfaces;
        }

        const Noneable<std::string>& get_description() const
        {
            return CACHED_GET(m_state.description, atspi_accessible_get_description);
        }

        const Noneable<pid_t>& get_pid() const
        {
            return CACHED_GET(m_state.pid, atspi_accessible_get_process_id);
        }

        const Noneable<std::string>& get_process_name() const
        {
            auto pid = this->get_pid();
            if (!pid.is_none() && pid != -1)
            {
                if (m_state.process_name.is_none())
                    m_state.process_name = Process::get_process_name(pid);
            }
            return m_state.process_name;
        }

        const Noneable<std::string>& get_toolkit_name() const
        {
            return CACHED_GET(m_state.toolkit_name, atspi_accessible_get_toolkit_name);
        }

        const Noneable<std::string>& get_toolkit_version() const
        {
            return CACHED_GET(m_state.toolkit_version, atspi_accessible_get_toolkit_version);
        }

        AtspiEditableText* get_editable_text_iface() const
        {
            if (!m_state.editable_text_iface)
            {
                auto accptr = m_accessible.get();
                if (accptr)
                    m_state.editable_text_iface =
                        atspi_accessible_get_editable_text_iface(accptr.get());
            }
            return m_state.editable_text_iface;
        }

        Ptr get_application() const
        {
            if (false &&   // disabled, still too crashy
                !m_state.application)
            {
                if (auto accptr = m_accessible.get())
                {
                    GError *error = nullptr;
                    AtspiAccessiblePtr value =
                        {atspi_accessible_get_application(accptr.get(), &error),
                         g_object_unref};
                    if(check_ok(error, "atspi_accessible_get_application"))
                        m_state.application =
                            std::make_shared<CachedAccessible>(*this, value);
                }
            }
            return m_state.application;
        }

        const Noneable<std::string> get_app_name() const
        {
            Ptr app = get_application();
            if (app)
                return app->get_name();
            return {};
        }

        const Noneable<std::string> get_app_description() const
        {
            Ptr app = get_application();
            if (app)
                return app->get_description();
            return {};
        }

        virtual void invalidate_extents() const override
        {
            m_state.extents.set_none();
        }

        double get_scale() const
        {
            double scale = config()->get_window_scaling_factor();
            if (scale != 1.0)
            {
                // Only Gtk-3 widgets return scaled coordinates, all others,
                // including Gtk-2 apps like firefox, clawsmail and Qt-apps,
                // apparently don't.
                if (is_toolkit_gtk3())
                    scale = 1.0;
            }
            return scale;
        }

        // Screen rect after scaling.
        virtual Noneable<Rect> get_extents() const override
        {
            if (m_state.extents.is_none())
            {
                if (auto accptr = m_accessible.get())
                {
                    AtspiIFaceComponentPtr component =
                        {atspi_accessible_get_component_iface(accptr.get()),
                         g_object_unref};
                    if (component)
                    {
                        GError* error = nullptr;
                        AtspiRectPtr r = {atspi_component_get_extents(component.get(), ATSPI_COORD_TYPE_SCREEN, &error),
                                          g_free};
                        if (check_ok(error, "atspi_component_get_extents"))
                        {
                            double f = 1.0 / get_scale();
                            m_state.extents = Rect(r->x * f, r->y * f, r->width * f, r->height * f);
                        }
                    }
                }
            }

            return m_state.extents;
        }

        Ptr get_frame() const
        {
            if (!m_state.frame)
                m_state.frame = get_accessible_frame(this);
            return m_state.frame;
        }

        virtual Noneable<Rect> get_frame_extents() const override
        {
            Ptr frame = get_frame();
            if (frame)
                return frame->get_extents();
            return {};
        }

        // Accessible of the top level window to which accessible belongs.
        Ptr get_accessible_frame(const CachedAccessible* accessible) const
        {
            Ptr frame;
            LOG_ATSPI << "searching for top level:";

            AtspiAccessiblePtr parent = accessible->m_accessible.get();
            if (parent)
            {
                while (true)
                {
                    GError* error = nullptr;
                    parent = {atspi_accessible_get_parent(parent.get(), &error), g_object_unref};
                    if (!parent || error)
                        break;

                    AtspiRole role = atspi_accessible_get_role(parent.get(), &error);
                    if (error)
                        break;

                    LOG_ATSPI << "parent: " << role;
                    if (role == ATSPI_ROLE_FRAME ||
                        role == ATSPI_ROLE_DIALOG ||
                        role == ATSPI_ROLE_WINDOW ||
                        role == ATSPI_ROLE_NOTIFICATION)
                    {
                        frame = std::make_shared<CachedAccessible>(*this, parent);
                        break;
                    }
                }
            }
            return frame;
        }

        ////// uncached functions, not throwing exceptions //////

        virtual Noneable<Span> get_selection(int selection_num=0) const override
        {
            if (auto accptr = m_accessible.get())
            {
                AtspiIFaceTextPtr iface(
                    atspi_accessible_get_text_iface(accptr.get()),
                    g_object_unref);
                if (iface)
                {
                    GError* error = nullptr;
                    AtspiRangePtr range(
                         atspi_text_get_selection(iface.get(), selection_num, &error),
                         g_free);
                    if (check_ok(error, "atspi_text_get_selection"))
                    {
                        // Gtk-2 applications return 0,0 when there is no selection.
                        // Gtk-3 applications return caret positions in that case.
                        // LibreOffice Writer in Vivid initially returns -1,-1 when there
                        // is no selection, later the caret position.
                        int start = range->start_offset;
                        int end = range->end_offset;
                        if (start > 0 &&
                            end > 0 &&
                            start <= end)
                        {
                            return Span{start, end-start};
                        }
                    }
                }
            }
            return {};
        }

        virtual void set_caret_offset(int offset) override
        {
            if (auto accessible = m_accessible.get())
            {
                AtspiIFaceTextPtr iface(
                     atspi_accessible_get_text_iface(accessible.get()),
                     g_object_unref);
                if (iface)
                {
                    GError* error = nullptr;
                    atspi_text_set_caret_offset(iface.get(), offset, &error);
                    check_ok(error, "atspi_text_set_caret_offset");
                }
            }
        }

        virtual bool insert_text(int position, const std::string& text) override
        {
            if (auto accessible = m_accessible.get())
            {
                AtspiIFaceEditableTextPtr iface
                    (atspi_accessible_get_editable_text_iface(accessible.get()),
                     g_object_unref);
                if (iface)
                {
                    GError* error = nullptr;
                    atspi_editable_text_insert_text(iface.get(), position, text.c_str(),
                                                    static_cast<gint>(text.size()),
                                                    &error);
                    return check_ok(error, "atspi_editable_text_insert_text");
                }
            }
            return false;
        }

        virtual bool delete_text(int start_pos, int end_pos) override
        {
            if (auto accessible = m_accessible.get())
            {
                AtspiIFaceEditableTextPtr iface(
                    atspi_accessible_get_editable_text_iface(accessible.get()),
                    g_object_unref);
                if (iface)
                {
                    GError* error = nullptr;
                    atspi_editable_text_delete_text(iface.get(), start_pos, end_pos,
                                                    &error);
                    return check_ok(error, "atspi_editable_text_delete_text");
                }
            }
            return false;
        }


        ////// uncached functions, throwing exceptions //////

        virtual TextPos get_caret_offset() const override
        {
            TextPos offset{};

            if (auto accessible = m_accessible.get())
            {
                AtspiIFaceTextPtr iface(atspi_accessible_get_text_iface(accessible.get()),
                                   g_object_unref);
                if (iface)
                {
                    GError* error = nullptr;
                    offset = atspi_text_get_caret_offset(iface.get(), &error);

                    throw_on_error(error, "atspi_text_get_caret_offset");
                }
            }
            return offset;
        }

        virtual TextLength get_character_count() const override
        {
            TextLength count{};

            if (auto accessible = m_accessible.get())
            {
                AtspiIFaceTextPtr iface(atspi_accessible_get_text_iface(accessible.get()),
                                   g_object_unref);
                if (iface)
                {
                    GError* error = nullptr;
                    count = atspi_text_get_character_count(iface.get(), &error);

                    throw_on_error(error, "atspi_text_get_character_count");
                }
            }
            return count;
        }

        virtual TextSpanPtr get_text(const Span& span) const override
        {
            TextSpanPtr result;

            if (auto accessible = m_accessible.get())
            {
                AtspiIFaceTextPtr iface(
                    atspi_accessible_get_text_iface(accessible.get()),
                    g_object_unref);
                if (iface)
                {
                    // FIXME: deprecated, replace with atspi_text_get_string_at_offset
                    GError* error = nullptr;
                    GStrPtr text =
                        {atspi_text_get_text(iface.get(), span.begin, span.end(), &error),
                         g_free};

                    throw_on_error(error, "atspi_text_get_text");

                    result = std::make_shared<TextSpan>(span.begin,
                                                        span.end(),
                                                        text.get(),
                                                        span.begin);
                }
            }
            return result;
        }

        virtual TextSpanPtr get_line_at_offset(TextPos offset) const override
        {
            return get_text_at_offset(offset, ATSPI_TEXT_BOUNDARY_LINE_START);
        }
        virtual TextSpanPtr get_line_before_offset(TextPos offset) const override
        {
            return get_text_before_offset(offset, ATSPI_TEXT_BOUNDARY_LINE_START);
        }

        TextSpanPtr get_text_at_offset(TextPos offset, AtspiTextBoundaryType boundary_type) const
        {
            TextSpanPtr result;
            if (auto accessible = m_accessible.get())
            {
                AtspiIFaceTextPtr iface(
                    atspi_accessible_get_text_iface(accessible.get()),
                    g_object_unref);
                if (iface)
                {
                    // FIXME: deprecated, replace with atspi_text_get_string_at_offset
                    GError* error = nullptr;
                    AtspiTextRangePtr text_range =
                        {atspi_text_get_text_at_offset(iface.get(), offset, boundary_type, &error),
                         g_free};

                    throw_on_error(error, "atspi_text_get_text_at_offset");

                    result = std::make_shared<TextSpan>(text_range->start_offset,
                                                        text_range->end_offset - text_range->start_offset,
                                                        text_range->content,
                                                        text_range->start_offset);
                }
            }
            return result;
        }

        TextSpanPtr get_text_before_offset(TextPos offset, AtspiTextBoundaryType boundary_type) const
        {
            TextSpanPtr result;
            if (auto accessible = m_accessible.get())
            {
                AtspiIFaceTextPtr iface(
                    atspi_accessible_get_text_iface(accessible.get()),
                    g_object_unref);
                if (iface)
                {
                    // FIXME: deprecated, replace with atspi_text_get_string_at_offset
                    GError* error = nullptr;
                    AtspiTextRangePtr text_range =
                        {atspi_text_get_text_before_offset(iface.get(), offset, boundary_type, &error),
                         g_free};

                    throw_on_error(error, "atspi_text_get_text_at_offset");

                    result = std::make_shared<TextSpan>(text_range->start_offset,
                                                        text_range->end_offset - text_range->start_offset,
                                                        text_range->content,
                                                        text_range->start_offset);
                }
            }
            return result;
        }

        // Text of the given accessible, no caching
        std::string get_text(int begin, int end)
        {
            std::string str;

            if (auto accessible = m_accessible.get())
            {
                AtspiIFaceTextPtr iface(
                    atspi_accessible_get_text_iface(accessible.get()),
                    g_object_unref);
                if (iface)
                {
                    // FIXME: deprecated, replace with atspi_text_get_string_at_offset
                    GError* error = nullptr;
                    GStrPtr text(
                        atspi_text_get_text(iface.get(), begin, end, &error),
                        g_free);

                    throw_on_error(error, "atspi_text_get_text");

                    str = text.get();
                }
            }
            return str;
        }

        ////// Higher-level functions //////

        bool is_focused(bool invalidate=false)
        {
            // re-read properties?
            if (invalidate)
                invalidate_state_set();

            AtspiStateSet* state_set = get_state_set();
            if (state_set)
                return atspi_state_set_contains(state_set, ATSPI_STATE_FOCUSED);

            return false;
        }

        // Is this an accessible onboard should be shown for?
        bool is_editable()
        {
            auto role = get_role();
            AtspiStateSet* state_set = get_state_set();
            if (!role.is_none() && state_set)
            {
                if (contains(array_of<AtspiRole> (
                        ATSPI_ROLE_TEXT,
                        ATSPI_ROLE_TERMINAL,
                        ATSPI_ROLE_DATE_EDITOR,
                        ATSPI_ROLE_PASSWORD_TEXT,
                        ATSPI_ROLE_EDITBAR,
                        ATSPI_ROLE_ENTRY,
                        ATSPI_ROLE_DOCUMENT_TEXT,
                        ATSPI_ROLE_DOCUMENT_FRAME,
                        ATSPI_ROLE_DOCUMENT_EMAIL,
                        ATSPI_ROLE_SPIN_BUTTON,
                        ATSPI_ROLE_COMBO_BOX,
                        ATSPI_ROLE_DATE_EDITOR,
                        ATSPI_ROLE_PARAGRAPH,      // LibreOffice Writer
                        ATSPI_ROLE_HEADER,
                        ATSPI_ROLE_FOOTER
                        ), role.value))
                {
                    if (role == ATSPI_ROLE_TERMINAL ||
                        atspi_state_set_contains(state_set, ATSPI_STATE_EDITABLE))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        // Is this accessible unlikely to steal the focus from
        // a previously focused editable accessible?
        bool is_not_focus_stealing()
        {
            auto role = get_role();
            auto state_set = get_state_set();
            if (role.is_none() && state_set)
            {
                // Mainly firefox elements after the workaround
                // for firefox 50.
                if ((role == ATSPI_ROLE_DOCUMENT_FRAME ||
                     role == ATSPI_ROLE_LINK) &&
                     !atspi_state_set_contains(state_set, ATSPI_STATE_EDITABLE))
                {
                    return true;
                }
            }
            return false;
        }

        virtual bool is_text_entry() const override
        {
            auto& interfaces = get_interfaces();
            return contains(interfaces, std::string("Text"));
        }

        virtual bool is_password_entry() const override
        {
            auto role = get_role();
            return role == ATSPI_ROLE_PASSWORD_TEXT;
        }

        // Is this a (most likely firefox') URL bar?
        virtual bool is_urlbar() const override
        {
            auto& attributes = get_attributes();
            std::string s = get_value(attributes, "class");
            return contains(s, "urlbar");
        }

        // Is this possibly byobu running in a terminal?
        virtual bool is_byobu() const override
        {
            auto description = get_description();
            return !description.is_none() &&
                   contains(lower(description), "byobu");
        }

        // Is this a terminal?
        virtual bool is_terminal() const override
        {
            auto role = get_role();
            return role == ATSPI_ROLE_TERMINAL;
        }

        // Is accessible a single line text entry?
        virtual bool is_single_line() const override
        {
            auto state_set = get_state_set();
            return state_set &&
                   atspi_state_set_contains(state_set, ATSPI_STATE_SINGLE_LINE);
        }

        // Are the accessible attributes from a gtk3 widget?
        bool is_toolkit_gtk3() const
        {
            auto& attributes = get_attributes();
            return get_value(attributes, "toolkit") == "gtk";
        }

        virtual Noneable<Rect> get_character_extents(int offset) override
        {
            return get_character_extents(this->getptr(), offset);
        }

        // Screen rect of the character at offset of the accessible.
        // No caching.
        Noneable<Rect> get_character_extents(const Ptr& accessible, int offset) const
        {
            Noneable<Rect> rect;

            if (auto acc = accessible->m_accessible.get())
            {
                AtspiIFaceTextPtr iface(
                    atspi_accessible_get_text_iface(acc.get()),
                    g_object_unref);
                if (iface)
                {
                    GError* error = nullptr;
                    AtspiRectPtr r = {atspi_text_get_character_extents(iface.get(), offset, ATSPI_COORD_TYPE_SCREEN, &error),
                                      g_free};
                    if (check_ok(error, "atspi_text_get_character_extents"))
                    {
                        double f = 1.0 / get_scale();
                        rect = Rect(r->x * f, r->y * f, r->width * f, r->height * f);
                    }
                }
            }
            return rect;
        }

        // Can insert text via Atspi?
        // Advantages:
        // - faster, no individual key presses
        // - trouble-free insertion of all unicode characters
        virtual bool can_insert_text() const override
        {
            auto& interfaces = get_interfaces();
            if (contains(interfaces, "EditableText"))
            {
                // Support for atspi text insertion is spotty.
                // Firefox, LibreOffice Writer, gnome-terminal don't support it,
                // even if they claim to implement the EditableText interface.

                // Allow direct text insertion for gtk widgets
                if (is_toolkit_gtk3())
                    return true;
            }
            return false;
        }
};


typedef CachedAccessible::Ptr CachedAccessiblePtr;

CachedAccessible::~CachedAccessible()
{}



std::unique_ptr<AtspiStateTracker> AtspiStateTracker::make(const ContextBase& context)
{
    return std::make_unique<AtspiStateTracker>(context);
}

static vector<string> focus_event_names      {"text-entry-activated"};
static vector<string> text_event_names       {"text-changed", "text-caret-moved"};
static vector<string> key_stroke_event_names {"key-pressed"};
static vector<string> async_event_names      {"async-focus-changed",
                                              "async-text-changed",
                                              "async-text-caret-moved"};
static vector<string> event_names = async_event_names +
                                    focus_event_names +
                                    text_event_names +
                                    key_stroke_event_names;

AtspiStateTracker::AtspiStateTracker(const ContextBase& context) :
    Super(context),
    m_poll_unity_timer(std::make_unique<Timer>(context))
{

}

AtspiStateTracker::~AtspiStateTracker()
{
    register_atspi_listeners(false);
}

void AtspiStateTracker::on_listeners_changed()
{
    update_listeners();
    if (!has_listeners())
        LOG_INFO << "all listeners disconnected";
}

bool AtspiStateTracker::has_listeners()
{
    return text_entry_activated.has_listeners() ||
           text_changed.has_listeners() ||
           text_caret_moved.has_listeners() ||
           key_pressed.has_listeners();
}

void AtspiStateTracker::update_listeners()
{
    bool register_;
    register_ = text_entry_activated.has_listeners();
    register_atspi_focus_listeners(register_);

    register_ = text_changed.has_listeners() || text_caret_moved.has_listeners();
    register_atspi_text_listeners(register_);

    register_ = key_pressed.has_listeners();
    register_atspi_keystroke_listeners(register_);
}

void AtspiStateTracker::register_atspi_listeners(bool register_)
{
    register_atspi_focus_listeners(register_);
    register_atspi_text_listeners(register_);
    register_atspi_keystroke_listeners(register_);
}

// Start listening to an AT-SPI event.
// Creates a new event listener for each event, since this seems
// to be the only way to allow reliable deregistering of events.
template<typename Callback>
void atspi_connect(AtspiStateTracker* this_,
                   AtspiListenerPtr& listener,
                   const char* event, Callback callback)
{
    if (!listener)
    {
        GError* error = nullptr;

        listener = {atspi_event_listener_new(callback, this_, NULL),
            g_object_unref};
        atspi_event_listener_register(listener.get(),
                                      event, &error);
        this_->check_ok(error,
            (string("atspi_event_listener_register (") + event + ")").c_str());
    }
}

void atspi_disconnect(AtspiStateTracker* this_,
                      AtspiListenerPtr& listener,
                      const char* event)
{
    if (listener)
    {
        GError *error = nullptr;

        atspi_event_listener_deregister(listener.get(),
                                       event, &error);
        this_->check_ok(error,
            (string("atspi_event_listener_deregister (") + event + ")").c_str());

        listener = nullptr;
    }
}

void AtspiStateTracker::register_atspi_focus_listeners(bool register_)
{
    LOG_TRACE << repr(register_);

    if (m_focus_listeners_registered != register_)
    {
        if (register_)
        {
            atspi_connect(this, m_listener_focus, "focus",
                          [](AtspiEvent* event, void* user_data)
                          {
                              auto st = static_cast<AtspiStateTracker*>(user_data);
                              st->on_atspi_global_focus(event);
                          });
            atspi_connect(this, m_listener_object_focus, "object:state-changed:focused",
                          [](AtspiEvent* event, void* user_data)
                          {
                              auto st = static_cast<AtspiStateTracker*>(user_data);
                              st->on_atspi_object_focus(event);
                          });

            // private asynchronous events
            m_connections.connect(async_focus_changed,
                                 [&](const ASyncEvent& e){on_async_focus_changed(e);});
            m_connections.connect(async_text_caret_moved,
                                 [&](const ASyncEvent& e){on_async_text_caret_moved(e);});
            m_connections.connect(async_text_changed,
                                 [&](const ASyncEvent& e){on_async_text_changed(e);});
        }
        else
        {
            m_poll_unity_timer->stop();

            atspi_disconnect(this, m_listener_focus, "focus");
            atspi_disconnect(this, m_listener_object_focus, "object:state-changed:focused");

            m_connections.disconnect_all();
            async_text_caret_moved.disconnect(this);
            async_text_changed.disconnect(this);
        }

        m_focus_listeners_registered = register_;
    }
}

void AtspiStateTracker::register_atspi_text_listeners(bool register_)
{
    LOG_TRACE << repr(register_);

    if (m_text_listeners_registered != register_)
    {
        if (register_)
        {
            atspi_connect(this, m_listener_text_changed, "object:text-changed",
                          [](AtspiEvent* event, void* user_data)
                          {
                              auto st = static_cast<AtspiStateTracker*>(user_data);
                              st->on_atspi_text_changed(event);
                          });
            atspi_connect(this, m_listener_text_caret_moved, "object:text-caret-moved",
                          [](AtspiEvent* event, void* user_data)
                          {
                              auto st = static_cast<AtspiStateTracker*>(user_data);
                              st->on_atspi_text_caret_moved(event);
                          });
        }
        else
        {
            atspi_disconnect(this, m_listener_text_changed, "object:text-changed");
            atspi_disconnect(this, m_listener_text_caret_moved, "object:text-caret-moved");
        }
    }

    m_text_listeners_registered = register_;
}

void AtspiStateTracker::register_atspi_keystroke_listeners(bool register_)
{
#if 0
    if (m_keystroke_listeners_registered != register_)
    {
        modifier_masks = range(16);

        if (register)
        {
            if (!this->_keystroke_listener)
            {
                this->_keystroke_listener = \
                        Atspi.DeviceListener.new(this->_on_atspi_keystroke,
                                                 None);
            }

            for (auto modifier_mask  : modifier_masks)
            {
                Atspi.register_keystroke_listener(
                            this->_keystroke_listener,
                            None,;        // key set, None=all
                        modifier_mask,
                        Atspi.KeyEventType.PRESSED,
                        Atspi.KeyListenerSyncType.SYNCHRONOUS);
            }
        }
        else
        {
            // Apparently any single deregister call will turn off
            // all the other registered modifier_masks too. Since
            // deregistering takes extremely long (~2.5s for 16 calls)
            // seize the opportunity and just pick a single arbitrary
            // mask (Quantal).
            modifier_masks = [2];

            for (auto modifier_mask : modifier_masks)
            {
                Atspi.deregister_keystroke_listener(
                            this->_keystroke_listener,
                            None,  // key set, None=all
                            modifier_mask,
                            Atspi.KeyEventType.PRESSED);
            }
        }
    }
#endif
    m_keystroke_listeners_registered = register_;
}

void AtspiStateTracker::freeze()
{
    register_atspi_listeners(false);
    m_frozen = true;
}

void AtspiStateTracker::thaw()
{
    m_frozen = false;
    update_listeners();
}

void AtspiStateTracker::on_atspi_global_focus(AtspiEvent* event)
{
    on_atspi_focus(event, true);
}

void AtspiStateTracker::on_atspi_object_focus(AtspiEvent* event)
{
    on_atspi_focus(event);
}

void AtspiStateTracker::on_atspi_focus(AtspiEvent* event, bool focus_received)
{
    bool focused = (focus_received ||
                    event->detail1 != 0);  // received focus?
    ASyncEvent ae;
    ae.accessible = get_cached_accessible(event->source);
    if (ae.accessible)
        ae.accessible->get_role();
    ae.focused = focused;
    async_focus_changed.emit_async(ae);
}

void AtspiStateTracker::on_atspi_text_changed(AtspiEvent* event)
{
    #ifdef VERBOSE_ATSPI_LOGGING
    LOG_ATSPI << "detail1=" << event->detail1
              << " detail2=" << event->detail2
              << " source=" << event->source
              << " type=" << event->type;
    #endif
    ASyncEvent ae;
    ae.accessible = get_cached_accessible(event->source);
    ae.type = event->type;
    ae.span = {event->detail1, event->detail2};
    async_text_changed.emit_async(ae);
}

void AtspiStateTracker::on_atspi_text_caret_moved(AtspiEvent* event)
{
    #ifdef VERBOSE_ATSPI_LOGGING
    LOG_ATSPI << "detail1=" << event->detail1
              << " detail2=" << event->detail2
              << " source=" << event->source
              << " type=" << event->type;
    #endif
    ASyncEvent ae;
    ae.accessible = get_cached_accessible(event->source);
    ae.caret = event->detail1;
    async_text_caret_moved.emit_async(ae);
}

void AtspiStateTracker::on_atspi_keystroke(AtspiEvent* event)
{
    (void)event;
    #if 0
    if (event->type == ATSPI_KEY_PRESSED_EVENT)
    {
    }
    LOG_DEBUG(LogCategory::ATSPI, "key-stroke {} {} {} {}";
            .format(event.modifiers,
                    event.hw_code, event.id, event.is_text));
    // keysym = event.id // What is this? Not an XK_ keysym apparently.
    ae = AsyncEvent(hw_code=event.hw_code,
                    modifiers=event.modifiers);
    this->emit_async("key-pressed", ae);
    #endif
}

void AtspiStateTracker::on_async_focus_changed(const ASyncEvent& event)
{
    auto accessible = event.accessible;
    bool focused = event.focused;

    // Don't access the accessible while frozen. This leads to deadlocks
    // while displaying Onboard's own dialogs/popup menu's.
    if (m_frozen)
        return;

    log_accessible(accessible, focused);

    if (!accessible)
        return;

    auto app_name = lower(accessible->get_app_name());
    if (app_name == "unity")
        handle_focus_changed_unity(event);
    else
        handle_focus_changed_apps(event);
}

void AtspiStateTracker::handle_focus_changed_apps(const ASyncEvent& event)
{
    auto accessible = event.accessible;
    bool focused = event.focused;

    if (!accessible)
        return;

    // Since Trusty, focus events no longer come reliably in a
    // predictable order. -> Store the last editable accessible
    // so we can pick it over later focused non-editable ones.
    // Helps to keep the keyboard open in presence of popup selections
    // e.g. in GNOME's file dialog and in Unity Dash.
    if (is_same(m_focused_accessible, accessible))
    {
        if (!focused)
            m_focused_accessible = nullptr;
    }
    else
    {
        int pid = accessible->get_pid();

        if (focused)
        {
            m_poll_unity_timer->stop();

            if (accessible->is_editable())
            {
                m_focused_accessible = accessible;
                m_focused_pid = pid;
            }

            // Static accessible, i.e. something that cannot
            // accidentally steal the focus from an editable
            // accessible. e.g. firefox ATSPI_ROLE_DOCUMENT_FRAME?
            else if (accessible->is_not_focus_stealing())
            {
                m_focused_accessible = nullptr;
                m_focused_pid = 0;
            }
            else
            {
                // Wily: attempt to hide when unity dash closes
                // (there's no focus-lost event).
                // Also check duration since last activation to
                // skip out of order focus events (firefox
                // ATSPI_ROLE_DOCUMENT_FRAME) for a short while
                // after opening dash.
                using namespace std::chrono;
                if (focused &&
                    (Clock::now() - m_active_accessible_activation_time) > 500ms)
                {
                    if (m_focused_pid != pid)
                    {
                        m_focused_accessible = nullptr;
                        LOG_ATSPI << "dropping accessible due to "
                                  << "pid change: " << m_focused_pid << " != " << pid;
                    }
                }
            }
        }
    }

    // Has the previously focused accessible lost the focus?
    auto& active_accessible = m_focused_accessible;
    if (active_accessible &&
        !active_accessible->is_focused(true))
    {
        // Zesty: Firefox 50+ loses focus of the URL entry after
        // typing just a few letters and focuses a completion
        // menu item instead. Let's pretend the accessible is
        // still focused in that case.
        bool is_firefox_completion = \
                m_focused_accessible->is_urlbar() &&
                accessible->get_role() == ATSPI_ROLE_MENU_ITEM;

        if (!is_firefox_completion)
            active_accessible = nullptr;
    }

    set_active_accessible(active_accessible);
}

void AtspiStateTracker::handle_focus_changed_unity(const ASyncEvent& event)
{
    auto accessible = event.accessible;
    bool focused = event.focused;

    // Wily: prevent random icons, buttons && toolbars
    // in unity dash from hiding Onboard. Somehow hovering
    // over those buttons silently drops the focus from the
    // text entry. Let's pretend the buttons don't exist
    // && keep the previously saved text entry active.

    // Zesty: Don't fight lost focus events anymore, only
    // react to focus events when the text entry gains focus.
    if (focused &&
        accessible->is_editable())
    {
        m_focused_accessible = accessible;
        set_active_accessible(accessible);

        // Only ever start polling if Dash is "ACTIVE".
        // The state_set might change in the future && the
        // keyboard better fail to auto-hide than to never show.
        CachedAccessiblePtr frame = accessible->get_frame();
        if (frame)
        {
            LOG_DEBUG << "dash focused, state_set: " << frame->get_states();

            if (frame->contains_state(ATSPI_STATE_ACTIVE))
            {
                m_poll_unity_timer->start(0.5,
                    [=](){return poll_unity_dash(accessible);});
            }
        }
    }
}

// For hiding we poll Dash's toplevel accessible
bool AtspiStateTracker::poll_unity_dash(CachedAccessiblePtr accessible)
{
    auto frame = accessible->get_frame();

    LOG_DEBUG << "polling unity dash state_set: " << frame->get_states();

    if (!frame->contains_state(ATSPI_STATE_ACTIVE))
    {
        m_focused_accessible = nullptr;
        set_active_accessible(nullptr);
        return false;
    }

    return true;
}

void AtspiStateTracker::set_active_accessible(const CachedAccessiblePtr& accessible)
{
    LOG_DEBUG << "1 " << m_active_accessible << " " << accessible;
    if (m_active_accessible != accessible)
    {
        m_active_accessible = accessible;

        LOG_DEBUG << "2 " << accessible << " " << m_active_accessible << " " << m_last_active_accessible;
        if (m_active_accessible ||
            m_last_active_accessible)
        {
            LOG_DEBUG << "3 " << accessible;
            // notify listeners
            text_entry_activated.emit(
                        std::dynamic_pointer_cast<UIElement>(m_active_accessible));

            m_last_active_accessible = m_active_accessible;
            m_active_accessible_activation_time = Clock::now();
        }
    }
}

void AtspiStateTracker::on_async_text_changed(const ASyncEvent& event)
{
    if (is_same(event.accessible, m_active_accessible))
    {
        std::string type = event.type;
        bool insert = endswith(type, "insert") ||
                      endswith(type, "insert:system");
        bool delete_ = endswith(type, "delete") ||
                       endswith(type, "delete:system");

        LOG_ATSPI << "type=" << type
                  << " insert=" << insert
                  << " delete=" << delete_;

        if (insert || delete_)
        {
            ASyncEvent e = event;
            e.insert = insert;
            text_changed.emit(e);
        }
        else
        {
            LOG_WARNING << "on_async_text_changed:"
                        << " unknown event type " << repr(event.type);
        }
    }
}

void AtspiStateTracker::on_async_text_caret_moved(const ASyncEvent& event)
{
    if (is_same(event.accessible, m_active_accessible))
        text_caret_moved.emit(event);
}

CachedAccessiblePtr AtspiStateTracker::get_cached_accessible(AtspiAccessible* accessible)
{
    if (accessible)
        return std::make_shared<CachedAccessible>(*this, accessible);
    return {};
}

bool AtspiStateTracker::check_ok(GError*& error, const char* func_name)
{
    if (error)
    {
        LOG_ATSPI << func_name
                  << ": " << error->domain << ": " << error->message
                  << " (" << error->code << ")";
        g_error_free(error);
        error = nullptr;
        return false;
    }
    return true;
}

void AtspiStateTracker::log_accessible(const CachedAccessiblePtr& accessible, bool focused)
{
    if (logger()->can_log(LogLevel::ATSPI))
    {
        std::string msg = sstr()
            << "AT-SPI focus event: focused=" << focused
            << " accessible=" << accessible;

        if (accessible)
        {
            auto name = accessible->get_name();
            auto role = accessible->get_role();
            auto role_name = accessible->get_role_name();
            auto states = accessible->get_states();
            bool editable = accessible->contains_state(ATSPI_STATE_EDITABLE);
            Rect extents = accessible->get_extents();

            std::string msg2 = sstr()
                << " name=" << repr(name)
                << " role=" << to_string(role)
                << "(" << role << ", " << repr(role_name) << ")"
                << " editable=" << repr(editable)
                << " states=" << states
                << " extents=" << extents;
            msg += msg2;
        }
        LOG_ATSPI << msg;
    }
}



