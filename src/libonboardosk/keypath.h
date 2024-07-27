#ifndef KEYPATH_H
#define KEYPATH_H

#include <memory>
#include <string>
#include <vector>

#include "tools/point_decl.h"
#include "tools/rect_decl.h"
#include "tools/enum_helpers.h"

// operation
MAKE_ENUM(SegmentType,
    MOVE_TO,
    LINE_TO,
    CLOSE_PATH,
)

class PathSegment
{
    public:
        PathSegment(SegmentType type_, const std::vector<double> coords_) :
            type(type_), coords(coords_)
        {}

        SegmentType type;
        std::vector<double> coords;
};

class KeyPath
{
    public:
        using Ptr = std::shared_ptr<KeyPath>;
        using Polygon = std::vector<double>;

    public:
        static Ptr from_svg_path(const std::string& path_str);

        static Ptr from_rect(const Rect& rect);

        Ptr copy();

        // Append a SVG path data string to the path.
        /*
        Doctests:;
        // absolute move_to command
        >>> p = KeyPath.from_svg_path("M 100 200 120 -220");
        >>> print(p.segments);
        [[0, [100.0, 200.0]], [1, [120.0, -220.0]]];

        // relative move_to command
        >>> p = KeyPath.from_svg_path("m 100 200 10 -10");
        >>> print(p.segments);
        [[0, [100.0, 200.0]], [1, [110.0, 190.0]]];

        // relative move_to && close_path segments
        >>> p = KeyPath.from_svg_path("m 100 200 10 -10 z");
        >>> print(p.segments);
        [[0, [100.0, 200.0]], [1, [110.0, 190.0]], [2, []]];

        // spaces && commas && are optional where possible
        >>> p = KeyPath.from_svg_path("m100,200 10-10z");
        >>> print(p.segments);
        [[0, [100.0, 200.0]], [1, [110.0, 190.0]], [2, []]];

        // Inkscape in Zesty uses horizontal && vertical lines in paths.
        >>> p = KeyPath.from_svg_path(
        ...     "m 257.5,59.5 h 25 v 37 h -20 v -19 h -5 z");
        >>> print(p.segments[:3]);
        [[0, [257.5, 59.5]], [1, [282.5, 59.5]], [1, [282.5, 96.5]]];
        >>> print(p.segments[3:]);
        [[1, [262.5, 96.5]], [1, [262.5, 77.5]], [1, [257.5, 77.5]], [2, []]];
        */
        void append_svg_path(const std::string& path_str);

        // Append a single command and its coordinate data to the path.
        /*

            Doctests:;
            // first lowercase move_to position is absolute
            >>> p = KeyPath();
            >>> p.append_command("m", [100, 200]);
            >>> print(p.segments);
            [[0, [100, 200]]];

            // move_to segments become line_to segments after the first position
            >>> p = KeyPath();
            >>> p.append_command("M", [100, 200, 110, 190]);
            >>> print(p.segments);
            [[0, [100, 200]], [1, [110, 190]]];

            // further lowercase move_to positions are relative, must become absolute
            >>> p = KeyPath();
            >>> p.append_command("m", [100, 200, 10, -10, 10, -10]);
            >>> print(p.segments);
            [[0, [100, 200]], [1, [110, 190, 120, 180]]];

            // further lowercase segments must still be become absolute
            >>> p = KeyPath();
            >>> p.append_command("m", [100, 200, 10, -10, 10, -10]);
            >>> p.append_command("l", [1, -1, 1, -1]);
            >>> print(p.segments);
            [[0, [100, 200]], [1, [110, 190, 120, 180]], [1, [121, 179, 122, 178]]];

            // L is an absolute line
            >>> p = KeyPath();
            >>> p.append_command("M", [100, 200]);
            >>> p.append_command("L", [10, 20, 30, 40]);
            >>> p.append_command("L", [10, 20]);
            >>> print(p.segments);
            [[0, [100, 200]], [1, [10, 20, 30, 40]], [1, [10, 20]]];

            // h is a relative horizontal line
            >>> p = KeyPath();
            >>> p.append_command("M", [100, 200]);
            >>> p.append_command("h", [50]);
            >>> p.append_command("l", [10, 20]);
            >>> print(p.segments);
            [[0, [100, 200]], [1, [150, 200]], [1, [160, 220]]];

            // H is an absolute horizontal line
            >>> p = KeyPath();
            >>> p.append_command("M", [100, 200]);
            >>> p.append_command("H", [50]);
            >>> p.append_command("l", [10, 20]);
            >>> print(p.segments);
            [[0, [100, 200]], [1, [50, 200]], [1, [60, 220]]];

            // v is a relative vertical line
            >>> p = KeyPath();
            >>> p.append_command("M", [100, 200]);
            >>> p.append_command("v", [60]);
            >>> p.append_command("l", [20, 10]);
            >>> print(p.segments);
            [[0, [100, 200]], [1, [100, 260]], [1, [120, 270]]];

            // V is an absolute vertical line
            >>> p = KeyPath();
            >>> p.append_command("M", [100, 200]);
            >>> p.append_command("V", [60]);
            >>> p.append_command("l", [20, 10]);
            >>> print(p.segments);
            [[0, [100, 200]], [1, [100, 60]], [1, [120, 70]]];
        */

        void append_command(std::string& cmd, std::vector<double>& coords);

        // Split SVG path date into command && coordinate tokens.
        /*
                    Doctests:;
                    >>> KeyPath._tokenize_svg_path("m 10,20");
                    ['m', '10', ',', '20'];
                    >>> KeyPath._tokenize_svg_path("   m   10  , \\n  20 ");
                    ['m', '10', ',', '20'];
                    >>> KeyPath._tokenize_svg_path("m 10,20 30,40 z");
                    ['m', '10', ',', '20', '30', ',', '40', 'z'];
                    >>> KeyPath._tokenize_svg_path("m10,20 30,40z");
                    ['m', '10', ',', '20', '30', ',', '40', 'z'];
                    >>> KeyPath._tokenize_svg_path("M100.32 100.09 100. -100.");
                    ['M', '100.32', '100.09', '100.', '-100.'];
                    >>> KeyPath._tokenize_svg_path("m123+23 20,-14L200,200");
                    ['m', '123', '+23', '20', ',', '-14', 'L', '200', ',', '200'];
                    >>> KeyPath._tokenize_svg_path("m123+23 20,-14L200,200");
                    ['m', '123', '+23', '20', ',', '-14', 'L', '200', ',', '200']
                    */
        std::vector<std::string> tokenize_svg_path(const std::string& path_str);

        Rect get_bounds()
        {
            if (!m_bounds_valid)
                m_bounds = calc_bounds();
            return m_bounds;
        }

        // Compute the bounding box of the path.
        /*

                    Doctests:;
                    // Simple move_to path, something inkscape would create.
                    >>> p = KeyPath.from_svg_path("m 100,200 10,-10 z");
                    >>> print(p.get_bounds());
                    Rect(x=100.0 y=190.0 w=10.0 h=10.0);
        */
        Rect calc_bounds();

        // Returns a new path which is larger by dx and dy on all sides.
        KeyPath::Ptr inflate(double d)
        {
            return inflate(d, d);
        }
        KeyPath::Ptr inflate(double dx, double dy)
        {
            Rect rect = get_bounds().inflate(dx, dy);
            return fit_in_rect(rect);
        }

        // Scales and translates the path so that rect
        // becomes its new bounding box.
        Ptr fit_in_rect(const Rect& rect);

        // Interpolate between self and path1.
        // Paths must have the same structure (length and operations).
        // pos: 0.0 = self, 1.0 = path1.;
        Ptr linint(const Ptr& path1, const Point& pos={1.0, 1.0},
                   double offset_x = 0.0, double offset_y = 0.0);

        bool is_point_within(Point point);
        bool is_point_in_polygon(std::vector<double> vertices, const Point& pt);

        template< typename F >
        void for_each_polygon(const F& func)
        {
            find_polygon_if([&](const Polygon& polygon)
                                {func(polygon); return false;});
        }

        // Loop through all independent polygons in the path.
        // Can't handle splines and arcs, everything has to
        // be polygons from here.
        template< typename F >
        bool find_polygon_if(const F& predicate)
        {
            std::vector<double> polygon;

            for (auto& segment  : this->segments)
            {
                auto op = segment.type;
                if (op == SegmentType::LINE_TO)
                    polygon.insert(polygon.end(), segment.coords.begin(), segment.coords.end());

                else
                    if (op == SegmentType::MOVE_TO)
                        polygon = segment.coords;

                    else
                        if (op == SegmentType::CLOSE_PATH)
                            if (predicate(polygon))
                                return true;
            }
            return false;
        }

    private:
        Point m_last_abs_pos{0.0, 0.0};
        Rect m_bounds;                      // cached bounding box
        bool m_bounds_valid{false};

    public:
        std::vector<PathSegment> segments;  // normalized list of path segments (all absolute)
};


#endif // KEYPATH_H
