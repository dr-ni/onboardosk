
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "tools/color.h"
#include "tools/noneable.h"
#include "tools/iostream_helpers.h"
#include "tools/path_helpers.h"
#include "tools/platform_helpers.h"

#include "colorscheme.h"
#include "layoutkey.h"

// ColorSchemeLoader
#include <map>
#include <set>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "tools/logger.h"
#include "configuration.h"
#include "exception.h"
#include "xmlparser.h"

using std::array;
using std::string;
using std::vector;


class CSKeyGroup;
typedef  std::shared_ptr<CSKeyGroup> CSKeyGroupPtr;


// Base class of color scheme items
class ColorSchemeItem : public TreeItem<ColorSchemeItem>
{
    public:
        using Ptr = std::shared_ptr<ColorSchemeItem>;

        virtual bool is_window()
        {
            return false;
        }
        virtual bool is_layer()
        {
            return false;
        }
        virtual bool is_icon()
        {
            return false;
        }
        virtual bool is_key_group()
        {
            return false;
        }
        virtual bool is_color()
        {
            return false;
        }

        // Find the key group that has key_id
        CSKeyGroupPtr find_key_id(const string& key_id);
};
typedef ColorSchemeItem::Ptr ColorSchemeItemPtr;

// Container for a window's colors
class CSWindow : public ColorSchemeItem
{
    public:
        using Ptr = std::shared_ptr<CSWindow>;

        virtual bool is_window() override
        {
            return true;
        }

        string type;   // "keyboard", "key-popup"
};

// Container for a layer's colors
class CSLayer : public ColorSchemeItem
{
    public:
        using Ptr = std::shared_ptr<CSLayer>;

        virtual bool is_layer() override
        {
            return true;
        }
};


// Container for a Icon's' colors
class CSIcon : public ColorSchemeItem
{
    public:
        using Ptr = std::shared_ptr<CSIcon>;

        virtual bool is_icon() override
        {
            return true;
        }
};

#define SHARED_PTR_CLASS(class_name, base_class_name) class class_name : public base_class_name \
{ \
    public: \
        using Super = base_class_name; \
        using This = class_name; \
        using Ptr = std::shared_ptr<class_name>;



// A single color, rgb + opacity
class CSColor : public ColorSchemeItem
{
    public:
        using Super = ColorSchemeItem;
        using Ptr = std::shared_ptr<CSColor>;

        virtual void dump(std::ostream& out) const
        {
            out << "CSColor(";
            Super::dump(out);
            out << " element='" << this->element << "'"
                << " rgb=" << this->rgb
                << " opacity=" << this->opacity;
            out << ")";
        }

        virtual bool is_color() override
        {
            return true;
        }

        virtual bool matches(const string& element_, const KeyState* state={})
        {
            UNUSED(state);
            return this->element == element_;
        }

    public:
        string element;
        Noneable<RGBA> rgb;
        Noneable<double> opacity;
};


// A single key (or layer) color.
class CSKeyColor : public CSColor
{
    public:
        using Super = CSColor;
        using Ptr = std::shared_ptr<CSKeyColor>;

        virtual void dump(std::ostream& out) const
        {
            out << "CSKeyColor(";
            ColorSchemeItem::dump(out);
            out << " element='" << this->element << "'"
                << " rgb=" << this->rgb
                << " opacity=" << this->opacity
                << " state=" << this->state;
            out << ")";
        }

    // Returns true if self matches the given parameters.
    // state attributes match if they are equal or None, i.e. an
    // empty state dict always matches.
    virtual bool matches(const string& element_, const KeyState* state_={}) override
    {
        bool result = true;

        if (this->element != element_)
            return false;

        if (!state_)
            return false;

        for (auto it : *state_)
        {
            auto attr = it.first;
            auto value = it.second;

            // Special case for fill color
            // By default the fill color is only applied to the single
            // state where nothing is pressed, active, locked, etc.
            // All other elements apply to all state permutations if
            // not asked to do otherwise.
            // Allows for hard coded default fill colors to take over without
            // doing anything special in the color scheme files.
            bool default_value = value;  // "don't care", always match unspecified states

            if (element_ == "fill" &&
                contains(array<string, 4>{"active", "locked", "pressed", "scanned"}, attr) &&
               !this->state.contains(attr))
            {
                default_value = false;   // consider unspecified states to be false
            }

            if ((element_ == "label" || element_ == "secondary-label") &&
               attr == "insensitive" &&
               !this->state.contains(attr))
            {
                default_value = false;   // consider unspecified states to be false
            }

            if (this->state.get(attr, default_value) != value)
            {
                result = false;
            }
        }

        return result;
    }

    public:
        KeyState state;   // dict whith "pressed"=true, "active"=false, etc.
};


// A group of key ids and their colors
class CSKeyGroup : public ColorSchemeItem
{
    public:
        using Super = ColorSchemeItem;
        using Ptr = std::shared_ptr<CSKeyGroup>;

        virtual void dump(std::ostream& out) const
        {
            out << "CSKeyGroup(";
            Super::dump(out);
            out << " key_ids=" << this->key_ids;
            out << ")";
        }

        virtual bool is_key_group() override
        {
            return true;
        }

        void find_element_color(const string& element, const KeyState& state, Noneable<RGBA>& rgb, Noneable<double>& opacity)
        {
            rgb.set_none();
            opacity.set_none();

            // walk key groups from self down to the root
            // SIGSEGV if ColorSchemeItem::Ptr is a reference!
            find_to_root_if([&](const ColorSchemeItem::Ptr& item) -> bool
            {
                if (item->is_key_group())
                {
                    // run through all colors of the key group, top to bottom
                    for (auto child : item->get_children())
                    {
                        if (child->is_color())
                        {
                            auto result = child->find_post_order_if([&](const ColorSchemeItem::Ptr& color_item) -> bool
                            {
                                //auto color = std::dynamic_pointer_cast<CSColor>(color_item);
                                auto color = dynamic_cast<CSColor*>(color_item.get());

                                // matching color found?
                                if (color &&
                                    color->matches(element, &state))
                                {
                                    if (rgb.is_none())
                                    {
                                        rgb = color->rgb;
                                    }
                                    if (opacity.is_none())
                                    {
                                        opacity = color->opacity;
                                    }
                                    if (!rgb.is_none() && !opacity.is_none())
                                    {
                                        return true; // break early
                                    }
                                }
                                return false;
                            });

                            if (result)
                                return true;
                        }
                    }
                }
                return false;
            });
        }

    public:
        vector<string> key_ids;
};

typedef CSKeyGroup::Ptr CSKeyGroupPtr;


// Container for a layers colors
class CSRoot : public ColorSchemeItem
{
    public:
        template<class T>
        vector<typename T::Ptr> get_child_items()
        {
            vector<typename T::Ptr> items;
            for (auto item : get_children())
            {
                auto derived = std::dynamic_pointer_cast<T>(item);
                if (derived)
                    items.emplace_back(derived);
            }
            return items;
        }


        // Get list of window in order of appearance
        // in the color scheme file.
        vector<CSWindow::Ptr> get_windows()
        {return get_child_items<CSWindow>();}

        // Get list of layer items in order of appearance
        // in the color scheme file.
        vector<CSLayer::Ptr> get_layers()
        {return get_child_items<CSLayer>();}

        // Get list of the icon items in order of appearance
        // in the color scheme file.
        vector<CSIcon::Ptr> get_icons()
        {return get_child_items<CSIcon>();}

        // Default key group for keys that aren't part of any key group
        CSKeyGroupPtr get_default_key_group()
        {
            for (auto& child : get_children())
                if (child->is_key_group())
                    return std::dynamic_pointer_cast<CSKeyGroup>(child);
            return {};
        }
};


CSKeyGroupPtr ColorSchemeItem::find_key_id(const string& key_id)
{
    if (this->is_key_group())
    {
        CSKeyGroup::Ptr group = std::dynamic_pointer_cast<CSKeyGroup>(getptr());
        auto& key_ids = group->key_ids;
        if (contains(key_ids, key_id))
            return group;
    }

    for (auto child : get_children())
    {
        auto group = child->find_key_id(key_id);
        if (group)
            return group;
    }

    return {};
}



std::unique_ptr<ColorScheme> ColorScheme::make(const ContextBase& context)
{
    return std::make_unique<This>(context);
}

constexpr const Version ColorScheme::COLOR_SCHEME_FORMAT_LEGACY;
constexpr const Version ColorScheme::COLOR_SCHEME_FORMAT_TREE;
constexpr const Version ColorScheme::COLOR_SCHEME_WINDOW_COLORS;
constexpr const Version ColorScheme::COLOR_SCHEME_FORMAT;

ColorScheme::ColorScheme(const ContextBase& context) :
    Super(context)
{
}

bool ColorScheme::is_key_in_scheme(LayoutItemPtr item)
{
    auto key = dynamic_cast<LayoutKey*>(item.get());
    if (key && m_root->find_key_id(key->theme_id))
        return true;
    if (m_root->find_key_id(item->id))
        return true;
    return false;
}

RGBA ColorScheme::get_key_rgba(const LayoutItemPtr& item, string element, const KeyState* state_in)
{
    LayoutKeyPtr key = std::dynamic_pointer_cast<LayoutKey>(item);

    KeyState state;
    if (state_in)
        state = *state_in;
    else
    if (key)
    {
        key->get_state(state);
        if (state.contains("sensitive"))
        {
            state["insensitive"] = !item->sensitive;
            state.remove("sensitive");
        }
    }

    Noneable<RGBA> rgb;
    Noneable<double> opacity;
    Noneable<RGBA> root_rgb;
    Noneable<double> root_opacity;
    CSKeyGroupPtr key_group;

    // First try to find the theme_id then fall back to the generic id
    vector<string> ids;
    if (key)
        ids.emplace_back(key->theme_id);
    ids.emplace_back(key->id);

    // Let numbered keys fall back to their base id, e.g. instead
    // of prediction0, prediction1,... have only "prediction" in
    // the color scheme.
    if (key)  // item can be a DrawingItem too
    {
        if (key->id == "correctionsbg")
        {
            ids.emplace_back("wordlist");
        }
        else
        if (key->id == "predictionsbg")
        {
            ids.emplace_back("wordlist");
        }
        else
        if (key->is_prediction_key())
        {
            ids.emplace_back("prediction");
        }
        else
        if (key->is_correction_key())
        {
            ids.emplace_back("correction");
        }
        else
        if (key->is_layer_button())
        {
            ids.emplace_back(key->get_similar_theme_id("layer"));
            ids.emplace_back("layer");
        }
    }

    // look for a matching key_group && color in the color scheme
    for (auto id : ids)
    {
        key_group = m_root->find_key_id(id);
        if (key_group)
        {
            key_group->find_element_color(element, state, rgb, opacity);
            break;
        }
    }

    // Get root colors as fallback for the case when key id
    // wasn't mentioned anywhere in the color scheme.
    CSKeyGroupPtr root_key_group = m_root->get_default_key_group();
    if (root_key_group)
        root_key_group->find_element_color(element, state, root_rgb, root_opacity);

    // Special case for layer buttons:
    // don't take fill color from the root group,
    // we want the layer fill color instead (via get_key_default_rgba()).
    if (key &&
        ((element == "fill" && key->is_layer_button()) ||
         (element == "label" && key->is_correction_key())))
    {
        // Don't pick layer fill opacity when there is
        // an rgb color defined in the color scheme.
        if (!rgb.is_none() &&
            opacity.is_none())
        {
            opacity = root_opacity;
            if (opacity.is_none())
            {
                opacity = 1.0;
            }
        }
    }
    else
    if (!key_group)
    {
        // All other colors fall back to the root group's colors
        rgb = root_rgb;
        opacity = root_opacity;
    }

    if (rgb.is_none())
        rgb = get_key_default_rgba(item, element, &state);

    if (opacity.is_none())
        opacity = this->get_key_default_rgba(item, element, &state).a;

    RGBA rgba = rgb;
    rgba.a = opacity;
    return rgba;
}

RGBA ColorScheme::get_key_default_rgba(const LayoutItemPtr& item, const string& element, const KeyState* state)
{
    std::map<string, RGBA> colors = {
        {"fill",                     {0.9,  0.85, 0.7, 1.0}},
        {"prelight",                 {0.0,  0.0,  0.0, 1.0}},
        {"pressed",                  {0.6,  0.6,  0.6, 1.0}},
        {"active",                   {0.5,  0.5,  0.5, 1.0}},
        {"locked",                   {1.0,  0.0,  0.0, 1.0}},
        {"scanned",                  {0.45, 0.45, 0.7, 1.0}},
        {"stroke",                   {0.0,  0.0,  0.0, 1.0}},
        {"label",                    {0.0,  0.0,  0.0, 1.0}},
        {"secondary-label",          {0.5,  0.5,  0.5, 1.0}},
        {"dwell-progress",           {0.82, 0.19, 0.25, 1.0}},
        {"correction-label",         {1.0,  0.5,  0.5, 1.0}},
    };

    RGBA rgba = {0.0, 0.0, 0.0, 1.0};
    LayoutKeyPtr key = std::dynamic_pointer_cast<LayoutKey>(item);

    if (element == "fill")
    {
        if (key &&
            key->is_layer_button() &&
            !state->any_true())
        {
            // Special case for base fill color of layer buttons:
            // default color is layer fill color (as in onboard <=0.95).
            auto layer_index = key->get_layer_index();
            rgba = get_layer_fill_rgba(layer_index);
        }

        else
        if (state->get("pressed"))
        {
            KeyState new_state = *state;
            new_state["pressed"] = false;
            rgba = get_key_rgba(key, element, &new_state);

            // Make the default pressed color a slightly darker
            // || brighter variation of the unpressed color.
            HLS hls = rgba.to_hls();

            // boost lightness changes for very dark && very bright colors
            // Ad-hoc formula, purly for aesthetics
            double amount = -(std::log((hls.l+.001)*(1-(hls.l-.001))))*0.05 + 0.08;

            if (hls.l < .5)  // dark color?
                rgba = rgba.brighten(+amount); // brigther
            else
                rgba = rgba.brighten(-amount); // darker
        }

        else
        if (state->get("scanned"))
        {
            rgba = colors["scanned"];

            // Make scanned active modifier keys stick out by blending
            // scanned color with non-scanned color.
            if (state->get("active"))   // includes locked
            {
                // inactive scanned color
                KeyState new_state = *state;
                new_state["active"] = false;
                new_state["locked"] = false;
                RGBA scanned = get_key_rgba(key, element, &new_state);

                // unscanned fill color
                new_state = *state;
                new_state["scanned"] = false;
                RGBA fill = this->get_key_rgba(key, element, &new_state);

                // blend inactive scanned color with unscanned fill color
                rgba = scanned.average(fill);
            }
        }

        else
        if (state->get("prelight"))
        {
            rgba = colors["prelight"];
        }
        else
        if (state->get("locked"))
        {
            rgba = colors["locked"];
        }
        else
        if (state->get("active"))
        {
            rgba = colors["active"];
        }
        else
        {
            rgba = colors["fill"];
        }
    }

    else
    if (element == "stroke")
    {
        rgba = colors["stroke"];
    }

    else
    if (element == "label")
    {
        if (key && key->is_correction_key())
        {
            rgba = colors["correction-label"];
        }
        else
        {
            rgba = colors["label"];
        }

        // dim label color for insensitive keys
        if (state->get("insensitive"))
        {
            rgba = get_insensitive_color(item, state, element);
        }
    }

    else
    if (element == "secondary-label")
    {
        rgba = colors["secondary-label"];

        // dim label color for insensitive keys
        if (state->get("insensitive"))
        {
            rgba = get_insensitive_color(item, state, element);
        }
    }

    else
    if (element == "dwell-progress")
    {
        rgba = colors["dwell-progress"];
    }

    else
    {
        assert(false);   // unknown element
    }

    return rgba;
}

RGBA ColorScheme::get_insensitive_color(const LayoutItemPtr& item, const KeyState* state, const string& element)
{
    KeyState new_state = *state;
    new_state["insensitive"] = false;
    RGBA fill = get_key_rgba(item, "fill", &new_state);
    RGBA rgba = get_key_rgba(item, element, &new_state);
    HLS hlsf = fill.to_hls();
    HLS hlsl = rgba.to_hls();

    // Leave only one third of the lightness difference
    // between label && fill color.
    double amount = (hlsl.l - hlsf.l) * 2.0 / 3.0;
    return rgba.brighten(-amount);
}

RGBA ColorScheme::get_window_rgba(const string& window_type, const string& element)
{
    Noneable<RGBA> rgb;
    Noneable<double> opacity;
    CSWindow::Ptr window;

    for (auto& item : m_root->get_windows())
    {
        if (item->type == window_type)
        {
            window = item;
            break;
        }
    }

    if (window)
    {
        for (auto& item : window->get_children())
        {
            auto color = std::dynamic_pointer_cast<CSColor>(item);
            if (color &&
                color->element == element)
            {
                rgb = color->rgb;
                opacity = color->opacity;
                break;
            }
        }
    }

    if (rgb.is_none())
        rgb = {1.0, 1.0, 1.0, 1.0};

    if (opacity.is_none())
        opacity = 1.0;

    RGBA rgba = rgb;
    rgba.a = opacity;
    return rgba;
}

RGBA ColorScheme::get_layer_fill_rgba(size_t layer_index)
{
    Noneable<RGBA> rgb;
    Noneable<double> opacity;
    auto layers = m_root->get_layers();

    // If there is no layer definition for this index,
    // repeat the last defined layer color.
    layer_index = std::min(layer_index, layers.size() - 1);

    for (auto& item : layers[layer_index]->get_children())
    {
        auto color = std::dynamic_pointer_cast<CSColor>(item);
        if (color &&
            color->element == "background")
        {
            rgb = color->rgb;
            opacity = color->opacity;
            break;
        }
    }

    if (rgb.is_none())
    {
        rgb = {0.5, 0.5, 0.5, 1.0};
    }
    if (opacity.is_none())
    {
        opacity = 1.0;
    }

    RGBA rgba = rgb;
    rgba.a = opacity;
    return rgba;
}

RGBA ColorScheme::get_icon_rgba(const string& element)
{
    Noneable<RGBA> rgb;
    Noneable<double> opacity;
    auto icons = m_root->get_icons();

    for (auto& icon : icons)
    {
        for (auto& child : icon->get_children())
        {
            auto color = std::dynamic_pointer_cast<CSColor>(child);
            if (color &&
                color->element == element)
            {
                rgb = color->rgb;
                opacity = color->opacity;
                break;
            }
        }
    }

    // default icon background is layer0 background
    Noneable<RGBA> rgba_default;
    if (element == "background")
    {
        // hard-coded default is the most common color
        rgba_default = {0.88, 0.88, 0.88, 1.0};
    }
    else
    {
        assert(false);
    }

    if (rgb.is_none() && !rgba_default.is_none())
    {
        rgb = rgba_default;
    }
    if (opacity.is_none() && !rgba_default.is_none())
    {
        opacity = rgba_default.value.a;
    }

    if (rgb.is_none())
    {
        rgb = {0.5, 0.5, 0.5, 1.0};
    }
    if (opacity.is_none())
    {
        opacity = 1.0;
    }

    RGBA rgba = rgb;
    rgba.a = opacity;
    return rgba;
}




typedef std::set<std::string> UsedKeysMap;

// Load all color schemes from either the user or the system directory.
ColorSchemeLoader::ColorSchemeLoader(const ContextBase& context) :
    ContextBase(context)
{}

std::vector<ColorSchemePtr> ColorSchemeLoader::load_color_schemes(bool is_system)
{
    vector<ColorSchemePtr> color_schemes;
    string path;
    if (is_system)
        path = config()->get_system_color_scheme_dir();
    else
        path = config()->get_user_color_scheme_dir();

    vector<string> filenames = find_color_schemes(path);
    for (auto filename : filenames)
    {
        auto color_scheme = load(filename, is_system);
        if (color_scheme)
            color_schemes.emplace_back(color_scheme);
    }

    return color_schemes;
}

// Returns the full path names of all color schemes found in the given path.
std::vector<std::string> ColorSchemeLoader::find_color_schemes(const std::string& path)
{
    vector<std::string> filenames;

    try
    {
        for (fs::directory_entry e : fs::directory_iterator(path))
            if (fs::is_regular_file(e) &&
                e.path().extension() ==
                    "." + config()->color_scheme_file_extension)
                filenames.emplace_back(e.path());
    }
    catch (const fs::filesystem_error& ex)
    {
        LOG_ERROR << "directory_iterator failed: " << ex.what();
    }

    return filenames;
}

// Parses common properties of all items
static void parse_xml_node_item(const XMLNodePtr& node, ColorSchemeItem* item)
{
    node->get_attribute("id", item->id);
}

static ColorSchemeItemPtr parse_window(const XMLNodePtr& node)
{
    auto item = std::make_shared<CSWindow>();
    node->get_attribute("type", item->type);
    parse_xml_node_item(node, item.get());
    return item;
}

static ColorSchemeItemPtr parse_layer(const XMLNodePtr& node)
{
    auto item = std::make_shared<CSLayer>();
    parse_xml_node_item(node, item.get());
    return item;
}

static ColorSchemeItemPtr parse_icon(const XMLNodePtr& node)
{
    auto item = std::make_shared<CSIcon>();
    parse_xml_node_item(node, item.get());
    return item;
}

static ColorSchemeItemPtr  parse_key_group(const XMLNodePtr& node, UsedKeysMap& used_keys)
{
    CSKeyGroup::Ptr item = std::make_shared<CSKeyGroup>();
    parse_xml_node_item(node, item.get());

    // read key ids
    string text;
    for (auto child=node->get_first_child(); child ; child = child->get_next_sibling())
        if (child->get_node_type() == XMLNodeType::TEXT)
            text += child->get_text();

    auto tokens = re_findall(text, R"([\w-]+(?:[.][\w-]+)?)");

    vector<string> key_ids;
    for (auto& token : tokens)
    {
        auto key_id = strip(token);
        if (!key_id.empty())
        {
            // check for duplicate key definitions
            if (contains(used_keys, key_id))
            {
                throw Exception(sstr()
                    << "Duplicate key_id '" << key_id << "' found "
                    << "in color scheme file. "
                    << "Key_ids must occur only once.");
            }
            used_keys.insert(key_id);
            key_ids.emplace_back(key_id);
        }
    }

    item->key_ids = key_ids;

    return item;
}

static double hexstring_to_float(const string& hex)
{
    return static_cast<double>(to_int(hex, 16));
}

void parse_state_attibute(const XMLNodePtr& node, const string& name, KeyState& state)
{
    string value;
    if (node->get_attribute(name, value))
    {
        bool b = value == "true";
        state[name] = b;

        if (name == "locked" && b)
        {
            state["active"] = true;  // locked implies active
        }
    }
}

static ColorSchemeItemPtr parse_color(const XMLNodePtr& node)
{
    auto item = std::make_shared<CSKeyColor>();
    parse_xml_node_item(node, item.get());

    node->get_attribute("element", item->element);

    string value;
    if (node->get_attribute("rgb", value))
    {
        item->rgb = {hexstring_to_float(slice(value, 1, 3))/255,
                     hexstring_to_float(slice(value, 3, 5))/255,
                     hexstring_to_float(slice(value, 5, 7))/255,
                     1.0};
    }

    if (node->get_attribute("opacity", value))
    {
        item->opacity = to_double(value);
    }

    parse_state_attibute(node, "prelight", item->state);
    parse_state_attibute(node, "pressed", item->state);
    parse_state_attibute(node, "active", item->state);
    parse_state_attibute(node, "locked", item->state);
    parse_state_attibute(node, "insensitive", item->state);
    parse_state_attibute(node, "scanned", item->state);

    return item;
}

// Recursive function that parses all nodes of the xml tree
static vector<ColorSchemeItemPtr> parse_xml_node(
    const XMLNodePtr& parent_node,
    const ColorSchemeItem::Ptr& parent_item,
    UsedKeysMap& used_keys)
{
    vector<ColorSchemeItemPtr> items;
    for (auto node= parent_node->get_first_child(); node ; node = node->get_next_sibling())
    {
        if (node->get_node_type() == XMLNodeType::ELEMENT)
        {
            ColorSchemeItemPtr item;
            string tag = node->get_tag_name();

            if (tag == "window")
            {
                item = parse_window(node);
            }
            else
            if (tag == "layer")
            {
                item = parse_layer(node);
            }
            else
            if (tag == "icon")
            {
                item = parse_icon(node);
            }
            else
            if (tag == "key_group")
            {
                item = parse_key_group(node, used_keys);
            }
            else
            if (tag == "color")
            {
                item = parse_color(node);
            }

            if (item)
            {
                item->m_parent = parent_item.get();
                auto children = parse_xml_node(node, item, used_keys);
                item->set_children(children);
                items.emplace_back(item);
            }
        }
    }

    return items;
}


// Load a color scheme and return it as a new instance.
ColorSchemePtr ColorSchemeLoader::load(const std::string& filename, bool is_system)
{
    ColorSchemePtr color_scheme;
    string value;

    std::unique_ptr<XMLParser> parser = XMLParser::make_xml2(*this);
    auto xml_node = parser->parse_file_utf8(filename);
    if (!xml_node)
    {
        LOG_WARNING << "failed to read '" << filename << "'";
        return {};
    }

    // check layout format
    auto format = ColorScheme::COLOR_SCHEME_FORMAT_LEGACY;
    if (xml_node->get_attribute("format", value))
        format = Version::from_string(value);

    string name = xml_node->get_attribute("name");

    try
    {
        vector<ColorSchemeItem::Ptr> items;
        if (format >= ColorScheme::COLOR_SCHEME_FORMAT_TREE)  // tree format?
        {
            UsedKeysMap used_keys;
            items = parse_xml_node(xml_node, {}, used_keys);
        }
        else
        {
            LOG_WARNING
                << "Loading legacy color scheme format '" << format << "', "
                << "please consider upgrading to current format "
                << "'" << ColorScheme::COLOR_SCHEME_FORMAT << "': "
                << "'" << filename << "'";

            //items = ColorScheme._parse_legacy_color_scheme(dom);
        }

        if (!items.empty())
        {
            auto root = std::make_shared<CSRoot>();
            root->set_children(items);

            color_scheme = ColorScheme::make(*this);
            color_scheme->m_name = name;
            color_scheme->m_filename = filename;
            color_scheme->m_is_system = is_system;
            color_scheme->m_root = root;
            //print(root.dumps())
        }
    }
    catch (const Exception& ex)
    {
        LOG_ERROR << "loading '" << filename << "' failed: "
                  << ex.what();
    }

    return color_scheme;
}


