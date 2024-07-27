#include <memory>

#include "tools/cairocontext.h"
#include "drawingcontext.h"

DrawingContext::DrawingContext(cairo_t* cr) :
    m_cr(std::make_shared<CairoContext>(cr))
{

}

