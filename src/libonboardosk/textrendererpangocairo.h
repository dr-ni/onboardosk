#ifndef TEXTRENDERERPANGOCAIRO_H
#define TEXTRENDERERPANGOCAIRO_H

#include <string>

#include "tools/point_fwd.h"

struct _PangoContext;
typedef struct _PangoContext PangoContext;

struct _PangoLayout;
typedef struct _PangoLayout PangoLayout;

struct _PangoFontDescription;
typedef struct _PangoFontDescription PangoFontDescription;

class CairoContext;
class UString;

class TextRendererPangoCairo
{
    public:
        TextRendererPangoCairo();  // creates PangoContext without cairo
        TextRendererPangoCairo(CairoContext* cc);
        TextRendererPangoCairo(PangoContext* pc);

        ~TextRendererPangoCairo();

        void set_text(const std::string& text);

        void set_font(const std::string& font_str, double font_size);

        // layout size in pixels
        Size get_size();

        // font_size for a single device pixel
        double get_pixel_scale();

        void show(CairoContext* cc);

        // Shorten very long words and add an ellipsis.
        double ellipsize(UString& ellipsized_text, const UString& text, double max_width);
    private:
        void create(CairoContext* cc);

    private:
        CairoContext* m_cc{};
        PangoContext* m_pc{};
        PangoLayout* m_layout{};
        std::string m_text;
        std::string m_font_str;
        double m_font_size{1.0};
};


#endif // TEXTRENDERERPANGOCAIRO_H
