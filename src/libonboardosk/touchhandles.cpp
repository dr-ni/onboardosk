#include <array>
#include <cmath>

#include "tools/container_helpers.h"
#include "tools/cairocontext.h"
#include "drawingcontext.h"
#include "layoutview.h"
#include "touchhandles.h"

using std::array;

TouchHandle::HandleAnglesMap TouchHandle::m_handle_angles;

TouchHandle::TouchHandle(const ContextBase& context, Handle::Enum id_) :
    Super(context),
    id(id_)
{
    // initialize angles
    if (m_handle_angles.empty())
    {
        for (size_t i=0; i<Handle::EDGES.size(); i++)
        {
            auto h = Handle::EDGES[i];
            m_handle_angles[h] = i * M_PI / 2.0;
        }
        for (size_t i=0; i<Handle::CORNERS.size(); i++)
        {
            auto h = Handle::CORNERS[i];
            m_handle_angles[h] = i * M_PI / 2.0 + M_PI / 4.0;
        }
        m_handle_angles[Handle::MOVE] = 0.0;
    }
}

TouchHandle::~TouchHandle()
{}

Rect TouchHandle::get_rect() const
{
    Rect rect = m_rect;
    if (!rect.empty() &&
        this->pressed)
    {
        rect = rect.offset(1.0, 1.0);
    }
    return rect;
}

double TouchHandle::get_radius() const
{
    Size sz = get_rect().get_size();
    return std::min(sz.w, sz.h) / 2.0;
}

Rect TouchHandle::get_shadow_rect() const
{
    Rect rect = get_rect();
    if (!rect.empty())
    {
        rect = rect.inflate(m_shadow_size + 1.0);
        rect.w += m_shadow_offset.x;
        rect.h += m_shadow_offset.y;
    }
    return rect;
}

double TouchHandle::get_arrow_angle() const
{
    return m_handle_angles[this->id];
}

bool TouchHandle::is_edge_handle() const
{
    return contains(Handle::EDGES, this->id);
}

bool TouchHandle::is_corner_handle() const
{
    return contains(Handle::CORNERS, this->id);
}

void TouchHandle::update_position(const Rect& canvas_rect)
{
    Size sz = m_size;
    sz.w = std::min(sz.w, canvas_rect.w / 3.0);
    sz.h = sz.w;
    m_scale = 1.0;

    Point pc = canvas_rect.get_center();
    if (this->id == Handle::MOVE)
    {
        double d = std::min(canvas_rect.w - 2.0 * sz.w,
                            canvas_rect.h - 2.0 * sz.h);
        m_scale = 1.4;
        sz.w = std::min(sz.w * m_scale, d);
        sz.h = std::min(sz.h * m_scale, d);
    }

    double x{};
    double y{};

    if (contains(array<Handle::Enum, 3>{
                 Handle::WEST,
                 Handle::NORTH_WEST,
                 Handle::SOUTH_WEST}, this->id))
        x = canvas_rect.left();

    if (contains(array<Handle::Enum, 3>{
                 Handle::NORTH,
                 Handle::NORTH_WEST,
                 Handle::NORTH_EAST}, this->id))
        y = canvas_rect.top();

    if (contains(array<Handle::Enum, 3>{
                 Handle::EAST,
                 Handle::NORTH_EAST,
                 Handle::SOUTH_EAST}, this->id))
        x = canvas_rect.right() - sz.w;

    if (contains(array<Handle::Enum, 3>{
                 Handle::SOUTH,
                 Handle::SOUTH_WEST,
                 Handle::SOUTH_EAST}, this->id))
        y = canvas_rect.bottom() - sz.h;

    if (contains(array<Handle::Enum, 3>{
                 Handle::MOVE,
                 Handle::EAST,
                 Handle::WEST}, this->id))
        y = pc.y - sz.h / 2.0;

    if (contains(array<Handle::Enum, 3>{
                 Handle::MOVE,
                 Handle::NORTH,
                 Handle::SOUTH}, this->id))
        x = pc.x - sz.w / 2.0;

    m_rect = {x, y, sz.w, sz.h};
}

bool TouchHandle::hit_test(const Point& point) const
{
    Rect rect = get_rect().grow(m_hit_proximity_factor);
    double radius = get_radius() * m_hit_proximity_factor;

    if (!rect.empty() && rect.contains(point))
    {
        CairoSurface surface({32, 32});
        CairoContext cc(&surface);
        build_handle_path(&cc, rect, radius);
        return cc.in_fill(point);
    }
    return false;

    Point pc = rect.get_center();
    double d = pc.distance2(point);
    return d <= radius*radius;
}

void TouchHandle::draw(DrawingContext& dc)
{
    double alpha_factor = this->pressed ? 1.5 : 1.0;

    auto cc = dc.get_cc();
    cc->new_path();

    draw_handle_shadow(cc);
    draw_handle(cc, alpha_factor);
    draw_arrows(cc);
}

void TouchHandle::redraw()
{
    Rect rect = get_shadow_rect();
    if (!rect.empty())
    {
        if (m_view)
            m_view->redraw_area(rect);
    }
}

void TouchHandle::draw_handle(CairoContext* cc, double alpha_factor)
{
    double radius = get_radius();
    double line_width = radius / 15.0;

    double alpha = m_handle_alpha * alpha_factor;
    if (this->pressed)
    {
        cc->set_source_rgba({0.78, 0.33, 0.17, alpha});
    }
    else if (this->prelight)
    {
        cc->set_source_rgba({0.98, 0.53, 0.37, alpha});
    }
    else
    {
        cc->set_source_rgba({0.78, 0.33, 0.17, alpha});
    }

    build_handle_path(cc);

    cc->fill_preserve();
    cc->set_line_width(line_width);
    cc->stroke();
}

void TouchHandle::draw_handle_shadow(CairoContext* cc)
{
    Rect rect = get_rect();

    cc->save();

    // There is a massive performance boost for groups when clipping is used.
    // Integer limits are again dramatically faster (x4) than using floats.
    // for 1000x draw_drop_shadow:
    //     with clipping: ~300ms, without: ~11000ms
    cc->rectangle(get_shadow_rect().floor());
    cc->clip();

    cc->push_group();

    // draw the shadow
    cc->push_group_with_content(CairoContext::Content::ALPHA);
    build_handle_path(cc);
    cc->set_source_rgba({0.0, 0.0, 0.0, 1.0});
    cc->fill();
    auto group = cc->pop_group();
    cc->drop_shadow(group, rect,
                    m_shadow_size,
                    m_shadow_offset,
                    m_shadow_alpha,
                    5);

    // cut out the handle area, because the handle is transparent
    cc->save();
    cc->set_operator(CairoContext::Operator::CLEAR);
    cc->set_source_rgba({0.0, 0.0, 0.0, 1.0});
    build_handle_path(cc);
    cc->fill();
    cc->restore();

    cc->pop_group_to_source();
    cc->paint();

    cc->restore();
}

void TouchHandle::draw_arrows(CairoContext* cc)
{
    double radius = get_radius();
    Point pc = get_rect().get_center();
    double scale = radius / 2.0 / m_scale * 1.2;

    size_t num_arrows;
    double angle = get_arrow_angle();

    if (this->id == Handle::MOVE)
    {
        num_arrows = 4;
        if (this->lock_x_axis)
        {
            num_arrows -= 2;
            angle += M_PI * 0.5;
        }
        if (this->lock_y_axis)
        {
            num_arrows -= 2;
        }
    }
    else
    {
        num_arrows = 2;
    }
    double angle_step = 2.0 * M_PI / num_arrows;

    for (size_t i=0; i<num_arrows; i++)
    {
        cc->save();

        cc->translate(pc);
        cc->rotate(angle + i * angle_step);
        cc->scale({scale, scale});

        // arrow distance from center
        if (this->id == Handle::MOVE)
            cc->translate({0.9, 0});
        else
            cc->translate({0.30, 0});

        draw_arrow(cc);

        cc->restore();
    }
}

void TouchHandle::draw_arrow(CairoContext* cc)
{
    cc->move_to({0.0, -0.5});
    cc->line_to({0.5,  0.0});
    cc->line_to({0.0,  0.5});
    cc->close_path();

    cc->set_source_rgba({1.0, 1.0, 1.0, 0.8});
    cc->fill_preserve();

    cc->set_source_rgba({0.0, 0.0, 0.0, 0.8});
    cc->set_line_width(0.0);
    cc->stroke();
}

void TouchHandle::build_handle_path(CairoContext* cc, const Rect& rect, double radius) const
{
    Point pc = rect.get_center();
    double corner_radius_ = this->corner_radius;

    double angle = get_arrow_angle();
    CairoMatrix m;
    m.translate(pc);
    m.rotate(angle);

    if (is_edge_handle())
    {
        Point p0 = m.transform({radius, -radius});
        Point p1 = m.transform({radius, radius});
        cc->arc(pc, radius, angle + M_PI / 2.0, angle + M_PI / 2.0 + M_PI);
        cc->line_to(p0);
        cc->line_to(p1);
        cc->close_path();
    }
    else
        if (this->is_corner_handle())
        {
            m.rotate(-M_PI / 4.0);  // rotate to SOUTH_EAST

            cc->arc(pc, radius, angle + 3 * M_PI / 4.0,
                    angle + 5 * M_PI / 4.0);
            {
                Point pt = m.transform({radius, -radius});
                cc->line_to(pt);
            }

            if (corner_radius_ >= 0.0)
            {
                // outer corner, following the rounded window corner
                Point pt  = m.transform({radius,  radius - corner_radius_});
                Point ptc = m.transform({radius - corner_radius_,
                                               radius - corner_radius_});
                cc->line_to(pt);
                cc->arc(ptc, corner_radius_,
                        angle - M_PI / 4.0,  angle + M_PI / 4.0);
            }
            else
            {
                Point pt = m.transform({radius,  radius});
                cc->line_to(pt);
            }

            {
                Point pt = m.transform({-radius,  radius});
                cc->line_to(pt);
            }
            cc->close_path();
        }
        else
        {
            cc->arc(pc, radius, 0, 2.0 * M_PI);
        }
}



TouchHandles::TouchHandles(const ContextBase& context) :
    Super(context)
{
    m_handle_pool.emplace_back(std::make_unique<TouchHandle>(context, Handle::MOVE));
    m_handle_pool.emplace_back(std::make_unique<TouchHandle>(context, Handle::NORTH_WEST));
    m_handle_pool.emplace_back(std::make_unique<TouchHandle>(context, Handle::NORTH));
    m_handle_pool.emplace_back(std::make_unique<TouchHandle>(context, Handle::NORTH_EAST));
    m_handle_pool.emplace_back(std::make_unique<TouchHandle>(context, Handle::EAST));
    m_handle_pool.emplace_back(std::make_unique<TouchHandle>(context, Handle::SOUTH_EAST));
    m_handle_pool.emplace_back(std::make_unique<TouchHandle>(context, Handle::SOUTH));
    m_handle_pool.emplace_back(std::make_unique<TouchHandle>(context, Handle::SOUTH_WEST));
    m_handle_pool.emplace_back(std::make_unique<TouchHandle>(context, Handle::WEST));
}

TouchHandles::~TouchHandles()
{}

void TouchHandles::set_view(LayoutView* view)
{
    for (auto& handle : m_handle_pool)
        handle->m_view = view;
}

void TouchHandles::update_positions(const Rect& canvas_rect)
{
    m_rect = canvas_rect;
    for (auto& handle : m_handles)
        handle->update_position(canvas_rect);
}

void TouchHandles::draw(DrawingContext& dc)
{
    if (m_opacity != 0.0)
    {
        auto cc = dc.get_cc();
        Rect clip_rect = cc->get_clip_rect();
        for (auto handle : m_handles)
        {
            Rect rect = handle->get_shadow_rect();
            if (rect.intersects(clip_rect))
            {
                cc->save();
                cc->rectangle(rect.floor());
                cc->clip();
                cc->push_group();

                handle->draw(dc);

                cc->pop_group_to_source();
                cc->paint_with_alpha(m_opacity);;
                cc->restore();
            }
        }
    }
}

void TouchHandles::redraw()
{
    if (!m_rect.empty())
    {
        for (auto handle : m_handles)
            handle->redraw();
    }
}

Handle::Enum TouchHandles::hit_test(const Point& point)
{
    if (m_active)
    {
        for (auto& handle : m_handles)
            if (handle->hit_test(point))
                return handle->id;
    }
    return Handle::NONE;
}

void TouchHandles::set_prelight(Handle::Enum handle_id)
{
    for (auto handle : m_handles)
    {
        bool prelight = handle->id == handle_id && !handle->pressed;
        if (handle->prelight != prelight)
        {
            handle->prelight = prelight;
            handle->redraw();
        }
    }
}

void TouchHandles::set_pressed(Handle::Enum handle_id)
{
    for (auto handle : m_handles)
    {
        bool pressed = handle->id == handle_id;
        if (handle->pressed != pressed)
        {
            handle->pressed = pressed;
            handle->redraw();
        }
    }
}

void TouchHandles::set_corner_radius(double corner_radius)
{
    for (auto handle : m_handles)
        handle->corner_radius = corner_radius;
}

void TouchHandles::set_monitor_dimensions(const Size& size_px, const Size& size_mm)
{
    double min_monitor_mm = 50;
    Size target_size_mm = {5, 5};
    Size min_size = TouchHandle::m_fallback_size;

    Size sz;
    if (size_mm.w < min_monitor_mm ||
        size_mm.h < min_monitor_mm)
    {
        sz = {0, 0};
    }
    else
    {
        sz = size_px / size_mm * target_size_mm;
        //sz.h = sz.w;
    }

    sz = {std::max(sz.w, min_size.w), std::max(sz.h, min_size.h)};

    for (auto handle : m_handles)
        handle->m_size = sz;
}

void TouchHandles::lock_x_axis(bool lock)
{
    for (auto handle : m_handles)
        handle->lock_x_axis = lock;
}

void TouchHandles::lock_y_axis(bool lock)
{
    for (auto handle : m_handles)
        handle->lock_y_axis = lock;
}


