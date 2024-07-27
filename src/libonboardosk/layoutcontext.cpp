
#include "tools/rect.h"

#include "layoutcontext.h"

std::ostream&operator<<(std::ostream& out, const LayoutContext& c) {
    out << "LayoutContext("
        << "log=" << c.log_rect
        << "canvas=" << c.canvas_rect
        << ")";
    return out;
}

KeyPath::Ptr LayoutContext::log_to_canvas_path(KeyPath::Ptr path)
{
    auto result = path->copy();
    for (auto& segment : result->segments)
    {
        auto& coords = segment.coords;
        for (size_t i=0; i<coords.size(); i+=2)
        {
            coords[i]   = log_to_canvas_x(coords[i]);
            coords[i + 1] = log_to_canvas_y(coords[i + 1]);
        }
    }
    return result;
}
