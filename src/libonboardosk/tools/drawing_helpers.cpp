#include <cmath>
#include <vector>

#include <cairo.h>

#include "drawing_helpers.h"

using std::size_t;

void polygon_to_rounded_path(std::vector<double> coords, double r_pct, double chamfer_size,
                             std::vector<std::vector<double>>& path)
{
    double r = chamfer_size * 2.0 * std::min(r_pct/100.0, 0.5); // full range at 50%

    size_t n = coords.size();
    for (size_t i=0; i<n; i+=2)
    {
        size_t i0 = i;
        size_t i1 = i + 2;
        if (i1 >= n)
            i1 -= n;

        size_t i2 = i + 4;
        if (i2 >= n)
            i2 -= n;

        double x0 = coords[i0];
        double y0 = coords[i0+1];
        double x1 = coords[i1];
        double y1 = coords[i1+1];
        double x2 = coords[i2];
        double y2 = coords[i2+1];

        double vax = x1 - x0;
        double vay = y1 - y0;
        double la = std::sqrt(vax*vax + vay*vay);
        double uax = vax / la;
        double uay = vay / la;

        double vbx = x2 - x1;
        double vby = y2 - y1;
        double lb = std::sqrt(vbx*vbx + vby*vby);
        double ubx = vbx / lb;
        double uby = vby / lb;

        double ra = std::min(r, la * 0.5);     // offset of curve begin && end
        double rb = std::min(r, lb * 0.5);
        double ka = (ra-1) * r_pct/200.0; // offset of control points
        double kb = (rb-1) * r_pct/200.0;

        double x;
        double y;
        if (i == 0)
        {
            x = x0 + ra*uax;
            y = y0 + ra*uay;
            path.emplace_back(std::vector<double>{x, y});
        }

        x = x1 - ra*uax;
        y = y1 - ra*uay;
        path.emplace_back(std::vector<double>{x, y});

        x = x1 + rb*ubx;
        y = y1 + rb*uby;
        double c0x = x1 - ka*uax;
        double c0y = y1 - ka*uay;
        double c1x = x1 + kb*ubx;
        double c1y = y1 + kb*uby;
        path.emplace_back(std::vector<double>{x, y, c0x, c0y, c1x, c1y});
    }
}


