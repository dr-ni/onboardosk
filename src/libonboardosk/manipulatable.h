#ifndef MANIPULATABLE_H
#define MANIPULATABLE_H

#include "tools/rect_decl.h"
#include "handle.h"
#include "onboardoskview.h"


class HandleFunction
{
    public:
        enum Enum
        {
            NORMAL,
            ASPECT_RATIO,
        };
};

// Interface for objects moveable and or resizable by ViewManipulator
class Manipulatable
{
    public:
        Manipulatable() {}
        virtual ~Manipulatable() {}

        //virtual Manipulatable* get_top_level_manipulatable() = 0;

        // outer rect of the frame including resize handles
        virtual Rect get_resize_frame_rect() = 0;

        virtual HandleFunction::Enum get_handle_function(Handle::Enum handle) = 0;
        // { (void)handle; return HandleFunction::NORMAL; }

        virtual double get_drag_threshold() = 0;
        // { return 8; }

        // Rectangle in canvas coordinates that must not leave the screen.
        virtual Rect get_always_visible_rect() = 0;

        virtual Handle::Enum hit_test_move_resize(const Point& point) = 0;

        // User controlled drag initiated, but drag hasn't actually begun yet.
        virtual void on_drag_initiated() = 0;

        // Moving/resizing has begun.
        virtual void on_drag_activated() = 0;

        // User controlled drag ended.
        virtual void on_drag_done() = 0;

        virtual Point get_position() = 0;   // in root window/stage coordinates
        virtual Size get_size() = 0;   // in root window/stage coordinates
        virtual Point limit_position(const Point& pt) = 0;
        virtual void set_cursor_type(OnboardOskCursorType cursor_type) = 0;
        virtual std::vector<Rect> get_monitor_rects() = 0;  // rect of each monitor

        virtual void move(const Point& pt) = 0;
        virtual void move_resize(const Rect& r) = 0;
};

#endif // MANIPULATABLE_H
