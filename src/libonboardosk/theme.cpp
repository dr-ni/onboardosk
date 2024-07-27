
#include "tools/container_helpers.h"
#include "tools/path_helpers.h"
#include "tools/string_helpers.h"
#include "theme.h"

// ThemeLoader
#include <map>
#include <set>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "tools/logger.h"
#include "configuration.h"
#include "exception.h"
#include "xmlparser.h"

using std::string;
using std::vector;


std::unique_ptr<Theme> Theme::make(const ContextBase& context)
{
    return std::make_unique<Theme>(context);
}

Theme::Theme(const ContextBase& context) :
    ContextBase(context)
{

}
bool Theme::operator==(const Theme& other) const
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wfloat-equal"    // disable warning of clang static analyzer
    return color_scheme_basename == other.color_scheme_basename &&
            background_gradient == other.background_gradient &&
            key_style == other.key_style &&
            roundrect_radius == other.roundrect_radius &&
            key_size == other.key_size &&
            key_stroke_width == other.key_stroke_width &&
            key_fill_gradient == other.key_fill_gradient &&
            key_stroke_gradient == other.key_stroke_gradient &&
            key_gradient_direction == other.key_gradient_direction &&
            key_label_font == other.key_label_font &&
            key_label_overrides == other.key_label_overrides &&
            key_shadow_strength == other.key_shadow_strength &&
            key_shadow_size == other.key_shadow_size;

    #pragma GCC diagnostic pop
}

void Theme::set_color_scheme_filename(const std::string& filename)
{
    this->color_scheme_basename = get_basename(filename);
}

std::string Theme::get_superkey_label()
{
    LabelOverrideValue value;
    get_value(this->key_label_overrides, "LWIN", value);
    return value.label; // assumes RWIN=LWIN
}

std::string Theme::get_superkey_size_group()
{
    LabelOverrideValue value;
    get_value(this->key_label_overrides, "LWIN", value);
    return value.group; // assumes RWIN=LWIN
}

void Theme::set_superkey_label(const Noneable<std::string>& label,
                               std::string& size_group)
{
    if (label.is_none())
    {
        remove(this->key_label_overrides, "LWIN");
        remove(this->key_label_overrides, "RWIN");
    }
    else
    {
        this->key_label_overrides["LWIN"] = {label.value, size_group};
        this->key_label_overrides["RWIN"] = {label.value, size_group};
    }
}




// Load all color schemes from either the user or the system directory.
ThemeLoader::ThemeLoader(const ContextBase& context) :
    ContextBase(context)
{}

std::vector<ThemePtr> ThemeLoader::load_themes(bool is_system)
{
    vector<ThemePtr> themes;
    string path;
    if (is_system)
        path = config()->get_system_theme_dir();
    else
        path = config()->get_user_theme_dir();

    vector<string> filenames = find_themes(path);
    for (auto filename : filenames)
    {
        auto theme = load(filename, is_system);
        if (theme)
            themes.emplace_back(theme);
    }

    return themes;
}

// Returns the full path names of all color schemes found in the given path.
std::vector<std::string> ThemeLoader::find_themes(const std::string& path)
{
    vector<std::string> filenames;

    try
    {
        for (fs::directory_entry e : fs::directory_iterator(path))
            if (fs::is_regular_file(e) &&
                e.path().extension() ==
                    "." + config()->theme_file_extension)
                filenames.emplace_back(e.path());
    }
    catch (const fs::filesystem_error& ex)
    {
        LOG_ERROR << "directory_iterator failed: " << ex.what();
    }

    return filenames;
}

static void parse_key_label_overrides(
    const XMLNodePtr& parent_node,
    const Theme::Ptr& theme)
{
    for (auto node= parent_node->get_first_child(); node ; node = node->get_next_sibling())
    {
        if (node->get_node_type() == XMLNodeType::ELEMENT)
        {
            string tag = node->get_tag_name();

            if (tag == "key")
            {
                string id = node->get_attribute("id");
                string label = node->get_attribute("label");
                string group = node->get_attribute("group");
                if (!id.empty())
                    theme->key_label_overrides[id] = {label, group};
            }
        }
    }
}

static void parse_xml_node(
    const XMLNodePtr& parent_node,
    const Theme::Ptr& theme)
{
    for (auto node= parent_node->get_first_child(); node ; node = node->get_next_sibling())
    {
        if (node->get_node_type() == XMLNodeType::ELEMENT)
        {
            string tag = node->get_tag_name();
            string text = node->get_text();

            if (tag == "color_scheme")
            {
                theme->color_scheme_basename = text;
            }
            else
            if (tag == "background_gradient")
            {
                theme->background_gradient = to_double(text);
            }
            else
            if (tag == "key_style")
            {
                theme->key_style = KeyStyle::to_key_style(text);
            }
            else
            if (tag == "roundrect_radius")
            {
                theme->roundrect_radius = to_double(text);
            }
            else
            if (tag == "key_size")
            {
                theme->key_size = to_double(text);
            }
            else
            if (tag == "key_stroke_width")
            {
                theme->key_stroke_width = to_double(text);
            }
            else
            if (tag == "key_fill_gradient")
            {
                theme->key_fill_gradient = to_double(text);
            }
            else
            if (tag == "key_stroke_gradient")
            {
                theme->key_stroke_gradient = to_double(text);
            }
            else
            if (tag == "key_gradient_direction")
            {
                theme->key_gradient_direction = to_double(text);
            }
            else
            if (tag == "key_label_font")
            {
                theme->key_label_font = text;
            }
            else
            if (tag == "key_label_overrides")
            {
                parse_key_label_overrides(node, theme);
            }
            else
            if (tag == "key_shadow_strength")
            {
                theme->key_shadow_strength = to_double(text);
            }
            else
            if (tag == "key_shadow_size")
            {
                theme->key_shadow_size = to_double(text);
            }
        }
    }
}

// Load a color scheme and return it as a new instance.
ThemePtr ThemeLoader::load(const std::string& filename, bool is_system)
{
    ThemePtr theme;
    string value;

    std::unique_ptr<XMLParser> parser = XMLParser::make_xml2(*this);
    auto xml_node = parser->parse_file_utf8(filename);
    if (!xml_node)
    {
        LOG_WARNING << "failed to read '" << filename << "'";
        return {};
    }

    // check layout format
    auto format = Theme::THEME_FORMAT_INITIAL;
    if (xml_node->get_attribute("format", value))
        format = Version::from_string(value);

    string name = xml_node->get_attribute("name");

    theme = Theme::make(*this);
    theme->m_name = name;
    theme->m_filename = filename;
    theme->m_is_system = is_system;

    try
    {
        parse_xml_node(xml_node, theme);
    }
    catch (const Exception& ex)
    {
        LOG_ERROR << "loading '" << filename << "' failed: "
                  << ex.what();
        theme.reset();
    }

    return theme;
}

