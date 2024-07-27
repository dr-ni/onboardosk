#ifndef COLORSCHEME_H
#define COLORSCHEME_H

#include <memory>
#include <string>

#include "tools/color.h"
#include "tools/container_helpers.h"
#include "tools/version.h"

#include "layoutdecls.h"
#include "onboardoskglobals.h"

using string = std::string;

class KeyState;
class CSRoot;

typedef std::shared_ptr<CSRoot> ColorSchemeRootPtr;



class ColorScheme : public ContextBase
{
    public:

        // onboard 0.95
        static constexpr const Version COLOR_SCHEME_FORMAT_LEGACY  {1, 0};

        // onboard 0.97, tree format, rule-based color matching
        static constexpr const Version COLOR_SCHEME_FORMAT_TREE    {2, 0};

        // onboard 0.99, added window colors
        static constexpr const Version COLOR_SCHEME_WINDOW_COLORS  {2, 1};

        // current format
        static constexpr const Version COLOR_SCHEME_FORMAT         {COLOR_SCHEME_WINDOW_COLORS};

        ColorScheme();

    public:
        using Super = ContextBase;
        using This = ColorScheme;
        using Ptr = std::shared_ptr<This>;

        ColorScheme(const ContextBase& context);
        static std::unique_ptr<This> make(const ContextBase& context);

        bool is_key_in_scheme(LayoutItemPtr item);

        // Color for the given key element and optionally key state.
        // If <state> is None the key state is retrieved from <item>.
        RGBA get_key_rgba(const LayoutItemPtr& item, string element, const KeyState* state_in={});

        // Window colors.
        // window_type may be "keyboard" or "key-popup".
        // element may be "border"
        RGBA get_window_rgba(const string& window_type, const string& element);

        // Background fill color of the layer at the given index.
        RGBA get_layer_fill_rgba(size_t layer_index);

        // Color for the given element of the floating icon.
        RGBA get_icon_rgba(const string& element);

    private:
        RGBA get_key_default_rgba(const LayoutItemPtr& item, const string& element, const KeyState* state);
        RGBA get_insensitive_color(const LayoutItemPtr& item, const KeyState* state, const string& element);

    private:
        std::string m_name;
        std::string m_filename;
        bool m_is_system{false};
        ColorSchemeRootPtr m_root;       // tree root

        friend class ColorSchemeLoader;
};

typedef  std::shared_ptr<ColorScheme> ColorSchemePtr;



typedef std::map<std::string, ColorSchemePtr> ColorSchemeMap;

class ColorSchemeLoader : public ContextBase
{
    public:
        using Super = ContextBase;
        using This = ColorSchemeLoader;

        ColorSchemeLoader(const ContextBase& context);

        void get_merged_color_schemes(ColorSchemeMap& color_schemes);
        std::vector<ColorSchemePtr> load_color_schemes(bool is_system = false);
        std::vector<std::string> find_color_schemes(const std::string& path);
        ColorSchemePtr load(const std::string& filename, bool is_system = false);
};


#endif // COLORSCHEME_H
