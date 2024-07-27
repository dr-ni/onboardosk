#ifndef TOPLEVELVIEW_H
#define TOPLEVELVIEW_H

#include <vector>

#include "tools/point_decl.h"
#include "tools/rect_decl.h"

#include "onboardoskglobals.h"
#include "onboardoskview.h"
#include "signalling.h"

struct _OnboardOskEvent;
typedef struct _OnboardOskEvent OnboardOskEvent;

class DrawingContext;

class ViewBase : public OnboardOskView,
                 public ContextBase,
                 public std::enable_shared_from_this<ViewBase>
{
    public:
        using Ptr = std::shared_ptr<ViewBase>;

        ViewBase(const ContextBase& context,
                 OnboardOskViewType view_type_,
                 const std::string& name_);
        virtual ~ViewBase();

        virtual void destroy();

        Ptr getptr() {
            return std::enable_shared_from_this<ViewBase>::shared_from_this();
        }

        // Unique names make it easier to lookup views in JavaScript.
        void set_name(const std::string& name_);
        OnboardOskViewType get_view_type() const;

        ViewBase* get_parent() {return m_parent;}
        void set_parent(ViewBase* parent) {m_parent = parent;}
        virtual std::vector<Ptr>& get_children() {return m_children;}

        ViewBase* find_view_at_point(const Point& point);

        bool is_toplevel() {return m_parent == nullptr;}

        // notify toolkit-dependent code of a newly added view
        void notify_toplevel_added();

        // notify toolkit-dependent code of imminent view removal
        void notify_toplevel_remove();

        // called from the toolkit-dependent code, only for toplevels views
        virtual void on_toplevel_geometry_changed() {on_geometry_changed();}

        // called for all views when their geometry changes
        virtual void on_geometry_changed() {}

        // update this view's geometry from toolkit-dependent code
        virtual bool is_geometry_valid() {return m_rect_valid;}
        virtual void invalidate_geometry() {m_rect_valid = false;}
        virtual void update_geometry();

        // recalculate all views' geometry
        virtual void recalc_geometry() = 0;

        // Current rect of the keyboard in parent coordinates. If
        // there is no parent view (m_parent == nullptr), this is a
        // toplevel view and the rect is in root window/stage coordinates.
        Rect get_rect() const {return m_rect;}
        virtual void set_rect(const Rect& rect) {m_rect = rect;}
        void set_origin(const Offset& origin) {m_origin = origin;}

        Rect get_canvas_rect() {return {Point{0.0, 0.0}, m_rect.get_size()};}

        // Convert canvas coordinates to parent window coordinates.
        // If there is no parent window, consider the result to be in
        // root window/stage coordinates;
        Point canvas_to_parent(const Point& point) const;
        Rect canvas_to_parent(const Rect& rect) const;
        Point canvas_to_root(const Point& point) const;
        Rect canvas_to_root(const Rect& rect) const;

        Point parent_to_canvas(const Point& point) const;
        Rect parent_to_canvas(const Rect& rect) const;
        Point root_to_canvas(const Point& point) const;
        Rect root_to_canvas(const Rect& rect) const;

        virtual Point get_position();
        virtual Size get_size();

        virtual Rect get_limit_rect();  // rect of root window/clutter stage

        // Limit the given window position to monitor boundaries in order
        // to keep the current always_visible_rect fully in view.
        virtual Point limit_position(const Point& pt);

        // Limits the given window position to keep the
        // always_visible_rect fully in view.
        static Point limit_position(const Point& pt, const Rect& always_visible_rect,
                                    const std::vector<Rect>& limit_rects);

        Rect limit_size(const Rect& rect);

        virtual std::vector<Rect> get_monitor_rects();  // rect of each monitor

        // toolkit-dependent calls for toplevel views only
        virtual void set_cursor_type(OnboardOskCursorType cursor_type);
        virtual void move(const Point& pt);
        virtual void move_resize(const Rect& r);
        virtual void set_visible(bool visible);

        // Rectangle in canvas coordinates that must not leave the screen.
        virtual Rect get_always_visible_rect() = 0;

        virtual void redraw();
        virtual void redraw_area(const Rect& r);

        virtual void draw(DrawingContext& dc) = 0;

        virtual void on_event(OnboardOskEvent* event) = 0;

        virtual ViewBase* find_toplevel_view_from_leaf();
        virtual const ViewBase* find_toplevel_view_from_leaf() const;

        // Traverse tree depth-first, pre-order.
        template <typename F>
        void for_each(const F& func)
        {
            func(getptr());

            for (auto& child : get_children())
                child->for_each(func);
        }

    protected:
        virtual Rect on_restore_view_rect(const Rect& rect) {return rect;}
        virtual Rect on_save_view_rect(const Rect& rect) {return rect;}

        // Read orientation dependent rect.
        // Overload this in derived classes.
        virtual Rect read_window_rect(bool is_landscape)
        {(void)is_landscape; return {};}

        // Write orientation dependent rect.
        // Overload this in derived classes.
        virtual void write_window_rect(bool is_landscape, const Rect& rect)
        {(void)is_landscape; (void)rect;}

    private:
        Point parent_to_canvas_recursive(const Point& point) const;

    public:
        // signal called on destruction (for KeyboardView::on_event)
        DEFINE_SIGNAL(<>, destroy_notify, this);

    protected:
        std::string m_name;     // storage for OnboardOskView::name
        ViewBase* m_parent{};   // parent view or nullptr if toplevel view
        std::vector<Ptr> m_children;  // child views, if there are any

        // Bounding rect of the main layout views, canvas coordinates.
        Rect m_rect;
        bool m_rect_valid{false};

        // Offset to add for the begin of the canvas area with
        // window decoration present.
        Offset m_origin;

        friend class ViewRectPersist;
};

typedef ViewBase::Ptr ViewBasePtr;

// Geometry and physical size of the monitor the view is visible on.
void get_monitor_dimensions(const ViewBase* view, Size& size, Size& size_mm);

// Convert a physical size in mm to pixels of windows's monitor,
Size physical_to_monitor_pixel_size(ViewBase* view, const Size& size_mm,
                                    Size fallback_size={});

#endif // TOPLEVELVIEW_H
