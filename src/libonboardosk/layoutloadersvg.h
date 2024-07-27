#ifndef LAYOUTLOADERSVG_H
#define LAYOUTLOADERSVG_H

#include <memory.h>
#include <string>

#include "layoutdecls.h"

class ContextBase;


class LayoutLoaderSVG
{
    public:
        virtual ~LayoutLoaderSVG() = default;
        virtual LayoutItemPtr load(std::string layout_filename, ColorSchemePtr color_scheme) = 0;

        static std::unique_ptr<LayoutLoaderSVG> make(const ContextBase& context);
};


#endif // LAYOUTLOADERSVG_H
