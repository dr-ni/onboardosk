#include <vector>
#include <memory>

#include "tools/string_helpers.h"
#include "tools/container_helpers.h"

#include "keypath.h"
#include "exception.h"

using namespace std;

KeyPath::Ptr KeyPath::from_svg_path(const std::string& path_str)
{
    Ptr path = std::make_shared<KeyPath>();
    path->append_svg_path(path_str);
    return path;
}

KeyPath::Ptr KeyPath::from_rect(const Rect& rect)
{
    double x0 = rect.x;
    double y0 = rect.y;
    double x1 = rect.right();
    double y1 = rect.bottom();
    Ptr path = std::make_shared<KeyPath>();

    path->segments.emplace_back(
                PathSegment{SegmentType::MOVE_TO, {x0, y0}});
    path->segments.emplace_back(
                PathSegment {SegmentType::LINE_TO, {x1, y0, x1, y1, x0, y1}});
    path->segments.emplace_back(
                PathSegment {SegmentType::CLOSE_PATH, {}});

    path->m_bounds = rect;
    return path;
}

KeyPath::Ptr KeyPath::copy()
{
    KeyPath::Ptr result = make_shared<KeyPath>();
    for (auto& s  : this->segments)
    {
        result->segments.emplace_back(
                    PathSegment {s.type, s.coords});
    }
    return result;
}

void KeyPath::append_svg_path(const std::string& path_str)
{
    string cmd_str;
    vector<double> coords;
    vector<string> tokens = tokenize_svg_path(path_str);
    for (auto token  : tokens)
    {
        if (token.empty())
            continue;

        double val = to_double(token);
        if (val != 0.0 || std::isdigit(token[0]))
        {
            coords.emplace_back(val);
        }
        else if (isalpha_str(token))
        {
            if (!cmd_str.empty())
                append_command(cmd_str, coords);
            cmd_str = token;
            coords.clear();
        }
        else if (token == ",")
        {
        }
        else
        {
            throw  SVGException(sstr()
                << "unexpected token '" << token << " in svg path data");
        }
    }

    if (!cmd_str.empty())
    {
        this->append_command(cmd_str, coords);
    }
}

void KeyPath::append_command(std::string& cmd, std::vector<double>& coords)
{
    Point pos;

    if ((contains(vector<std::string>{"m", "M", "l", "L"}, cmd) && coords.size() < 2) ||
        (contains(vector<std::string>{"h", "H", "v", "V"}, cmd) && coords.size() < 1))
            throw  SVGException(sstr()
                << "too few coordinate values for '" << cmd << "' in svg path data");

    // Convert lowercase segments from relative to absolute coordinates.
    if (cmd == "m" || cmd == "l")
    {
        // Don't convert the very first coordinate, it is already absolute.
        size_t start;
        if (!this->segments.empty())
        {
            start = 0;
            pos = m_last_abs_pos;
        }
        else
        {
            start = 2;
            pos = Point(coords[0], coords[1]);
        }

        for (size_t i=start; i< coords.size(); i+=2)
        {
            pos.x += coords[i];
            pos.y += coords[i+1];
            coords[i]   = pos.x;
            coords[i+1] = pos.y;
        }
    }

    if (cmd == "m" || cmd == "M")
    {
        {
            PathSegment segment = {SegmentType::MOVE_TO, {coords[0], coords[1]}};
            this->segments.emplace_back(segment);
        }
        if (coords.size() > 2)
        {
            PathSegment segment = {SegmentType::LINE_TO, slice(coords, 2)};
            this->segments.emplace_back(segment);
        }
        m_last_abs_pos = Point(*(coords.end()-2),
                               *(coords.end()-1));
    }
    else
    if (cmd == "l" || cmd == "L")
    {
        PathSegment segment = {SegmentType::LINE_TO, coords};
        this->segments.emplace_back(segment);
        m_last_abs_pos = Point(*(coords.end()-2),
                               *(coords.end()-1));
    }
    else
    if (cmd == "h")
    {
        pos = m_last_abs_pos;
        pos.x += coords[0];
        PathSegment segment = {SegmentType::LINE_TO, {pos.x, pos.y}};
        this->segments.emplace_back(segment);
        m_last_abs_pos = pos;
    }
    else
    if (cmd == "H")
    {
        pos = m_last_abs_pos;
        pos.x = coords[0];
        PathSegment segment = {SegmentType::LINE_TO, {pos.x, pos.y}};
        this->segments.emplace_back(segment);
        m_last_abs_pos = pos;
    }
    else
    if (cmd == "v")
    {
        pos = m_last_abs_pos;
        pos.y += coords[0];
        PathSegment segment = {SegmentType::LINE_TO, {pos.x, pos.y}};
        this->segments.emplace_back(segment);
        m_last_abs_pos = pos;
    }
    else
    if (cmd == "V")
    {
        pos = m_last_abs_pos;
        pos.y = coords[0];
        PathSegment segment = {SegmentType::LINE_TO, {pos.x, pos.y}};
        this->segments.emplace_back(segment);
        m_last_abs_pos = pos;
    }
    else
    if (cmd == "z")
    {
        PathSegment segment = {SegmentType::CLOSE_PATH, {}};
        this->segments.emplace_back(segment);
    }
}

std::vector<std::string> KeyPath::tokenize_svg_path(const std::string& path_str)
{
    std::vector<std::string> results;
    std::vector<std::string> tokens = re_findall(path_str,
                                                 R"(\b[A-Za-z]\b|[+-]?[0-9.]+)");  //C++11 raw string R"(...)"
    for (auto& token : tokens)
    {
        std::string t = strip(token);
        if (!t.empty())
            results.emplace_back(t);
    }
    return results;
}

Rect KeyPath::calc_bounds()
{
    if (this->segments.empty() || this->segments[0].coords.size() < 2)
        return {};

    double xmin, xmax;
    double ymin, ymax;
    xmin = xmax = this->segments[0].coords[0];
    ymin = ymax = this->segments[0].coords[1];
    for (auto& segment : this->segments)
    {
        auto& coords = segment.coords;
        for (size_t i=0; i<coords.size(); i+=2)
        {
            double x = coords[i];
            double y = coords[i+1];
            if (xmin > x)
                xmin = x;
            if (xmax < x)
                xmax = x;
            if (ymin > y)
                ymin = y;
            if (ymax < y)
                ymax = y;
        }
    }
    return Rect(xmin, ymin, xmax - xmin, ymax - ymin);
}

KeyPath::Ptr KeyPath::fit_in_rect(const Rect& rect)
{
    auto result = copy();
    Rect bounds = get_bounds();
    double scalex = rect.w / bounds.w;
    double scaley = rect.h / bounds.h;
    Point dorg = bounds.get_center();
    double dx = rect.x - (dorg.x + (bounds.x - dorg.x) * scalex);
    double dy = rect.y - (dorg.y + (bounds.y - dorg.y) * scaley);
    for (auto& segment : result->segments)
    {
        auto& coords = segment.coords;
        for (size_t i=0; i<coords.size(); i+=2)
        {
            coords[i] = dx + dorg.x + (coords[i] - dorg.x) * scalex;
            coords[i+1] = dy + dorg.y + (coords[i+1] - dorg.y) * scaley;
        }
    }
    return result;
}

KeyPath::Ptr KeyPath::linint(const KeyPath::Ptr& path1, const Point& pos, double offset_x, double offset_y)
{
    auto result = copy();
    auto& segments0 = result->segments;
    auto& segments1 = path1->segments;
    for (size_t i=0; i<segments0.size(); i++)
    {
        auto& coords0 = segments0[i].coords;
        auto& coords1 = segments1[i].coords;
        for (size_t j=0; j<coords0.size(); j+=2)
        {
            double x = coords0[j];
            double y = coords0[j+1];
            double x1 = coords1[j];
            double y1 = coords1[j+1];
            double dx = x1 - x;
            double dy = y1 - y;
            coords0[j] = x + pos.x * dx + offset_x;
            coords0[j+1] = y + pos.y * dy + offset_y;
        }
    }

    return result;
}

bool KeyPath::is_point_within(Point point)
{
    return find_polygon_if([&](const Polygon& polygon)
    {
        return is_point_in_polygon(polygon, point);
    });
}

bool KeyPath::is_point_in_polygon(std::vector<double> vertices, const Point& pt)
{
    bool c = false;
    size_t n = vertices.size();

    if (n < 2)
        return false;
    double x = pt.x;
    double y = pt.y;
    double x0 = vertices[n - 2];
    double y0 = vertices[n - 1];

    for (size_t i=0; i<n; i+=2)
    {
        double x1 = vertices[i];
        double y1 = vertices[i+1];
        if (((y1 <= y && y < y0) || (y0 <= y && y < y1)) &&
            (x < (x0 - x1) * (y - y1) / (y0 - y1) + x1))
        {
            c = !c;
        }
        x0 = x1;
        y0 = y1;
    }

    return c;
}
