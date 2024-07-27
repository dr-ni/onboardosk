#ifndef IMAGESVG_H
#define IMAGESVG_H

#include <memory>

#include "tools/point_fwd.h"

class Image;
typedef std::shared_ptr<Image> ImagePtr;

ImagePtr image_svg_from_file_and_size(const std::string& fn, const Size& size);

#endif // IMAGESVG_H
