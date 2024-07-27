#ifndef LAYOUTKEYVIEWCAIRO_H
#define LAYOUTKEYVIEWCAIRO_H


#include "tools/color.h"
#include "tools/noneable.h"
#include "tools/point_decl.h"
#include "tools/rect_fwd.h"

#include "onboardoskglobals.h"
#include "layoutdecls.h"


class CairoContext;
class DrawingContext;
class LayoutKey;
typedef int CornerMask;

class LayoutItemView : public ContextBase
{
    public:
        using Super = ContextBase;

        LayoutItemView(const ContextBase& context) :
            Super(context)
        {}
        virtual ~LayoutItemView();

        virtual void draw_item(DrawingContext& dc) = 0;
        virtual Size calc_label_base_extents(const std::string& label) = 0;
};


class LayoutKeyView : public LayoutItemView
{
    public:
        using Super = LayoutItemView;

        LayoutKeyView(const ContextBase& context, LayoutKey* item) :
            Super(context),
            m_item(item)
        {}
        virtual ~LayoutKeyView();

    protected:
        LayoutKey* m_item{};

};

class DwellProgressController;

class DwellProgressViewCairo
{
    public:
        DwellProgressViewCairo(DwellProgressController* c);
        class DwellProgressController* m_controller{};

    public:
        bool is_dwelling();

        void draw(DrawingContext& dc, const Rect& rect,
                                 const RGBA& rgba={1, 0, 0, .75},
                                 const Noneable<RGBA>& rgba_bg={});
    private:
        void draw_progress_indicator(CairoContext* cr, const Rect& rect,
                                  const RGBA& rgba,
                                  const Noneable<RGBA>& rgba_bg);
};

class TextRendererPangoCairo;

class LayoutKeyViewCairo : public LayoutKeyView
{
    private:
        struct LabelRun {
                TextRendererPangoCairo* text_layout;
                Offset offset;
                RGBA rgba;
        };

    public:
        using Super = LayoutKeyView;
        using This = LayoutKeyViewCairo;

        LayoutKeyViewCairo(const ContextBase& context, LayoutKey* key);
        virtual ~LayoutKeyViewCairo();
        static std::unique_ptr<This> make(const ContextBase& context, LayoutKey* key);

        virtual void draw_item(DrawingContext& dc) override;
        virtual Size calc_label_base_extents(const std::string& label) override;

    private:
        bool can_draw_cached() {return false;}
        void draw(DrawingContext& dc, LOD lod=LOD::FULL);
        void draw_geometry(DrawingContext& dc, LOD lod);
        void draw_flat_key(DrawingContext& dc, const RGBA& fill, double line_width);
        void draw_gradient_key(DrawingContext& dc, const RGBA& fill, double line_width, LOD lod);
        void draw_dish_key(DrawingContext& dc, const RGBA& fill, LOD lod);
        void build_canvas_path(DrawingContext& dc, const Rect* rect={},  const KeyPathPtr& path={});
        void build_complex_path(DrawingContext& dc, const KeyPathPtr& path);
        void build_rounded_path(DrawingContext& dc, const KeyPathPtr& path, double r_pct, double chamfer_size);
        void build_rect_path(DrawingContext& dc, const Rect& rect);
        void build_rect_path_custom(DrawingContext& dc, const Rect& rect, CornerMask corner_mask);
        void draw_image(DrawingContext& dc, LOD lod);
        void draw_label(DrawingContext& dc, LOD lod);
        std::vector<LabelRun> get_label_runs(CairoContext* cr);
        TextRendererPangoCairo* get_text_renderer(const std::string& text, TextRendererSlot slot, double font_size);
        void prepare_text_layout(TextRendererPangoCairo* layout, const std::string& text, double font_size);
        std::string get_popup_indicator(CairoContext* cr);

    private:
        Noneable<std::string> m_popup_indicator;
        DwellProgressViewCairo m_progress;
};

#endif // LAYOUTKEYVIEWCAIRO_H
