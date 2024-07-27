#include <memory>

#include "tools/cairocontext.h"
#include "tools/iostream_helpers.h"
#include "tools/noneable.h"

#include "colorscheme.h"
#include "layoutdrawingitem.h"


std::ostream&operator<<(std::ostream& s, const KeyState& state){
    s << "KeyState(" << state.m_map << ")";
    return s;
}


RGBA LayoutDrawingItem::get_color(const std::string& element,
                                  const KeyState* state)
{
    std::string color_key = element;
    RGBA color;
    if (get_cached_color(color_key, color))
        return color;
    return cache_color(element, color_key, state);
}

RGBA LayoutDrawingItem::cache_color(const std::string& element,
                                    const std::string& color_key,
                                    const KeyState* state)
{
    RGBA rgba;

    if (m_color_scheme)
    {
        rgba = m_color_scheme->get_key_rgba(getptr(), element, state);
    }
    else if (element == "label")
    {
        rgba = {0.0, 0.0, 0.0, 1.0};
    }
    else
    {
        rgba = {1.0, 1.0, 1.0, 1.0};
    }

    m_colors[color_key] = rgba;

    return rgba;
}


std::unique_ptr<LayoutRectangle> LayoutRectangle::make(const ContextBase& context)
{
    return std::make_unique<LayoutRectangle>(context);
}

void LayoutRectangle::draw_item(DrawingContext& dc)
{
    auto cr = dc.get_cc();
    cr->save();
    cr->set_source_rgba(get_fill_color());
    cr->rectangle(get_canvas_rect());
    cr->fill();
    cr->restore();
}

void LayoutRectangle::dump(std::ostream& out) const
{
    out << "LayoutRectangle(";
    super::dump(out);
    out << ")";
}


std::unique_ptr<LayoutPanel> LayoutPanel::make(const ContextBase& context)
{
    return std::make_unique<LayoutPanel>(context);
}

void LayoutPanel::dump(std::ostream& out) const
{
    out << "LayoutPanel(";
    Super::dump(out);
    out << " compact=" << repr(compact);
    out << ")";
}

Rect LayoutPanel::calc_bounds()
{
    // If there is no visible item, return an empty rect.
    auto& children = get_children();
    if (std::none_of(children.begin(), children.end(), [](const LayoutItemPtr& item) {
            return item->is_visible();
        }))
        return {};

    Noneable<Rect> bounds;
    for (auto child : get_children())
    {
        if (!this->compact || child->visible)
        {
            Rect rect = child->get_border_rect();
            if (!rect.empty())
            {
                if (bounds.is_none())
                    bounds = rect;
                else
                    bounds = rect.union_(bounds);
            }
        }
    }

    return bounds;
}




