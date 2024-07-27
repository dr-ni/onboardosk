#include <memory>
#include <set>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "tools/container_helpers.h"
#include "tools/iostream_helpers.h"
#include "tools/logger.h"
#include "tools/rect.h"
#include "tools/version.h"

#include "colorscheme.h"
#include "configuration.h"
#include "exception.h"
#include "keygeometry.h"
#include "layoutcharacterpalette.h"
#include "layoutkey.h"
#include "layoutloadersvg.h"
#include "layoutroot.h"
#include "layoutwordlist.h"
#include "onboardoskglobals.h"
#include "theme.h"
#include "translation.h"
#include "xmlparser.h"
#include "virtkey.h"

using namespace std;



class LayoutItemFactory
{
    private:
        using MakeFunc = std::unique_ptr<LayoutItem>(*)(const ContextBase& context);

    public:
        static LayoutItemPtr make(const string& class_name, const ContextBase& context)
        {
            static std::map<std::string, MakeFunc> m_map;
            if (m_map.empty())
            {
                m_map[LayoutBox::CLASS_NAME] = reinterpret_cast<MakeFunc>(LayoutBox::make);
                m_map[LayoutPanel::CLASS_NAME] = reinterpret_cast<MakeFunc>(LayoutPanel::make);
                m_map[LayoutKey::CLASS_NAME] = reinterpret_cast<MakeFunc>(LayoutKey::make);
                m_map[LayoutRectangle::CLASS_NAME] = reinterpret_cast<MakeFunc>(LayoutRectangle::make);
                m_map[LayoutPanelWordList::CLASS_NAME] = reinterpret_cast<MakeFunc>(LayoutPanelWordList::make);
                m_map[LayoutWordListKey::CLASS_NAME] = reinterpret_cast<MakeFunc>(LayoutWordListKey::make);
                m_map[LayoutBarKey::CLASS_NAME] = reinterpret_cast<MakeFunc>(LayoutBarKey::make);
                m_map[EmojiPalettePanel::CLASS_NAME] = reinterpret_cast<MakeFunc>(EmojiPalettePanel::make);
                m_map[SymbolPalettePanel::CLASS_NAME] = reinterpret_cast<MakeFunc>(SymbolPalettePanel::make);
            }

            if (contains(m_map, class_name))
                return m_map[class_name](context);
            return {};
        }
};

// Cache of SVG provided key attributes.
class SVGNode : public std::enable_shared_from_this<SVGNode>
{
    public:
        using Ptr = shared_ptr<SVGNode>;
        using Nodes = vector<Ptr>;
        SVGNode() = default;

        Ptr getptr() { return shared_from_this(); }

        void extract_key_params(KeyGeometry::Ptr& geometry,
                                Rect& bounds_)
        {
            Nodes nodes;
            if (!children.empty())
                nodes = slice(children, 0, 2);
            else
                nodes.emplace_back(this->getptr());
            bounds_ = nodes[0]->bounds;

            vector<KeyPath::Ptr> paths;
            for (auto node : nodes)
                if (node->path)
                    paths.emplace_back(node->path);
            if (!paths.empty())
                geometry = KeyGeometry::from_paths(paths);
        }

    public:
        Nodes children;
        string id;                  // svg_id
        Rect bounds;                // logical bounding rect, aka border rect
        KeyPath::Ptr path;          // optional path for arbitrary shapes
};
typedef SVGNode::Ptr SVGNodePtr;
typedef std::map<std::string, SVGNode::Ptr> SVGNodeMap;

class LayoutLoaderSVGImpl : public LayoutLoaderSVG, public ContextBase
{
    private:
        // onboard <= 0.95
        const Version LAYOUT_FORMAT_LEGACY      {1, 0};

        // onboard 0.96, initial layout-tree
        const Version LAYOUT_FORMAT_LAYOUT_TREE {2, 0};

        // onboard 0.97, scanner overhaul, no more scan columns,
        // new attributes scannable, scan_priority
        const Version LAYOUT_FORMAT_SCANNER     {2, 1};

        // onboard 0.99, prerelease on Nexus 7,
        // new attributes key.action, key.sticky_behavior.
        // allow (i.e. have by default) keycodes for modifiers.
        const Version LAYOUT_FORMAT_2_2         {2, 2};

        // onboard 0.99, key_templates in key_def.xml and include tags.
        const Version LAYOUT_FORMAT_3_0         {3, 0};

        // sub-layouts for popups, various new key attributes,
        // label_margin, theme_id, popup_id
        const Version LAYOUT_FORMAT_3_1         {3, 1};

        // new key attributes show_active
        const Version LAYOUT_FORMAT_3_2         {3, 2};

        // current format
        const Version LAYOUT_FORMAT             {LAYOUT_FORMAT_3_2};

    public:
        LayoutLoaderSVGImpl(const ContextBase& context) :
            ContextBase(context)
        {
        }
        virtual ~LayoutLoaderSVGImpl() = default;

        // public interface
        virtual LayoutItemPtr load(std::string layout_filename, ColorSchemePtr color_scheme) override;

    private:
        LayoutItemPtr load_or_include(std::string layout_filename, ColorSchemePtr color_scheme,
                                      std::string root_layout_dir, LayoutItemPtr parent_item=nullptr);
        LayoutItemPtr load_layout(std::string layout_filename, LayoutItemPtr parent_item=nullptr);

        void fill_in_target_layer_id(LayoutItemPtr& layout);
        void parse_xml_node(XMLNodePtr& parent_node, LayoutItemPtr& parent_item);
        void parse_include(XMLNodePtr& node, LayoutItemPtr& parent);
        void parse_keysym_rule(XMLNodePtr& node, LayoutItemPtr& parent);
        void parse_key_template(XMLNodePtr& node, LayoutItemPtr& parent);
        LayoutItemPtr init_item(const Attributes& attributes, const string& default_class_name);
        LayoutItemPtr parse_sublayout(XMLNodePtr& node, LayoutItemPtr& parent);
        LayoutItemPtr parse_box(XMLNodePtr& node);
        LayoutItemPtr parse_panel(XMLNodePtr& node);
        LayoutItemPtr parse_key(XMLNodePtr& node, LayoutItemPtr& parent);
        void init_key(LayoutKey::Ptr& key, Attributes& attributes);
        void parse_key_labels(const Attributes& attributes, LayoutKey::Ptr& key, LabelMap& labels);
        static vector<ModMask> m_label_modifier_masks;
        void parse_layout_labels(const Attributes& attributes, LabelMap& labels);
        const SVGNodeMap& get_svg_keys(const string& filename);
        void load_svg_keys(const string& filename, SVGNodeMap& svg_nodes);
        void parse_svg(XMLNode::Ptr& parent_node, vector<SVGNode::Ptr>& svg_nodes);
        const Attributes* find_template(LayoutItemPtr& scope_item, const string& class_name, const vector<string>& ids);
        void get_keysym_rules(LayoutItemPtr scope_item, KeysymRules& keysym_rules);
        void get_system_keyboard_layout(string& layout, string& variant);
        string get_system_layout_string();
        bool has_matching_layout(const string& layout_str);

    private:
        Version m_format;
        string m_layout_filename;
        string m_root_layout_dir;
        ColorSchemePtr m_color_scheme;
        map<string, SVGNodeMap> m_svg_cache;
        string m_system_layout;
        string m_system_variant;
};

std::unique_ptr<LayoutLoaderSVG> LayoutLoaderSVG::make(const ContextBase& context)
{
    return make_unique<LayoutLoaderSVGImpl>(context);
}

// precalc mask permutations
vector<ModMask> LayoutLoaderSVGImpl::m_label_modifier_masks =
    permute_modmask(Modifier::LABEL_MODIFIERS);

// Load layout root file
LayoutItemPtr LayoutLoaderSVGImpl::load(std::string layout_filename, ColorSchemePtr color_scheme)
{
    get_system_keyboard_layout(m_system_layout, m_system_variant);
    LOG_INFO << "current system keyboard layout(variant):"
             << " '" << get_system_layout_string() << "'";

    LayoutItemPtr layout;
    try
    {
        layout = load_or_include(layout_filename, color_scheme,
                                      fs::path(layout_filename).remove_filename());
    }
    catch (const Exception& ex)
    {
        throw LayoutException(sstr()
            << "loading '" << m_layout_filename << "' failed: "
            << ex.what());
    }

    if (layout)
    {
        // purge attributes only used during loading
        layout->for_each([](const LayoutItemPtr& item) {
            item->templates.clear();
            item->keysym_rules.clear();
        });

        // Fill in missing target_layer_id attributes
        fill_in_target_layer_id(layout);
    }

    return layout;
}

LayoutItemPtr LayoutLoaderSVGImpl::load_or_include(string layout_filename,
                                                   ColorSchemePtr color_scheme,
                                                   string root_layout_dir,
                                                   LayoutItemPtr parent_item)
{
    m_layout_filename = layout_filename;
    m_color_scheme = color_scheme;
    m_root_layout_dir = root_layout_dir;
    return load_layout(layout_filename, parent_item);
}

LayoutItemPtr LayoutLoaderSVGImpl::load_layout(std::string layout_filename, LayoutItemPtr parent_item)
{
    LayoutItemPtr layout;

    unique_ptr<XMLParser> parser = XMLParser::make_xml2(*this);
    auto xml_node = parser->parse_file_utf8(layout_filename);
    if (!xml_node)
    {
        LOG_WARNING << "failed to read '" << layout_filename << "'";
        return {};
    }

    // check layout format, no format version means legacy layout
    auto format = LAYOUT_FORMAT_LEGACY;
    if (xml_node->has_attribute("format"))
        format = Version::from_string(xml_node->get_attribute("format"));
    m_format = format;

    LayoutItemPtr root = LayoutRoot::make(*this);  // root, representing the 'keyboard' tag
    root->set_id("__root__");  // id for debug prints

    // Init included root with the parent item's svg filename.
    // -> Allows to skip specifying svg filenames in includes.
    if (parent_item)
        root->filename = parent_item->filename;

    if (format >= LAYOUT_FORMAT_LAYOUT_TREE)
    {
        parse_xml_node(xml_node, root);
        layout = root;
    }
    else
    {
        LOG_WARNING << "Loading legacy layout, format"
                    << " '" << format << "'. "
                    << "Please consider upgrading to current format"
                    << " '" << LAYOUT_FORMAT<< "'";
        /*
        auto items = parse_legacy_layout(xml_node)
        if (items)
        {
            root.set_children(items);
            layout = root;
        }
        */
    }

    m_svg_cache.clear();  // free the memory
    return layout;
}

// Make sure the target_layer_id attribute is set for all layer buttons.
void LayoutLoaderSVGImpl::fill_in_target_layer_id(LayoutItemPtr& layout)
{
    auto layer_ids = layout->get_layer_ids();
    if (!layer_ids.empty())
    {
        layout->for_each([&](const LayoutItemPtr& item)
        {
            if (item->is_key())
            {
                auto key = dynamic_pointer_cast<LayoutKey>(item);
                if (key->is_layer_button() &&
                    !key->is_sublayer_button())
                {
                    size_t index = key->get_layer_index();
                    if (index < layer_ids.size())
                    {
                        key->target_layer_id = layer_ids[index];
                    }
                }
            }
        });
    }
}

// Recursively traverse the XML nodes of the layout tree.
void LayoutLoaderSVGImpl::parse_xml_node(XMLNodePtr& parent_node, LayoutItemPtr& parent_item)
{
    std::set<std::string> loaded_ids;

    for (auto node= parent_node->get_first_child(); node ; node = node->get_next_sibling())
    {
        if (node->get_node_type() == XMLNodeType::ELEMENT)
        {
            // Skip over items with non-matching "layout" (keymap) attribute.
            // Items with the same id are processed from top to bottom,
            // the first match wins. If no item matches we fall back to
            // the item without "layout" attribute.
            // This allows to switch between alternative key definitions
            // depending on the current system layout.
            bool can_load = false;
            if (!node->has_attribute("id"))
            {
                can_load = true;
            }
            else
            {
                string id = node->get_attribute("id");
                if (!contains(loaded_ids, id))
                {
                    if (node->has_attribute("layout"))
                    {
                        string layouts_str = node->get_attribute("layout");
                        can_load = has_matching_layout(layouts_str);

                        // don't look at items with this id again
                        if (can_load)
                            loaded_ids.emplace(id);
                    }
                    else
                    {
                        can_load = true;
                    }
                }
            }

            if (can_load)
            {
                string tag = node->get_tag_name();

                // rule && control tags
                if (tag == "include")
                {
                    parse_include(node, parent_item);
                }
                else
                if (tag == "key_template")
                {
                    parse_key_template(node, parent_item);
                }
                else
                if (tag == "keysym_rule")
                {
                    parse_keysym_rule(node, parent_item);
                }
                else
                if (tag == "layout")
                {
                    LayoutItemPtr item = parse_sublayout(node, parent_item);
                    if (item)
                    {
                        parent_item->append_sublayout(item);
                        parse_xml_node(node, item);
                    }
                }
                else
                {
                    LayoutItemPtr item;

                    // actual items that make up the layout tree
                    if (tag == "box")
                    {
                        item = parse_box(node);
                    }
                    else
                    if (tag == "panel")
                    {
                        item = parse_panel(node);
                    }
                    else
                    if (tag == "key")
                    {
                        item = parse_key(node, parent_item);
                    }

                    if (item)
                    {
                        parent_item->append_child(item);
                        parse_xml_node(node, item);
                    }
                }
            }
        }
    }
}

void LayoutLoaderSVGImpl::parse_include(XMLNodePtr& node, LayoutItemPtr& parent)
{
    if (node->has_attribute("file"))
    {
        string filename = node->get_attribute("file");
        string filepath = config()->find_layout_filename("layout include", filename);
        if (filepath.empty())
            throw LayoutException(sstr()
                << "layout include '" << filename << "' not found");

        LOG_INFO << "Including layout '" << filepath << "'";

        auto loader = LayoutLoaderSVG::make(*this);
        auto loader_impl = dynamic_cast<LayoutLoaderSVGImpl*>(loader.get());
        auto incl_root = loader_impl->load_or_include(filepath, m_color_scheme,
                                                      m_root_layout_dir, parent);
        if (incl_root)
        {
            parent->append_children(incl_root->get_children());
            parent->update_keysym_rules(incl_root->keysym_rules);
            parent->update_templates(incl_root->templates);
        }
    }
}

// Templates are partially define layout items. Later non-template
// items inherit attributes of templates with matching id.
void LayoutLoaderSVGImpl::parse_key_template(XMLNodePtr& node, LayoutItemPtr& parent)
{
    Attributes attributes;
    node->get_attributes(attributes);
    string id = get_value(attributes, "id");
    if (id.empty())
    {
        throw LayoutException(sstr()
            << "'id' attribute required for template"
            << " " << get_values(attributes)
            << " in layout '" << m_layout_filename << "'");
    }

    KeyTemplates templates;
    templates[{id, "LayoutKey"}] = attributes;
    parent->update_templates(templates);
}

// Keysym rules link attributes like "label", "image"
// to certain keysyms.
void LayoutLoaderSVGImpl::parse_keysym_rule(XMLNodePtr& node, LayoutItemPtr& parent)
{
    map<string, string> attributes;
    node->get_attributes(attributes);

    string keysym_str = get_value(attributes, "keysym");
    if (!keysym_str.empty())
    {
        int keysym;
        remove(attributes, "keysym");
        if (startswith(keysym_str, "0x"))
        {
            keysym = to_int(keysym_str, 16);
        }
        else
        {
            // translate symbolic keysym name
            keysym = 0;
        }

        if (keysym)
        {
            KeysymRules rules;
            rules[keysym] = attributes;
            parent->update_keysym_rules(rules);
        }
    }
}

// Parses attributes common to all LayoutItems
LayoutItemPtr LayoutLoaderSVGImpl::init_item(const Attributes& attributes, const string& default_class_name)
{
    string id = get_value(attributes, "id");

    // allow to override the item's default class
    string class_name = default_class_name;
    if (contains(attributes, "class"))
    {
        class_name = attributes.at("class");

        // Typo up to Onboard 1.4
        if (class_name == "WordlistKey")
            class_name = "WordListKey";
    }

    // create the item
    LayoutItemPtr item = LayoutItemFactory::make(class_name, *this);
    if (!item)
        throw LayoutException(sstr()
            << "key '" << id << "':"
            << " unknowm class '" << class_name << "'");

    item->id = id;

    string value;

    if (get_value(attributes, "group", value))
        item->group = value;

    if (get_value(attributes, "layer", value))
        item->layer_id = value;

    if (get_value(attributes, "filename", value))
        item->filename = value;

    if (get_value(attributes, "visible", value))
        item->visible = value == "true";

    if (get_value(attributes, "sensitive", value))
        item->sensitive = value == "true";

    if (get_value(attributes, "border", value))
        item->border = to_double(value);

    if (get_value(attributes, "expand", value))
        item->m_expand = (value == "true");

    if (get_value(attributes, "unlatch_layer", value))
        item->unlatch_layer = (value == "true");

    if (get_value(attributes, "scannable", value) &&
        lower(value) == "false")
        item->scannable = false;

    if (get_value(attributes, "scan_priority", value))
        item->scan_priority = to_int(value);

    return item;
}

LayoutItemPtr LayoutLoaderSVGImpl::parse_sublayout(XMLNodePtr& node, LayoutItemPtr& parent)
{
    map<string, string> attributes;
    node->get_attributes(attributes);

    LayoutItemPtr item = init_item(attributes, "LayoutPanel");

    // make templates accessible in the sublayout
    item->sublayout_parent = parent.get();
    return item;
}

LayoutItemPtr LayoutLoaderSVGImpl::parse_box(XMLNodePtr& node)
{
    map<string, string> attributes;
    node->get_attributes(attributes);

    LayoutItemPtr item_ptr = init_item(attributes, "LayoutBox");
    LayoutBox* item = dynamic_cast<LayoutBox*>(item_ptr.get());

    string value;

    if (get_value(attributes, "orientation", value))
        item->horizontal = lower(value) == "horizontal";

    if (get_value(attributes, "spacing", value))
        item->spacing = to_double(value);

    if (get_value(attributes, "compact", value))
        item->compact = value == "true";

    return item_ptr;
}

LayoutItemPtr LayoutLoaderSVGImpl::parse_panel(XMLNodePtr& node)
{
    map<string, string> attributes;
    node->get_attributes(attributes);

    LayoutItemPtr item_ptr = init_item(attributes, "LayoutPanel");
    LayoutPanel* item = dynamic_cast<LayoutPanel*>(item_ptr.get());

    string value;

    if (get_value(attributes, "compact", value))
        item->compact = lower(value) == "true";

    return item_ptr;
}

LayoutItemPtr LayoutLoaderSVGImpl::parse_key(XMLNodePtr& node, LayoutItemPtr& parent)
{
    LayoutItemPtr result;

    string id = node->get_attribute("id");
    string class_name;
    if (id == "inputline")
        class_name = "InputlineKey";
    else
        class_name = "LayoutKey";

    if (id == "BD01")
        printf("%s", id.c_str());

    // find template attributes
    Attributes attributes;
    if (node->has_attribute("id"))
    {
        string theme_id;
        LayoutKey::parse_id(id, theme_id, id);
        auto template_attribs = find_template(parent, "LayoutKey",
                                           vector<string>{id});
        if (template_attribs)
            update_map(attributes, *template_attribs);
    }

    // let current node override any preceding templates
    map<string, string> node_attributes;
    node->get_attributes(node_attributes);
    update_map(attributes, node_attributes);

    // handle common layout-item attributes
    LayoutItemPtr item_ptr = init_item(attributes, class_name);
    LayoutKey::Ptr key = dynamic_pointer_cast<LayoutKey>(item_ptr);
    key->m_parent = parent.get();  // assign early to have get_filename() work

    // handle key-specific attributes
    init_key(key, attributes);

    // get key geometry from the closest svg file
    string filename = key->get_filename();
    if (filename.empty() &&
        get_value(attributes, "group") != "wsbutton")
    {
        LOG_WARNING << "Ignoring key '" << key->id << "'. No svg filename defined.";
    }
    else
    {
        bool geometry_valid = false;
        auto svg_nodes = get_svg_keys(filename);
        if (!svg_nodes.empty())
        {
            SVGNode::Ptr svg_node;
            bool svg_node_valid;

            // try svg_id first, if there is one
            if (key->svg_id != key->id)
                svg_node_valid = get_value(svg_nodes, key->svg_id, svg_node);
            // then the regular id
            else
                svg_node_valid = get_value(svg_nodes, key->id, svg_node);

            if (svg_node_valid)
            {
                KeyGeometry::Ptr geometry;
                Rect r;
                svg_node->extract_key_params(geometry, r);
                key->set_initial_border_rect(r);
                key->set_border_rect(r);
                key->geometry = geometry;
                result = item_ptr;
                geometry_valid = true;
            }
        }

        if (!geometry_valid)
        {
            LOG_WARNING << "Ignoring key '" << key->id << "'."
                        << " No svg object found for '" << key->svg_id << "'"
                        << " in '" << filename << "'.";
        }
    }

    return result;  // ignore keys not found in an svg file
}

void LayoutLoaderSVGImpl::init_key(LayoutKey::Ptr& key, Attributes& attributes)
{
    // Re-parse the id to distinguish between the short key_id
    // && the optional longer theme_id.
    string full_id = attributes["id"];
    string theme_id = get_value(attributes, "theme_id");
    string svg_id = get_value(attributes, "svg_id");
    key->set_ids(full_id, theme_id, svg_id);

    if (contains(key->get_id(), "_"))
    {
        LOG_WARNING << "underscore in key id '" << key->get_id() << "', please use dashes";
    }

    string value;

    if (get_value(attributes, "modifier", value))
    {
        key->modifier = Modifier::mod_name_to_modifier(value);
        if (key->modifier == Modifier::NONE)
            throw LayoutException(sstr()
                << "Unrecognized modifier '" << value << "' in"
                << " definition of '" << full_id << "'");
    }

    if (get_value(attributes, "action", value))
    {
        key->action = KeyAction::layout_string_to_key_action(value);
        if (key->action == KeyAction::NONE)
            throw LayoutException(sstr()
                << "Unrecognized key action '" << value << "'"
                << " in definition of '" << full_id << "'");
    }

    if (contains(attributes, "char"))
    {
        key->keystr = attributes["char"];
        key->type = KeyType::CHAR;
    }
    else
    if (contains(attributes, "keysym"))
    {
        value = attributes["keysym"];
        key->type = KeyType::KEYSYM;
        if (startswith(value, "0x")) // for hex keysym?
            key->keysym = static_cast<KeySym>(to_int(value, 16));
        else
            key->keysym = static_cast<KeySym>(to_int(value, 10));
    }
    else
    if (contains(attributes, "macro"))
    {
        key->macro_id = attributes["macro"];
        key->type = KeyType::MACRO;
    }
    else
    if (contains(attributes, "script"))
    {
        key->script_name = attributes["script"];
        key->type = KeyType::SCRIPT;
    }
    else
    if (contains(attributes, "keycode"))
    {
        key->keycode = static_cast<KeyCode>(to_int(attributes["keycode"]));
        key->type = KeyType::KEYCODE;
    }
    else
    if (contains(attributes, "target_layer"))
    {
        //key.code = key.id[:];
        key->target_layer_id = attributes["target_layer"];
        key->type = KeyType::BUTTON;
    }
    else
    if (contains(attributes, "button"))
    {
        //key.code = key.id[:];
        key->type = KeyType::BUTTON;
    }
    else
    if (key->modifier)
    {
        key->type = KeyType::LEGACY_MODIFIER;
    }
    else
    {
        // key without action: just draw it, do nothing on click
        key->action = KeyAction::NONE;
    }

    // get the size group of the key
    string group_name;
    if (contains(attributes, "group"))
    {
        group_name = attributes["group"];
    }
    else
    {
        group_name = "_default";
    }

    // get the optional image filename
    if (get_value(attributes, "image", value))
    {
        auto fields = split(value, ';');
        if (!fields.empty())
            key->image_filenames[ImageSlot::NORMAL] = fields[0];
    }
    if (get_value(attributes, "image_active", value))
    {
        key->image_filenames[ImageSlot::ACTIVE] = value;
    }

    // get labels
    LabelMap labels;
    this->parse_key_labels(attributes, key, labels);

    // Replace label && size group with overrides from
    // theme and/or system defaults.
    const auto& overrides = config()->get_key_label_overrides();
    LabelOverrideValue override;
    if (get_value(overrides, key->id, override))
    {
        if (!override.label.empty())
        {
            labels = {{0, override.label}};
            if (!override.group.empty())
                group_name = override.group;
        }
    }

    key->labels = labels;
    key->group = group_name;

    // optionally  override the theme's default key_style
    if (get_value(attributes, "key_style", value))
    {
        key->style = KeyStyle::to_key_style(value);
        if (key->style == KeyStyle::NONE)
            throw LayoutException(sstr()
                << "Invalid value '" << value << "'"
                << "for key_style attribute of key '" << key->id << "'");
    }

    // select what gets drawn, different from "visible" flag as this
    // doesn't affect the layout.
    if (get_value(attributes, "show", value))
    {
        if (lower(value) == "false")
        {
            key->show_face = false;
            key->show_border = false;
        }
    }
    if (get_value(attributes, "show_face", value))
    {
        if (lower(value) == "false")
            key->show_face = false;
    }
    if (get_value(attributes, "show_border", value))
    {
        if (lower(value) == "false")
            key->show_border = false;
    }

    // show_active: allow to display key in latched or locked state
    // legacy show_active behavior was hard-coded for layer0
    if (m_format < LayoutLoaderSVGImpl::LAYOUT_FORMAT_3_2)
    {
        if (key->id == "layer0")
        {
            key->show_active = false;
        }
    }
    if (get_value(attributes, "show_active", value))
    {
        if (lower(value) == "false")
            key->show_active = false;
    }

    if (get_value(attributes, "label_x_align", value))
    {
        key->label_x_align = to_double(value);
    }
    if (get_value(attributes, "label_y_align", value))
    {
        key->label_y_align = to_double(value);
    }

    if (get_value(attributes, "label_margin", value))
    {
        auto fields = split(replace_all(value, " ", ""), ',');
        if (fields.size() >= 1)
        {
            Size margin;
            margin.w = to_double(fields[0]);
            if (fields.size() >= 2)
                margin.h = to_double(fields[1]);
            else
                margin.h = margin.w;
            key->label_margin = margin;
        }
    }

    if (get_value(attributes, "sticky", value))
    {
        string sticky = lower(value);
        if (sticky == "true")
        {
            key->sticky = true;
        }
        else
        if (sticky == "false")
        {
            key->sticky = false;
        }
        else
        {
            throw LayoutException(sstr()
                << "Invalid value '" << sticky << "'"
                << "for 'sticky' attribute of key '" << key->id << "'");
        }
    }
    else
    {
        key->sticky = false;
    }

    // legacy sticky key behavior was hard-coded for CAPS
    if (m_format < LayoutLoaderSVGImpl::LAYOUT_FORMAT_2_2)
    {
        if (key->id == "CAPS")
            key->sticky_behavior = StickyBehavior::LOCK_ONLY;
    }

    if (get_value(attributes, "sticky_behavior", value))
    {
        key->sticky_behavior = StickyBehavior::to_behavior(value);
        if (key->sticky_behavior == StickyBehavior::NONE)
            throw LayoutException(sstr()
                << "Unrecognized sticky behavior '" << value << "' in"
                << " definition of '" << full_id << "'");
    }

    if (get_value(attributes, "tooltip", value))
    {
        key->tooltip = value;
    }

    if (get_value(attributes, "popup_id", value))
    {
        key->popup_id = value;
    }

    if (get_value(attributes, "chamfer_size", value))
    {
        key->chamfer_size = to_double(value);
    }

    key->m_color_scheme = m_color_scheme;
}

void LayoutLoaderSVGImpl::parse_key_labels(const Attributes& attributes, LayoutKey::Ptr& key, LabelMap& labels)
{
    // Get labels from keyboard mapping first.
    if (key->type == KeyType::KEYCODE &&
        key->id != "BKSP")
    {
        auto& vkmodmasks = m_label_modifier_masks;
        vector<string> vklabels;

        VirtKey* vk = get_virtkey();
        if (vk)
            vklabels = vk->get_labels_from_keycode(key->keycode, vkmodmasks);

        if (!vklabels.empty())  // can also happen if group is not yet valid
        {
            for (size_t i=0; i<vkmodmasks.size(); i++)
                labels[vkmodmasks[i]] = vklabels[i];
        }
        else
        {
            if (upper(key->id) == "SPCE")
                labels[0] = "No keyboard labels available";
            else
                labels[0] = "?";
        }
    }

    // If key is a macro (snippet) generate label from its number.
    else
    if (key->type == KeyType::MACRO)
    {
        int index = to_int(key->macro_id);
        const SnippetsMap& snippets = config()->snippets;
        SnippetsValue snippet = get_value(snippets, index);
        string tooltip = sstr() << "Snippet " << index;
        if (snippet.label.empty())
        {
            labels[0] = "     --     ";
            // i18n: full string is "Snippet n, unassigned"
            tooltip += _(", unassigned");
        }
        else
        {
            labels[0] = replace_all(snippet.label, "\\n", "\n");
        }
        key->tooltip = tooltip;
    }

    // get labels from the key/template definition in the layout
    LabelMap layout_labels;
    parse_layout_labels(attributes, layout_labels);
    if (!layout_labels.empty())
        labels = layout_labels;

    // override with per-keysym labels
    KeysymRules keysym_rules;
    get_keysym_rules(key, keysym_rules);
    if (key->type == KeyType::KEYCODE)
    {
        VirtKey* vk = get_virtkey();
        if (vk)
        {  // no keyboard found?
            auto& vkmodmasks = m_label_modifier_masks;
            vector<KeySym> vkkeysyms =
                vk->get_keysyms_from_keycode(key->keycode, vkmodmasks);

            // replace all labels with keysyms matching a keysym rule
            for (size_t i=0; i<vkkeysyms.size(); i++)
            {
                KeySym keysym = vkkeysyms[i];
                const Attributes& attribs = get_value(keysym_rules, keysym);
                string label;
                if (get_value(attribs, "label", label))
                {
                    ModMask mask = vkmodmasks[i];
                    labels[mask] = label;
                }
            }
        }
    }

    // Translate labels - Gettext behaves oddly when translating
    // empty strings
    for (auto it : labels)
        if (!it.second.empty())
            it.second = _(it.second);
}

// Deprecated label definitions up to v0.98.x
void LayoutLoaderSVGImpl::parse_layout_labels(const Attributes& attributes, LabelMap& labels)
{
    // modifier masks were hard-coded in python-virtkey
    if (contains(attributes, "label"))
    {
        string value;

        labels[0] = get_value(attributes, "label");

        if (get_value(attributes, "cap_label", value))
        {
            labels[1] = value;
        }
        if (get_value(attributes, "shift_label", value))
        {
            labels[2] = value;
        }
        if (get_value(attributes, "altgr_label", value))
        {
            labels[128] = value;
        }
        if (get_value(attributes, "altgrNshift_label", value))
        {
            labels[129] = value;
        }
    }
}

const SVGNodeMap& LayoutLoaderSVGImpl::get_svg_keys(const string& filename)
{
    if (!contains(m_svg_cache, filename))
        load_svg_keys(filename, m_svg_cache[filename]);

    return m_svg_cache[filename];
}

void LayoutLoaderSVGImpl::load_svg_keys(const string& filename, SVGNodeMap& svg_nodes)
{
    string fn = fs::path(m_root_layout_dir) / filename;
    unique_ptr<XMLParser> parser = XMLParser::make_xml2(*this);
    auto xml_node = parser->parse_file_utf8(fn);
    if (!xml_node)
    {
        throw LayoutException(sstr()
            << "load_svg_keys: failed to read '" << fn << "'");
    }
    vector<SVGNode::Ptr> v_svg_nodes;
    parse_svg(xml_node, v_svg_nodes);

    for (auto& node : v_svg_nodes)
        svg_nodes[node->id] = node;
}

void LayoutLoaderSVGImpl::parse_svg(XMLNode::Ptr& parent_node, vector<SVGNode::Ptr>& svg_nodes)
{
    for (auto xml_node= parent_node->get_first_child(); xml_node ; xml_node = xml_node->get_next_sibling())
    {
        if (xml_node->get_node_type() == XMLNodeType::ELEMENT)
        {
            string tag = xml_node->get_tag_name();
            if (contains(vector<string>{"rect", "path", "g"}, tag))
            {
                SVGNodePtr svg_node = make_shared<SVGNode>();
                string id = xml_node->get_attribute("id");
                svg_node->id = id;

                if (tag == "rect")
                {
                    svg_node->bounds =
                        Rect(to_double(xml_node->get_attribute("x")),
                             to_double(xml_node->get_attribute("y")),
                             to_double(xml_node->get_attribute("width")),
                             to_double(xml_node->get_attribute("height")));
                }

                else
                if (tag == "path")
                {
                    string data = xml_node->get_attribute("d");

                    try
                    {
                        svg_node->path = KeyPath::from_svg_path(data);
                    }
                    catch(const Exception& ex)
                    {
                        throw LayoutException(sstr()
                            << ex.what()
                            << " while reading geometry of '" << id << "'");
                    }

                    svg_node->bounds = svg_node->path->get_bounds();
                    if (svg_node->bounds.empty())
                    {
                        throw LayoutException(sstr()
                            << "empty bounding box of svg path"
                            << " while reading geometry of '" << id << "':"
                            << " '" << data << "'");
                    }
                }

                else
                if (tag == "g") // group
                    parse_svg(xml_node, svg_node->children);

                svg_nodes.emplace_back(svg_node);
            }

            parse_svg(xml_node, svg_nodes);  // append child nodes
        }
    }
}

// Look for a template definition upwards from item until the root.
const Attributes* LayoutLoaderSVGImpl::find_template(LayoutItemPtr& scope_item, const string& class_name, const vector<string>& ids)
{
    Attributes* result = nullptr;
    scope_item->find_to_global_root_if([&](const LayoutItemPtr& item)
    {
        auto& templates = item->templates;
        for (auto id  : ids)
        {
            TemplateKey key = {id, class_name};
            if (contains(templates, key))
            {
                result = &templates[key];
                return true;
            }
        }
        return false;
    });

    return result;
}

// Collect and merge keysym_rule from the root to item.
// Rules in nested items overwrite their parents'.
void LayoutLoaderSVGImpl::get_keysym_rules(LayoutItemPtr scope_item,
                                           KeysymRules& keysym_rules)
{
    vector<LayoutItemPtr> items;
    scope_item->for_each_to_root([&](const LayoutItemPtr& item)
    { items.emplace_back(item); });

    for (auto it = items.rbegin(); it != items.rend(); ++it)   // reverse iterator
    {
        auto item = *it;
        if (!item->keysym_rules.empty())
        {
            update_map(keysym_rules, item->keysym_rules);
        }
    }
}

// get names of the currently active layout group and variant
void LayoutLoaderSVGImpl::get_system_keyboard_layout(string& layout, string& variant)
{
    vector<string> names;

    auto vk = get_virtkey();
    if (vk)
        names = vk->get_current_rules_names();

    if (names.empty())
        names = {"base", "pc105", "us", "", ""};

    layout = names[2];
    variant = names[3];
}

string LayoutLoaderSVGImpl::get_system_layout_string()
{
    string s = m_system_layout;
    if (!m_system_variant.empty())
    {
        s += "(" + m_system_variant + ")";
    }
    return s;
}

// Check if one ot the given layout strings matches;
// system keyboard layout and variant.
//
// Doctests:;
// >>> l = LayoutLoaderSVG();
// >>> l._system_layout = "ch";
// >>> l._system_variant = "fr";
// >>> l._has_matching_layout("ch(x), us, de");
// false;
// >>> l._has_matching_layout("abc, ch(fr)");
// true;
// >>> l._system_variant = "";
// >>> l._has_matching_layout("ch(x), us, de");
// false;
// >>> l._has_matching_layout("ch, us, de");
// true;
bool LayoutLoaderSVGImpl::has_matching_layout(const string& layout_str)
{
    auto layouts = split(layout_str, ',');  // comma separated layout specifiers
    for (auto value : layouts)
    {
        string layout;
        string variant;
        auto fields = re_search(strip(value),
            R"(([^\(]+) (?: \( ([^\)]*) \) )?)");  // C++11 raw string R"(...)"
        if (fields.size() >= 1)
            layout = fields[0];
        if (fields.size() >= 2)
            variant = fields[1];

        if (layout == m_system_layout &&
           (variant.empty() || startswith(m_system_variant, variant)))
        {
            return true;
        }
    }
    return false;
}

