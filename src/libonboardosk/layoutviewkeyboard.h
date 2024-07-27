#ifndef LAYOUTVIEWKEYBOARD_H
#define LAYOUTVIEWKEYBOARD_H

#include <chrono>

#include "handle.h"
#include "layoutview.h"
#include "manipulatable.h"
#include "onboardoskevent.h"


class TouchHandles;
class ViewManipulator;
class ViewManipulatorAspectRatio;

class LayoutViewKeyboard : public LayoutView, public Manipulatable
{
        friend class ViewManipulatorAspectRatio;
    public:
        using Super = LayoutView;
        using This = LayoutViewKeyboard;

        using SteadyClock = std::chrono::steady_clock;
        using SteadyTimePoint = std::chrono::time_point<SteadyClock>;

        LayoutViewKeyboard(const ContextBase& context,
                           const std::string& name_);
        virtual ~LayoutViewKeyboard();

        static std::unique_ptr<This> make(const ContextBase& context,
                                          const std::string& name);

        ViewManipulator* get_view_manipulator();

        virtual void show_touch_handles(bool show) override;
        virtual void set_touch_handles_active(bool active) override;
        virtual void reset_touch_handles() override;
        virtual void on_touch_handles_opacity(double opacity, bool done) override;

        virtual void start_move_view() override;
        virtual void stop_move_view() override;

    private:
        // InputSequenceTarget interface
        virtual void on_input_sequence_begin(const InputSequencePtr& sequence) override;
        virtual void on_input_sequence_update(const InputSequencePtr& sequence) override;
        virtual void on_input_sequence_end(const InputSequencePtr& sequence) override;

        virtual void on_enter_notify(const OnboardOskEvent* event) override;
        virtual void on_leave_notify(const OnboardOskEvent* event) override;

        // Manipulatable interface
        virtual Rect get_resize_frame_rect() override;

        virtual HandleFunction::Enum get_handle_function(Handle::Enum handle) override;

        virtual double get_drag_threshold() override;

        // Rectangle in canvas coordinates that must not leave the screen.
        virtual Rect get_always_visible_rect() override;

        virtual Handle::Enum hit_test_move_resize(const Point& point);

        // User controlled drag initiated, but drag hasn't actually begun yet.
        virtual void on_drag_initiated() override;

        // Moving/resizing has begun.
        virtual void on_drag_activated() override;

        // User controlled drag ended.
        virtual void on_drag_done() override;

        virtual std::vector<Rect> get_monitor_rects() override  // rect of each monitor
        {return ViewBase::get_monitor_rects();}

        virtual Point get_position() override   // in root window/stage coordinates
        {
            auto toplevel = find_toplevel_view_from_leaf();
            return toplevel->ViewBase::get_position();
        }
        virtual Size get_size() override   // in root window/stage coordinates
        {
            auto toplevel = find_toplevel_view_from_leaf();
            return toplevel->ViewBase::get_size();
        }
        virtual Point limit_position(const Point& pt) override
        {
            auto toplevel = find_toplevel_view_from_leaf();
            return toplevel->ViewBase::limit_position(pt);
        }
        virtual void set_cursor_type(OnboardOskCursorType cursor_type) override
        {
            auto toplevel = find_toplevel_view_from_leaf();
            toplevel->ViewBase::set_cursor_type(cursor_type);
        }
        virtual void move(const Point& pt) override
        {
            auto toplevel = find_toplevel_view_from_leaf();
            toplevel->ViewBase::move(pt);
        }
        virtual void move_resize(const Rect& r) override
        {
            auto toplevel = find_toplevel_view_from_leaf();
            toplevel->ViewBase::move_resize(r);
        }

    private:
        virtual AspectChangeRange get_docking_aspect_change_range() override;
        virtual void update_layout() override;
        virtual void draw(DrawingContext& dc) override;
        virtual void on_toplevel_geometry_changed() override;
        virtual void on_geometry_changed() override;

        void assure_on_current_desktop();

        virtual void on_transition_done(bool visible_before, bool visible_now) override;
        bool on_input_sequence_begin_delayed(const InputSequencePtr& sequence,
                                             Handle::Enum hit_handle = Handle::NONE);

        void update_touch_handles_positions();
        void update_double_click_time();

        bool overcome_initial_key_resistance(const InputSequencePtr& sequence);

        // Set/reset the cursor for frame resize handles
        void do_set_cursor_at(const Point& point, const LayoutKeyPtr& hit_key={});

        virtual void recalc_geometry() override;

        // Tell ViewManipulator about the active resize handles
        void update_window_handles();

        std::vector<Handle::Enum> get_active_drag_handles(bool all_handles=false);

    private:
        std::unique_ptr<ViewManipulatorAspectRatio> m_view_manipulator;
        std::unique_ptr<TouchHandles> m_touch_handles;
        int m_moved_to_desktop_count{0};
        int m_desktop_switch_count{0};

        OnboardOskEventTime m_double_click_time;  // in ms
        OnboardOskEventTime m_last_click_time;
        LayoutKeyPtr m_last_click_key;
};

#endif // LAYOUTVIEWKEYBOARD_H
