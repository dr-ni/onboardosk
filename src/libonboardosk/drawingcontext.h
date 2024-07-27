#ifndef DRAWINGCONTEXT_H
#define DRAWINGCONTEXT_H

#include <functional>
#include <memory>

#include "tools/color.h"
#include "tools/rect_decl.h"
#include "layoutdecls.h"


struct _cairo;
typedef struct _cairo cairo_t;
class CairoContext;

// Yet another context; this one helps to pass around all the
// values needed for drawing LayoutItems.
class DrawingContext
{
    public:
        DrawingContext(cairo_t* m_cr);
        CairoContext* get_cc() {return m_cr.get();}

        void draw_layer_background(LayoutItemPtr item)
        {
            if (draw_layer_background_func)
                draw_layer_background_func(*this, item);
        }

    public:
        std::shared_ptr<CairoContext> m_cr{};
        Rect draw_rect;
        LOD lod{LOD::FULL};
        bool draw_cached{true};
        class LayoutView* view{};
        std::function<void (DrawingContext&, LayoutItemPtr&)> draw_layer_background_func;
};



#endif // DRAWINGCONTEXT_H
