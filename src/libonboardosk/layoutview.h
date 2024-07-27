#ifndef LAYOUTVIEW_H
#define LAYOUTVIEW_H

#include <memory>
#include <set>

#include "tools/color.h"
#include "tools/rect_fwd.h"

#include "configdecls.h"
#include "inputsequencetarget.h"
#include "keyboarddecls.h"
#include "keyboardpopup_fwd.h"
#include "layoutdecls.h"
#include "onboardoskglobals.h"
#include "toplevelview.h"

enum class BackgroundStyle
{
    CLEAR,                  // fully transparent
    OPAQUE,                 // no transparency
    TRANSPARENT,            // transparency from color scheme
    TRANSPARENT_XEMBED,     // XEmbed mode with transparency as used for unity-greeter (deprecated)
};

class DrawingContext;
class LayoutPopup;
class InputEventReceiver;

class LayoutView : public ViewBase, public InputSequenceTarget
{
    public:
        using Super = ViewBase;
        using This = LayoutView;
        using Ptr = std::shared_ptr<This>;

        LayoutView(const ContextBase& context,
                   OnboardOskViewType view_type_,
                   const std::string& name_);

        Ptr getptr() {
            return std::dynamic_pointer_cast<LayoutView>(shared_from_this());
        }

        void set_layout_subtree(const LayoutItemPtr& root_item)
        { m_layout_subtree = root_item; }

        LayoutItemPtr get_layout()
        { return m_layout_subtree; }
        InputEventReceiver* get_input_event_receiver() {return m_input_event_receiver.get();}

        void on_layout_loaded();
        virtual void on_transition_done(bool visible_before, bool visible_now)
        {
            (void)visible_before;
            (void)visible_now;
        }

        LOD get_lod() { return m_lod; }
        void set_lod(LOD lod) { m_lod = lod; }
        // Reset to full level of detail
        void reset_lod();

        void invalidate_for_resize();

        // Clear cached key surfaces, e.g. after resizing,
        // change of theme settings.
        void invalidate_keys();

        // Clear cached images, e.g. after changing window_scaling_factor.
        void invalidate_images();

        // Clear cached shadow surfaces, e.g. after resizing,
        // change of theme settings.
        void invalidate_shadows();

        void invalidate_shadow_quality()
        {
            m_shadow_quality_valid = false;
        }

        // Clear cached resolution independent label extents, e.g.
        // after changes to the systems font dpi setting (gtk-xft-dpi).
        void invalidate_label_extents();

        void redraw_items(const std::vector<LayoutItemPtr> items, bool invalidate=false);
        void process_updates();

        virtual void update_layout();

        virtual void draw(DrawingContext& dc);

        virtual void show_touch_handles(bool show) {(void)show;}
        virtual void set_touch_handles_active(bool active) {(void)active;}
        virtual void reset_touch_handles() {}
        virtual void on_touch_handles_opacity(double opacity, bool done) {(void)opacity; (void)done;}

        // Popup with predefined layout items.
        virtual void show_popup_layout(const LayoutKeyPtr& key, const LayoutItemPtr& sublayout);

        // Popup with alternative chars, aka internatioanl character selection
        virtual void show_popup_alternative_chars(const LayoutKeyPtr& key, const std::vector<std::string>& alternatives);

        virtual void show_prediction_menu(const LayoutKeyPtr& key, MouseButton::Enum button)
        {
            (void) key;
            (void) button;
        }
        virtual void edit_snippet(int snippet_id)
        {
            (void) snippet_id;
        }

        Rect get_base_aspect_rect();
        Rect get_view_frame_rect();

        virtual void on_event(OnboardOskEvent* event) override;
        bool has_input_sequences();

        virtual void start_move_view() {}
        virtual void stop_move_view() {}

        Point get_last_sequence_point() {return m_last_sequence_point;}

        // For popups: get the view the popup originated from.
        // Else assumes this is the originating view itself.
        LayoutView* get_originating_view();
        void set_originating_view(const LayoutViewPtr& view);

    protected:
        // InputSequenceTarget interface
        virtual void on_input_sequence_update(const InputSequencePtr& sequence) override;

        LayoutKeyPtr get_key_at_location(const Point& point);
        bool is_decorated() {return get_background_style() == BackgroundStyle::TRANSPARENT;}
        virtual AspectChangeRange get_docking_aspect_change_range() {return {};}

        RGBA get_layer_fill_rgba(size_t layer_index);
        RGBA get_background_rgba();
        RGBA get_popup_window_rgba(const std::string& element);

    private:
        BackgroundStyle get_background_style();

        void draw_background(DrawingContext& dc, BackgroundStyle bgstyle);
        void clear_background(DrawingContext& dc);
        void draw_opaque_background(DrawingContext& dc, size_t layer_index=0);
        void draw_transparent_background(DrawingContext& dc);
        void draw_xembed_background(DrawingContext& dc);
        void draw_layer_background(DrawingContext& dc, LayoutItemPtr& item, const std::vector<std::string>& layer_ids);
        void draw_layer_key_background(DrawingContext& dc, double alpha = 1.0,
                                       LayoutItemPtr item={}, std::string layer_id={});

        void draw_side_bars(DrawingContext& dc);
        bool can_draw_frame();
        bool can_draw_sidebars();
        virtual void draw_view_frame(DrawingContext& dc);
        void draw_keyboard_frame(DrawingContext& dc);

        Rect get_damage_rect(DrawingContext& dc);
        Rect get_canvas_content_rect();
        Rect get_aspect_corrected_layout_rect(const Rect& rect,
                                              const Rect& base_aspect_rect);

        LayoutPopupPtr create_key_popup();
        void show_key_popup(const LayoutPopupPtr& popup,
                            const LayoutKeyPtr& key);

    private:
        std::unique_ptr<InputEventReceiver> m_input_event_receiver;

        LayoutItemPtr m_layout_subtree;
        LayoutViewPtr m_originating_view;  // view for resizing

        bool m_shadow_quality_valid{false};

        LOD m_lod{LOD::FULL};

        Point m_last_sequence_point;
};

typedef LayoutView::Ptr LayoutViewPtr;

#endif // LAYOUTVIEW_H
