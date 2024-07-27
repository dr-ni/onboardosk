#ifndef IMAGE_H
#define IMAGE_H

#include <memory>

#include "color.h"
#include "noneable.h"
#include "point_decl.h"
#include "rect_fwd.h"


class CairoContext;

enum class ImageStyle
{
    SINGLE_COLOR = 0,
    MULTI_COLOR = 1,
    DESATURATED = 2,
};

class Image
{
    public:
        using Ptr = std::shared_ptr<Image>;

        Image();
        virtual ~Image();

        static Ptr from_file_and_size(const std::string& fn, const Size& size);
        void draw_styled(CairoContext* cc, const Rect& rect,
                         const RGBA& rgba, ImageStyle image_style,
                         double window_scaling_factor=1.0);

        virtual void draw(CairoContext* cr, const Rect& r) = 0;
        virtual void set_as_source(CairoContext* cr, const Rect& r) = 0;
        virtual Size get_content_size()  = 0;

        Size get_size() const {return m_size;}

    protected:
        Size m_size;
};

typedef Image::Ptr ImagePtr;



#endif // IMAGE_H
