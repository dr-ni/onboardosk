
#include <array>
#include <string>
#include <vector>

#include "tools/cairocontext.h"
#include "tools/image.h"

#include "configuration.h"
#include "drawingcontext.h"
#include "dwellprogresscontroller.h"
#include "keygeometry.h"
#include "keypath.h"
#include "layoutkey.h"
#include "layoutkeyviewcairo.h"
#include "textrendererpangocairo.h"
#include "theme.h"

using std::array;
using std::string;
using std::vector;


DwellProgressViewCairo::DwellProgressViewCairo(DwellProgressController* c) :
    m_controller(c)
{}

bool DwellProgressViewCairo::is_dwelling()
{
    return m_controller && m_controller->is_dwelling();
}

void DwellProgressViewCairo::draw(DrawingContext& dc,
                                  const Rect& rect,
                                  const RGBA& rgba,
                                  const Noneable<RGBA>& rgba_bg)
{
    if (!m_controller)
        return;

    auto cr = dc.get_cc();
    if (m_controller->opacity <= 0.0)
    {
    }
    if (m_controller->opacity >= 1.0)
    {
        this->draw_progress_indicator(cr, rect, rgba, rgba_bg);
    }
    else
    {
        cr->save();
        cr->rectangle(rect.floor());
        cr->clip();
        cr->push_group();

        draw_progress_indicator(cr, rect, rgba, rgba_bg);

        cr->pop_group_to_source();
        cr->paint_with_alpha(m_controller->opacity);
        cr->restore();
    }
}

void DwellProgressViewCairo::draw_progress_indicator(CairoContext* cr,
                                                 const Rect& rect,
                                                 const RGBA& rgba,
                                                 const Noneable<RGBA>& rgba_bg)
{
    Point center = rect.get_center();

    double radius = std::min(rect.w, rect.h) / 2.0;

    double alpha0 = -M_PI / 2.0;
    auto k = m_controller->get_done_ratio();
    k = std::min(k, 1.0);
    double alpha = k * M_PI * 2.0;

    if (!rgba_bg.is_none())
    {
        cr->set_source_rgba(rgba_bg);
        cr->move_to(center);
        cr->arc(center, radius, 0, 2 * M_PI);
        cr->close_path();
        cr->fill();
    }

    cr->move_to(center);
    cr->arc(center, radius, alpha0, alpha0 + alpha);
    cr->close_path();

    cr->set_source_rgba(rgba);
    cr->fill_preserve();

    cr->set_source_rgba({0, 0, 0, 1});
    cr->set_line_width(0);
    cr->stroke();
}



LayoutItemView::~LayoutItemView()
{}

LayoutKeyView::~LayoutKeyView()
{}

std::unique_ptr<LayoutKeyViewCairo> LayoutKeyViewCairo::make(const ContextBase& context, LayoutKey* key)
{
    return std::make_unique<This>(context, key);
}


LayoutKeyViewCairo::LayoutKeyViewCairo(const ContextBase& context, LayoutKey* key) :
    Super(context, key),
    m_progress(key->get_dwell_progress_controller())
{}

LayoutKeyViewCairo::~LayoutKeyViewCairo()
{}

void LayoutKeyViewCairo::draw_item(DrawingContext& dc)
{
    if (dc.draw_cached && can_draw_cached())
    {
        //draw_cached(dc);
    }
    else
    {
        draw(dc, dc.lod);
    }
}

void LayoutKeyViewCairo::draw(DrawingContext& dc, LOD lod)
{
    draw_geometry(dc, lod);
    draw_image(dc, lod);
    draw_label(dc, lod);
}

void LayoutKeyViewCairo::draw_geometry(DrawingContext& dc, LOD lod)
{
    if (!m_item->can_draw_geometry())
        return;

    double line_width = 0.0;

    if (lod == LOD::FULL && m_item->show_border)
    {
        double scale = m_item->get_stroke_width();
        if (scale != 0.0)
        {
            auto root = m_item->get_layout_root();
            Size sz    = root->get_context()->scale_log_to_canvas(Size(1.0, 1.0));
            line_width = (sz.w + sz.h) / 2.4;
            line_width = std::min(line_width, 3.0) * scale;
            line_width = std::max(line_width, 1.0);
        }
    }

    RGBA fill = m_item->get_fill_color();

    auto key_style = m_item->get_style();
    if (key_style == KeyStyle::FLAT)
    {
        draw_flat_key(dc, fill, line_width);
    }

    else
    if (key_style == KeyStyle::GRADIENT)
    {
        draw_gradient_key(dc, fill, line_width, lod);
    }

    else
    if (key_style == KeyStyle::DISH)
    {
        draw_dish_key(dc, fill, lod);
    }
}

void LayoutKeyViewCairo::draw_flat_key(DrawingContext& dc, const RGBA& fill, double line_width)
{
    auto cr = dc.get_cc();

    build_canvas_path(dc);

    if (m_item->show_face)
    {
        cr->set_source_rgba(fill);
        if (line_width != 0.0)
            cr->fill_preserve();
        else
            cr->fill();
    }

    if (line_width != 0.0)
    {
        cr->set_source_rgba(m_item->get_stroke_color());
        cr->set_line_width(line_width);
        cr->stroke();
    }
}

void LayoutKeyViewCairo::draw_gradient_key(DrawingContext& dc, const RGBA& fill, double line_width, LOD lod)
{
    // simple gradients for fill && stroke
    double fill_gradient = config()->current_theme()->key_fill_gradient / 100.0;
    double stroke_gradient = m_item->get_stroke_gradient();
    double alpha = m_item->get_gradient_angle();

    auto cc = dc.get_cc();

    Rect rect = m_item->get_canvas_rect();
    build_canvas_path(dc, &rect);

    Line line = cc->gradient_line(rect, alpha);

    // fill
    if (m_item->show_face)
    {
        if (fill_gradient != 0.0 && lod != LOD::MINIMAL)
        {
            LinearGradient pat(cc, line);
            RGBA rgba = fill.brighten(+fill_gradient * 0.5);
            pat.add_color_stop_rgba(0, rgba);
            rgba = fill.brighten(-fill_gradient * 0.5);
            pat.add_color_stop_rgba(1, rgba);
            cc->set_source(pat);
            //cr->set_source_rgba(fill);
        }
        else
        {
            cc->set_source_rgba(fill);
        }

        if (m_item->show_border)
        {
            cc->fill_preserve();
        }
        else
        {
            cc->fill();
        }
    }

    // stroke
    if (m_item->show_border)
    {
        if (stroke_gradient != 0.0)
        {
            if (lod != LOD::MINIMAL)
            {
                RGBA stroke = fill;
                LinearGradient pat(cc, line);
                RGBA rgba = stroke.brighten(+stroke_gradient * 0.5);
                pat.add_color_stop_rgba(0, rgba);
                rgba = stroke.brighten(-stroke_gradient * 0.5);
                pat.add_color_stop_rgba(1, rgba);
                cc->set_source(pat);
            }
            else
            {
                cc->set_source_rgba(fill);
            }
        }
        else
        {
            cc->set_source_rgba(m_item->get_stroke_color());
        }

        cc->set_line_width(line_width);
    }
    cc->stroke();
}

void draw_dish_key_border(CairoContext* cr,
                          vector<vector<double>>& path,
                          vector<vector<double>>& path_top,
                          KeyPath::Polygon& polygon,
                          KeyPath::Polygon& polygon_top,
                          const RGBA& base_rgba, double stroke_gradient,
                          double lightx, double lighty)
{
    size_t n = polygon.size();
    size_t m = path.size();

    // Lambert lighting
    vector<RGBA> edge_colors;
    for (size_t i=0; i<n; i+=2)
    {
        double x0 = polygon.at(i);
        double y0 = polygon.at(i+1);
        double x1;
        double y1;

        if (i < n-2)
        {
            x1 = polygon.at(i+2);
            y1 = polygon.at(i+3);
        }
        else
        {
            x1 = polygon.at(0);
            y1 = polygon.at(1);
        }

        double nx = y1 - y0;
        double ny = -(x1 - x0);
        double ln = sqrt(nx*nx + ny*ny);
        double I = ln == 0.0 ? 0.0 :
                   (nx * lightx + ny * lighty) / ln
                   * stroke_gradient * 0.8;
        edge_colors.emplace_back(base_rgba.brighten(I));
    }

    // draw border sections
    size_t edge = 0;
    for (size_t i=0; i<m-2; m += 2)
    {
        // get path points
        size_t i1 = i + 1;
        size_t i2 = i + 2;
        if (i2 >= m)
            i2 -= m;
        size_t i3 = i + 3;
        if (i3 >= m)
            i3 = 1;

        auto& p0 = path.at(i);
        double p0x = p0.at(0);
        double p0y = p0.at(1);

        auto& p1 = path.at(i1);
        double p1x = p1.at(0);
        double p1y = p1.at(1);

        auto& p2 = path.at(i2);
        double p2x = p2.at(0);
        double p2y = p2.at(1);

        auto& p3 = path.at(i3);
        double p3x = p3.at(0);
        double p3y = p3.at(1);

        auto& p4 = path_top.at(i);
        double ptop0x = p4.at(0);
        double ptop0y = p4.at(1);

        auto& p5 = path_top.at(i1);
        double ptop1x = p5.at(0);
        double ptop1y = p5.at(1);

        auto& p6 = path_top.at(i2);
        double ptop2x = p6.at(0);
        double ptop2y = p6.at(1);

        // get polygon points; only to
        // fill gaps at concave corners.
        size_t j0 = edge*2;
        size_t j1 = j0 + 2;
        if (j1 >= n)
            j1 -= n;
        size_t j2 = j0 + 4;
        if (j2 >= n)
            j2 -= n;

        double ptopax = polygon_top.at(j0);
        double ptopay = polygon_top.at(j0 + 1);
        double ptopbx = polygon_top.at(j1);
        double ptopby = polygon_top.at(j1 + 1);
        double ptopcx = polygon_top.at(j2);
        double ptopcy = polygon_top.at(j2 + 1);
        double vax = ptopbx - ptopax;
        double vay = ptopby - ptopay;
        double nbx = ptopcy - ptopby;
        double nby = -(ptopcx - ptopbx);
        bool concave = vax*nbx + vay*nby < 0.0;

        // Fake Gouraud shading: draw a gradient between mid points
        // of the lines connecting the base with the top path.
        auto pat = LinearGradient(cr, {
            {(p1x + ptop1x) * 0.5, (p1y + ptop1y) * 0.5},
            {(p2x + ptop2x) * 0.5, (p2y + ptop2y) * 0.5}
        });
        size_t edge1 = (edge + 1) % edge_colors.size();
        pat.add_color_stop_rgba(0.0, edge_colors[edge]);
        pat.add_color_stop_rgba(1.0, edge_colors[edge1]);
        cr->set_source(pat);

        // Draw corners && edges with enough overlap to avoid
        // artefacts at touching line boundaries.
        cr->move_to(p0x, p0y);
        cr->line_to(p1x, p1y);
        cr->curve_to(p2.at(2), p2.at(3), p2.at(4), p2.at(5), p2.at(0), p2.at(1));
        cr->line_to(p3x, p3y);
        cr->line_to(ptop2x, ptop2y);
        if (concave)
        {
            cr->line_to(ptopbx, ptopby);
        }
        cr->line_to(ptop1x, ptop1y);
        cr->line_to(ptop0x, ptop0y);
        cr->close_path();
        cr->fill();

        edge += 1;
    }
}

void LayoutKeyViewCairo::draw_dish_key(DrawingContext& dc, const RGBA& fill, LOD lod)
{
    Rect canvas_rect = m_item->get_canvas_rect();
    KeyGeometry::Ptr geometry = m_item->geometry;
    if (!geometry)
        geometry = KeyGeometry::from_rect(m_item->get_border_rect());

    Size size_scale = geometry->scale_log_to_size({1.0, 1.0});

    // compensate for smaller size due to missing stroke
    canvas_rect = canvas_rect.inflate(1.0);

    // parameters for the base path
    RGBA base_rgba = fill.brighten(-0.200);
    double stroke_gradient = m_item->get_stroke_gradient();
    double light_dir = m_item->get_light_direction() - M_PI * 0.5;  // 0 = light from top
    double lightx = cos(light_dir);
    double lighty = sin(light_dir);

    Offset key_offset;
    Size key_size;
    m_item->get_key_offset_size(key_offset, key_size, geometry);
    double radius_pct = std::max(config()->current_theme()->roundrect_radius, 2.0);
    radius_pct = std::max(radius_pct, 2.0); // too much +-1 fudging for square corners
    double chamfer_size = m_item->get_chamfer_size();
    chamfer_size = (m_item->get_context()->scale_log_to_canvas_x(chamfer_size) +
                    m_item->get_context()->scale_log_to_canvas_y(chamfer_size)) * 0.5;

    // parameters for the top path, key face
    double stroke_width  = m_item->get_stroke_width();
    Offset key_offset_top = key_offset;
    key_offset_top.y -= config()->dish_key_y_offset * stroke_width;
    Size border = config()->dish_key_border;
    Scale scale_top = 1.0 - (border * stroke_width * size_scale * 2.0);
    Size key_size_top = key_size * scale_top;
    double chamfer_size_top = chamfer_size * (scale_top.x + scale_top.y) * 0.5;

    // create all paths we're going to use
    vector<KeyPath::Polygon> polygons;
    vector<vector<vector<double>>> polygon_paths;
    m_item->get_canvas_polygons(geometry, key_offset, key_size,
                                radius_pct, chamfer_size,
                                polygons, polygon_paths);

    vector<KeyPath::Polygon> polygons_top;
    vector<vector<vector<double>>> polygon_paths_top;
    m_item->get_canvas_polygons(geometry, key_offset_top,
                                key_size_top - size_scale,
                                radius_pct, chamfer_size_top,
                                polygons_top, polygon_paths_top);
    vector<KeyPath::Polygon> polygons_top1;
    vector<vector<vector<double>>> polygon_paths_top1;
    m_item->get_canvas_polygons(geometry, key_offset_top,
                                key_size_top,
                                radius_pct, chamfer_size_top,
                                polygons_top, polygon_paths_top);

    auto cr = dc.get_cc();

    // draw key border
    if (m_item->show_border)
    {
        if (lod == LOD::MINIMAL)
        {
            cr->set_source_rgba(base_rgba);
            for (auto path  : polygon_paths)
            {
                cr->rounded_polygon_path_to_cairo_path(path);
                cr->fill();
            }
        }
        else
        {
            for (size_t i=0; i<polygons.size(); i++)
            {
                auto& polygon = polygons[i];
                auto& polygon_top = polygons_top[i];
                auto& path = polygon_paths[i];
                auto& path_top = polygon_paths_top[i];

                draw_dish_key_border(cr, path, path_top,
                                     polygon, polygon_top,
                                     base_rgba, stroke_gradient,
                                     lightx, lighty);
            }
        }
    }

    // Draw the key face, the smaller top rectangle.
    if (m_item->show_face)
    {
        if (lod == LOD::MINIMAL)
        {
            cr->set_source_rgba(fill);
        }
        else
        {
            // Simulate the concave key dish with a gradient that has
            // a sligthly brighter middle section.
            double angle;
            if (m_item->is_space())
                angle = M_PI / 2.0;  // space has a convex top
            else
                angle = 0.0;       // all others are concave

            double fill_gradient   = config()->current_theme()->key_fill_gradient / 100.0;
            RGBA dark_rgba = fill.brighten(-fill_gradient * 0.5);
            RGBA bright_rgba = fill.brighten(+fill_gradient * 0.5);
            Line line = cr->gradient_line(canvas_rect, angle);

            LinearGradient pat(cr, line);
            pat.add_color_stop_rgba(0.0, dark_rgba);
            pat.add_color_stop_rgba(0.5, bright_rgba);
            pat.add_color_stop_rgba(1.0, dark_rgba);
            cr->set_source(pat);
        }

        for (auto path : polygon_paths_top1)
        {
            cr->rounded_polygon_path_to_cairo_path(path);
            cr->fill();
        }
    }
}

// Build cairo path of the key geometry.
void LayoutKeyViewCairo::build_canvas_path(DrawingContext& dc, const Rect* rect, const KeyPathPtr& path)
{
    if (m_item->geometry)
    {
        if (path)
            build_complex_path(dc, path);
        else
            build_complex_path(dc, m_item->get_canvas_path());
    }
    else
    {
        if (rect)
            build_rect_path(dc, *rect);
        else
            build_rect_path(dc, m_item->get_canvas_rect());
    }
}

void LayoutKeyViewCairo::build_complex_path(DrawingContext& dc, const KeyPathPtr& path)
{
    double roundness = config()->current_theme()->roundrect_radius;
    double chamfer_size = m_item->get_chamfer_size();
    chamfer_size = m_item->get_context()->scale_log_to_canvas_y(chamfer_size);
    build_rounded_path(dc, path, roundness, chamfer_size);
}

void LayoutKeyViewCairo::build_rounded_path(DrawingContext& dc, const KeyPathPtr& path, double r_pct, double chamfer_size)
{
    auto cr = dc.get_cc();
    path->for_each_polygon([&] (const KeyPath::Polygon& polygon) {
        cr->rounded_polygon(polygon, r_pct, chamfer_size);
    });
}

void LayoutKeyViewCairo::build_rect_path(DrawingContext& dc, const Rect& rect)
{
    auto cr = dc.get_cc();
    double roundness = config()->current_theme()->roundrect_radius;
    if (roundness != 0.0)
    {
        cr->roundrect_curve(rect, roundness);
    }
    else
    {
        cr->rectangle(rect);
    }
}

void LayoutKeyViewCairo::build_rect_path_custom(DrawingContext& dc, const Rect& rect, CornerMask corner_mask)
{
    auto cr = dc.get_cc();
    double roundness = config()->current_theme()->roundrect_radius;
    if (roundness != 0.0)
    {
        cr->roundrect_curve(rect, roundness, corner_mask);
    }
    else
    {
        cr->rectangle(rect);
    }
}

template <typename F>
void for_each_label_iteration(LayoutKey* key, LOD lod, const F& func)
{
    double stroke_gradient = key->get_stroke_gradient();
    if (lod == LOD::FULL &&
        key->can_emboss())
    {
        auto root = key->get_layout_root();
        double d = 0.4;  // fake-emboss distance
        //d = max(src_size[1] * 0.02, 0.0)
        double max_offset = 2;

        double alpha = key->get_gradient_angle();
        double xo = root->get_context()->scale_log_to_canvas_x(d * cos(alpha));
        double yo = root->get_context()->scale_log_to_canvas_y(d * sin(alpha));
        xo = std::min(floor(round(xo)), max_offset);
        yo = std::min(floor(round(yo)), max_offset);

        double luminosity_factor = stroke_gradient * 0.25;

        // shadow
        func(Offset(xo, yo), -luminosity_factor, false);

        // highlight
        func(Offset(-xo, -yo), luminosity_factor, false);
    }

    // normal
    func({}, 0.0, true);
}

// Draws the key's optional image.
// Fixme: merge with draw_label, can't do this for 0.99 because
// the Gdk.flush() workaround on the nexus 7 might fail.
void LayoutKeyViewCairo::draw_image(DrawingContext& dc, LOD lod)
{
    if (m_item->image_filenames.empty() || !m_item->show_image)
        return;

    Rect rect = m_item->get_canvas_label_rect();
    if (rect.w < 1 || rect.h < 1)
        return;

    auto image = m_item->get_image(rect.get_size());
    if (!image)
    {
        return;
    }

    Size src_size = image->get_size();
    Offset alignment = m_item->align_label(src_size, rect.get_size());

    Noneable<RGBA> image_rgba;
    if (m_item->has_image_color())
        image_rgba = m_item->get_image_color();
    RGBA fill = m_item->get_fill_color();
    auto cr = dc.get_cc();

    for_each_label_iteration(m_item, lod, [&](const Offset& offset,
                                  double luminosity,
                                  bool is_last)
    {
        // draw dwell progress after fake emboss, before final image
        if (is_last && m_progress.is_dwelling())
            m_progress.draw(dc, m_item->get_dwell_progress_canvas_rect(),
                                m_item->get_dwell_progress_color());

        Rect r = rect.offset(alignment + offset);

        if (image_rgba.is_none())
        {
            image->draw_styled(cr, r, {}, m_item->image_style);
        }
        else
        {
            RGBA rgba;
            if (luminosity != 0.0)
                rgba = fill.brighten(luminosity);  // darker
            else
                rgba = image_rgba;

            image->draw_styled(cr, r, rgba, m_item->image_style);
        }
    });
}

void LayoutKeyViewCairo::draw_label(DrawingContext& dc, LOD lod)
{
    // Skip cairo errors when drawing labels with font size 0
    // This may happen for hidden keys && keys with bad size groups.
    if (m_item->font_size == 0.0 || !m_item->show_label)
        return;

    auto cr = dc.get_cc();
    auto runs = get_label_runs(cr);
    if (runs.empty())
        return;

    RGBA fill = m_item->get_fill_color();

    for_each_label_iteration(m_item, lod, [&](const Offset& offset,
                                  double luminosity,
                                  bool is_last)
    {
        // draw dwell progress after fake emboss, before final image
        if (is_last && m_progress.is_dwelling())
            m_progress.draw(dc, m_item->get_dwell_progress_canvas_rect(),
                                m_item->get_dwell_progress_color());

        for (auto& run : runs)
        {
            RGBA rgba = run.rgba;
            if (luminosity != 0.0)
                rgba = fill.brighten(luminosity); // darker
            cr->move_to(offset + run.offset);
            cr->set_source_rgba(rgba);
            run.text_layout->show(cr);
        }
    });
}

vector<LayoutKeyViewCairo::LabelRun> LayoutKeyViewCairo::get_label_runs(CairoContext* cr)
{
    vector<LabelRun> runs;
    Rect canvas_rect = m_item->get_canvas_label_rect();

    // secondary label
    string label = m_item->get_secondary_label();
    if (!label.empty() &&
        count_codepoints(label) == 1 &&
        config()->keyboard->show_secondary_labels)
    {
        double font_size = m_item->font_size * 0.5;
        auto layout = get_text_renderer(
                label, TextRendererSlot::SECONDARY, font_size);
        Size src_size = layout->get_size();
        Size alignment = m_item->align_secondary_label(src_size,
                                                       canvas_rect.get_size());
        Offset offset = (canvas_rect.get_position() + alignment).floor();
        RGBA rgba = m_item->get_secondary_label_color();

        runs.emplace_back(LabelRun{layout, offset, rgba});
    }

    // popup indicator
    if (!m_item->popup_id.empty() &&
        !config()->is_xid_mode())
    {
        string label_ = get_popup_indicator(cr);
        double font_size = m_item->font_size;
        auto layout = get_text_renderer(
                label_, TextRendererSlot::POPUP_INDICATOR, font_size);

        Size src_size = layout->get_size();
        Size alignment = m_item->align_popup_indicator(src_size,
                                                       canvas_rect.get_size());
        Offset offset = (canvas_rect.get_position() + alignment).floor();
        RGBA rgba = m_item->get_secondary_label_color();

        runs.emplace_back(LabelRun{layout, offset, rgba});
    }

    // main label
    label = m_item->get_label();
    if (!label.empty())
    {
        double font_size = m_item->font_size;
        auto layout = get_text_renderer(
                label, TextRendererSlot::LABEL, font_size);

        Size src_size = layout->get_size();
        Size alignment = m_item->align_label(src_size,
                                             canvas_rect.get_size());
        Offset offset = (canvas_rect.get_position() + alignment).floor();
        RGBA rgba = m_item->get_label_color();

        runs.emplace_back(LabelRun{layout, offset, rgba});
    }

    return runs;
}

// Find the shortest ellipsis possible with the current font.
// The font is assumed to never change during the livetime of the key.
string LayoutKeyViewCairo::get_popup_indicator(CairoContext* cr)
{
    (void)cr;

    if (m_popup_indicator.is_none())
    {
        array<string, 2> labels = {"…", "..."};  // label candidates

        const double BASE_FONTDESCRIPTION_SIZE = 10000000.0;
        Noneable<double> wmin;
        string result = "";
        for (auto& label : labels)
        {
            auto layout = get_text_renderer(label, TextRendererSlot::DEFAULT,
                                            BASE_FONTDESCRIPTION_SIZE);
            double w = layout->get_size().w;
            if (wmin.is_none() || w < wmin)
            {
                wmin = w;
                result = label;
            }
        }
        m_popup_indicator = result;
    }

    return m_popup_indicator;
}

// Attempt to keep PangoLayout memory leaks down by re-using
// global layout objects, but it's not very effective. Pango's
// memory usage is still growing by 10s of MB when resizing.
TextRendererPangoCairo* LayoutKeyViewCairo::get_text_renderer(
        const string& text,
        TextRendererSlot slot,
        double font_size)
{
    int font_size_int = static_cast<int>(font_size);
    auto tr = Super::get_text_renderer(slot, font_size_int);
    if (tr)
        prepare_text_layout(tr, text, font_size_int);
    return tr;
}

void LayoutKeyViewCairo::prepare_text_layout(TextRendererPangoCairo* layout,
                                             const string& text, double font_size)
{
    layout->set_text(text);
    layout->set_font(config()->get_key_label_font(), font_size);
}

// Calculate font-size independent extents.
Size LayoutKeyViewCairo::calc_label_base_extents(const string& label)
{
    const double BASE_FONTDESCRIPTION_SIZE = 10000000.0;
    auto layout = get_text_renderer(label, TextRendererSlot::DEFAULT,
                                    BASE_FONTDESCRIPTION_SIZE);
    prepare_text_layout(layout, label, BASE_FONTDESCRIPTION_SIZE);
    Size sz = layout->get_size();
    sz.w = std::max(sz.w, 1.0);
    sz.h = std::max(sz.h, 1.0);
    return sz / BASE_FONTDESCRIPTION_SIZE;
}


