
#include <memory>

#include "tools/logger.h"
#include "tools/cairocontext.h"
#include "tools/container_helpers.h"
#include "tools/string_helpers.h"

#include "characterdata.h"
#include "keyboard.h"
#include "layoutcharacterpalette.h"
#include "layoutkey.h"
#include "layoutroot.h"
#include "layoutscrolledpanel.h"

static const Vec2 EMOJI_IMAGE_MARGIN = {1.5, 2.5};
static const Vec2 EMOJI_HEADER_MARGIN = {2.5, 3.5};

static const char* FAVORITE_EMOJI_ID = "favourite-emoji";
// static const char* SEARCH_EMOJI_ID = "search-emoji";


static void draw_separators(DrawingContext& cr, const Rect& clip_rect,
                            const std::vector<Rect>& rects);

class CharacterPaletteKey : public LayoutFlatKey
{
    public:
        using Super = LayoutFlatKey;
        using Ptr = std::shared_ptr<CharacterPaletteKey>;

        CharacterPaletteKey(const ContextBase& context) :
            Super(context)
        {}

        static constexpr const char* CLASS_NAME = "CharacterPaletteKey";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual void dump(std::ostream& out) const override
        {
            out << "CharacterPaletteKey(";
            Super::dump(out);
            out << ")";
        }
        virtual void draw_item(DrawingContext& dc) override
        {
            Super::draw_item(dc);
        }
};


class PaletteHeaderKey : public LayoutFlatKey
{
    public:
        using Super = LayoutFlatKey;
        using Ptr = std::shared_ptr<PaletteHeaderKey>;

        PaletteHeaderKey(const ContextBase& context) :
            Super(context)
        {}

        static constexpr const char* CLASS_NAME = "PaletteHeaderKey";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual void dump(std::ostream& out) const override
        {
            out << "PaletteHeaderKey(";
            Super::dump(out);
            out << ")";
        }

        virtual KeyStyle::Enum get_style() const override
        {
            // No gradient when active to match the background rectangle.
            if (is_active_only())
                return KeyStyle::FLAT;
            return Super::get_style();
        }

        virtual RGBA get_fill_color() override
        {
            // Show active state with the inactive fill color.
            // In inactive state the key is transparent (FlatKey).
            if (is_active_only())
            {
                KeyState state;
                return this->get_color("fill", &state);
            }
            return this->get_color("fill");
        }

        virtual void on_release(LayoutView*, MouseButton::Enum, EventType::Enum) override
        {
            auto palette_panel = std::dynamic_pointer_cast<LayoutPalettePanel>(get_parent());
            if (palette_panel)
                palette_panel->scroll_to_category_index(this->keyindex);
        }
};


class CharacterGridPanel : public LayoutScrolledPanel
{
    public:
        using Super = LayoutScrolledPanel;
        using This = CharacterGridPanel;
        using Ptr = std::shared_ptr<This>;

        CharacterGridPanel(const ContextBase& context) :
            Super(context)
        {}

        virtual void dump(std::ostream& out) const override
        {
            out << "CharacterGridPanel(";
            Super::dump(out);
            out << ")";
        }

        RGBA get_fill_color()
        {
            return {0, 0, 0, 1};
        }

        virtual void update_log_rect() override
        {
            set_clip_rect(get_canvas_border_rect());
            Super::update_log_rect();
        }

        void update_content()
        {
            Rect flow_rect = get_rect();
            flow_rect.w = 0;
            Rect key_rect = this->m_key_border_rect;
            Vec2 key_spacing{0, 0};
            double subcategory_spacing = key_rect.w * 0.25;
            std::vector<std::string> key_labels;
            std::vector<Rect> key_rects;
            std::vector<Rect> separator_rects;
            Noneable<Rect> bounding_box;
            Rect category_rect;

            if (!m_symbol_data)
                return;

            auto& subcategories = m_symbol_data->get_subcategories();

            for (size_t i=0; i<subcategories.size(); i++)
            {
                auto& category = subcategories[i];
                auto& sequences = category.sequences;
                std::vector<Rect> rects;
                Rect bounds;
                flow_rect.flow_layout(rects, bounds,
                    key_rect, sequences.size(), key_spacing.x, key_spacing.y,
                    true, true);

                extend(key_labels, sequences);
                extend(key_rects, rects);

                if (bounding_box.is_none())
                    bounding_box = bounds;
                else
                    bounding_box = bounds.union_(bounding_box);

                // keep track of category bounds (spanning multiple subcategories)
                if (category.level == 0) // start of category?
                {
                    if (i > 0)
                    {
                        m_category_rects.emplace_back(category_rect);
                        m_category_key_rects.emplace_back(key_rects);
                    }
                    category_rect = bounds;
                }
                category_rect = category_rect.union_(bounds);

                flow_rect.x += bounds.w;

                // separator
                if (i < subcategories.size() - 1)
                {
                    Rect r = flow_rect;
                    r.w = subcategory_spacing;
                    r = r.grow(0.125, 0.75);
                    separator_rects.emplace_back(r);
                }

                flow_rect.x += subcategory_spacing;
            }

            m_category_rects.emplace_back(category_rect);
            m_category_key_rects.emplace_back(key_rects);

            m_key_labels = key_labels;
            m_key_slots.resize(key_rects.size());
            m_key_rects = key_rects;
            m_separator_rects = separator_rects;

            lock_y_axis(true);
            set_scroll_rect(bounding_box);
        }

        virtual bool is_background_at(const Point& log_point) override
        {
            for (auto r : m_key_rects)
                if (r.contains(log_point))
                    return false;
            return true;
        }

        void scroll_to_category(size_t category_index)
        {
            double x = 0;
            if (category_index < m_category_rects.size())
                x = m_category_rects[category_index].x;
            set_scroll_offset({-x, 0.0});
        }

        virtual void on_scroll_offset_changed() override
        {
            double offset = -get_scroll_offset().x;
            size_t category_index = 0;
            for (size_t i=0; i<m_category_key_rects.size(); i++)
            {
                Rect& r = m_category_rects[i];
                if (offset < r.x)
                    break;
                category_index = i;
            }

            auto palette_panel = std::dynamic_pointer_cast<LayoutPalettePanel>(get_parent());
            if (palette_panel)
                palette_panel->set_active_category_index(category_index);            
        }

        virtual void update_contents_on_scroll() override
        {
            Super::update_contents_on_scroll();

            // only called asynchronously -> needs its own
            // commit for smooth scolling
            get_keyboard()->commit_ui_updates();
        }

        virtual void on_damage(const Rect& damage_rect) override
        {
            auto& key_slots = m_key_slots;
            std::vector<LayoutKeyPtr> items;

            for (size_t ic=0; ic<m_category_rects.size(); ic++)
            {
                Rect& category_rect = m_category_rects[ic];
                if (damage_rect.intersects(category_rect))
                {

                    auto& category_key_rects = m_category_key_rects[ic];
                    for (size_t i=0; i<category_key_rects.size(); i++)
                    {
                        Rect& rect = category_key_rects[i];
                        if (damage_rect.intersects(rect))
                        {
                            auto& key = key_slots[i];
                            if (!key)
                            {
                                key = get_key(i);
                                key_slots[i] = key;
                            }
                            items.emplace_back(key);
                        }
                        else
                        {
                            key_slots[i] = nullptr;
                        }
                    }
                }
            }

            set_children(items);

            auto keyboard = get_keyboard();
            if (!this->m_has_emoji)
                keyboard->invalidate_labels();

            keyboard->invalidate_item(getptr());
            keyboard->invalidate_layout();
        }

        LayoutKeyPtr get_key(size_t index)
        {
            std::string id_ = "_palette_character" + std::to_string(index);
            std::string label = m_key_labels[index];

            LayoutKeyPtr key;

            auto it = m_key_pool.find(label);
            if (it == m_key_pool.end())
            {
                key = create_key(id_, label, m_key_rects[index],
                                 this->m_key_group, this->m_color_scheme,
                                 this->m_has_emoji);

                // only cache emoji keys, as these are the most expensive ones
                if (0) // this->has_emoji
                    m_key_pool[label] = key;
            }

            if (key)
                key->set_id(id_);

            return key;
        }

        LayoutKeyPtr create_key(const std::string& id_, const std::string& label,
                                const Rect& key_border_rect, const std::string& key_group,
                                const ColorSchemePtr& color_scheme, bool has_emoji)
        {
            auto key = std::make_shared<CharacterPaletteKey>(*this);

            key->type = KeyType::CHAR;
            key->keystr = label;
            key->action = KeyAction::DELAYED_STROKE;
            key->set_border_rect(key_border_rect);
            if (has_more_codepoints_than(label, 2))
                key->group = id_;
            else
                key->group = key_group;
            key->m_color_scheme = color_scheme;

            if (has_emoji)
            {
                std::string fn = emoji_filename_from_sequence(label);
                if (!fn.empty())
                {
                    key->image_filenames = {{ImageSlot::NORMAL, fn}};
                    key->image_style = ImageStyle::MULTI_COLOR;
                    key->label_margin = EMOJI_IMAGE_MARGIN;
                }
            }

            if (key->image_filenames.empty())
                key->labels = {{0, label}};

            // key->can_draw_cached = false;   // TODO: decide if still needed when caching is implemented

            return key;
        }

        virtual void draw_tree(DrawingContext& dc) override
        {
            Super::draw_tree(dc);

            Rect clip_rect_ = get_canvas_rect();

            LayoutContext* key_context = get_scrolled_context();
            std::vector<Rect> rects;
            for (auto& r : m_separator_rects)
                rects.emplace_back(key_context->log_to_canvas_rect(r));

            draw_separators(dc, clip_rect_, rects);
        }

    private:
        SymbolData* m_symbol_data{};
        bool m_has_emoji{false};
        Rect m_key_border_rect;
        std::string m_key_group;
        ColorSchemePtr m_color_scheme;
        RGBA background_rgba;
        std::vector<std::string> m_key_labels;
        std::vector<Rect> m_key_rects;
        std::vector<LayoutKeyPtr> m_key_slots;
        std::map<std::string, LayoutKeyPtr> m_key_pool;
        std::vector<Rect> m_separator_rects;
        std::vector<Rect> m_category_rects;
        std::vector<std::vector<Rect>> m_category_key_rects;

        friend LayoutPalettePanel;
};



LayoutPalettePanel::LayoutPalettePanel(const ContextBase& context) :
    Super(context),
    m_character_grid(std::make_unique<CharacterGridPanel>(context))
{}

void LayoutPalettePanel::dump(std::ostream& out) const
{
    Super::dump(out);
    out << " compact=" << repr(compact);
}

void LayoutPalettePanel::update_log_rect()
{
    update_content();
    Super::update_log_rect();
}

void LayoutPalettePanel::update_content()
{
    if (m_header_keys.empty())
        create_header_content();
}

void LayoutPalettePanel::create_header_content()
{
    std::vector<LayoutKeyPtr> keys;

    auto background = std::dynamic_pointer_cast<LayoutKey>(find_id("character-palette"));
    auto header_template = std::dynamic_pointer_cast<LayoutKey>(find_id("palette-header-template"));
    auto key_template = std::dynamic_pointer_cast<LayoutKey>(find_id("palette-key-template"));

    if (!background ||
        !header_template ||
        !key_template)
    {
        return;
    }

    const auto& color_scheme = key_template->m_color_scheme;

    // take rect at 100% key size
    Rect remaining_rect = background->get_fullsize_rect();

    // create header keys
    std::vector<LayoutKeyPtr> ks;
    Rect r;
    auto headers = m_symbol_data->get_category_labels() + m_extra_labels;
    create_header_keys(ks, r,
                       remaining_rect, header_template->get_border_rect(),
                       "_" + m_content_type + "_header",
                       color_scheme, headers);
    keys += ks;
    remaining_rect.h -= r.h;

    // create scrolled panel with grid of keys
    auto grid = std::make_shared<CharacterGridPanel>(*this);
    grid->m_symbol_data = m_symbol_data.get();
    grid->set_id("_character_grid");
    grid->set_border_rect(remaining_rect);
    grid->m_key_border_rect = key_template->get_border_rect();
    grid->m_key_group = "_" + m_content_type;
    grid->m_color_scheme = color_scheme;
    grid->m_has_emoji = m_content_type == "emoji";

    clear_children();
    append_child(background);
    append_child(header_template);
    append_child(key_template);
    append_child(grid);
    append_children(keys);

    m_character_grid = grid;
    m_header_keys = keys;

    grid->update_content();
    scroll_to_category_index(0);
}

void LayoutPalettePanel::create_header_keys(
        std::vector<LayoutKeyPtr>& keys,
        Rect& header_rect, const Rect& palette_rect,
        const Rect& header_key_border_rect, const std::string& header_key_group,
        const ColorSchemePtr& color_scheme, const std::vector<std::string>& labels)
{
    Vec2 spacing{0, 0};

    size_t n = labels.size();
    header_rect = palette_rect;
    header_rect.y = palette_rect.bottom() - header_key_border_rect.h;
    header_rect.h = header_key_border_rect.h;
    std::vector<Rect> key_rects = header_rect.subdivide(n, 1, spacing);

    for (size_t i=0; i<n; i++)
    {
        auto label = labels[i];
        LayoutKeyPtr key;
        if (is_favorites_index(i, n))
        {
            //key = std::make_shared<PaletteFavoritesKey>(*this);
            key = std::make_shared<PaletteHeaderKey>(*this);
            key->set_id(FAVORITE_EMOJI_ID);
        }
        else
        {
            key = std::make_shared<PaletteHeaderKey>(*this);
            key->set_id("_palette_header" + std::to_string(i));
        }
        key->type = KeyType::BUTTON;
        key->keyindex = i;
        key->set_border_rect(key_rects[i]);
        key->group = header_key_group;
        key->m_color_scheme = color_scheme;
        key->unlatch_layer = false;

        configure_header_key(key, label);

        keys.emplace_back(key);
    }
}

void LayoutPalettePanel::configure_header_key(const LayoutKeyPtr& key, const std::string& label)
{
    key->labels = {{0, label}};
    key->label_margin = {1, 1};
}

void LayoutPalettePanel::set_active_category_index(size_t index)
{
    if (m_active_category_index != index)
    {
        m_active_category_index = index;

        std::vector<LayoutKeyPtr> keys_to_redraw;
        for (auto key : m_header_keys)
        {
            bool active = (key->keyindex == m_active_category_index);
            if (key->active != active)
            {
                key->active = active;
                keys_to_redraw.emplace_back(key);
            }
        }

        auto keyboard = get_keyboard();
        keyboard->invalidate_keys(keys_to_redraw);
        //keyboard->commit_ui_updates();
    }
}

void LayoutPalettePanel::scroll_to_category_index(size_t index)
{
    set_active_category_index(index);
    m_character_grid->scroll_to_category(m_active_category_index);
}

bool LayoutPalettePanel::is_favorites_index(size_t index, size_t num_keys)
{
    (void) index;
    (void) num_keys;
    return false;
}



std::unique_ptr<EmojiPalettePanel> EmojiPalettePanel::make(const ContextBase& context)
{
    return std::make_unique<This>(context);
}

EmojiPalettePanel::EmojiPalettePanel(const ContextBase& context) :
    Super(context)
{
    m_content_type = "emoji";
    m_symbol_data = CharacterData().get_symbol_data(CharacterData::EMOJI);
    //m_extra_labels = {"⭐"};
}

void EmojiPalettePanel::dump(std::ostream& out) const
{
    out << "EmojiPalettePanel(";
    Super::dump(out);
    out << ")";
}

void EmojiPalettePanel::configure_header_key(const LayoutKeyPtr& key, const std::string& label)
{
    std::string fn = emoji_filename_from_sequence(label);
    if (!fn.empty())
    {
        key->image_filenames = {{ImageSlot::NORMAL, fn}};
        key->label_margin = EMOJI_HEADER_MARGIN;

        if (contains(m_extra_labels, label))
            key->image_style = ImageStyle::MULTI_COLOR;
        else
            key->image_style = ImageStyle::DESATURATED;
    }

    if (key->image_filenames.empty())
        Super::configure_header_key(key, label);
}

void EmojiPalettePanel::on_visibility_changed(bool visible_)
{
    Super::on_visibility_changed(visible_);
    #if 0 // not implemented yet
    if (visible)
        this->keyboard.show_symbol_search(this->content_type);
    else
        this->keyboard.hide_symbol_search();
    #endif
}

bool EmojiPalettePanel::is_favorites_index(size_t index, size_t num_keys)
{
    (void) index;
    (void) num_keys;
    return false;  // index == num_keys - 1
}



std::unique_ptr<SymbolPalettePanel> SymbolPalettePanel::make(const ContextBase& context)
{
    return std::make_unique<This>(context);
}

SymbolPalettePanel::SymbolPalettePanel(const ContextBase& context) :
    Super(context)
{
    m_content_type = "symbols";
    m_symbol_data = CharacterData().get_symbol_data(CharacterData::SYMBOLS);
}

void SymbolPalettePanel::dump(std::ostream& out) const
{
    out << "SymbolPalettePanel(";
    Super::dump(out);
    out << ")";
}



static void draw_separators(DrawingContext& dc, const Rect& clip_rect,
                            const std::vector<Rect>& rects)
{
    if (rects.empty())
        return;

    auto cc = dc.get_cc();

    RGBA rgba{0.3, 0.3, 0.3, 1.0};
    RGBA dark_rgba{0, 0, 0, 1};
    RGBA bright_rgba = rgba;

    Rect r = rects[0];
    LinearGradient pat(cc, Line{{r.x, r.top()}, {r.x, r.bottom()}});
    pat.add_color_stop_rgba(0.0, dark_rgba);
    pat.add_color_stop_rgba(0.5, bright_rgba);
    pat.add_color_stop_rgba(1.0, dark_rgba);
    cc->set_source(pat);
    cc->set_line_width(2);

    for (auto& rect : rects)
    {
        if (clip_rect.intersects(rect))
        {
            double xc = rect.get_center_x();
            cc->move_to(xc, rect.top());
            cc->line_to(xc, rect.bottom());
            cc->stroke();
        }
    }
}


