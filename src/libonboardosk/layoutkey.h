#ifndef LAYOUTKEY_H
#define LAYOUTKEY_H

#include <memory>
#include <string>

#include "tools/enum_helpers.h"
#include "tools/image.h"
#include "tools/noneable.h"

#include "keyboarddecls.h"
#include "keydefinitions.h"
#include "keygeometry.h"
#include "layoutdrawingitem.h"
#include "stickybehavior.h"


typedef std::map<ModMask, std::string> LabelMap;

MAKE_ENUM(KeyType,
    NONE,
    CHAR,
    KEYSYM,
    KEYCODE,
    MACRO,
    SCRIPT,
    KEYPRESS_NAME,   // never used?
    BUTTON,
    LEGACY_MODIFIER,
    PREDICTION,
    CORRECTION,
)

enum class ImageSlot
{
    NORMAL = 0,
    ACTIVE = 1,
};
typedef std::map<ImageSlot, std::string> ImageFilenameMap;
typedef struct {
    Size requested_size;
    ImagePtr image;
} ImageCacheEntry;
typedef std::map<ImageSlot, ImageCacheEntry> ImageCacheMap;

typedef std::map<ModMask, Size> LabelBaseSizeMap;

enum class ImageStyle;
class LayoutKeyView;
class DwellProgressController;

class LayoutKey : public LayoutDrawingItem
{
    public:
        // optional id of a sublayout used as long-press popup
        std::string popup_id;

        // Type of action to do when key is pressed.
        KeyAction::Enum action{KeyAction::NONE};

        // Type of key stroke to send
        KeyType type{};

        // Data used in sending key strokes.
        union
        {
            KeyCode keycode{};
            KeySym keysym;
            size_t keyindex;
        };
        std::string keystr;    // consider these to be in the union too
        std::string macro_id;
        std::string script_name;

        // name of the layer to control (for layer buttons)
        std::string target_layer_id;

        // Keys that stay stuck when pressed like modifiers.
        bool sticky{false};

        // Behavior if sticky is enabled, see StickyBehavior.
        StickyBehavior::Enum sticky_behavior;

        // modifier bit
        Modifier::Enum modifier{Modifier::NONE};

        // true when key is being hovered over (!implemented yet)
        bool prelight{false};

        // true when key is being pressed.
        bool pressed{false};

        // true when key stays 'on'
        bool active{false};

        // true when key is sticky && pressed twice.
        bool locked{false};

        // true when Onboard is in scanning mode && key is highlighted
        bool scanned{false};

        // true when action was triggered e.g. key-strokes were sent on press
        bool activated{false};

        // Size to draw the label text in Pango units
        double font_size{1.0};

        // Labels which are displayed by this key
        LabelMap labels;  // {modifier_mask : label, ...}

        // label that is currently displayed by this key
        std::string label;

        // mod_mask for the currently configured label
        ModMask m_mod_mask{};

        // smaller label of a currently invisible modifier level
        std::string secondary_label;

        // Images displayed by this key (optional)
        ImageFilenameMap image_filenames;

        // true for mask images to be drawn with the key's label color.
        // false for multi-color images.
        ImageStyle image_style{ImageStyle::SINGLE_COLOR};

        // horizontal label alignment
        double label_x_align;

        // vertical label alignment
        double label_y_align;

        // label margin (x, y)
        Size label_margin;

        // tooltip text
        std::string tooltip;

        // can show label popup
        //bool label_popup = true;   // unused, apparently

        // optional path data for keys with arbitrary shapes
        KeyGeometry::Ptr geometry;

        // size of rounded corners at 100% round_rect_radius
        Noneable<double> chamfer_size;

        // Optional key_style to override the default theme's style.
        Noneable<KeyStyle::Enum> style;

        // Toggles for what gets drawn.
        bool show_face{true};
        bool show_border{true};
        bool show_label{true};
        bool show_image{true};

        // Allow to display active state, i.e. either latched or locked state.
        // Depending on sticky_behavior the button will still become logically
        // active, it just isn't shown. Used for layer0 buttons mainly. They don't
        // need to stick out, it's usually obvious when the first layer is active.
        bool show_active{true};

        std::unique_ptr<LayoutKeyView> view;

    public:
        using Super = LayoutDrawingItem;
        using This = LayoutKey;
        using Ptr = std::shared_ptr<This>;

        LayoutKey(const ContextBase& context);
        virtual ~LayoutKey() override;

        static constexpr const char* CLASS_NAME = "LayoutKey";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        static std::unique_ptr<This> make(const ContextBase& context);
        std::unique_ptr<LayoutKeyView> make_view(const ContextBase& context);

        virtual void dump(std::ostream& out) const override;

        Ptr getptr() {
            return std::dynamic_pointer_cast<LayoutKey>(shared_from_this());
        }

        std::string get_svg_id()
        {
            return this->svg_id;
        }

        virtual void get_state(KeyState& state);

        std::string& get_label()
        {
            return this->label;
        }

        std::string& get_secondary_label()
        {
            return this->secondary_label;
        }

        void configure_labels(ModMask mod_mask);

        std::string get_label(ModMask mod_mask) const;

        // secondary label, usually the label of the shift state
        std::string get_secondary_label(const std::string primary_label, ModMask mod_mask);

        virtual bool is_key() const override
        { return true; }

        bool is_layer_button() const;
        bool is_sublayer_button() const;

        bool is_prediction_key() const;

        bool is_correction_key() const;

        bool is_word_suggestion() const;

        // Modifiers are all latchable/lockable non-button keys:;
        // "LWIN", "RTSH", "LFSH", "RALT", "LALT",
        // "RCTL", "LCTL", "CAPS", "NMLK";
        bool is_modifier() const;

        bool is_click_type_key() const;
        bool is_button() const;
        bool is_pressed_only() const;
        bool is_active_only() const;
        bool is_text_changing() const;
        bool is_return() const;
        bool is_space() const;
        bool is_separator() const;

        // Should this key cancel pending word separators?
        bool is_separator_cancelling() const;

        size_t get_layer_index();

        std::string get_target_layer_id();

        // Split off the parent path of the (dot-separated)
        // target_layer_id.
        std::string get_target_layer_parent_id();

        LayoutItem::Ptr get_popup_layout();

        bool can_show_label_popup();

        // returns x- and yoffset of the aligned label
        Offset align_label(const Size& label_size, const Size& key_size, bool ltr=true);

        // returns x- and yoffset of the aligned label
        Offset align_secondary_label(const Size& label_size, const Size& key_size, bool ltr=true);

        // returns x- and yoffset of the aligned label
        Offset align_popup_indicator(const Size& label_size, const Size& key_size, bool ltr=true);

        virtual KeyStyle::Enum get_style() const;

        virtual double get_stroke_width() const;

        virtual double get_stroke_gradient() const;

        virtual double get_light_direction() const;

        DwellProgressController* get_dwell_progress_controller();
        Rect get_dwell_progress_canvas_rect();

        bool can_emboss() const;

        bool has_image_color() const;
        RGBA get_image_color();

        virtual RGBA get_color(const std::string& element, const KeyState* state=nullptr) override;

        // Get bounding box of the key at 100% size in logical coordinates
        Rect get_fullsize_rect() const;

        // Get bounding box of the key at 100% size in canvas coordinates
        Rect get_canvas_fullsize_rect();

        // Get bounding box in logical coordinates.
        // Just the relatively static unpressed rect withough fake key action.
        Rect get_unpressed_rect();

        // Get bounding box in logical coordinates
        virtual Rect get_rect() const override;

        Rect get_sized_rect(Noneable<bool> horizontal = {}) const;

        // Label area in logical coordinates
        Rect get_label_rect(Rect rect = {});

        Rect get_canvas_label_rect();

        // Original path including border in logical coordinates.
        KeyPath::Ptr get_border_path();

        // Path of the key geometry in logical coordinates.
        // Key size and fake press movement are applied.
        KeyPath::Ptr get_path();

        KeyPath::Ptr get_canvas_border_path();

        KeyPath::Ptr get_canvas_path();

        KeyPath::Ptr get_hit_path();

        // Max size of the rounded corner areas in logical coordinates.
        double get_chamfer_size(Rect rect = {});

        void get_key_offset_size(Offset& offset, Size& size, KeyGeometry::Ptr geometry = {});

        void get_canvas_polygons(KeyGeometry::Ptr geometry,
                                 const Offset& offset, const Size& size,
                                 double radius_pct, double chamfer_size,
                                 std::vector<KeyPath::Polygon> polygons,
                                 std::vector<std::vector<std::vector<double>>> polygon_paths);

        virtual double get_best_font_size(ModMask mod_mask);

        ImagePtr get_image(const Size& size);

        virtual void draw_item(DrawingContext& dc) override;
        virtual bool can_draw_geometry();

        double get_gradient_angle();

        // generic invalidate request from the keyboard
        virtual void invalidate() override
        {
            invalidate_key();
        }

        // Clear buffered patterns, e.g. after resizing, change of settings...
        void invalidate_caches()
        {
            this->invalidate_key();
            this->invalidate_shadow();
        }

        void invalidate_key()
        {
            //this->_key_surfaces = {};
        }

        // Images only have to be expicitely cleared when the
        // window_scaling_factor changes.
        void invalidate_image()
        {
            m_image_cache.clear();
        }

        void invalidate_shadow()
        {
            //m_shadow_surface = None;
        }

        // Cached label extents are resolution independent. Calling this
        // is only necessary when the system font dpi change.
        void invalidate_label_extents()
        {
            m_label_base_sizes.clear();
        }

        virtual void on_press(LayoutView* view_, MouseButton::Enum button, EventType::Enum event_type)
        {
            (void) view_;
            (void) button;
            (void) event_type;
        }

        virtual void on_release(LayoutView* view_, MouseButton::Enum button, EventType::Enum event_type)
        {
            (void) view_;
            (void) button;
            (void) event_type;
        }

    protected:
        virtual Size get_label_base_extents(ModMask mod_mask);

    private:
        // shrink keys to key_size
        Rect apply_key_size(Rect rect, Noneable<bool> horizontal = {}) const;

        // dx, dy, dw, dh for fake physical key action of pressed keys.
        // Logical coordinate system.
        void get_pressed_deltas(double& x, double& y, double& w, double& h) const;

    private:
        ImageCacheMap m_image_cache;
        LabelBaseSizeMap m_label_base_sizes;
        std::unique_ptr<DwellProgressController> m_dwell_progress_controller;

        friend class FixedFontMixin;
};

typedef LayoutKey::Ptr LayoutKeyPtr;


class LayoutWordListKey : public LayoutKey
{
    public:
        using Super = LayoutKey;
        using Ptr = std::shared_ptr<LayoutWordListKey>;

        LayoutWordListKey(const ContextBase& context) :
            Super(context)
        {}

        static std::unique_ptr<LayoutWordListKey> make(const ContextBase& get_context);

        static constexpr const char* CLASS_NAME = "WordListKey";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual KeyStyle::Enum get_style() const override
        {
            auto style_ = Super::get_style();
            if (style_ == KeyStyle::DISH)
                style_ = KeyStyle::GRADIENT;
            return style_;
        }

        virtual double get_stroke_width() const override
        {
            // Turn down stroke width -> Only subtly bevel the wordlist bar.
            double value = Super::get_stroke_width();
            return std::min(value, 0.6);
        }

        virtual double get_stroke_gradient() const override
        {
            // Turn down stroke gradient -> Only subtly bevel the wordlist bar.
            double value = Super::get_stroke_gradient();
            return std::min(value, 0.3);
        }

        virtual double get_light_direction() const override
        {
            return -0.3 * M_PI / 180;
        }

        virtual void draw_shadow_cached(DrawingContext& dc) //override TODO: port shadows
        {
            (void)dc;
            // no shadow
        }
};


class LayoutFullSizeKey : public LayoutWordListKey
{
    public:
        using Super = LayoutWordListKey;
        using Ptr = std::shared_ptr<LayoutFullSizeKey>;

        LayoutFullSizeKey(const ContextBase& context) :
            Super(context)
        {}

        static constexpr const char* CLASS_NAME = "FullSizeKey";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        // Get bounding box in logical coordinates
        virtual Rect get_rect()  const override
        {
            // Disable key_size, let wordlist creation have complete size control.
            return get_fullsize_rect();
        }
};


// Key without background until pressed.
// Used in word suggestion bar and in character palette headers.
class LayoutFlatKey : public LayoutFullSizeKey
{
    public:
        using Super = LayoutFullSizeKey;
        using Ptr = std::shared_ptr<LayoutFlatKey>;

        LayoutFlatKey(const ContextBase& context);

        static constexpr const char* CLASS_NAME = "FlatKey";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual bool can_draw_geometry() override;

        virtual double get_stroke_width() const override;
};

// Key without background until pressed.
// Used in word suggestion bar and in character palette headers.
class LayoutBarKey : public LayoutFlatKey
{
    public:
        using Super = LayoutFlatKey;
        using Ptr = std::shared_ptr<LayoutBarKey>;

        LayoutBarKey(const ContextBase& context) :
            Super(context)
        {}

        static std::unique_ptr<LayoutBarKey> make(const ContextBase& context);

        static constexpr const char* CLASS_NAME = "BarKey";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

};

// Font size independent of text length
class FixedFontMixin
{
    public:
        FixedFontMixin(LayoutKey* key);
        ~FixedFontMixin()
        {}

        // Get the maximum font size that would not cause the label to
        // overflow the height of the key.
        double get_best_font_size(ModMask mod_mask);

        // Calculate font size based on the height of the key
        double calc_font_size(LayoutContext* context, const Size& size,
                              bool use_width=false);

        // Update resolution independent extents of the label layout.
        Size get_label_base_extents(ModMask mod_mask);

    private:
        LayoutKey* m_key{};
};

// Key without background until pressed.
// Used in word suggestion bar and in character palette headers.
class LayoutWordKey : public LayoutBarKey, public FixedFontMixin
{
    public:
        using Super = LayoutBarKey;
        using Ptr = std::shared_ptr<LayoutBarKey>;

        LayoutWordKey(const ContextBase& context) :
            Super(context),
            FixedFontMixin(this)
        {}

        static constexpr const char* CLASS_NAME = "WordKey";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual double get_best_font_size(ModMask mod_mask) override
        {
            return FixedFontMixin::get_best_font_size(mod_mask);
        }

        virtual Size get_label_base_extents(ModMask mod_mask) override
        {
            return FixedFontMixin::get_label_base_extents(mod_mask);
        }
};
#endif // LAYOUTKEY_H
