#ifndef TOUCHHANDLES_H
#define TOUCHHANDLES_H

#include <vector>

#include "tools/cairocontext.h"
#include "tools/rect_fwd.h"

#include "handle.h"
#include "onboardoskglobals.h"

class DrawingContext;
class LayoutView;

class TouchHandle : public ContextBase
{
    public:
        using Super = ContextBase;
        TouchHandle(const ContextBase& context, Handle::Enum id);
        ~TouchHandle();

        Rect get_rect() const;
        double get_radius() const;
        Rect get_shadow_rect() const;
        double get_arrow_angle() const;
        bool is_edge_handle() const;
        bool is_corner_handle() const;

        void update_position(const Rect& canvas_rect);

        bool hit_test(const Point& point) const;

        void draw(DrawingContext& dc);
        void redraw();

    private:
        void draw_handle(CairoContext* cc, double alpha_factor);
        void draw_handle_shadow(CairoContext* cc);
        void draw_arrows(CairoContext* cc);
        void draw_arrow(CairoContext* cc);
        void build_handle_path(CairoContext* cc) const
        {
            build_handle_path(cc, get_rect());
        }
        void build_handle_path(CairoContext* cc, const Rect& rect) const
        {
            build_handle_path(cc, rect, get_radius());
        }
        void build_handle_path(CairoContext* cc, const Rect& rect, double radius) const;

    public:
        Handle::Enum id{Handle::NONE};
        bool prelight{false};
        bool pressed{false};
        double corner_radius{0.0};     // radius of the outer corners (window edges)
        bool lock_x_axis{false};
        bool lock_y_axis{false};

    private:
        friend class TouchHandles;

        LayoutView* m_view{};

        Size m_size{40.0, 40.0};
        static constexpr const Size m_fallback_size{40, 40};
        double m_hit_proximity_factor{1.5};
        Rect m_rect;
        double m_scale{1.0};   // scale of handle relative to resize handles
        double m_handle_alpha{0.45};
        double m_shadow_alpha{0.04};
        double m_shadow_size{8};
        Offset m_shadow_offset{0.0, 2.0};
        double m_screen_dpi{96};

        using HandleAnglesMap = std::map<Handle::Enum, double>;
        static  HandleAnglesMap m_handle_angles;
};



class TouchHandles : public ContextBase
{
    public:
        using Super = ContextBase;
        TouchHandles(const ContextBase& context);
        ~TouchHandles();

        void set_active(bool active)
        { m_active = active; }

        bool is_active()
        { return m_active; }

        void set_opacity(double opacity)
        { m_opacity = opacity; }

        template<class T>   // T may be vector or array
        void set_active_handles(const T& handle_ids)
        {
            m_handles.clear();
            for (auto& handle : m_handle_pool)
                if (contains(handle_ids, handle->id))
                    m_handles.emplace_back(handle.get());
        }

        void set_view(LayoutView* view);
        void update_positions(const Rect& canvas_rect);
        void draw(DrawingContext& dc);
        void redraw();
        Handle::Enum hit_test(const Point& point);
        void set_prelight(Handle::Enum handle_id);
        void set_pressed(Handle::Enum handle_id);
        void set_corner_radius(double corner_radius);
        void set_monitor_dimensions(const Size& size_px, const Size& size_mm);

        // Set to false to constraint movement in x.
        void lock_x_axis(bool lock);

        // Set to true to constraint movement in y.
        void lock_y_axis(bool lock);

    private:
        double m_opacity{1.0};
        bool m_active{false};
        Rect m_rect;
        std::vector<std::unique_ptr<TouchHandle>> m_handle_pool;
        std::vector<TouchHandle*> m_handles;
};



#endif // TOUCHHANDLES_H
