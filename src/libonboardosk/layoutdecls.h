#ifndef LAYOUTDECLS_H
#define LAYOUTDECLS_H

// Forward declarations needed by other includes.

#include <ostream>
#include <memory>
#include <string>

#include "tools/keydecls.h"


typedef std::string LayoutItemId;
typedef LayoutItemId KeyId;

class ColorScheme;
typedef  std::shared_ptr<ColorScheme> ColorSchemePtr;

class Theme;
typedef  std::shared_ptr<Theme> ThemePtr;

class LayoutItem;
typedef  std::shared_ptr<LayoutItem> LayoutItemPtr;

class LayoutKey;
typedef  std::shared_ptr<LayoutKey> LayoutKeyPtr;

class LayoutPanel;
typedef  std::shared_ptr<LayoutPanel> LayoutPanelPtr;

class LayoutRoot;
typedef  std::shared_ptr<LayoutRoot> LayoutRootPtr;

class LayoutPanelWordList;
typedef  std::shared_ptr<LayoutPanelWordList> LayoutPanelWordListPtr;

class LayoutWordListKey;
typedef  std::shared_ptr<LayoutWordListKey> LayoutWordListKeyPtr;

class KeyPath;
typedef  std::shared_ptr<KeyPath> KeyPathPtr;

class Image;
typedef  std::shared_ptr<Image> ImagePtr;

enum class LOD
{
    MINIMAL,    // clearly visible reduced detail, fastest
    REDUCED,    // slightly reduced detail
    FULL,       // full detail
};


class KeyStyle
{
    public:
        enum Enum {
            FLAT,
            GRADIENT,
            DISH,
            NONE,
        };

        static Enum to_key_style(const std::string s);
};

class KeyAction
{
    public:
        enum Enum {
            SINGLE_STROKE,          // press on button down, release on up, allows key-repeat (default)
            DELAYED_STROKE,         // press+release on button up, supports long press (MENU)
            DOUBLE_STROKE,          // press+release on button down and up, (CAPS, NMLK)
            NONE,
        };

        static Enum layout_string_to_key_action(const std::string s);
};
std::string to_string(KeyAction::Enum e);
std::ostream& operator<< (std::ostream& out, KeyAction::Enum& e);


#endif // LAYOUTDECLS_H
