#ifndef AUTOSHOW_H
#define AUTOSHOW_H

#include <vector>

#include "tools/border_fwd.h"
#include "tools/noneable.h"
#include "tools/point_decl.h"
#include "tools/rect_fwd.h"

#include "onboardoskglobals.h"
#include "uielementdecls.h"

class AtspiStateTracker;
class HardwareSensorTracker;
class UIElement;
class Timer;
class UDevTracker;


class AutoShow : public ContextBase
{
    private:
        struct AutoShowLock
        {
            // Just use shared_ptr here. unique_ptr blows
            // up the code with custom lvalue and rvalue
            // copy constructors for set_default() and
            // rvalue assignment operator for vector::erase().
            std::shared_ptr<Timer> timer;

            bool lock_show{true};
            bool lock_hide{true};
            Noneable<bool> visibility_change;
        };

    public:
        AutoShow(const ContextBase& context);
        virtual ~AutoShow();

        void reset();
        void enable(bool enable);
        void enable_tablet_mode_detection(bool enable);

        // Detect if physical keyboard devices are present in the system.
        // When detected, auto-show is locked.
        void enable_keyboard_device_detection(bool enable);

        // Lock showing and/or hiding the keyboard window.
        // There is a separate, independent lock for each unique "reason".
        // If duration is specified, automatically unlock after these number of
        // seconds.
        void lock(const std::string& reason,
                  const Noneable<double>& duration,
                  bool lock_show, bool lock_hide);

        // Remove a specific lock named by "reason".;
        // Returns the change in visibility that occurred while this lock was;
        // active. None for no change.;
        Noneable<bool> unlock(const std::string& reason);

        // Remove all locks.
        void unlock_all();

        bool is_locked(const std::string& reason);

        bool is_show_locked();
        bool is_hide_locked();

        // Lock window permanently visible in response to the user showing it.
        // Optionally freeze hiding/showing for a limited time.
        void lock_visible(bool lock, const Noneable<double>& thaw_time=1.0);

        void request_keyboard_visible(bool visible, Noneable<double> delay={});

        // Get the alternative window rect suggested by auto-show || None if
        // no repositioning is required.
        bool get_repositioned_window_rect(Rect& result, const LayoutView* view, const Rect& home,
                                          const std::vector<Rect>& limit_rects,
                                          const Border& test_clearance, const Border& move_clearance,
                                          bool horizontal=true, bool vertical=true);

    private:
        bool is_text_entry_active();

        bool can_hide_keyboard();
        bool can_show_keyboard();

        // Show the keyboard on click of an already focused text entry
        // (LP: 1078602). Do this only for single line text entries to
        // still allow clicking longer documents without having onboard show up.
        void on_text_caret_moved();
        void on_text_entry_activated(const UIElementPtr& entry);
        void on_tablet_mode_changed(bool active);
        void on_keyboard_device_detection_changed(bool detected);

        void handle_tablet_mode_changed(bool tablet_mode_active);

        // Begin AUTO_SHOW or AUTO_HIDE transition
        void show_keyboard(bool show, Noneable<double> delay={});

        bool begin_transition(bool show);

        Noneable<Point> find_close_position(const LayoutView* view, const Rect& home,
                                  const Rect& app_rect, const Rect& acc_rect,
                                  const std::vector<Rect>& limit_rects,
                                  const Border& test_clearance, const Border& move_clearance,
                                  bool horizontal=true, bool vertical=true);
        Noneable<Point> find_non_occluding_position(const LayoutView* view, const Rect& home,
                                          const Rect& acc_rect, const std::vector<Rect>& limit_rects,
                                          const Border& test_clearance, const Border& move_clearance,
                                          bool horizontal=true, bool vertical=true);

        bool on_lock_timer(const std::string& reason);

    private:
        // Delay from the last focus event until the keyboard is shown/hidden.
        // Raise it to reduce unnecessary transitions (flickering).
        // Lower it for more immediate reactions.
        const double m_show_reaction_time{0.0};
        const double m_hide_reaction_time{0.3};

        bool m_lock_visible = false;
        std::vector<std::pair<std::string, AutoShowLock>> m_locks;
        std::unique_ptr<Timer> m_auto_show_timer;

        UIElementPtr m_active_ui_element{};

        AtspiStateTracker* m_atspi_state_tracker{};
        HardwareSensorTracker* m_hw_sensor_tracker{};
        UDevTracker* m_udev_tracker{};

        std::shared_ptr<Timer> m_retired_timer;
};



#endif // AUTOSHOW_H
