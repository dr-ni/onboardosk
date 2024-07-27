#include <algorithm>
#include <array>

#include <cairo.h>

#include "cairocontext.h"
#include "drawing_helpers.h"
#include "rect_decl.h"


// Spot check some enum values, since our type is
// just a copy of the cairo type (to prevent cairo
// includes everywhere).
static_assert(static_cast<int>(CairoContext::Operator::CLEAR) ==
              static_cast<int>(CAIRO_OPERATOR_CLEAR));
static_assert(static_cast<int>(CairoContext::Operator::HSL_SATURATION) ==
              static_cast<int>(CAIRO_OPERATOR_HSL_SATURATION));
static_assert(static_cast<int>(CairoContext::Operator::HSL_COLOR) ==
              static_cast<int>(CAIRO_OPERATOR_HSL_COLOR));
static_assert(static_cast<int>(CairoContext::Operator::HSL_LUMINOSITY) ==
              static_cast<int>(CAIRO_OPERATOR_HSL_LUMINOSITY));


CairoContext::CairoContext(cairo_t* cr) :
    m_cr(cr)
{
    if (m_cr)
    {
        cairo_reference(m_cr);
    }

}

CairoContext::CairoContext(CairoSurface* surface)
{
    //m_surface = cairo_surface_create_similar();
    //m_surface = cairo_image_surface_create();
    m_surface = surface->m_surface;
    if (m_surface)
    {
        cairo_surface_reference(m_surface);
        m_cr = cairo_create(m_surface);
    }
}

CairoContext::~CairoContext()
{
    if (m_surface)
    {
        cairo_surface_destroy(m_surface);
        m_surface = nullptr;
    }

    if (m_cr)
    {
        cairo_destroy(m_cr);
        m_cr = nullptr;
    }
}

void CairoContext::save()
{
    cairo_save(m_cr);
}

void CairoContext::restore()
{
    cairo_restore(m_cr);
}

void CairoContext::set_source_rgba(const RGBA& rgba)
{
    cairo_set_source_rgba(m_cr, rgba.r, rgba.g, rgba.b, rgba.a);
}

void CairoContext::set_source(CairoPattern& pat)
{
    cairo_set_source(m_cr, pat.m_pattern);
}

void CairoContext::set_operator(CairoContext::Operator op)
{
    cairo_set_operator(m_cr, static_cast<cairo_operator_t>(op));
}

void CairoContext::rectangle(const Rect& r)
{
    cairo_rectangle(m_cr, r.x, r.y, r.w, r.h);
}

void CairoContext::move_to(double x, double y)
{
    cairo_move_to(m_cr, x, y);
}

void CairoContext::line_to(double x, double y)
{
    cairo_line_to(m_cr, x, y);
}

void CairoContext::arc(double xc, double yc, double radius, double angle1, double angle2)
{
    cairo_arc(m_cr, xc, yc, radius, angle1, angle2);
}

void CairoContext::arc(const Point& center, double radius, double angle1, double angle2)
{
    cairo_arc(m_cr, center.x, center.y, radius, angle1, angle2);
}

void CairoContext::curve_to(double x1, double y1, double x2, double y2, double x3, double y3)
{
    cairo_curve_to(m_cr, x1, y1, x2, y2, x3, y3);
}

void CairoContext::new_path()
{
    cairo_new_path(m_cr);
}

void CairoContext::close_path()
{
    cairo_close_path(m_cr);
}

void CairoContext::set_line_width(double w)
{
    cairo_set_line_width(m_cr, w);
}

void CairoContext::stroke()
{
    cairo_stroke(m_cr);
}

void CairoContext::fill()
{
    cairo_fill(m_cr);
}

void CairoContext::fill_preserve()
{
    cairo_fill_preserve(m_cr);
}

void CairoContext::paint()
{
    cairo_paint(m_cr);
}

void CairoContext::paint_with_alpha(double alpha)
{
    cairo_paint_with_alpha(m_cr, alpha);
}

bool CairoContext::in_fill(const Point& pt)
{
    return cairo_in_fill(m_cr, pt.x, pt.y);
}

void CairoContext::mask(const CairoPattern& pattern)
{
    cairo_mask(m_cr, pattern.m_pattern);
}

void CairoContext::translate(const Offset& offset)
{
    cairo_translate(m_cr, offset.x, offset.y);
}

void CairoContext::scale(const Scale& scale)
{
    cairo_scale(m_cr, scale.x, scale.y);
}

void CairoContext::rotate(double angle)
{
    cairo_rotate(m_cr, angle);
}

CairoPattern CairoContext::get_source()
{
    return {this, cairo_get_source(m_cr)};
}

void CairoContext::push_group()
{
    cairo_push_group(m_cr);
}

void CairoContext::push_group_with_content(CairoContext::Content content)
{
    cairo_push_group_with_content(m_cr, static_cast<cairo_content_t>(content));
}

CairoPattern CairoContext::pop_group()
{
    return {this, cairo_pop_group(m_cr)};
}

void CairoContext::pop_group_to_source()
{
    cairo_pop_group_to_source(m_cr);
}

Rect CairoContext::get_clip_rect()
{
    double x1, y1, x2, y2;
    cairo_clip_extents (m_cr, &x1, &y1, &x2, &y2);
    return Rect(x1, y1, x2 - x1, y2 - y1);
}

void CairoContext::clip()
{
    cairo_clip(m_cr);
}

Line CairoContext::gradient_line(const Rect& rect, double alpha)
{
    using namespace std;

    // Find rotated gradient start && end points.
    // Line end points follow the largest extent of the rotated rectangle.
    // The gradient reaches across the entire rectangle.
    double a = rect.w / 2.0;
    double b = rect.h / 2.0;
    array<Point, 4> coords = {{{-a, -b}, {a, -b}, {a, b}, {-a, b}}};
    array<double, 4> vx;
    transform(coords.begin(), coords.end(), vx.begin(), [&](const Point& pt) {
        return pt.x * cos(alpha) - pt.y * sin(alpha);
    });
    double dx = *max_element(vx.begin(), vx.end()) -
                *min_element(vx.begin(), vx.end());
    double r = dx / 2.0;
    return {{r * cos(alpha) + rect.x + a,
             r * sin(alpha) + rect.y + b},
            {-r * cos(alpha) + rect.x + a,
             -r * sin(alpha) + rect.y + b}};
}

void CairoContext::roundrect_arc(const Rect& rect, double r)
{
    double x0 = rect.x;
    double y0 = rect.y;
    double x1 = rect.right();
    double y1 = rect.bottom();

    // top left
    move_to(x0+r, y0);

    // top right
    line_to(x1-r,y0);
    arc(x1-r, y0+r, r, -M_PI/2, 0);

    // bottom right
    line_to(x1, y1-r);
    arc(x1-r, y1-r, r, 0, M_PI/2);

    // bottom left
    line_to(x0+r, y1);
    arc(x0+r, y1-r, r, M_PI/2, M_PI);

    // top left
    line_to(x0, y0+r);
    arc(x0+r, y0+r, r, M_PI, M_PI*1.5);

    close_path ();
}

void CairoContext::roundrect_curve(const Rect& rect, double r_pct,
                                   CornerMask corner_mask)
{
    double x0 = rect.x;
    double y0 = rect.y;
    double w  = rect.w;
    double h  = rect.h;
    double x1 = x0 + w;
    double y1 = y0 + h;

    // full range at 50%
    double r = std::min(w, h) * std::min(r_pct / 100.0, 0.5);

    // position of control points for circular curves
    double k = (r - 1) * r_pct / 200.0;

    // top left
    if (corner_mask & 0b0001)
    {
        move_to(x0 + r, y0);
    }
    else
    {
        move_to(x0, y0);
    }

    // top right
    if (corner_mask & 0b0010)
    {
        line_to(x1 - r, y0);
        curve_to(x1 - k, y0, x1, y0 + k, x1, y0 + r);
    }
    else
    {
        line_to(x1, y0);
    }

    // bottom right
    if (corner_mask & 0b0100)
    {
        line_to(x1, y1 - r);
        curve_to(x1, y1 - k, x1 - k, y1, x1 - r, y1);
    }
    else
    {
        line_to(x1, y1);
    }

    // bottom left
    if (corner_mask & 0b1000)
    {
        line_to(x0 + r, y1);
        curve_to(x0 + k, y1, x0, y1 - k, x0, y1 - r);
    }
    else
    {
        line_to(x0, y1);
    }

    // top left
    if (corner_mask & 0b0001)
    {
        line_to(x0, y0 + r);
        curve_to(x0, y0 + k, x0 + k, y0, x0 + r, y0);
    }
    else
    {
        line_to(x0, y0);
    }

    close_path();
}

void CairoContext::rounded_polygon(const std::vector<double> coords, double r_pct, double chamfer_size)
{
    std::vector<std::vector<double>> path;
    polygon_to_rounded_path(coords, r_pct, chamfer_size, path);
    rounded_polygon_path_to_cairo_path(path);
}

void CairoContext::rounded_polygon_path_to_cairo_path(const std::vector<std::vector<double>>& path)
{
    if (!path.empty())
    {
        if (path[0].size() < 2)
            return;

        move_to(path[0][0], path[0][1]);
        for (size_t i=1; i<path.size(); i+=2)
        {
            auto& p0 = path[i];
            if (p0.size() >= 2)
                line_to(p0[0], p0[1]);
            auto& p1 = path[i+1];
            if (p1.size() >= 6)
                curve_to(p1[2], p1[3], p1[4], p1[5], p1[0], p1[1]);
        }
        close_path();
    }
}

void CairoContext::drop_shadow(CairoPattern& pattern, const Rect& bounds,
                               double blur_radius, const Offset& offset,
                               double alpha, int steps)
{
    Point origin = bounds.get_center();
    set_source_rgba({0.0, 0.0, 0.0, alpha});
    for (int i=0; i<steps; i++)
    {
        double x = (i ? i : 0.5) / static_cast<double>(steps);
        double k = std::sqrt(std::abs(std::log(1-x))) * 0.7 * blur_radius; // gaussian
        //k = i / float(steps) * blur_radius         // linear

        Size sz = bounds.get_size();
        Scale sc = (sz + k) / sz;

        save();

        translate(origin);
        scale(sc);
        translate(-origin + offset);
        mask(pattern);

        restore();
    }
}


CairoPattern::CairoPattern(CairoContext* cr, cairo_pattern_t* pattern) :
    m_cr(cr),
    m_pattern(pattern)
{
    if (m_pattern)
        cairo_pattern_reference(m_pattern);
}

CairoPattern::CairoPattern(const CairoPattern& pattern)
{
    *this = pattern;
}

CairoPattern::~CairoPattern()
{
    if (m_pattern)
        // Decrement reference count.
        // Pattern ownership may have transferred to cairo with calling set_source().
        cairo_pattern_destroy(m_pattern);
}

CairoPattern& CairoPattern::operator=(const CairoPattern& pattern)
{
    m_cr = pattern.m_cr;

    if (m_pattern)
        cairo_pattern_destroy(m_pattern);
    m_pattern = pattern.m_pattern;
    if (m_pattern)
        cairo_pattern_reference(m_pattern);
    return *this;
}

unsigned CairoPattern::get_refcount()
{
    return cairo_pattern_get_reference_count(m_pattern);
}


LinearGradient::LinearGradient(CairoContext* cr, const Line& l) :
    CairoPattern(cr)
{
    m_pattern = cairo_pattern_create_linear (l.begin.x, l.begin.y,
                                             l.end.x, l.end.y);
}

void LinearGradient::add_color_stop_rgba(double offset, const RGBA& rgba)
{
    cairo_pattern_add_color_stop_rgba(m_pattern, offset, rgba.r, rgba.g, rgba.b, rgba.a);
}


CairoSurface::CairoSurface(cairo_surface_t* surface) :
    m_surface(surface)
{
    if (m_surface)
        cairo_surface_reference(m_surface);
}

CairoSurface::CairoSurface(const CairoSurface& surface)
{
    *this = surface;
}

CairoSurface::CairoSurface(const Size& size)
{
    cairo_format_t format = CAIRO_FORMAT_ARGB32;
    m_surface = cairo_image_surface_create(format, size.w, size.h);
}

CairoSurface::~CairoSurface()
{
    if (m_surface)
        // Decrement reference count.
        // surface ownership may have transferred to cairo with calling set_source().
        cairo_surface_destroy(m_surface);
}

CairoSurface& CairoSurface::operator=(const CairoSurface& surface)
{
    m_cr = surface.m_cr;

    if (m_surface)
        cairo_surface_destroy(m_surface);
    m_surface = surface.m_surface;
    if (m_surface)
        cairo_surface_reference(m_surface);
    return *this;
}

bool CairoSurface::ok()
{
    cairo_status_t status = cairo_surface_status(m_surface);
    return status == CAIRO_STATUS_SUCCESS;
}

unsigned CairoSurface::get_refcount()
{
    return cairo_surface_get_reference_count(m_surface);
}


CairoMatrix::CairoMatrix() :
    m_matrix(std::make_unique<cairo_matrix_t>())
{
    cairo_matrix_init_identity(m_matrix.get());
}

CairoMatrix::CairoMatrix(const CairoMatrix& matrix)
{
    *this = matrix;
}

CairoMatrix::~CairoMatrix()
{
}

CairoMatrix& CairoMatrix::operator=(const CairoMatrix& matrix)
{
    *(m_matrix.get()) = *(matrix.m_matrix.get());
    return *this;
}

void CairoMatrix::translate(const Offset& offset)
{
    cairo_matrix_translate(m_matrix.get(), offset.x, offset.y);
}

void CairoMatrix::scale(const Scale& scale)
{
    cairo_matrix_scale(m_matrix.get(), scale.x, scale.y);
}

void CairoMatrix::rotate(double alpha)
{
    cairo_matrix_rotate(m_matrix.get(), alpha);
}

Point CairoMatrix::transform(const Point& pt)
{
    Point result = pt;
    cairo_matrix_transform_point(m_matrix.get(), &result.x, &result.y);
    return result;
}
