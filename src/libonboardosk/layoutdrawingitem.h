#ifndef DRAWINGITEM_H
#define DRAWINGITEM_H

#include <ostream>
#include <cairo.h>

#include "tools/color.h"
#include "layoutitem.h"
#include "drawingcontext.h"


class KeyState
{
    public:
        using Map = std::map<std::string, bool>;

        bool contains(const std::string& name) const
        {
            auto it = m_map.find(name);
            return it != m_map.end();
        }

        bool get(const std::string& name, bool default_value=false) const
        {
            auto it = m_map.find(name);
            if (it != m_map.end())
                return it->second;
            return default_value;
        }

        void remove(const std::string& name)
        {
            auto it = m_map.find(name);
            if (it != m_map.end())
                m_map.erase(it);
        }

        bool any_true() const
        {
            for (auto& it : m_map)
                if (it.second)
                    return true;
            return false;
        }

        Map::iterator begin()
        { return m_map.begin();}
        Map::iterator end()
        { return m_map.end();}

        Map::const_iterator begin() const
        { return m_map.cbegin();}
        Map::const_iterator end() const
        { return m_map.cend();}

        bool& operator[] (const std::string& name)
        {
            return m_map[name];
        }

    private:
        Map m_map;

        friend std::ostream&operator<<(std::ostream& s, const KeyState& state);
};

std::ostream& operator<<(std::ostream& s, const KeyState& state);

// Base class of drawable Items.
class LayoutDrawingItem : public LayoutItem
{
    public:
        // extended id for key specific theme tweaks
        // e.g. theme_id=DELE.numpad (with id=DELE)
        std::string theme_id;

        // extended id for layout specific tweaks
        // e.g. "hide.wordlist", for hide button in wordlist mode
        std::string svg_id;

        // color scheme
        ColorSchemePtr m_color_scheme;

    private:
        // cached colors
        using Colors = std::map<std::string, RGBA>;
        Colors m_colors;

    public:
        LayoutDrawingItem(const ContextBase& context) :
            LayoutItem(context)
        {}
        virtual ~LayoutDrawingItem() override
        {}

        void set_ids(const std::string& id_,
                     const std::string& theme_id_={},
                     const std::string& svg_id_={})
        {
            parse_id(id_, this->theme_id, this->id);
            if (!theme_id_.empty())
                this->theme_id = theme_id_;
            this->svg_id = svg_id_.empty() ? this->id : svg_id_;
        }

        // The theme id has the form <id>.<arbitrary identifier>, where
        // the identifier should be a description of the location of
        // the key relative to its surroundings, e.g. 'DELE.next-to-backspace'.
        // Don't use layout names or layer ids for the theme id, they lose
        // their meaning when layouts are copied or renamed by users.
        static void parse_id(const std::string& value,
                             std::string& theme_id_out,
                             std::string& id_out)
        {
            if (!value.empty())
            {
                id_out = split(value, '.')[0];
                theme_id_out = value;
            }
        }

        // Split in prefix (id) before the dot and suffix after the dot.
        static void split_theme_id(const std::string& theme_id_,
                                   std::string& prefix_out,
                                   std::string& suffix_out)
        {
            auto components = split(theme_id_, '.');
            prefix_out = components[0];
            if (components.size() >= 2)
                suffix_out = components[1];
            else
                suffix_out.clear();
        }

        static std::string build_theme_id(const std::string& prefix,
                                   const std::string& postfix)
        {
            if (!postfix.empty())
                return prefix + "." + postfix;
            return prefix;
        }

        void get_similar_theme_id()
        {
            get_similar_theme_id(this->id);
        }

        std::string get_similar_theme_id(const std::string& prefix)
        {
            std::string theme_id_ = prefix;
            auto components = split(this->theme_id, '.');
            if (components.size() >= 2)
            {
                theme_id_ += "." + components[1];
            }
            return theme_id_;
        }

        virtual RGBA get_fill_color()
        {
            return get_color("fill");
        }

        RGBA get_stroke_color()
        {
            return get_color("stroke");
        }

        RGBA get_label_color()
        {
            return get_color("label");
        }

        RGBA get_secondary_label_color()
        {
            return get_color("secondary-label");
        }

        RGBA get_dwell_progress_color()
        {
            return get_color("dwell-progress");
        }

        virtual RGBA get_color(const std::string& element, const KeyState* state=nullptr);

        bool get_cached_color(const std::string& color_key, RGBA& color)
        {
            return get_value(m_colors, color_key, color);
        }

        RGBA cache_color(const std::string& element,
                         const std::string& color_key,
                         const KeyState* state=nullptr);

        //virtual void get_state(KeyState& state)
        //{}
};

// Item that draws a simple filled rectangle.
class LayoutRectangle : public LayoutDrawingItem
{
    public:
        using super = LayoutDrawingItem;
        using Ptr = std::shared_ptr<LayoutRectangle>;

        LayoutRectangle(const ContextBase& context) :
            LayoutDrawingItem(context)
        {}

        static std::unique_ptr<LayoutRectangle> make(const ContextBase& get_context);

        static constexpr const char* CLASS_NAME = "LayoutRectangle";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual void dump(std::ostream& out) const override;

        void draw_item(DrawingContext& dc);
};


// Group of keys layed out at fixed positions relative to each other.
class LayoutPanel : public LayoutDrawingItem
{
    public:
        // Don't extend bounding box into invisibles
        bool compact{false};

    public:
        using Super = LayoutDrawingItem;
        using Ptr = std::shared_ptr<LayoutPanel>;

        LayoutPanel(const ContextBase& context) :
            Super(context)
        {}

        static std::unique_ptr<LayoutPanel> make(const ContextBase& get_context);

        static constexpr const char* CLASS_NAME = "LayoutPanel";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual void dump(std::ostream& out) const override;

        // Scale panel to fit inside the given canvas_rect.
        virtual void do_fit_inside_canvas(const Rect& canvas_border_rect) override
        {
            LayoutItem::do_fit_inside_canvas(canvas_border_rect);

            // Setup children's transforms, take care of the border.
            if (get_border_rect().empty())
            {
                // Clear all item's transformations if there are no visible items.
                for (auto child  : this->get_children())
                    child->get_context()->canvas_rect = Rect();
            }
            else
            {
                LayoutContext context;
                context.log_rect = get_border_rect();
                context.canvas_rect = get_canvas_rect();  // exclude border

                for (auto child : this->get_children())
                {
                    Rect rect = get_context()->log_to_canvas_rect(child->get_context()->log_rect);
                    child->do_fit_inside_canvas(rect);
                }
            }
        }

        virtual void update_log_rect() override
        {
            this->get_context()->log_rect = calc_bounds();
        }

    private:
        // Calculate the bounding rectangle over all items of this panel
        Rect calc_bounds();
};


#endif // DRAWINGITEM_H
