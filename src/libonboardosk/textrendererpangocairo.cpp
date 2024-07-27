
#include <pango/pangocairo.h>

#include "tools/cairocontext.h"
#include "tools/ustringmain.h"

#include "textrendererpangocairo.h"


TextRendererPangoCairo::TextRendererPangoCairo()
{
    auto fontmap = pango_cairo_font_map_get_default();
    m_pc = pango_font_map_create_context(fontmap);
}

TextRendererPangoCairo::TextRendererPangoCairo(CairoContext* cc) :
    m_cc(cc)
{
}

TextRendererPangoCairo::TextRendererPangoCairo(PangoContext* pc) :
    m_pc(pc)
{
    g_object_ref(m_pc);
}

TextRendererPangoCairo::~TextRendererPangoCairo()
{
    if(m_layout)
        g_object_unref (m_layout);
    if(m_pc)
        g_object_unref (m_pc);
}

void TextRendererPangoCairo::set_text(const std::string& text)
{
    m_text = text;
    if (m_layout)
        pango_layout_set_text (m_layout, m_text.c_str(), -1);
}

void TextRendererPangoCairo::set_font(const std::string& font_str, double font_size)
{
    m_font_str = font_str;
    m_font_size = font_size;
}

Size TextRendererPangoCairo::get_size()
{
    if (!m_layout)
        create(m_cc);
    if (m_layout)
    {
        int w, h;
        pango_layout_get_size (m_layout, &w, &h);
        return {static_cast<double>(w / PANGO_SCALE),
                    static_cast<double>(h / PANGO_SCALE)};
    }
    return {};
}

double TextRendererPangoCairo::get_pixel_scale()
{
    return PANGO_SCALE;
}

void TextRendererPangoCairo::show(CairoContext* cc)
{
    if (!m_layout)
        create(cc);
    if (m_layout)
    {
        auto cr = cc->get_cr();
        pango_cairo_show_layout (cr, m_layout);
    }
}

double TextRendererPangoCairo::ellipsize(UString& ellipsized_text,
                                         const UString& text, double max_width)
{
    ellipsized_text = text;
    set_text(ellipsized_text.to_utf8());

    double w = get_size().w;
    if (w > max_width)
    {
        // Grow one char at a time. Inefficient, but it isn't done often
        // anyway. PangoLayout introspection is too broken to use its
        // built-in ellipsizing capability (Xenial).
        // TODO: use pango_layout_set_ellipsize
        w = 0;
        size_t n = text.size();
        for (size_t i=0; i<n; ++i)
        {
            ellipsized_text = text.slice(0, static_cast<int>(i)) + "...";
            set_text(ellipsized_text.to_utf8());

            double wt = get_size().w;

            if (wt > max_width)
                break;
            w = wt;
        }
    }
    return w;
}

void TextRendererPangoCairo::create(CairoContext* cc)
{
    if (!m_layout)
    {
        if (cc)
        {
            auto cr = cc->get_cr();
            m_layout = pango_cairo_create_layout (cr);
            if (!m_layout)
                return;
        }
        else if (m_pc)
        {
            m_layout = pango_layout_new(m_pc);
            if (!m_layout)
                return;
        }
    }

    PangoFontDescription* desc =
            pango_font_description_from_string (m_font_str.c_str());
    if (!desc)
        return;
    pango_font_description_set_size(
                desc,  std::max(1, static_cast<gint>(m_font_size)));
    pango_layout_set_font_description (m_layout, desc);
    pango_font_description_free (desc);

    pango_layout_set_width (m_layout, -1); // no wrapping, ellipsization

    set_text(m_text);
}
