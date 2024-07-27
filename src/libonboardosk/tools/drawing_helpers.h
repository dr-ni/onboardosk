#ifndef DRAWING_HELPERS_H
#define DRAWING_HELPERS_H

#include <vector>

struct _cairo;
typedef struct _cairo cairo_t;

void polygon_to_rounded_path(std::vector<double> coords, double r_pct, double chamfer_size,
                             std::vector<std::vector<double>>& path);

#endif // DRAWING_HELPERS_H
