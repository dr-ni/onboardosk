#ifndef ATSPISTATETRACKER_H
#define ATSPISTATETRACKER_H

#include <chrono>
#include <memory>

#include "tools/noneable.h"
#include "tools/rect_fwd.h"
#include "tools/textdecls.h"

#include "signalling.h"
#include "onboardoskglobals.h"

class Timer;

class UIElement;
typedef std::shared_ptr<UIElement> UIElementPtr;

class CachedAccessible;
typedef std::shared_ptr<CachedAccessible> CachedAccessiblePtr;

struct _AtspiEventListener;
typedef struct _AtspiEventListener AtspiEventListener;
// shared_ptr allows forward declaration with custom deleter
using AtspiListenerPtr = std::shared_ptr<AtspiEventListener>;

struct _AtspiEvent;
typedef struct _AtspiEvent AtspiEvent;

struct _AtspiAccessible;
typedef struct _AtspiAccessible AtspiAccessible;

struct _GError;
typedef struct _GError GError;

struct ASyncEvent
{
    CachedAccessiblePtr accessible;
    std::string type;
    Span span;
    TextPos caret{};
    bool insert{};
    bool focused{};
};


// Keeps track of the currently active accessible by listening
// to AT-SPI focus events.
class AtspiStateTracker : public ContextBase
{
    public:
        using Super = ContextBase;
        using Clock = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        AtspiStateTracker(const ContextBase& context);
        virtual ~AtspiStateTracker();

        static std::unique_ptr<AtspiStateTracker> make(const ContextBase& context);

        // Freeze AT-SPI message processing, e.g. while displaying
        // a dialog or popoup menu.
        void freeze();

        // Resume AT-SPI message processing.
        void thaw();

        bool check_ok(GError*& error, const char* func_name);

    private:
        void on_listeners_changed();
        bool has_listeners();
        void update_listeners();
        void register_atspi_listeners(bool register_);
        void register_atspi_focus_listeners(bool register_);
        void register_atspi_text_listeners(bool register_);
        void register_atspi_keystroke_listeners(bool register_);

        void on_atspi_global_focus(AtspiEvent* event);
        void on_atspi_object_focus(AtspiEvent* event);
        void on_atspi_focus(AtspiEvent* event, bool focus_received=false);
        void on_atspi_text_changed(AtspiEvent* event);
        void on_atspi_text_caret_moved(AtspiEvent* event);
        void on_atspi_keystroke(AtspiEvent* event);

        // ////////////////// asynchronous handlers ////////////////// //
        void on_async_focus_changed(const ASyncEvent& event);

        // Focus change in regular applications
        void handle_focus_changed_apps(const ASyncEvent& event);

        // Focus change in Unity Dash
        void handle_focus_changed_unity(const ASyncEvent& event);

        void set_active_accessible(const CachedAccessiblePtr& accessible);

        void on_async_text_changed(const ASyncEvent& event);

        void on_async_text_caret_moved(const ASyncEvent& event);

    private:
        bool poll_unity_dash(CachedAccessiblePtr accessible);
        CachedAccessiblePtr get_cached_accessible(AtspiAccessible* accessible);
        void log_accessible(const CachedAccessiblePtr& accessible, bool focused);

    private:
        bool m_focus_listeners_registered{false};
        bool m_keystroke_listeners_registered{false};
        bool m_text_listeners_registered{false};
        bool m_frozen{false};

        int m_keystroke_listener{};

        // asynchronously accessible members
        CachedAccessiblePtr m_focused_accessible;       // last focused editable accessible
        int m_focused_pid{};                            // pid of last focused editable accessible
        CachedAccessiblePtr m_active_accessible;        // currently active editable accessible
        TimePoint m_active_accessible_activation_time;  // time since focus received
        CachedAccessiblePtr m_last_active_accessible;

        std::unique_ptr<Timer> m_poll_unity_timer;

        AtspiListenerPtr m_listener_focus;
        AtspiListenerPtr m_listener_object_focus;
        AtspiListenerPtr m_listener_text_changed;
        AtspiListenerPtr m_listener_text_caret_moved;

    public:
        DEFINE_SIGNAL(<const UIElementPtr&>, text_entry_activated,
                      this, [&](){on_listeners_changed();});
        DEFINE_SIGNAL(<const ASyncEvent&>, text_changed,
                      this, [&](){on_listeners_changed();});
        DEFINE_SIGNAL(<const ASyncEvent&>, text_caret_moved,
                      this, [&](){on_listeners_changed();});
        DEFINE_SIGNAL(<>, key_pressed,
                      this, [&](){on_listeners_changed();});

    private:
        DEFINE_SIGNAL(<ASyncEvent>, async_focus_changed, this);
        DEFINE_SIGNAL(<ASyncEvent>, async_text_changed, this);
        DEFINE_SIGNAL(<ASyncEvent>, async_text_caret_moved, this);

        SignalConnections m_connections; // last to initialize, first to destruct
};



#endif // ATSPISTATETRACKER_H
