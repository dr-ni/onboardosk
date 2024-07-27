#ifndef LAYOUTSCROLLEDPANEL_H
#define LAYOUTSCROLLEDPANEL_H

#include <chrono>
#include <limits>
#include <memory>

#include "tools/noneable.h"

#include "layoutdrawingitem.h"


class Timer;

// LayoutPanel with inertial scrolling.
// get_border_rect(): size of the  panel
// _scroll_rect: extends of the area to be scrolled, logical coordinates
class LayoutScrolledPanel : public LayoutPanel
{
    public:
        using Super = LayoutPanel;
        using Clock = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;
        static constexpr const double NONE = std::numeric_limits<double>::max();

        LayoutScrolledPanel(const ContextBase& context);
        ~LayoutScrolledPanel();

        static constexpr const char* CLASS_NAME = "LayoutScrolledPanel";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual void dump(std::ostream& out) const override;

        virtual void update_log_rect() override
        {
            // do not calculate bounds from content
        }

        // Set size of the virtual area to be scrolled over.
        // Logical coordinates.
        void set_scroll_rect(const Rect& rect);

        // Set scrolled position. Logical coordinates.
        void set_scroll_offset(const Offset& offset);

        const Offset& get_scroll_offset();

        void stop_scrolling();

        // Set to false to constraint movement in x.
        void lock_x_axis(bool lock);

        // Set to true to constraint movement in y.
        void lock_y_axis(bool lock);

        void set_damage(const Rect& scrolled_log_rect);
        virtual void on_damage(const Rect& scrolled_log_rect) = 0;

        virtual bool on_input_sequence_begin(const InputSequencePtr& sequence) override;
        virtual bool on_input_sequence_update(const InputSequencePtr& sequence) override;
        virtual bool on_input_sequence_end(const InputSequencePtr& sequence) override;

        void drag_initiate(const InputSequencePtr& sequence);
        void drag_activate();
        bool drag_cancel(const InputSequencePtr& sequence);
        void drag_update(const InputSequencePtr& sequence);
        void drag_end();

        // Sequence begin received, but not yet actually dragging.
        bool is_drag_initiated();

        // Are we actually dragging?
        bool is_drag_active();

        // Has gesture been cancelled before it could become active?
        bool is_drag_cancelled();

        void start_animation();
        void stop_animation();
        bool step_scroll_position();

        // only called asynchronously in timer when scroll offset changed
        virtual void update_contents_on_scroll();

        void update_contents();

        virtual void on_scroll_offset_changed()
        {}

        // Portion of the virtual scroll rect visible in the Panel.
        // In logical coordinates.
        Rect get_visible_scrolled_rect();

        LayoutContext* get_scrolled_context()
        {
            return m_scrolled_context.get();
        }

        // Translate children.
        virtual void do_fit_inside_canvas(const Rect& canvas_border_rect) override;

        virtual bool is_background_at(const Point& log_point) {(void) log_point; return false;}
        virtual void draw_item(DrawingContext& dc) override;

    private:
        Rect m_scroll_rect;         // area to be scrolled, logical coordinates
        Offset m_scroll_offset;     // logical coordinate

        Noneable<Point> m_drag_begin_point;
        Offset m_drag_begin_scroll_offset;
        TimePoint m_drag_begin_time;
        bool m_drag_active{false};
        bool m_drag_cancelled{false};
        std::unique_ptr<Timer> m_step_timer;
        std::unique_ptr<Timer> m_cancel_timer;

        bool m_lock_x_axis{false};
        bool m_lock_y_axis{false};

        std::unique_ptr<LayoutContext> m_scrolled_context;

        Noneable<TimePoint> m_last_step_time;
        Offset m_target_offset{NONE, NONE};
        Vec2 m_acceleration;
        Vec2 m_velocity;
        Vec2 m_dampening;
};


#endif // LAYOUTSCROLLEDPANEL_H
