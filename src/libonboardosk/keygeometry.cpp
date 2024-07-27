
#include <memory>
#include <vector>
#include <assert.h>

#include "keygeometry.h"
#include "tools/string_helpers.h"
#include "exception.h"

KeyGeometry::Ptr KeyGeometry::from_paths(const std::vector<KeyPath::Ptr>& paths)
{
    assert(paths.size() >= 1);

    KeyPath::Ptr path0 = paths[0];
    KeyPath::Ptr path1;

    if (paths.size() >= 2)
    {
        path1 = paths[1];

        // Equal number of path segments?
        if (path0->segments.size() != path1->segments.size())
        {
            throw ValueException (sstr()
                << "paths to interpolate differ in number of segments "
                << "(" << path0->segments.size()
                << " vs. " << path1->segments.size() << ")");
        }

        // Same operations in all path segments?
        for (size_t i=0; i<path0->segments.size(); i++)
        {
            auto op0 = path0->segments[i].type;
            auto op1 = path1->segments[i].type;
            if (op0 != op1)
            {
                throw ValueException(sstr()
                    << "paths to interpolate have different operations "
                    << "at segment " << i
                    << " (op. " << op0 << " vs. op. " << op1 << ")");
            }
        }
    }

    auto geometry = std::make_shared<KeyGeometry>();
    geometry->m_path0 = path0;
    geometry->m_path1 = path1;
    return geometry;
}

KeyGeometry::Ptr KeyGeometry::from_rect(const Rect& rect)
{
    auto geometry = std::make_shared<KeyGeometry>();
    geometry->m_path0 = KeyPath::from_rect(rect);
    return geometry;
}

KeyPath::Ptr KeyGeometry::get_transformed_path(const Offset& offset, const Size& size)
{
    if (m_path1)
    {
        Point pos = (1.0 - size) * 2.0;
        return m_path0->linint(m_path1, pos, offset.x, offset.y);
    }
    else
    {
        Rect r0 = this->get_full_size_bounds();
        Rect r1 = this->get_half_size_bounds();

        Size s = (size - 0.5) * (r0.get_size() - r1.get_size());

        Rect rect = r1.inflate(s);
        rect.x += offset.x;
        rect.y += offset.y;
        return m_path0->fit_in_rect(rect);
    }
}

KeyPath::Ptr&KeyGeometry::get_full_size_path()
{
    return m_path0;
}

Rect KeyGeometry::get_full_size_bounds()
{
    return m_path0->get_bounds();
}

Rect KeyGeometry::get_half_size_bounds()
{
    Rect rect;

    if (m_path1)
    {
        rect = m_path1->get_bounds();
    }
    else
    {
        double d;
        rect = m_path0->get_bounds();
        if (rect.h < rect.w)
        {
            d = rect.h * 0.25;
        }
        else
        {
            d = rect.w * 0.25;
        }
        rect = rect.deflate(d);
    }
    return rect;
}

Size KeyGeometry::scale_log_to_size(const Size& sz)
{
    Rect r0 = this->get_full_size_bounds();
    Rect r1 = this->get_half_size_bounds();
    Size log = (r0.get_size() - r1.get_size()) * 2.0;
    return sz / log;
}
