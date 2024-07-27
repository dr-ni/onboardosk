#include <array>
#include <map>

#include "tools/noneable.h"
#include "tools/container_helpers.h"

#include "inputsequence.h"
#include "viewmanipulator.h"

using std::array;


static const std::map<Handle::Enum, OnboardOskCursorType> handle_to_cursor_type_map {
    {Handle::NORTH_WEST, ONBOARD_OSK_CURSOR_NW_RESIZE},
    {Handle::NORTH, ONBOARD_OSK_CURSOR_NORTH_RESIZE},
    {Handle::NORTH_EAST, ONBOARD_OSK_CURSOR_NE_RESIZE},
    {Handle::WEST, ONBOARD_OSK_CURSOR_WEST_RESIZE},
    {Handle::EAST, ONBOARD_OSK_CURSOR_EAST_RESIZE},
    {Handle::SOUTH_WEST, ONBOARD_OSK_CURSOR_SW_RESIZE},
    {Handle::SOUTH, ONBOARD_OSK_CURSOR_SOUTH_RESIZE},
    {Handle::SOUTH_EAST, ONBOARD_OSK_CURSOR_SE_RESIZE},
    {Handle::MOVE, ONBOARD_OSK_CURSOR_MOVE_OR_RESIZE_WINDOW},
    {Handle::NONE, ONBOARD_OSK_CURSOR_DEFAULT},
};

ViewManipulator::ViewManipulator(const ContextBase& context, Manipulatable* view) :
    Super(context),
    m_view(view)
{

}

ViewManipulator::~ViewManipulator()
{

}

void ViewManipulator::set_min_window_size(const Size& sz)
{
    m_min_view_size = sz;
}

Size ViewManipulator::get_min_view_size()
{
    return m_min_view_size;
}

double ViewManipulator::get_hit_frame_width()
{
    return m_hit_frame_width;
}

void ViewManipulator::enable_drag_protection(bool enable)
{
    m_drag_protection = enable;
}

void ViewManipulator::reset_drag_protection()
{
    m_temporary_unlock_time = {};
}

Rect ViewManipulator::get_resize_frame_rect()
{
    return m_view->get_resize_frame_rect();
}

Rect ViewManipulator::get_drag_start_rect()
{
    return m_drag_start_rect;
}

std::vector<Handle::Enum>& ViewManipulator::get_drag_handles()
{
    return m_drag_handles;
}

void ViewManipulator::set_drag_handles(const std::vector<Handle::Enum>& handles)
{
    m_drag_handles = handles;
}

HandleFunction::Enum ViewManipulator::get_handle_function(Handle::Enum handle)
{
    return m_view->get_handle_function(handle);
}

void ViewManipulator::lock_x_axis(bool lock)
{
    m_lock_x_axis = lock;
}

void ViewManipulator::lock_y_axis(bool lock)
{
    m_lock_y_axis = lock;
}

bool ViewManipulator::handle_press(const InputSequencePtr& sequence, bool move_on_background)
{
    Handle::Enum hit = hit_test_move_resize(sequence->point);
    if (hit != Handle::NONE)
    {
        if (hit == Handle::MOVE)
        {
            start_move(sequence->root_point);
        }
        else
        {
            start_resize(hit, sequence->root_point);

            auto function = get_handle_function(hit);
            if (function == HandleFunction::ASPECT_RATIO)
                on_handle_aspect_ratio_pressed();
        }

        return true;
    }

    if (move_on_background &&
        contains(get_drag_handles(), Handle::MOVE))
    {
        start_move(sequence->root_point);
        return true;
    }

    return false;
}

void ViewManipulator::handle_motion(const InputSequencePtr& sequence)
{
    Point pt = sequence->root_point;

    if (is_drag_requested() && !is_drag_initiated())
        start_drag(pt);

    if (!is_drag_initiated())
        return;

    Offset delta = pt - m_drag_start_point;

    // distance threshold, protection from accidental drags
    if (!m_drag_active)
    {
        double d = delta.length();
        bool drag_active = !m_drag_protection;

        if (m_drag_protection)
        {
            // snap off for temporary unlocking
            if (m_temporary_unlock_time == SteadyTimePoint{} &&
                d >= m_drag_threshold)
            {
                m_temporary_unlock_time = SteadyTimePoint{} + std::chrono::seconds(1);

                // Snap to cursor position for large drag thresholds
                // Dragging is smoother without snapping, but for large
                // thresholds, the cursor ends up far away from the
                // window && there is a danger of windows going offscreen.
                if (d >= m_drag_snap_threshold)
                {
                    // do nothing
                }
                else
                {
                    m_drag_start_offset += delta;
                }
            }

            if (m_temporary_unlock_time != SteadyTimePoint{})
            {
                drag_active = true;
            }
        }
        else
        {
            // unlock for touch handles too
            m_temporary_unlock_time = SteadyTimePoint{} + std::chrono::seconds(1);
        }

        m_drag_active |= drag_active;
    }

    // move/resize
    if (m_drag_active)
    {
        handle_motion_fallback(delta);

        // give keyboard window a chance to react
        m_view->on_drag_activated();
    }
}

void ViewManipulator::handle_motion_fallback(Offset delta)
{
    if (!is_drag_initiated())
        return;

    auto function = get_handle_function(m_drag_handle);
    if (function == HandleFunction::ASPECT_RATIO)
    {
        if (m_drag_handle == Handle::WEST)
            delta.x *= -1;

        on_handle_aspect_ratio_motion(delta);
    }
    else
    {
        Point pt;
        Noneable<Size> sz;

        Point p = m_drag_start_point + delta - m_drag_start_offset;

        if (m_drag_handle == Handle::MOVE)
        {
            // contrain axis movement
            if (m_lock_x_axis)
                p.x = m_view->get_position().x;
            if (m_lock_y_axis)
                p.y = m_view->get_position().y;

            // move window
            pt = m_view->limit_position(p);
            sz.set_none();
        }
        else
        {
            // resize window
            Size szmin = get_min_view_size();
            Rect rect = m_drag_start_rect;
            Point p0 = rect.left_top();
            Point p1 = rect.right_bottom();
            sz = rect.get_size();

            if (contains(array<Handle::Enum, 3>{Handle::NORTH,
                                                Handle::NORTH_WEST,
                                                Handle::NORTH_EAST},
                         m_drag_handle))
            {
                p0.y = std::min(p.y, p1.y - szmin.h);
            }
            if (contains(array<Handle::Enum, 3>{Handle::WEST,
                                                Handle::NORTH_WEST,
                                                Handle::SOUTH_WEST},
                         m_drag_handle))
            {
                p0.x = std::min(p.x, p1.x - szmin.w);
            }
            if (contains(array<Handle::Enum, 3>{Handle::EAST,
                                                Handle::NORTH_EAST,
                                                Handle::SOUTH_EAST},
                         m_drag_handle))
            {
                p1.x = std::max(p.x + sz.value.w, p0.x + szmin.w);
            }
            if (contains(array<Handle::Enum, 3>{Handle::SOUTH,
                                                Handle::SOUTH_WEST,
                                                Handle::SOUTH_EAST},
                         m_drag_handle))
            {
                p1.y = std::max(p.y + sz.value.h, p0.y + szmin.h);
            }

            p = {p0.x, p0.y};
            sz = {p1.x -p0.x, p1.y - p0.y};
        }

        move_resize(p, sz);
    }
}

void ViewManipulator::set_drag_cursor_at(const Point& point, bool allow_drag_cursors)
{
    OnboardOskCursorType cursor_type = ONBOARD_OSK_CURSOR_NONE;
    if (allow_drag_cursors ||
        m_drag_handle != Handle::NONE)  // already dragging a handle?
    {
        cursor_type = get_drag_cursor_at(point);
    }

    // set/reset cursor
    m_view->set_cursor_type(cursor_type);
}

void ViewManipulator::reset_drag_cursor()
{
    if (m_drag_handle == Handle::NONE)   // not currently dragging a handle?
        m_view->set_cursor_type(ONBOARD_OSK_CURSOR_DEFAULT);
}

OnboardOskCursorType ViewManipulator::get_drag_cursor_at(const Point& point)
{
    Handle::Enum hit = m_drag_handle;
    if (hit == Handle::NONE)
        hit = hit_test_move_resize(point);

    if ((hit != Handle::NONE &&
        hit != Handle::MOVE) || is_drag_active())   // delay it for move
        return handle_to_cursor_type_map.at(hit);
    return ONBOARD_OSK_CURSOR_NONE;
}

void ViewManipulator::move_resize(const Point& pt, const Size& sz)
{
    //print("move_resize", x, y, w, h)
    if (sz.w == 0.0 && sz.h == 0.0)
        m_view->move(pt);
    else
        m_view->move_resize({pt, sz});
}

void ViewManipulator::start_move()
{
    m_drag_requested = true;
    m_drag_handle = Handle::MOVE;
    m_last_drag_handle = m_drag_handle;
}

void ViewManipulator::start_move(const Point& point)
{
    start_drag(point);
    m_drag_handle = Handle::MOVE;
    m_last_drag_handle = m_drag_handle;
}

void ViewManipulator::stop_move()
{
    stop_drag();
}

void ViewManipulator::start_resize(Handle::Enum handle, const Point& point)
{
    start_drag(point);
    m_drag_handle = handle;
    m_last_drag_handle = m_drag_handle;
}

void ViewManipulator::start_drag(const Point& point)
{
    // remember pointer and view positions
    Point ptv = m_view->get_position();
    m_drag_start_point = point;
    m_drag_start_offset = point - ptv;
    m_drag_start_rect = Rect::from_position_size(m_view->get_position(),
                                                 m_view->get_size());
    // not yet actually moving the window
    m_drag_requested = true;
    m_drag_initiated = true;
    m_drag_active = false;

    // get the threshold
    m_drag_threshold = m_view->get_drag_threshold();

    // check if the temporary threshold unlocking has expired
    if (!m_drag_protection ||
        (m_temporary_unlock_time != SteadyTimePoint{} &&
         SteadyClock::now() - m_temporary_unlock_time >
         m_temporary_unlock_delay))
    {
        m_temporary_unlock_time = {};
    }

    // give view a chance to react
    m_view->on_drag_initiated();
}

void ViewManipulator::stop_drag()
{
    if (is_drag_initiated())
    {
        if (m_temporary_unlock_time == SteadyTimePoint{})
        {
            // snap back to start position
            if (m_drag_protection)
            {
                move_resize(m_drag_start_rect.get_position(),
                            m_drag_start_rect.get_size());
            }
        }
        else
        {
            // restart the temporary unlock period
            m_temporary_unlock_time = SteadyClock::now();
        }

        m_drag_start_offset = {};
        m_drag_handle = Handle::NONE;
        m_drag_requested = false;
        m_drag_initiated = false;
        m_drag_active = false;

        move_into_view();

        // give keyboard window a chance to react
        on_drag_done();
    }
}

bool ViewManipulator::is_drag_requested()
{
    return m_drag_requested;
}

bool ViewManipulator::is_drag_initiated()
{
    return m_drag_initiated;
}

bool ViewManipulator::is_drag_active()
{
    return this->is_drag_initiated() && m_drag_active;
}

bool ViewManipulator::is_moving()
{
    return is_drag_initiated() && m_drag_handle == Handle::MOVE;
}

bool ViewManipulator::was_moving()
{
    return m_last_drag_handle == Handle::MOVE;
}

bool ViewManipulator::is_resizing()
{
    return is_drag_initiated() && m_drag_handle  != Handle::MOVE;
}

void ViewManipulator::move_into_view()
{
    Point pt = m_view->get_position();
    Point ptl = m_view->limit_position(pt);
    if (pt != ptl)
        move_resize(ptl);
}

Handle::Enum ViewManipulator::hit_test_move_resize(const Point& point)
{
    auto hit_handle = m_view->hit_test_move_resize(point);
    if (hit_handle != Handle::NONE)
        return hit_handle;

    Rect canvas_rect = m_view->get_resize_frame_rect();
    auto& handles = get_drag_handles();
    double hit_frame_width = get_hit_frame_width();

    double w = std::min(canvas_rect.w / 2, hit_frame_width);
    double h = std::min(canvas_rect.h / 2, hit_frame_width);

    double x = point.x;
    double y = point.y;
    double x0 = canvas_rect.left();
    double y0 = canvas_rect.top();
    double x1 = canvas_rect.right();
    double y1 = canvas_rect.bottom();

    // try corners first
    for (auto handle : handles)
    {
        if (handle == Handle::NORTH_WEST)
        {
            if (x >= x0 && x < x0 + w &&
                y >= y0 && y < y0 + h)
                return handle;
        }

        if (handle == Handle::NORTH_EAST)
        {
            if (x <= x1 && x > x1 - w &&
                y >= y0 && y < y0 + h)
                return handle;
        }

        if (handle == Handle::SOUTH_EAST)
        {
            if (x <= x1 && x > x1 - w &&
                y <= y1 && y > y1 - h)
                return handle;
        }

        if (handle == Handle::SOUTH_WEST)
        {
            if (x >= x0 && x < x0 + w &&
                y <= y1 && y > y1 - h)
                return handle;
        }
    }

    // then check the edges
    for (auto handle : handles)
    {
        if (handle == Handle::WEST)
        {
            if (x < x0 + w && x >= x0 - 1)
                return handle;
        }
        if (handle == Handle::EAST)
        {
            if (x > x1 - w && x <= x1 + 1)
                return handle;
        }
        if (handle == Handle::NORTH)
        {
            if (y < y0 + h)
                return handle;
        }
        if (handle == Handle::SOUTH)
        {
            if (y > y1 - h)
                return handle;
        }
    }

    return Handle::NONE;
}

void ViewManipulator::on_drag_done()
{
    m_view->on_drag_done();
}

