#include <exception>

#include <librsvg/rsvg.h>

#include "tools/rect_decl.h"
#include "tools/string_helpers.h"

#include "cairocontext.h"
#include "image.h"


class ImageSVG : public Image
{
    public:
        using Ptr = std::shared_ptr<ImageSVG>;

        ImageSVG(RsvgHandle* handle, const Size& size) :
            m_handle(handle)
        {
            m_size = size;
        }

        virtual ~ImageSVG()
        {
            if (m_handle)
            {
                //rsvg_handle_close(m_handle, nullptr);
                g_object_unref(m_handle);
            }
        }

        virtual void draw(CairoContext* cr, const Rect& r) override;
        virtual void set_as_source(CairoContext* cr, const Rect& r) override;

        virtual Size get_content_size() override;

    private:
        RsvgHandle* m_handle{};
};


ImagePtr image_svg_from_file_and_size(const std::string& fn, const Size& size)
{
    //return std::make_shared<ImageSVG>(nullptr, size);
    GError *error = nullptr;
    auto handle = rsvg_handle_new_from_file(fn.c_str(), &error);
    if (error)
    {
        std::string msg = sstr()
            << "rsvg_handle_new_from_file failed for " << repr(fn)
            << ": " << error->domain << ": " << error->message
            << " (" << error->code << ")";
        g_error_free(error);
        throw std::runtime_error(msg);
    }

    return std::make_shared<ImageSVG>(handle, size);
}

void ImageSVG::draw(CairoContext* cr, const Rect& r)
{
    auto sz = get_content_size();

    cr->save();
    cr->translate(r.get_position());
    cr->scale(r.get_size()/sz);

    if (m_handle)
        rsvg_handle_render_cairo (m_handle, cr->get_cr());

    cr->restore();
}

void ImageSVG::set_as_source(CairoContext* cr, const Rect& r)
{
    // Keep pattern size and memory leaks down.
    // Clipping to integer boundaries is fastest.
    cr->rectangle(r.floor());
    cr->clip();

    cr->push_group_with_content(CairoContext::Content::COLOR_ALPHA);
    draw(cr, r);
    cr->pop_group_to_source();
}

Size ImageSVG::get_content_size()
{
    if (!m_handle)
        return {};

    RsvgDimensionData dd;
    rsvg_handle_get_dimensions(m_handle, &dd);
    return {static_cast<double>(dd.width), static_cast<double>(dd.height)};
}
