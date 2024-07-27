#ifndef KEVBOARDVIEW_H
#define KEVBOARDVIEW_H

#include <chrono>
#include <queue>

#include "tools/noneable.h"
#include "tools/rect_fwd.h"

#include "keyboard.h"
#include "keyboardpopup_fwd.h"
#include "onboardoskcallbacks.h"
#include "onboardoskevent.h"
#include "onboardoskglobals.h"
#include "toplevelview.h"

class AutoHide;
class AutoShow;
class InactivityTimer;
class FadeTimer;
class KeyboardAnimator;
class KeyboardKeyLogic;
class Timer;
class TouchFeedback;
class ViewRectPersist;

// Class representing the bounding rectangle of all layout views.
// Toolkits may or may not decide to assign an actual widget to
// the KeyboardView. There could just as well be only widgets for
// the individual LayoutViews of a layout.
class KeyboardView : public ViewBase
{
    public:
        using Super = ViewBase;
        using This = KeyboardView;

        KeyboardView(const ContextBase& context, const std::string& name_);
        virtual ~KeyboardView();

        static std::unique_ptr<This> make(const ContextBase& context,
                                          const std::string& name);

        KeyboardAnimator* get_animator() {return m_animator.get();}
        KeyboardKeyLogic* get_key_logic();

        // Highest level visibility change for direct user interaction.
        // Start of transition may be delayed until all keys have
        // been released.
        // Main method to toggle visibility manually.
        void request_visibility(bool visible);
        void request_visibility_toggle();

        bool is_visible();

        // Interactive visibility change with transition.
        // Transition starts immediately.
        // Locks auto-show visible (if visible==true).
        void set_visible_interactive(bool visible);

        // Start transition that leads to visibility change.
        void set_visible_with_transition(bool visible);

        // Low level immediate visibility change of toplevel windows/actors.
        virtual void set_visible(bool visible) override;

        void set_startup_visibility();

        // Lock all showing/hiding, but remember requests to do so.
        void lock_visibility();

        // Unlock all showing/hiding.
        void unlock_visibility();

        // Unlock all showing/hiding and apply the last request to do so.
        void unlock_and_apply_visibility();

        bool set_opacity(double opacity);
        double get_opacity();

        // Updates transparencies in response to user action.
        // Temporarily presents the window with active transparency when
        // inactive transparency is enabled.
        void update_transparency();

        void begin_inactivity_timer_transition(bool active);

        void on_transition_done(bool visible_before, bool visible_now);

        // auto-show, start repositioning
        void auto_position();
        void stop_auto_positioning();

        bool is_auto_show_locked(const std::string& reason);
        void auto_show_lock(const std::string& reason, Noneable<double> duration={},
                           bool lock_show=true, bool lock_hide=true);
        // Remove a specific lock named by "reason".
        void auto_show_unlock(const std::string& reason);

        // Remove lock and apply the last requested auto-show state while the
        // lock was applied.
        void auto_show_unlock_and_apply_visibility(const std::string& reason);

        // Helper for locking auto-show from AutoHide (hide-on-key-press)
        // and D-Bus property.
        void auto_show_lock_and_hide(const std::string& reason,
                                     Noneable<double> duration={});

        // If the user unhides onboard, don't auto-hide it until
        // he manually hides it again.
        void auto_show_lock_visible(bool visible);

        // poll for mouse click outside of onboards window
        void start_click_polling();
        void stop_click_polling();

        bool is_dwelling();
        bool already_dwelled(const LayoutItemPtr& key);
        void reset_already_dwelled();
        void start_dwelling(LayoutView* view, const LayoutKeyPtr& key);
        void cancel_dwelling();
        void stop_dwelling();

        void maybe_start_dwelling(LayoutView* view, const LayoutKeyPtr& key, const Point& point);
        void maybe_cancel_dwelling(const LayoutKeyPtr& key);

        LayoutPopup* get_key_popup();
        void set_key_popup(const LayoutPopupPtr& popup);
        void close_key_popup();

        virtual void redraw() override;
        void redraw_item(const LayoutItemPtr& key, bool invalidate=false);
        void redraw_items(const vector<LayoutItemPtr>& keys, bool invalidate=false);

        virtual void draw(DrawingContext& dc);

        virtual void on_event(OnboardOskEvent* event) override;
        bool has_input_sequences();

        void update_docking();
        bool update_docking_monitor_index();

        void update_home_rect(bool update_home_position=true);

        void restore_view_rect(bool startup = false);

        // Get the un-repositioned rect, the one auto-show falls back on
        // when there is nowhere else to move.
        Rect get_home_rect();

        // Returns the rect of the visible window with auto-show
        // repositioning taken into account.
        Rect get_visible_rect();

        // Where the keyboard by default goes when docked. It may
        // then still be moved elsewhere by auto-show or user action.
        Rect get_dock_rect();

        // Where the keyboard goes to hide when it slides off-screen.
        Rect get_docking_hideout_rect(const Rect& reference_rect={});

        // Rectangle of the potentially aspect-corrected
        // frame around the layout in canvas coordinates.
        Rect get_keyboard_frame_rect();

        // Base aspect ratio of the keyboard.
        // Really only makes sense with a single LayoutViewKeyboard.
        Rect get_base_aspect_rect();

        // Width of the frame around the keyboard; canvas coordinates.
        double get_frame_width();

        bool are_touch_handles_active();

        // Show/hide the enlarged resize/move handels.
        // Initiates an opacity fade.
        void show_touch_handles(bool show = true, bool auto_hide = true);
        void reset_touch_handles();

        void start_touch_handles_auto_hide();
        void stop_touch_handles_auto_hide();

        void show_touch_feedback(const LayoutKeyPtr key, const LayoutView* view);
        void hide_touch_feedback(const LayoutKeyPtr key={});

        void on_user_positioning_begin();
        void on_user_positioning_done(bool was_moving);

        // Move the window from a transition, not meant for user positioning.
        void reposition(const Point& pt);

        virtual void recalc_geometry() override;

    protected:
        // Returns the bounding rectangle of all move buttons
        // in canvas coordinates.
        // Overload for WindowManipulator
        virtual Rect get_always_visible_rect() override;

    private:
        bool on_dwell_update_timer();

        virtual void on_toplevel_geometry_changed() override;
        virtual void on_geometry_changed() override;

        bool on_auto_position_poll(std::chrono::milliseconds delay);
        void update_position(bool commit=true);
        bool get_repositioned_window_rect(Rect& result, const LayoutView* view, const Rect& home_rect);
        bool get_auto_show_repositioned_window_rect(Rect& result, const LayoutView* view, const Rect& home, const vector<Rect>& limit_rects,
                                                    const Border& test_clearance, const Border& move_clearance,
                                                    bool horizontal=true, bool vertical=true);
        void get_repositioning_constraints(bool& horizontal, bool& vertical);

        void update_auto_show_on_visibility_change(bool visible);

        bool on_click_polling_timer();
        MouseButton::Enum get_button_from_mask(OnboardOskStateMask mask);

        void get_docking_monitor_rects(Rect& area, Rect& geom);
        int get_docking_monitor_index(bool force_update=false);
        void reset_monitor_workarea();

        // Save the workarea, so we don't have to
        // check all the time if our strut is already installed.
        Rect update_monitor_workarea(int monitor_index);
        Rect get_monitor_workarea(int monitor_index);

        void realize_docking(bool enable);
        void clear_docking_struts();
        void set_docking_struts(bool enable, DockingEdge::Enum edge={}, bool expand=true);

        Size get_min_window_size();

        // Returns the rect of the hidden window with auto-show
        // repositioning taken into account.
        Rect get_hidden_rect();

        Rect get_current_rect();

        // Overload for WindowRectPersist.
        virtual Rect on_restore_view_rect(const Rect& rect) override;

        // Overload for WindowRectPersist.
        virtual Rect on_save_view_rect(const Rect& rect) override;

        // Read orientation dependent rect.
        // Overload for WindowRectPersist.
        virtual Rect read_window_rect(bool is_landscape) override;

        // Write orientation dependent rect.
        // Overload for WindowRectPersist.
        virtual void write_window_rect(bool is_landscape, const Rect& rect) override;

        void write_docking_size(const Size& size);

        void set_touch_handles_active(bool active);
        void on_touch_handles_opacity(double opacity, bool done);

        bool can_move_into_view();

        // Remember the last 3 rectangles of auto-show repositioning.
        // Time and order of configure events is somewhat unpredictable,
        // so don't rely only on a single remembered rect.
        void remember_rect(const Rect& rect);

        void ignore_configure_events();

    private:
        using SteadyClock = std::chrono::steady_clock;
        using SteadyTimePoint = std::chrono::time_point<SteadyClock>;

        static constexpr const char* LOCK_REASON_KEY_PRESSED = "key-pressed";
        static constexpr const char* LOCK_REASON_USER_POSITIONING = "user-positioning";

        friend class InactivityTimer;
        std::unique_ptr<ViewRectPersist> m_view_rect_persist;
        std::unique_ptr<KeyboardAnimator> m_animator;
        std::unique_ptr<AutoShow> m_auto_show;
        std::unique_ptr<AutoHide> m_auto_hide;
        std::unique_ptr<InactivityTimer> m_inactivity_timer;

        LayoutPopupPtr m_key_popup;

        Rect m_user_positioning_begin_rect;

        bool m_visible{false};

        bool m_visibility_locked{false};
        Noneable<bool> m_visibility_requested;

        double m_opacity{1.0};

        std::unique_ptr<Timer> m_dwell_update_timer;
        LayoutView* m_dwell_view{};
        LayoutKeyPtr m_dwell_key;
        LayoutKeyPtr m_last_dwelled_key;
        Point        m_dwell_end_point;

        std::unique_ptr<Timer> m_auto_position_poll_timer;
        bool m_auto_position_started{false};
        SteadyTimePoint m_poll_auto_position_start_time{};

        std::unique_ptr<Timer> m_outside_click_timer;
        bool m_outside_click_detected{false};
        SteadyTimePoint m_outside_click_start_time{};
        size_t m_outside_click_num{0};
        OnboardOskStateMask m_outside_click_button_mask;

        Rect m_home_rect;

        bool m_docking_enabled{false};
        DockingEdge::Enum m_docking_edge{DockingEdge::BOTTOM};
        Rect m_docking_rect;
        int m_docking_monitor_index{0};
        bool m_shrink_work_area{true};
        bool m_dock_expand{false};

        Noneable<int> m_current_docking_monitor_index;
        vector<double> m_current_struts;

        SteadyTimePoint m_realize_docking_time;

        std::map<int, Rect> m_monitor_workarea;

        std::unique_ptr<Timer> m_touch_handles_hide_timer;
        std::unique_ptr<FadeTimer> m_touch_handles_fade;
        bool m_touch_handles_active{false};
        bool m_touch_handles_auto_hide{false};

        std::unique_ptr<TouchFeedback> m_touch_feedback;

        std::queue<Rect> m_known_view_rects;
        SteadyTimePoint m_last_ignore_configure_time{};
};

#endif // KEVBOARDVIEW_H
