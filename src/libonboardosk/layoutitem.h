#ifndef LAYOUTITEM_H
#define LAYOUTITEM_H

#include <functional>
#include <memory>
#include <vector>
#include <map>

#include "tools/noneable.h"
#include "tools/rect_fwd.h"
#include "tools/string_helpers.h"

#include "layoutcontext.h"
#include "layoutdecls.h"
#include "onboardoskglobals.h"
#include "signalling.h"
#include "treeitem.h"

typedef std::pair<std::string, std::string> TemplateKey;  // item_id, class_id
typedef std::map<std::string, std::string> Attributes;  // {attribute : value}
typedef std::map<TemplateKey, Attributes > KeyTemplates;
typedef std::map<KeySym, Attributes > KeysymRules;  // {keysym : attributes}
typedef std::map<std::string, std::vector<LayoutItemPtr>> KeyGroups;  // {group, [items]}

class InputSequence;
typedef std::shared_ptr<InputSequence> InputSequencePtr;

class LayoutContext;
class DrawingContext;

// Base class of layoutable items """
class LayoutItem : public TreeItem<LayoutItem>, public ContextBase
{
    public:
        using Super = TreeItem<LayoutItem>;
        using Ptr = std::shared_ptr<LayoutItem>;

        using HitRect = struct {Rect r; LayoutItemPtr item;};
        using HitRects = std::vector<HitRect>;
        using HitRectMap = std::map<std::string, HitRects>;

    public:
        LayoutItem(const ContextBase& get_context);
        virtual ~LayoutItem();

        virtual std::string get_class_name() const = 0;

        Ptr getptr() {
            return std::dynamic_pointer_cast<LayoutItem>(shared_from_this());
        }

        Ptr get_parent()
        { return std::dynamic_pointer_cast<LayoutItem>(TreeItem::get_parent()); }

        Ptr get_sublayout_parent()
        {
            if (sublayout_parent)
                return sublayout_parent->getptr();
            return {};
        }

        virtual void dump(std::ostream& out) const override;

        LayoutContext* get_context()
        {
            return m_context.get();
        }

        const LayoutContext* get_context() const
        {
            return m_context.get();
        }

        // Get bounding box in logical coordinates
        virtual Rect get_rect() const;

        // Get bounding rect including border in logical coordinates
        Rect get_border_rect() const;

        // Set bounding rect including border in logical coordinates
        void set_border_rect(const Rect& border_rect);

        // Get initial bounding rect including border in logical coordinates
        Rect get_initial_border_rect() const;

        // Set initial bounding rect including border in logical coordinates.
        void set_initial_border_rect(const Rect& border_rect);

        // Get bounding box in canvas coordinates
        Rect get_canvas_rect() const;

        // Get bounding rect including border in canvas coordinates
        Rect get_canvas_border_rect() const;

        // Return the aspect ratio of the visible logical extents
        // of the layout tree.
        double get_log_aspect_ratio() const;

        // Get the logical extents of the layout tree.
        // Extents ignore invisible, "collapsed" items,
        // ie. an invisible click column is not included.
        virtual Size get_log_extents() const;

        // Get the canvas extents of the layout tree.
        Size get_canvas_extents() const;

        // Account for stroke width and antialiasing of keys and bars
        Size get_extra_render_size();

        // Scale item and its children to fit inside the given canvas_rect.
        void fit_inside_canvas(const Rect& canvas_border_rect);

        virtual void do_fit_inside_canvas(const Rect& canvas_border_rect);

        // Recursively update the log_rects of this sub-tree.
        void update_log_rects();

        // Update the log_rect of this item.
        // Override this for layout items that have to calculate their
        // logical rectangle.
        virtual void update_log_rect();

        // rectangle to test on click/touch
        Rect get_hit_rect();

        // Set clipping rectangle in canvas coordinates.
        void set_clip_rect(const Rect& canvas_rect);

        // Do clip given rect at the current clipping rect
        Rect clip(const Rect& canvas_rect) const
        { return clip_rect.intersection(canvas_rect); }

        // Returns true if the point lies within the items borders.
        bool is_point_within(const Point& canvas_point)
        { return get_hit_rect().contains(canvas_point); }

        void set_visible(bool visible);

        bool is_visible();

        virtual void on_visibility_changed(bool visible);

        // Checks if there is any visible key in the
        // subtree starting at self.
        bool has_visible_key();

        // Are all items in the path to the root visible?
        bool is_path_visible();

        // Are all items in the path to the root scannable?
        bool is_path_scannable();

        // Return the closeset scan_priority in the path to the root.
        int get_path_scan_priority();

        // Return the root layout item
        Ptr get_layout_root();

        // Return the first layer_id on the path from the tree root to self
        std::string get_layer();

        /*
        Doctests:
        >>> repr(LayoutItem.layer_to_parent_id(None))
        'None'
        >>> repr(LayoutItem.layer_to_parent_id("abc"))
        'None'
        >>> LayoutItem.layer_to_parent_id("abc.cde")
        'abc'
        >>> LayoutItem.layer_to_parent_id("abc.cde.fgh")
        'abc.cde'
        */
        // remove the last part of the layer_id
        std::string layer_to_parent_id(std::string layer_id) const;

        // Search the tree for layer ids and return them in order of appearance
        std::vector<std::string> get_layer_ids(const std::string& parent_layer_id);
        std::vector<std::string> get_layer_ids();
        void get_layer_ids(std::vector<std::string>& layer_ids);

        // Show all items of layers <layer_ids>, hide all items of
        // the other layers.
        void set_visible_layers(std::vector<std::string> layer_ids);

        // Traverse the tree and return all keys sorted by group.
        void get_key_groups(KeyGroups& key_groups_out);

        // Recursively searches for the closest definition of the svg filename.
        std::string get_filename();

        // Recursively searches for the closest definition of the
        // unlatch_layer attribute.
        Noneable<bool> can_unlatch_layer();

        virtual bool is_key() const
        { return false; }

        // recursively invalidate caches
        void invalidate_tree()
        {
            invalidate();
            for (auto& item : get_children())
                item->invalidate_tree();
        }

        // invalidate caches
        virtual void invalidate()
        {
            m_cached_hit_rects.clear();
        }

        // Traverses top to bottom all visible layout items of the
        // layout tree. Invisible paths are cut short.
        template< typename F >
        void for_each_visible_item(const F& func)
        {
            find_visible_item_if([&](const Ptr& item)
                                {func(item); return false;});
        }

        template< typename F >
        Ptr find_visible_item_if(const F& predicate)
        {
            if (visible)
            {
                auto item = getptr();
                if(predicate(item))
                    return item;

                for (auto child : get_children())
                    if(child->find_visible_item_if(predicate))
                        return child;
            }
            return {};
        }

        // Traverses all keys of the layout tree.
        template< typename F >
        void for_each_key(const F& func, std::string group_name="")
        {
            if (is_key())
                if (group_name.empty() || this->group == group_name)
                    func(getptr());

            for (auto item : get_children())
                item->for_each_key(func, group_name);
        }

        template< typename F >
        Ptr find_key_if(const F& predicate, std::string group_name="")
        {
            if (is_key())
                if (group_name.empty() || this->group == group_name)
                {
                    auto item = getptr();
                    if (predicate(item))
                        return item;
                }

            for (auto child : get_children())
                if(child->find_key_if(predicate, group_name))
                    return child;

            return {};
        }

        // Traverses all items of the tree including sublayouts.
        template< typename F >
        void for_each_global_item(const F& func)
        {
            func(getptr());

            for (auto item : get_children())
                item->for_each_global_item(func);

            for (auto item : this->sublayouts)
                item->for_each_global_item(func);
        }

        // Traverses all keys of the layout tree including sublayouts.
        template< typename F >
        void for_each_global_key(const F& func, std::string group_name="")
        {
            if (is_key())
                if (group_name.empty() || this->group == group_name)
                    func(getptr());

            for (auto item : get_children())
                item->for_each_global_key(func, group_name);

            for (auto item : this->sublayouts)
                item->for_each_global_key(func, group_name);
        }

        // Traverses all keys of the given layer.
        template< typename F >
        void for_each_layer_key(const F& func, std::string layer_id_="")
        {
            for_each_layer_item([&](const Ptr& item) {
                if (item->is_key())
                    func(item);
            }, layer_id_);
        }

        // Travers all items of the given layer.
        // The first layer definition found in the path to each key wins.
        // layer=None iterates through all keys that don't have a layer
        // specified anywhere in their path.
        template< typename F >
        void for_each_layer_item(const F& func, const std::string layer_id_="",
                                 bool only_visible=true)
        {
            std::string found_layer_id;
            for_each_layer_item_recursive(func, layer_id_, only_visible, found_layer_id);
        }

        template< typename F >
        void for_each_layer_item_recursive(const F& func, const std::string layer_id_,
                                           bool only_visible, std::string& found_layer_id_out)
        {
            if (only_visible && !this->visible)
                return;

            if (this->layer_id == layer_id_)
                found_layer_id_out = layer_id_;

            if (!this->layer_id.empty() &&
                this->layer_id != found_layer_id_out &&
               (layer_id_.empty() ||
                !(startswith(this->layer_id, layer_id_ + ".") ||
                  startswith(layer_id_, this->layer_id + "."))))
                return;

            if (found_layer_id_out == layer_id_)
                func(getptr());

            for (auto item : get_children())
                item->for_each_layer_item_recursive(func, layer_id_, only_visible,
                                                    found_layer_id_out);
        }

        void update_templates(const KeyTemplates& ts);

        void update_keysym_rules(const KeysymRules& rules);

        void append_sublayout(Ptr& sublayout);

        // Look for a sublayout item upwards, from self to the root.
        Ptr find_sublayout(const std::string& id);

        // Iterate from sublayouts all the way to the global layout root.
        // LayoutLoader needs this to access key templates from inside of
        // sublayouts.
        template< typename F >
        void for_each_to_global_root(const F& func)
        {
            auto item = getptr();
            while (item)
            {
                func(item);

                if (item->m_parent)
                    item = item->get_parent();
                else
                    item = item->get_sublayout_parent();
            }
        }

        template< typename F >
        Ptr find_to_global_root_if(const F& predicate)
        {
            auto item = getptr();
            while (item)
            {
                if(predicate(item))
                    return item;

                if (item->m_parent)
                    item = item->get_parent();
                else
                    item = item->get_sublayout_parent();
            }
            return {};
        }

        // return all root items of individual layout views
        std::vector<Ptr> get_view_subtrees()
        {
            return {this->getptr()};  // only one root for now
        }

        bool dispatch_input_sequence_begin(const InputSequencePtr& sequence);
        bool dispatch_input_sequence_update(const InputSequencePtr& sequence);
        bool dispatch_input_sequence_end(const InputSequencePtr& sequence);

        virtual bool on_input_sequence_begin(const InputSequencePtr& sequence);
        virtual bool on_input_sequence_update(const InputSequencePtr& sequence);
        virtual bool on_input_sequence_end(const InputSequencePtr& sequence);

        // Find the topmost key at point.
        Ptr get_item_at(const Point& point, const std::vector<std::string>& active_layer_ids);

        // Traverses top to bottom all visible layout items of the
        // layout tree. Invisible paths are cut short.
        virtual void draw_tree(DrawingContext& dc);
        virtual void draw_item(DrawingContext& dc);

        std::optional<Size> get_average_key_canvas_border_size();

    private:
        HitRects& get_hit_rects(const std::vector<std::string>& active_layer_ids);

    public:
        // signal called on destruction (for KeyboardView::on_event)
        DEFINE_SIGNAL(<>, destroy_notify, this);

        // group string of the item, label size group for keys
        std::string group;

        // Take this item out of the size group when updating the layout.
        // Instead chose the best label size for this item alone.
        bool ignore_group{false};

        // name of the layer the item is to be shown on, None for all layers
        std::string layer_id;

        // filename of the svg file where the key geometry is defined
        std::string filename;

        // State of visibility. Also determines if drawing space will be
        // assigned to this item and its children.
        bool visible{true};

        // sensitivity, aka. greying; False to stop interaction with the item
        bool sensitive{true};

        // Border around the item. The border "shrinks" the item and
        // is invisible but still sensitive to clicks.
        double border = 0.0;

        // Expand item in LayoutBoxes
        // "True"  expands the item into the space of invisible siblings.
        // "False" keeps it at the size of the even distribution of all siblings.
        //         Usually this will lock the key to the aspect ratio of its
        //         svg geometry.
        bool m_expand{true};

        // sublayout sub-trees
        std::vector<Ptr> sublayouts;

        // parent item of sublayout roots
        LayoutItem* sublayout_parent{nullptr};

        // override switching back to layer 0 on key press
        // True:  do switch to layer 0 on press
        // False: dont't
        // None:  maybe, hard-coded default-behavior for compatibility with <0.99
        Noneable<bool> unlatch_layer;

        // False if the key should be ignored by the scanner
        bool scannable{true};

        // Determines scanning order
        int scan_priority{0};  // None?

        // parsing helpers, only valid while loading a layout
        KeyTemplates templates;
        KeysymRules keysym_rules;

        // Clip children?
        Rect clip_rect;

        // Function to continue event processing on cancelling a gesture
        std::function<void(const InputSequencePtr& sequence)> sequence_begin_retry_func;

    private:
        // key context for transformation between logical and canvas coordinates
        std::unique_ptr<LayoutContext> m_context;

        HitRectMap m_cached_hit_rects;
};

typedef LayoutItem::Ptr LayoutItemPtr;


// Container for distributing items along a single horizontal or
// vertical axis. Items touch, but don't overlap.
class LayoutBox: public LayoutItem
{
    public:
        // Spread out child items horizontally or vertically.
        bool horizontal{true};

        // distance between items
        double spacing{1};

        // don't extend bounding box into invisible items
        bool compact{false};

    public:
        using super = LayoutItem;
        using Ptr = std::shared_ptr<LayoutBox>;

        LayoutBox(const ContextBase& context, bool horizontal_=true) :
            LayoutItem(context),
            horizontal(horizontal_)
        {}
        virtual ~LayoutBox() override
        {}

        static std::unique_ptr<LayoutBox> make(const ContextBase& get_context);

        static constexpr const char* CLASS_NAME = "LayoutBox";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual void dump(std::ostream& out) const override;

        virtual void update_log_rect() override
        { get_context()->log_rect = calc_bounds(); }

        // Scale items to fit inside the given canvas_rect
        virtual void do_fit_inside_canvas(const Rect& canvas_border_rect) override;

        // Get the logical extents of the layout tree.
        // Extents ignore invisible, "collapsed" items,
        // ie. an invisible click column is not included.
        virtual Size get_log_extents() const override;

    private:
        // Calculate the bounding rectangle over all items of this panel.
        // Include invisible items to stretch the visible ones into their
        // space too.
        Rect calc_bounds();
};


std::ostream& operator<< (std::ostream& out, const LayoutItem& item);


#endif
