#ifndef KEYBOARDPOPUP_H
#define KEYBOARDPOPUP_H

#include <memory>

#include "layoutdecls.h"
#include "layoutview.h"
#include "keyboardpopup_fwd.h"
#include "onboardoskglobals.h"


class CairoContext;
class LayoutContext;
class Timer;


// Display magnified labels as touch feedback
class TouchFeedback : public ContextBase
{
    public:
        using Super = ContextBase;
        using This = TouchFeedback;

        TouchFeedback(const ContextBase& context);
        ~TouchFeedback();

        void show(const LayoutKeyPtr& key, const LayoutView* view);
        void hide(const LayoutKeyPtr& key={});

    private:
        // Get a currently unused one from the pool of popups.
        LabelPopupPtr get_free_key_feedback_popup();
        Size get_popup_size(const ViewBase* view_for_monitor,
                            const LayoutKeyPtr& key);
        void do_hide(LabelPopup* popup);

    private:
        std::vector<LabelPopupPtr> m_key_feedback_popup_pool;
        std::map<LayoutKey*, LabelPopup*> m_visible_key_feedback_popups;
};


class KeyboardPopup
{
    public:
        KeyboardPopup();

        virtual ViewBase* get_view() = 0;

        LayoutKeyPtr get_key();
        void set_key(const LayoutKeyPtr& key);

        Point compute_position(const Rect& key_root_rect,
                               const Size& popup_size);

        Rect limit_to_workarea(const Rect& rect);

        double get_opacity();

    protected:
        virtual void on_toplevel_geometry_changed();
        virtual void recalc_geometry() {}
        virtual Rect get_always_visible_rect();

    protected:
        LayoutKeyPtr m_key;
};


class LabelPopup : public ViewBase, public KeyboardPopup
{
    public:
        using Super = ViewBase;

        LabelPopup(const ContextBase& context, const std::string& name_);

        virtual void destroy() override;
        virtual void draw(DrawingContext& dc) override;

    private:
        virtual void on_event(OnboardOskEvent* event) override;

        void draw_text(CairoContext* cc, const std::string& text,
                       const Rect& rect, const RGBA& rgba);


        TextRendererPangoCairo* get_text_renderer(const std::string& text,
                                                  double font_size);

        void prepare_text_layout(TextRendererPangoCairo* layout,
                                 const std::string& text, double font_size);

        // Calculate font-size independent extents.
        Size calc_label_base_extents(const std::string& label);

        double calc_font_size(const Rect& rect, const Size& base_extents);

        // KeyboardPopup overloads
        virtual ViewBase* get_view() override {return this;}
        virtual void on_toplevel_geometry_changed() override
        {KeyboardPopup::on_toplevel_geometry_changed();}
        virtual void recalc_geometry() override
        {KeyboardPopup::recalc_geometry();}
        virtual Rect get_always_visible_rect() override
        {return KeyboardPopup::get_always_visible_rect();}
};

class LayoutPopup : public LayoutView, public KeyboardPopup
{
    public:
        using Super = LayoutView;

        LayoutPopup(const ContextBase& context,
                    const std::string& name_);
        virtual ~LayoutPopup();

        void set_layout(const LayoutItemPtr& layout, double frame_width);
        void set_close_callback(const std::function<void(LayoutPopup*)>& func);

        double get_frame_width();
        Size compute_size();

        // Has the pointer ever entered the popup?
        bool got_motion();

        virtual void on_input_sequence_begin(const InputSequencePtr& sequence) override;
        virtual void on_input_sequence_update(const InputSequencePtr& sequence) override;
        virtual void on_input_sequence_end(const InputSequencePtr& sequence) override;

        virtual void draw(DrawingContext& dc) override;

        virtual void draw_view_frame(DrawingContext& dc) override;

        void on_popup_done();

    private:
        // KeyboardPopup overloads
        virtual ViewBase* get_view() override {return this;}
        virtual void on_toplevel_geometry_changed() override
        {KeyboardPopup::on_toplevel_geometry_changed();}
        virtual void recalc_geometry() override
        {KeyboardPopup::recalc_geometry();}
        virtual Rect get_always_visible_rect() override
        {return KeyboardPopup::get_always_visible_rect();}

    private:
        double m_frame_width{};
        bool m_drag_selected{false};   // grazed by the pointer?
        std::function<void(LayoutPopup*)> m_close_callback;
        std::unique_ptr<Timer> m_unpress_timer;
};


class LayoutBuilder : public ContextBase
{
    public:
        using Super = ContextBase;

        LayoutBuilder(const ContextBase& context);

        std::tuple<LayoutRootPtr, double> build(
                const LayoutKeyPtr& source_key,
                const LayoutItemPtr& layout);

        double calc_frame_width(LayoutContext* context);
};


class LayoutBuilderKeySequence : public LayoutBuilder
{
    public:
        using Super = LayoutBuilder;
        using KeySequence = std::vector<LayoutKeyPtr>;
        using KeyLines = std::vector<std::vector<LayoutKeyPtr>>;

        LayoutBuilderKeySequence(const ContextBase& context);

        std::tuple<LayoutRootPtr, double> build(
                const LayoutKeyPtr& source_key,
                const KeySequence& key_sequence);

        std::tuple<LayoutRootPtr, double> create_layout(
                const LayoutKeyPtr& source_key,
                const KeyLines& lines, size_t ncolumns);

        // Split sequence into lines.
        size_t layout_sequence(KeyLines& lines, const KeySequence& sequence);
};

class LayoutBuilderAlternatives : public LayoutBuilderKeySequence
{
    public:
        using Super = LayoutBuilderKeySequence;

        LayoutBuilderAlternatives(const ContextBase& context);

        std::tuple<LayoutRootPtr, double> build(
                const LayoutKeyPtr& source_key,
                const std::vector<std::string>& alternatives);
};


#endif // KEYBOARDPOPUP_H
