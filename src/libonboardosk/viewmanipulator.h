#ifndef VIEWMANIPULATOR_H
#define VIEWMANIPULATOR_H

#include <chrono>
#include <vector>

#include "tools/point_decl.h"
#include "tools/rect_decl.h"

#include "inputsequencedecls.h"
#include "layoutdecls.h"
#include "onboardoskglobals.h"
#include "manipulatable.h"


class ViewManipulator : public ContextBase
{
    public:
        using Super = ContextBase;
        using SteadyClock = std::chrono::steady_clock;
        using SteadyTimePoint = std::chrono::time_point<SteadyClock>;

        ViewManipulator(const ContextBase& context, Manipulatable* view);
        virtual ~ViewManipulator();

        void set_min_window_size(const Size& sz);
        Size get_min_view_size();

        double get_hit_frame_width();

        void enable_drag_protection(bool enable);
        void reset_drag_protection();

        Rect get_resize_frame_rect();

        Rect get_drag_start_rect();

        std::vector<Handle::Enum>& get_drag_handles();
        void set_drag_handles(const std::vector<Handle::Enum>& handles);

        HandleFunction::Enum get_handle_function(Handle::Enum handle);

        // Set to false to constraint movement in x.
        void lock_x_axis(bool lock);

        // Set to true to constraint movement in y.
        void lock_y_axis(bool lock);

        bool handle_press(const InputSequencePtr& sequence,
                          bool move_on_background = false);

        void handle_motion(const InputSequencePtr& sequence);

        // set the mouse cursor
        void set_drag_cursor_at(const Point& point, bool allow_drag_cursors=true);

        // reset mouse cursor to the default
        void reset_drag_cursor();

        void start_move();
        void stop_move();

        // waiting for initial motion event
        bool is_drag_requested();

        // Button pressed down on a drag handle, not yet actually dragging
        bool is_drag_initiated();

        // Are we actually moving/resizing
        bool is_drag_active();

        bool is_moving();
        bool was_moving();
        bool is_resizing();

        void stop_drag();

        // Overload this to process start of dragging of
        // handles with ASPECT_RATIO function.
        virtual void on_handle_aspect_ratio_pressed() {}

        // Overload this to process motion changes of
        // handles with ASPECT_RATIO function.
        virtual void on_handle_aspect_ratio_motion(const Offset& delta) {(void)delta;}

        virtual void on_drag_done();

    private:
        // handle dragging for window move and resize
        void handle_motion_fallback(Offset delta);

        void start_move(const Point& point);
        void start_resize(Handle::Enum handle, const Point& point={});

        // When dragging is not being started from an event, but e.g. the move button
        // leave the point at none. We will get the actual pointer position from the first
        // motion event.
        void start_drag(const Point& point={});

        // If the window has somehow ended up off-screen,
        // move the always-visible-rect back into view.
        void move_into_view();

        // Limit the given window rect to fit on screen.
        Rect limit_size(const Rect& rect);

        Handle::Enum hit_test_move_resize(const Point& point);

        OnboardOskCursorType get_drag_cursor_at(const Point& point);
        void do_set_cursor_at(const Point& point, const LayoutKeyPtr& hit_key) {(void)point; (void)hit_key;}

        void move_resize(const Point& pt, const Size& sz={});

    protected:
        Manipulatable* m_view{};

    private:

        double m_hit_frame_width{10.0};         // size of resize corners and edges
        bool m_drag_protection{true};                // enable protection threshold
        SteadyTimePoint m_temporary_unlock_time{};

        // seconds until protection threshold returns
        // counts from drag end in fallback mode
        std::chrono::milliseconds m_temporary_unlock_delay{std::chrono::seconds(6)};

        Size m_min_view_size{50.0, 50.0};

        Point m_drag_start_point;
        Offset m_drag_start_offset;
        Rect m_drag_start_rect{};
        Handle::Enum m_drag_handle{Handle::NONE};
        std::vector<Handle::Enum> m_drag_handles{Handle::ALL.cbegin(), Handle::ALL.cend()};
        bool m_drag_requested{false};   // waiting for initial motion event
        bool m_drag_initiated{false};   // have start point now, can begin
        bool m_drag_active{false};      // has window move/resize actually started yet?
        double m_drag_threshold{8.0};
        double m_drag_snap_threshold{16.0};

        bool m_lock_x_axis{false};
        bool m_lock_y_axis{false};

        Handle::Enum m_last_drag_handle{Handle::NONE};
};

#endif // VIEWMANIPULATOR_H
