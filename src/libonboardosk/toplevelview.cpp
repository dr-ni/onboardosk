#include <cassert>

#include "tools/container_helpers.h"
#include "tools/noneable.h"

#include "onboardoskcallbacks.h"
#include "toplevelview.h"


ViewBase::ViewBase(const ContextBase& context,
                   OnboardOskViewType view_type_,
                   const std::string& name_) :
    ContextBase(context)
{
    this->ooc = get_cinstance();
    this->name = nullptr;
    this->view_type = view_type_;  // must be changed later
    this->user_data = nullptr;

    set_name(name_);
}

ViewBase::~ViewBase()
{
    this->destroy_notify.emit();   // notify listeners of destruction
}

void ViewBase::destroy()
{
    notify_toplevel_remove();
    remove_toplevel_view(this);
}

void ViewBase::set_name(const std::string& name_)
{
    this->m_name = name_;
    this->name = m_name.data();
}

OnboardOskViewType ViewBase::get_view_type() const
{
    return this->view_type;
}

ViewBase* ViewBase::find_view_at_point(const Point& point)
{
    if (get_canvas_rect().contains(point))
    {
        for (auto& child : reversed(get_children()))
        {
            auto view = child->find_view_at_point(
                            child->parent_to_canvas(point));
            if (view)
                return view;
        }
        return this;
    }
    return {};
}

void ViewBase::notify_toplevel_added()
{
    auto callbacks = get_global_callbacks();
    if (callbacks->on_top_level_view_added)
        callbacks->on_top_level_view_added(get_cinstance(), this);
}

void ViewBase::notify_toplevel_remove()
{
    auto callbacks = get_global_callbacks();
    if (callbacks->on_top_level_view_remove)
        callbacks->on_top_level_view_remove(get_cinstance(), this);
}

Point ViewBase::get_position()   // in root window/stage coordinates
{return m_rect.get_position();}

Size ViewBase::get_size()   // in root window/stage coordinates
{return m_rect.get_size();}

Rect ViewBase::get_limit_rect()
{
    auto callbacks = get_global_callbacks();
    if (callbacks->get_screen_geometry)
    {
        Rect r;
        callbacks->get_screen_geometry(get_cinstance(), &r.x, &r.y, &r.w, &r.h);
        return r;
    }
    return {};
}

Point ViewBase::limit_position(const Point& pt)
{
    // rect to stay always visible, in canvas coordinates
    Rect visible_rect = get_always_visible_rect();
    const std::vector<Rect> rects = get_monitor_rects();
    return limit_position(pt, visible_rect, rects);
}

Point ViewBase::limit_position(const Point& pt,
                               const Rect& always_visible_rect,
                               const std::vector<Rect>& limit_rects)
{
    if (always_visible_rect.empty() ||
        limit_rects.empty())
        return pt;

    // rect to stay always visible, in canvas coordinates
    Rect r = always_visible_rect;
    r = r.floor();  // avoid rounding errors

    // transform always visible rect to screen coordinates,
    // take window decoration into account.
    Rect rs = r;
    rs.x += pt.x;
    rs.y += pt.y;

    Noneable<double> dmin;
    Rect rsmin;
    for (auto& limits : limit_rects)
    {
        // get limited candidate rect
        Rect rsc = rs;
        rsc.x = std::max(rsc.x, limits.left());
        rsc.x = std::min(rsc.x, limits.right() - rsc.w);
        rsc.y = std::max(rsc.y, limits.top());
        rsc.y = std::min(rsc.y, limits.bottom() - rsc.h);

        // closest candidate rect wins
        double d2 = rs.get_center().distance2(rsc.get_center());
        if (dmin.is_none() || d2 < dmin)
        {
            dmin = d2;
            rsmin = rsc;
        }
    }

    return {rsmin.x - r.x, rsmin.y - r.y};
}

Rect ViewBase::limit_size(const Rect& rect)
{
    Rect limits = get_limit_rect();
    Rect r = rect;
    if (!limits.empty())    // LP #1633284
    {
        if (r.w > limits.w)
            r.w = limits.w - 40;
        if (r.h > limits.h)
            r.h = limits.h - 20;
    }
    return r;
}

std::vector<Rect> ViewBase::get_monitor_rects()
{
    auto callbacks = get_global_callbacks();
    assert(callbacks->get_n_monitors);
    assert(callbacks->get_monitor_geometry);

    std::vector<Rect> rects;
    int n = callbacks->get_n_monitors(get_cinstance());
    for (int i=0; i<n; i++)
    {
        Rect r;
        callbacks->get_monitor_geometry(get_cinstance(),
                                        i, &r.x, &r.y, &r.w, &r.h);
        rects.emplace_back(r);
    }
    return rects;
}

Point ViewBase::canvas_to_parent(const Point& point) const
{
    return point + get_rect().get_position();
}

Rect ViewBase::canvas_to_parent(const Rect& rect) const
{
    Point a = canvas_to_parent(rect.left_top());
    Point b = canvas_to_parent(rect.right_bottom());
    return {a, b - a};
}

Point ViewBase::canvas_to_root(const Point& point) const
{
    Point pt = point;
    auto view = this;
    while (view)
    {
        pt = view->canvas_to_parent(pt);
        view = view->m_parent;
    }
    return pt;
}

Rect ViewBase::canvas_to_root(const Rect& rect) const
{
    Point a = canvas_to_root(rect.left_top());
    Point b = canvas_to_root(rect.right_bottom());
    return {a, b - a};
}

Point ViewBase::parent_to_canvas(const Point& point) const
{
    return point - get_rect().get_position();
}

Rect ViewBase::parent_to_canvas(const Rect& rect) const
{
    Point a = parent_to_canvas(rect.left_top());
    Point b = parent_to_canvas(rect.right_bottom());
    return {a, b - a};
}

Point ViewBase::root_to_canvas(const Point& point) const
{
    return parent_to_canvas_recursive(point);
}

Point ViewBase::parent_to_canvas_recursive(const Point& point) const
{
    Point pt;
    if (m_parent)
        pt = m_parent->parent_to_canvas_recursive(point);
    else
        pt = point;
    return parent_to_canvas(pt);
}

Rect ViewBase::root_to_canvas(const Rect& rect) const
{
    Point a = root_to_canvas(rect.left_top());
    Point b = root_to_canvas(rect.right_bottom());
    return {a, b - a};
}

void ViewBase::update_geometry()
{
    assert(is_toplevel());  // only toplevels have windows/actors
    auto callbacks = get_global_callbacks();
    if (callbacks->view_get_geometry)
    {
        OnboardOskViewGeometry g;
        callbacks->view_get_geometry(this, &g);
        m_rect = {g.x, g.y, g.w, g.h};
        m_rect_valid = true;
    }
}

void ViewBase::set_cursor_type(OnboardOskCursorType cursor_type)
{
    assert(is_toplevel());  // only toplevels have windows/actors
    auto callbacks = get_global_callbacks();
    if (callbacks->view_set_cursor_type)
        callbacks->view_set_cursor_type(this, cursor_type);
}

void ViewBase::move(const Point& pt)
{
    assert(is_toplevel());  // only toplevels have windows/actors
    auto callbacks = get_global_callbacks();
    if (callbacks->view_move)
    {
        callbacks->view_move(this, pt.x, pt.y);
        m_rect_valid = false;
    }
}

void ViewBase::move_resize(const Rect& r)
{
    assert(is_toplevel());  // only toplevels have windows/actors
    auto callbacks = get_global_callbacks();
    if (callbacks->view_move_resize)
    {
        callbacks->view_move_resize(this, r.x, r.y, r.w, r.h);
        m_rect_valid = false;
    }
}

void ViewBase::set_visible(bool visible)
{
    assert(is_toplevel());  // only toplevels have windows/actors
    auto callbacks = get_global_callbacks();
    if (callbacks->view_set_visible)
        callbacks->view_set_visible(this, visible);
}

void ViewBase::redraw_area(const Rect& r)
{
    auto toplevel = find_toplevel_view_from_leaf();
    auto callbacks = get_global_callbacks();
    if (callbacks->view_queue_draw_area)
        callbacks->view_queue_draw_area(toplevel, r.x, r.y, r.w, r.h);
}

void ViewBase::redraw()
{
    auto toplevel = find_toplevel_view_from_leaf();
    auto callbacks = get_global_callbacks();
    if (callbacks->view_queue_draw)
        callbacks->view_queue_draw(toplevel);
}

const ViewBase* ViewBase::find_toplevel_view_from_leaf() const
{
    return const_cast<ViewBase*>(this)->find_toplevel_view_from_leaf();
}

ViewBase* ViewBase::find_toplevel_view_from_leaf()
{
    ViewBase* view = this;
    for(;;)
    {
        ViewBase* parent = view->get_parent();
        if (!parent)
            break;
        view = parent;
    }
    return view;
}



void get_monitor_dimensions(const ViewBase* view, Size& size, Size& size_mm)
{
    if (!view)
        return;

    auto cinstance = const_cast<ViewBase*>(view)->get_cinstance();
    auto callbacks = view->get_global_callbacks();
    int monitor_index = 0;
    Rect r;

    if (callbacks->get_monitor_at_view)
        monitor_index = callbacks->get_monitor_at_view(cinstance,
                                                       view);
    if (callbacks->get_monitor_geometry)
        callbacks->get_monitor_geometry(cinstance,
                                        monitor_index, &r.x, &r.y, &r.w, &r.h);
    if (callbacks->get_monitor_size_mm)
        callbacks->get_monitor_size_mm(cinstance,
                                       monitor_index, &size_mm.w, &size_mm.h);
    size = r.get_size();

    // Simulate certain devices for testing
    int device = -1;       // keep this at None
    // device = 1
    if (device == 0)
    {
        // dimension unavailable
        size_mm = {0, 0};
    }
    else if (device == 1)
    {
        // Nexus 7, as it should be reported
        size = {1280, 800};
        size_mm = {150, 94};
    }
}

Size physical_to_monitor_pixel_size(ViewBase* view,
                                    const Size& size_mm,
                                    Size fallback_size)
{
    Size sz;
    Size sz_mm;
    Size result;
    get_monitor_dimensions(view, sz, sz_mm);
    if (sz.x > 0 && sz.y > 0 &&
        sz_mm.x > 0 && sz_mm.y > 0)
    {
        result.w = sz_mm.x == 0.0 ? fallback_size.x : sz.x * size_mm.x / sz_mm.x;
        result.h = sz_mm.y == 0.0 ? fallback_size.y : sz.y * size_mm.y / sz_mm.y;
    }
    else
    {
        result = fallback_size;
    }
    return result;
}

