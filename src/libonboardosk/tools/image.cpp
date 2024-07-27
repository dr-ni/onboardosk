
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "rect_decl.h"

#include "cairocontext.h"
#include "image.h"
#include "imagesvg.h"

using std::vector;
using std::string;


Image::Image()
{}

Image::~Image()
{}

Image::Ptr Image::from_file_and_size(const std::string& fn, const Size& size)
{
    string extension = fs::path(fn).extension();
    if (extension == ".svg")
        return image_svg_from_file_and_size(fn, size);
    return {};
}
#include <iostream>

// Draw the image in the theme's label color.
// Only the alpha channel of the image is used.
void Image::draw_styled(CairoContext* cc, const Rect& rect,
                        const RGBA& rgba, ImageStyle image_style,
                        double window_scaling_factor)
{
    auto sz = get_content_size();
    Rect r = Rect{Point{0 ,0}, sz}.inscribe_with_aspect(rect);

    cc->save();

    //cr->translate(r.get_position());

    //cr->translate(rect.get_position());
    if (window_scaling_factor != 1.0)
        cc->scale({1.0 / window_scaling_factor, 1.0 / window_scaling_factor});

    // colored?
    if (image_style == ImageStyle::MULTI_COLOR)
    {
        set_as_source(cc, r);
        cc->paint();
    }

    // grayscale?
    else
    if (image_style == ImageStyle::DESATURATED)
    {
        set_as_source(cc, r);
        auto pattern = cc->get_source();

        cc->push_group_with_content(CairoContext::Content::COLOR_ALPHA);
        //set_as_source(cr, r);
        cc->set_source(pattern);
        cc->paint();

        double saturation = 0.3;
        RGBA color = HLS(0, 0.5, saturation, 1.0).to_rgb();

        cc->set_source_rgba(color);
        //cr->set_source_rgba({1, 0, 0, 1});
        cc->set_operator(CairoContext::Operator::HSL_SATURATION);
        cc->paint();

        cc->pop_group_to_source();
        cc->mask(pattern);
    }

    // single color
    else
    {
        set_as_source(cc, r);
        auto pattern = cc->get_source();
        cc->set_source_rgba(rgba);
        cc->mask(pattern);
    }

    cc->restore();
}

