#ifndef LAYOUTCONTEXT_H
#define LAYOUTCONTEXT_H

#include <memory>

#include "tools/rect_fwd.h"

#include "keypath.h"


// Transforms logical coordinates to canvas coordinates and vice versa.
class LayoutContext
{
    public:
        LayoutContext() = default;

        Point log_to_canvas(const Point& pt) const {
            return {log_to_canvas_x(pt.x),
                        log_to_canvas_y(pt.y)};
        }

        Rect log_to_canvas_rect(const Rect& rect) const {
            if (rect.empty())
                return {};
            return {log_to_canvas_x(rect.x),
                    log_to_canvas_y(rect.y),
                    scale_log_to_canvas_x(rect.w),
                    scale_log_to_canvas_y(rect.h)};
        }

        double log_to_canvas_x(double x) const {
            return canvas_rect.x + (x - log_rect.x) * canvas_rect.w / log_rect.w;
        }

        double log_to_canvas_y(double y) const {
            return canvas_rect.y + (y - log_rect.y) * canvas_rect.h / log_rect.h;
        }

        Size scale_log_to_canvas(const Size& sz) const {
            return {scale_log_to_canvas_x(sz.w),
                    scale_log_to_canvas_y(sz.h)};
        }
        /*
    def scale_log_to_canvas_l(self, coord):
        return list(this->scale_log_to_canvas(coord))
        */

        double scale_log_to_canvas_x(double x) const {
            return x * this->canvas_rect.w / this->log_rect.w;
        }

        double scale_log_to_canvas_y(double y) const {
            return y * this->canvas_rect.h / this->log_rect.h;
        }

        Point canvas_to_log(const Point pt) const {
            return {canvas_to_log_x(pt.x),
                        canvas_to_log_y(pt.y)};
        }

        Rect canvas_to_log_rect(const Rect& rect) const {
            return {canvas_to_log_x(rect.x),
                        canvas_to_log_y(rect.y),
                        scale_canvas_to_log_x(rect.w),
                        scale_canvas_to_log_y(rect.h)};
        }

        double canvas_to_log_x(double x) const {
            return (x - canvas_rect.x) * log_rect.w / canvas_rect.w + log_rect.x;
        }

        double canvas_to_log_y(double y) const {
            return (y - canvas_rect.y) * log_rect.h / canvas_rect.h + log_rect.y;
        }

        Point scale_canvas_to_log(const Point& pt) const {
            return {this->scale_canvas_to_log_x(pt.x),
                    this->scale_canvas_to_log_y(pt.y)};
        }

        /*
    def scale_canvas_to_log_l(self, coord):
        return list(this->scale_canvas_to_log(coord))
    */
        double scale_canvas_to_log_x(double x) const {
            return x * this->log_rect.w / this->canvas_rect.w;
        }

        double scale_canvas_to_log_y(double y) const {
            return y * this->log_rect.h / this->canvas_rect.h;
        }

        KeyPath::Ptr log_to_canvas_path(KeyPath::Ptr path);


    public:
        // Logical rectangle as defined by the keyboard layout.
        // Never changed after loading.
        Rect initial_log_rect{0.0, 0.0, 1.0, 1.0};  // includes border

        // Logical rectangle as defined by the keyboard layout.
        // May be changed after loading e.g. for word suggestion keys.
        Rect log_rect{0.0, 0.0, 1.0, 1.0};  // includes border

        // Canvas rectangle in drawing units.
        Rect canvas_rect{0.0, 0.0, 1.0, 1.0};

};

std::ostream& operator<< (std::ostream& out, const LayoutContext& c);


#endif // LAYOUTCONTEXT_H
