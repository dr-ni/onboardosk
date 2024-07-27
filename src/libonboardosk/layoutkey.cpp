#include <assert.h>
#include <memory>
#include <string>

#include "tools/drawing_helpers.h"
#include "tools/image.h"
#include "tools/logger.h"
#include "tools/platform_helpers.h"
#include "tools/rect.h"
#include "tools/string_helpers.h"

#include "configuration.h"
#include "exception.h"
#include "dwellprogresscontroller.h"
#include "layoutkey.h"
#include "layoutkeyviewcairo.h"
#include "theme.h"

using namespace std;

LayoutKey::LayoutKey(const ContextBase& context) :
    Super(context)
{
    this->label_x_align = config()->default_label_x_align;
    this->label_y_align = config()->default_label_y_align;
    this->label_margin  = config()->label_margin;

    // init after m_dwell_progress_controller was constructed
    this->view = make_view(context);
}

LayoutKey::~LayoutKey()
{
}

std::unique_ptr<LayoutKey> LayoutKey::make(const ContextBase& context)
{
    return std::make_unique<This>(context);
}

std::unique_ptr<LayoutKeyView> LayoutKey::make_view(const ContextBase& context)
{
    if (context.get_toolkit() == Toolkit::CAIRO)
        return LayoutKeyViewCairo::make(context, this);
    return {};
}
void LayoutKey::dump(std::ostream& out) const
{
    out << get_class_name() << "(";
    Super::dump(out);
    out << repr(get_id())
        << " log=" << get_border_rect()
        << " canvas=" << get_canvas_border_rect();
    out << ")";
}

void LayoutKey::get_state(KeyState& state)
{
    state["prelight"]  = this->prelight;
    state["pressed"]   = this->pressed;
    state["active"]    = this->active;
    state["locked"]    = this->locked;
    state["scanned"]   = this->scanned;
    state["sensitive"] = this->sensitive;
}

void LayoutKey::configure_labels(ModMask mod_mask)
{
    this->m_mod_mask = mod_mask;
    this->label = get_label(mod_mask);
    this->secondary_label = get_secondary_label(this->label, mod_mask);

    // Don't let erroneous labels shrink their whole size group.
    this->ignore_group = startswith(this->label, "0x");
}

string LayoutKey::get_label(ModMask mod_mask) const
{
    if (labels.empty())
        return {};

    // primary label
    std::string label_;
    bool label_valid = get_value(this->labels, mod_mask, label_);
    if (!label_valid)
    {
        ModMask mask = mod_mask & Modifier::LABEL_MODIFIERS;
        label_valid = get_value(this->labels, mask, label_);
    }

    if (!label_valid)
    {
        // legacy fallback for 0.98 behavior && virtkey until 0.61.0
        if (mod_mask & Modifier::SHIFT)
        {
            if (mod_mask & Modifier::ALTGR && contains(this->labels, 129))
            {
                label_ = this->labels.at(129);
                label_valid = true;
            }
            else
                if (contains(this->labels, 1))
                {
                    label_ = this->labels.at(1);
                    label_valid = true;
                }
                else
                    if (contains(this->labels, 2))
                    {
                        label_ = this->labels.at(2);
                        label_valid = true;
                    }
        }

        else
            if (mod_mask & Modifier::ALTGR && contains(this->labels, 128))
            {
                label_ = this->labels.at(128);
                label_valid = true;
            }

            else
                if (mod_mask & Modifier::CAPS)
                {
                    // CAPS lock
                    if (contains(this->labels, 2))
                    {
                        label_ = this->labels.at(2);
                        label_valid = true;
                    }
                    else
                        if (contains(this->labels, 1))
                        {
                            label_ = this->labels.at(1);
                            label_valid = true;
                        }
                }
    }

    if (!label_valid)
    {
        label_valid = get_value(this->labels, 0, label_);
    }

    if (!label_valid)
    {
        label_ = "";
    }

    return label_;
}

string LayoutKey::get_secondary_label(const string primary_label, ModMask mod_mask)
{
    std::string label_;
    bool label_valid = false;
    if (!primary_label.empty())
    {
        ModMask mask;
        if (mod_mask & Modifier::SHIFT)
            mask = mod_mask & ~Modifier::SHIFT;
        else
            mask = mod_mask | Modifier::SHIFT;

        label_valid = get_value(this->labels, mask, label_);
        if (!label_valid)
        {
            mask = mask & Modifier::LABEL_MODIFIERS;
            label_valid = get_value(this->labels, mask, label_);
        }

        // Only keep secondary labels that show different characters
        if (label_valid && \
            upper(label_) == upper(primary_label))
        {
            label_.clear();
        }
    }

    return label_;
}

bool LayoutKey::is_layer_button() const
{
    return is_sublayer_button() ||
            startswith(get_id(), "layer");
}

bool LayoutKey::is_sublayer_button() const
{
    return !target_layer_id.empty();
}

bool LayoutKey::is_prediction_key() const
{
    //return this->type == KeyType::PREDICTION;
    return startswith(get_id(), "prediction");
}

bool LayoutKey::is_correction_key() const
{
    // return this->type == KeyType::CORRECTION;
    return startswith(get_id(), "correction") ||
            get_id() == "expand-corrections";
}

bool LayoutKey::is_word_suggestion() const
{
    return this->is_prediction_key() || this->is_correction_key();
}

bool LayoutKey::is_modifier() const
{
    return this->modifier != Modifier::NONE;
}

bool LayoutKey::is_click_type_key() const
{
    static std::array<std::string, 5> a = {
        "singleclick",
        "secondaryclick",
        "middleclick",
        "doubleclick",
        "dragclick"
    };
    return contains(a, get_id());
}

bool LayoutKey::is_button() const
{
    return this->type == KeyType::BUTTON;
}

bool LayoutKey::is_pressed_only() const
{
    return this->pressed && !(this->active ||
                              this->locked ||
                              this->scanned);
}

bool LayoutKey::is_active_only() const
{
    return this->active && !(this->pressed ||
                             this->locked ||
                             this->scanned);
}

bool LayoutKey::is_text_changing() const
{
    static std::array<KeyType, 6> ak {
        KeyType::KEYCODE,
                KeyType::KEYSYM,
                KeyType::CHAR,
                //KeyType::KEYPRESS_NAME,
                KeyType::MACRO,
                KeyType::PREDICTION,
                KeyType::CORRECTION,
    };

    static std::array<std::string, 14> aid {
        "LEFT", "RGHT", "UP", "DOWN",
        "HOME", "END", "PGUP", "PGDN",
        "INS", "ESC", "MENU",
        "Prnt", "Pause", "Scroll"
    };

    if (!this->is_modifier() &&
        contains(ak, this->type))
    {
        std::string id_ = get_id();
        if (!(startswith(id_, "F") && isdigit_str(slice(id_, 1))) &&
            !contains(aid, id_))
            return true;
    }
    return false;
}

bool LayoutKey::is_return() const
{
    auto s = get_id();
    return (s == "RTRN" ||
            s == "KPEN");
}

bool LayoutKey::is_space() const
{
    return get_id() == "SPCE";
}

bool LayoutKey::is_separator() const
{
    auto s = get_id();
    return (s == "SPCE" ||
            s == "TAB");
}

bool LayoutKey::is_separator_cancelling() const
{
    static std::array<std::string, 17> a =
    {
        "SPCE", "TAB",
        // Don't cancel for Backspace. We want to have
        // it appear to delete the pending separator.
        // This way it inserts a space, then immediately
        // deletes it.
        // "BKSP",
        "DELE",
        "LEFT", "RGHT", "UP", "DOWN",
        "HOME", "END", "PGUP", "PGDN",
        "INS", "ESC", "MENU",
        "Prnt", "Pause", "Scroll",
    };

    return (this->is_correction_key() ||
            this->is_return() ||
            contains(a, get_id()));
}

size_t LayoutKey::get_layer_index()
{
    assert(is_layer_button());
    assert(!is_sublayer_button());
    return to_size_t(get_id().substr(5, string::npos));
}

string LayoutKey::get_target_layer_id()
{
    return this->target_layer_id;
}

string LayoutKey::get_target_layer_parent_id()
{
    std::string layer_id_ = this->target_layer_id;
    auto pos = layer_id_.rfind(".");
    if (pos == std::string::npos)
        return {};
    return slice(layer_id_, 0, static_cast<int>(pos));
}

LayoutItem::Ptr LayoutKey::get_popup_layout()
{
    if (this->popup_id.empty())
        return {};
    return find_sublayout(this->popup_id);
}

bool LayoutKey::can_show_label_popup()
{
    return !is_modifier() &&
           !is_layer_button() &&
           !is_word_suggestion() &&
           !(this->type == KeyType::NONE);
    //&&  this->label_popup;
}

Offset LayoutKey::align_label(const Size& label_size, const Size& key_size, bool ltr)
{
    Vec2 align(this->label_x_align, this->label_y_align);

    if (!ltr) // right to left script?
        align.x = 1.0 - align.x;

    return align * (key_size - label_size);
}

Offset LayoutKey::align_secondary_label(const Size& label_size, const Size& key_size, bool ltr)
{
    Vec2 align(0.97, 0.0);

    if (!ltr) // right to left script?
        align.x = 1.0 - align.x;

    return align * (key_size - label_size);
}

Offset LayoutKey::align_popup_indicator(const Size& label_size, const Size& key_size, bool ltr)
{
    Vec2 align(1.0, this->label_y_align);

    if (!ltr) // right to left script?
        align.x = 1.0 - align.x;

    return align * (key_size - label_size);
}

KeyStyle::Enum LayoutKey::get_style() const
{
    if (this->style.is_none())
        return config()->current_theme()->key_style;
    return this->style;
}

double LayoutKey::get_stroke_width() const
{
    return config()->current_theme()->key_stroke_width / 100.0;
}

double LayoutKey::get_stroke_gradient() const
{
    return config()->current_theme()->key_stroke_gradient / 100.0;
}

double LayoutKey::get_light_direction() const
{
    return config()->current_theme()->key_gradient_direction * M_PI / 180.0;
}

DwellProgressController* LayoutKey::get_dwell_progress_controller()
{
    if (!m_dwell_progress_controller)
        m_dwell_progress_controller =
            std::make_unique<DwellProgressController>();
    return m_dwell_progress_controller.get();
}

Rect LayoutKey::get_dwell_progress_canvas_rect()
{
    Rect rect = get_label_rect().inflate(0.5);
    return this->get_context()->log_to_canvas_rect(rect);
}

bool LayoutKey::can_emboss() const
{
    return get_style() != KeyStyle::FLAT &&
           get_stroke_gradient() != 0.0 &&
           (this->image_filenames.empty() ||
            this->image_style == ImageStyle::SINGLE_COLOR);
}

bool LayoutKey::has_image_color() const
{
    return this->image_style == ImageStyle::SINGLE_COLOR;
}

RGBA LayoutKey::get_image_color()
{
    if (has_image_color())
        return get_label_color();
    return {};
}

RGBA LayoutKey::get_color(const string& element, const KeyState* state)
{
    std::string color_key = sstr() << element
                                   << this->prelight
                                   << this->pressed
                                   << this->active
                                   << this->locked
                                   << this->sensitive
                                   << this->scanned;
    RGBA color;
    if (get_cached_color(color_key, color))
        return color;
    return cache_color(element, color_key, state);
}

Rect LayoutKey::get_fullsize_rect() const
{
    return Super::get_rect();
}

Rect LayoutKey::get_canvas_fullsize_rect()
{
    return get_context()->log_to_canvas_rect(this->get_fullsize_rect());
}

Rect LayoutKey::get_unpressed_rect()
{
    Rect rect = get_fullsize_rect();
    return apply_key_size(rect);
}

Rect LayoutKey::get_rect() const
{
    return get_sized_rect();
}

Rect LayoutKey::get_sized_rect(Noneable<bool> horizontal) const
{
    Rect rect = get_fullsize_rect();

    // fake physical key action
    if (this->pressed)
    {
        double dx, dy, dw, dh;
        get_pressed_deltas(dx, dy, dw, dh);
        rect.x += dx;
        rect.y += dy;
        rect.w += dw;
        rect.h += dh;
    }

    return apply_key_size(rect, horizontal);
}

Rect LayoutKey::get_label_rect(Rect rect)
{
    if (rect.empty())
        rect = this->get_rect();

    auto key_style = this->get_style();
    if (key_style == KeyStyle::DISH)
    {
        double stroke_width  = this->get_stroke_width();
        auto border_ = config()->dish_key_border;
        border_ *= stroke_width;
        rect = rect.deflate(border_);
        rect.y -= config()->dish_key_y_offset * stroke_width;
        return rect;
    }
    else
    {
        return rect.deflate(label_margin);
    }
}

Rect LayoutKey::get_canvas_label_rect()
{
    Rect log_rect = this->get_label_rect();
    return get_context()->log_to_canvas_rect(log_rect);
}

KeyPath::Ptr LayoutKey::get_border_path()
{
    return this->geometry->get_full_size_path();
}

KeyPath::Ptr LayoutKey::get_path()
{
    Offset offset;
    Size size;
    get_key_offset_size(offset, size);
    return this->geometry->get_transformed_path(offset, size);
}

KeyPath::Ptr LayoutKey::get_canvas_border_path()
{
    auto path = this->get_border_path();
    return get_context()->log_to_canvas_path(path);
}

KeyPath::Ptr LayoutKey::get_canvas_path()
{
    auto path = this->get_path();
    return get_context()->log_to_canvas_path(path);
}

KeyPath::Ptr LayoutKey::get_hit_path()
{
    return get_canvas_border_path();
}

double LayoutKey::get_chamfer_size(Rect rect)
{
    if (!this->chamfer_size.is_none())
        return this->chamfer_size;

    if (rect.empty())
    {
        if (this->geometry)
        {
            rect = this->get_border_path()->get_bounds();
        }
        else
        {
            rect = this->get_rect();
        }
    }
    return std::min(rect.w, rect.h) * 0.5;
}

void LayoutKey::get_key_offset_size(Offset& offset, Size& size, KeyGeometry::Ptr geometry_)
{
    size.x = size.y = config()->current_theme()->key_size / 100.0;

    if (this->pressed)
    {
        double dw;
        double dh;
        this->get_pressed_deltas(offset.x, offset.y, dw, dh);
        if (dw != 0.0 || dh != 0.0)
        {
            if (!geometry_)
                geometry_ = this->geometry;

            Size sz = geometry_->scale_log_to_size(Size(dw, dh));
            size += sz * 0.5;
        }
    }
}

void LayoutKey::get_canvas_polygons(KeyGeometry::Ptr geometry_,
                                    const Offset& offset, const Size& size,
                                    double radius_pct, double chamfer_size_,
                                    std::vector<KeyPath::Polygon> polygons,
                                    std::vector<std::vector<std::vector<double>>> polygon_paths)
{
    KeyPath::Ptr path = geometry_->get_transformed_path(offset, size);
    KeyPath::Ptr canvas_path = get_context()->log_to_canvas_path(path);

    canvas_path->for_each_polygon([&](const KeyPath::Polygon& polygon)
    {
        polygons.emplace_back(polygon);
    });

    for (auto& polygon : polygons)
    {
        polygon_paths.emplace_back(std::vector<std::vector<double>>{});
        polygon_to_rounded_path(polygon, radius_pct, chamfer_size_, polygon_paths.back());
    }
}

double LayoutKey::get_best_font_size(ModMask mod_mask)
{
    // Base this on the unpressed rect, so fake physical key action
    // doesn't influence the font_size && doesn't cause surface cache
    // misses for that minor wiggle.
    Rect rect = get_label_rect(get_unpressed_rect());
    Size text_size = get_label_base_extents(mod_mask);
    Size label_area = rect.get_size() - (this->label_margin * 2.0);
    Size max_size = get_context()->scale_log_to_canvas(label_area / text_size);

    if (max_size.w < max_size.h)
        return max_size.w;
    else
        return max_size.h;
}

// Update resolution independent extents of the label layout.
Size LayoutKey::get_label_base_extents(ModMask mod_mask)
{
    Size sz;
    if (!get_value(m_label_base_sizes, mod_mask, sz))
    {
        sz = view->calc_label_base_extents(get_label());
        m_label_base_sizes[mod_mask] = sz;
    }

    return sz;
}

void LayoutKey::draw_item(DrawingContext& dc)
{
    this->view->draw_item(dc);
}

bool LayoutKey::can_draw_geometry()
{
    return this->show_face || this->show_border;
}

Rect LayoutKey::apply_key_size(Rect rect, Noneable<bool> horizontal) const
{
    double scale = (1.0 - config()->current_theme()->key_size / 100.0) * 0.5;
    double bx = rect.w * scale;
    double by = rect.h * scale;

    if (horizontal.is_none())
        horizontal = rect.h < rect.w;

    if (horizontal)
    {
        // keys with aspect > 1.0, e.g. space, shift
        bx = by;
    }
    else
    {
        // keys with aspect < 1.0, e.g. click, move, number block + && enter
        by = bx;
    }

    return rect.deflate(bx, by);
}

void LayoutKey::get_pressed_deltas(double& x, double& y, double& w, double& h) const
{
    double k;
    auto key_style = get_style();
    if (key_style == KeyStyle::GRADIENT)
    {
        k = 0.2;
    }
    else
        if (key_style == KeyStyle::DISH)
        {
            k = 0.45;
        }
        else
        {
            k = 0.0;
        }
    x = k;
    y = 2*k;
    w = 0.0;
    h = 0.0;
}

double LayoutKey::get_gradient_angle()
{
    return -M_PI/2.0 + get_light_direction();
}

// Get the cached image pixbuf object, load image
// and create it if necessary.
// Width and height in canvas coordinates.
ImagePtr LayoutKey::get_image(const Size& size)
{
    if (this->image_filenames.empty())
        return {};

    ImageSlot slot;
    if (this->active && contains(this->image_filenames, ImageSlot::ACTIVE))
        slot = ImageSlot::ACTIVE;
    else
        slot = ImageSlot::NORMAL;

    string image_filename = get_value(this->image_filenames, slot);
    if (image_filename.empty())
        return {};

    ImageCacheEntry cache_entry = get_value(m_image_cache, slot);
    Size last_size = cache_entry.requested_size;
    Size requested_size = size.floor();
    ImagePtr image= cache_entry.image;

    if (!image ||
        last_size != requested_size)
    {
        image.reset();

        string filename_ = config()->get_image_filename(image_filename);
        if (!filename_.empty())
        {
            LOG_INFO << "loading image " << repr(filename_);

            try
            {
                image = Image::from_file_and_size(filename_, requested_size);
            }
            catch(const Exception& ex)
            {
                LOG_ERROR << "Image::from_file_and_size failed: " << ex.what();
            }
        }

        this->m_image_cache[slot] = {requested_size, image};
    }

    return image;
}




LayoutFlatKey::LayoutFlatKey(const ContextBase& context) :
    Super(context)
{}

bool LayoutFlatKey::can_draw_geometry()
{
    return Super::can_draw_geometry() &&
           (this->pressed || this->active || this->scanned);
}

double LayoutFlatKey::get_stroke_width() const
{
    // Turn down stroke width -> no annoying banding at
    // what should be flat key edges.
    return 0.0;
}



std::unique_ptr<LayoutBarKey> LayoutBarKey::make(const ContextBase& context)
{
    return std::make_unique<LayoutBarKey>(context);
}



FixedFontMixin::FixedFontMixin(LayoutKey* key) :
    m_key(key)
{}

double FixedFontMixin::get_best_font_size(ModMask mod_mask)
{
    (void)mod_mask;
    return calc_font_size(m_key->get_context(),
                          m_key->get_fullsize_rect().get_size(),
                          true);
}

double FixedFontMixin::calc_font_size(LayoutContext* context, const Size& size, bool use_width)
{
    // Base this on the unpressed rect, so fake physical key action
    // doesn't influence the font_size and doesn't cause surface cache
    // misses for that minor wiggle.
    Size label_size = m_key->get_label_base_extents(0);

    double size_for_maximum_width  = context->scale_log_to_canvas_x(
                                         (size.w - m_key->label_margin.w * 2))
                                     / label_size.w;

    double size_for_maximum_height = context->scale_log_to_canvas_y(
                                         (size.h - m_key->label_margin.h * 2)) \
                                     / label_size.h;

    double font_size = size_for_maximum_height;
    if (use_width && size_for_maximum_width < font_size)
    {
        font_size = size_for_maximum_width;
    }

    return std::floor(font_size * 0.9);
}

Size FixedFontMixin::get_label_base_extents(ModMask mod_mask)
{
    Size sz;
    if (!get_value(m_key->m_label_base_sizes, mod_mask, sz))
    {
        sz = m_key->view->calc_label_base_extents(m_key->get_label());
        m_key->m_label_base_sizes[mod_mask] = sz;
    }

    return sz;
}


