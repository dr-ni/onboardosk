#ifndef CAIROCONTEXT_H
#define CAIROCONTEXT_H

#include <memory>
#include <vector>

#include "color.h"
#include "point_decl.h"
#include "rect_fwd.h"

struct _cairo;
typedef struct _cairo cairo_t;
struct _cairo_pattern;
typedef struct _cairo_pattern cairo_pattern_t;
struct _cairo_surface;
typedef struct _cairo_surface cairo_surface_t;
struct _cairo_matrix;
typedef struct _cairo_matrix cairo_matrix_t;

typedef int CornerMask;

class CairoContext;

class CairoPattern
{
    public:
        CairoPattern(CairoContext* cr, cairo_pattern_t* pattern=nullptr);
        CairoPattern(const CairoPattern& pattern);
        virtual ~CairoPattern();

        CairoPattern& operator=(const CairoPattern& pattern);

        unsigned get_refcount();

    protected:
        CairoContext* m_cr;
        cairo_pattern_t* m_pattern;

        friend CairoContext;
};


class LinearGradient : public CairoPattern
{
    public:
        LinearGradient(CairoContext* cr, const Line& l);

        void add_color_stop_rgba(double offset, const RGBA& rgba);
};


class CairoSurface
{
    public:
        CairoSurface(cairo_surface_t* surface=nullptr);
        CairoSurface(const CairoSurface& surface);
        CairoSurface(const Size& size);
        virtual ~CairoSurface();

        CairoSurface& operator=(const CairoSurface& surface);

        bool ok();
        unsigned get_refcount();

    protected:
        CairoContext* m_cr;
        cairo_surface_t* m_surface;

        friend CairoContext;
};

class CairoMatrix
{
    public:
        CairoMatrix();
        CairoMatrix(const CairoMatrix& matrix);
        virtual ~CairoMatrix();

        CairoMatrix& operator=(const CairoMatrix& matrix);
        void translate(const Offset& offset);
        void scale(const Scale& scale);
        void rotate(double alpha);

        Point transform(const Point& pt);

    protected:
        std::unique_ptr<cairo_matrix_t> m_matrix;

        friend CairoContext;
};

class CairoContext
{
    public:
        enum Operator  // = cairo_operator_t
        {
            CLEAR,

            SOURCE,
            OVER,
            IN,
            OUT,
            ATOP,

            DEST,
            DEST_OVER,
            DEST_IN,
            DEST_OUT,
            DEST_ATOP,

            XOR,
            ADD,
            SATURATE,

            MULTIPLY,
            SCREEN,
            OVERLAY,
            DARKEN,
            LIGHTEN,
            COLOR_DODGE,
            COLOR_BURN,
            HARD_LIGHT,
            SOFT_LIGHT,
            DIFFERENCE,
            EXCLUSION,
            HSL_HUE,
            HSL_SATURATION,
            HSL_COLOR,
            HSL_LUMINOSITY
        };

        enum Content // = cairo_content_t
        {
            COLOR       = 0x1000,
            ALPHA       = 0x2000,
            COLOR_ALPHA	= 0x3000
        };

    public:
        CairoContext(cairo_t* cr);
        CairoContext(CairoSurface* surface);
        ~CairoContext();

        void save();
        void restore();

        void set_source_rgba(const RGBA& rgba);
        void set_source(CairoPattern& pat);
        void set_operator(Operator op);

        void rectangle(const Rect& r);

        void move_to(const Point& pt) {move_to(pt.x, pt.y);}
        void move_to(double x, double y);
        void line_to(const Point& pt) {line_to(pt.x, pt.y);}
        void line_to(double x, double y);
        void arc(double xc, double yc, double radius, double angle1, double angle2);
        void arc(const Point& center, double radius, double angle1, double angle2);
        void curve_to(double x1, double y1, double x2, double y2, double x3, double y3);

        void new_path();
        void close_path();

        void set_line_width(double w);

        void stroke();
        void fill();
        void fill_preserve();
        void paint();
        void paint_with_alpha(double alpha);

        bool in_fill(const Point& pt);

        void mask(const CairoPattern& pattern);

        void translate(const Offset& offset);
        void scale(const Scale& scale);
        void rotate(double angle);

        CairoPattern get_source();
        void push_group();
        void push_group_with_content(Content content);
        CairoPattern pop_group();
        void pop_group_to_source();

        Rect get_clip_rect();
        void clip();

        // custom extensions
        static Line gradient_line(const Rect& rect, double alpha);

        void roundrect_arc(const Rect& rect, double r=15);

        // Uses B-splines for less even looks than with arcs, but
        // still allows for approximate circles at r_pct = 100.0.
        void roundrect_curve(const Rect& rect, double r_pct = 100, CornerMask corner_mask = 0b1111);

        void rounded_polygon(const std::vector<double> coords, double r_pct, double chamfer_size);
        void rounded_polygon_path_to_cairo_path(const std::vector<std::vector<double>>& path);

        // Mostly works, but has issues with clipping artefacts for
        // damage rects smaller than the full window rect.
        void drop_shadow(CairoPattern& pattern, const Rect& bounds, double blur_radius = 4.0,
                         const Offset& offset={0, 0}, double alpha=0.06, int steps=4);

        cairo_t* get_cr() {return m_cr;}

    private:
        cairo_t* m_cr{};
        cairo_surface_t* m_surface{};  // optional surface
};



#endif // CAIROCONTEXT_H
