#include <memory>

#include "tools/cairocontext.h"
#include "tools/container_helpers.h"
#include "tools/noneable.h"
#include "tools/rect.h"
#include "tools/string_helpers.h"

#include "drawingcontext.h"
#include "inputsequence.h"
#include "layoutitem.h"
#include "layoutkey.h"
#include "layoutcontext.h"

using namespace std;

ostream&operator<<(ostream& out, const LayoutItem& item) {
    item.dump(out);
    return out;
}

LayoutItem::LayoutItem(const ContextBase& context) :
    ContextBase(context),
    m_context(make_unique<LayoutContext>())
{}

LayoutItem::~LayoutItem()
{
    this->destroy_notify.emit();   // notify listeners of destruction
    this->sublayout_parent = {};
}

void LayoutItem::dump(ostream& out) const
{
    Super::dump(out);
    out << " layer_id=" << repr(layer_id)
        << " fn=" << repr(filename)
        << " vis=" << repr(visible)
        << " log=" << get_context()->log_rect
        << " cvs=" << get_context()->canvas_rect;
}

Rect LayoutItem::get_rect() const
{ return get_border_rect().deflate(border); }

Rect LayoutItem::get_border_rect() const
{ return get_context()->log_rect;}

void LayoutItem::set_border_rect(const Rect& border_rect)
{ get_context()->log_rect = border_rect; }

Rect LayoutItem::get_initial_border_rect() const
{ return get_context()->initial_log_rect; }

void LayoutItem::set_initial_border_rect(const Rect& border_rect)
{ get_context()->initial_log_rect = border_rect; }

Rect LayoutItem::get_canvas_rect() const
{ return get_context()->log_to_canvas_rect(get_rect()); }

Rect LayoutItem::get_canvas_border_rect() const
{ return get_context()->canvas_rect; }

double LayoutItem::get_log_aspect_ratio() const
{
    Size sz = get_log_extents();
    return sz.w / sz.h;
}

Size LayoutItem::get_log_extents() const
{ return get_border_rect().get_size(); }

Size LayoutItem::get_canvas_extents() const
{
    Size sz = get_log_extents();
    return get_context()->scale_log_to_canvas(sz);
}

Size LayoutItem::get_extra_render_size()
{
    const auto& root = get_layout_root();
    return root->get_context()->scale_log_to_canvas(Size(2.0, 2.0));
}

void LayoutItem::fit_inside_canvas(const Rect& canvas_border_rect)
{
    // recursively update item's bounding boxes
    update_log_rects();

    // recursively fit inside canvas
    do_fit_inside_canvas(canvas_border_rect);
}

void LayoutItem::do_fit_inside_canvas(const Rect& canvas_border_rect)
{
    get_context()->canvas_rect = canvas_border_rect;
}

void LayoutItem::update_log_rects()
{
    for_each_post_order([](const Ptr& item) {
        item->update_log_rect();
    });
}

void LayoutItem::update_log_rect()
{}

Rect LayoutItem::get_hit_rect()
{
    Rect rect = get_canvas_border_rect().inflate(1);

    // attempt to clip at the parent's clip_rect
    auto parent_ = get_parent();
    if (parent_ && !parent_->clip_rect.empty())
    {
        rect = parent_->clip(rect);
        if (rect.empty())
            return {};
    }

    return rect;
}

void LayoutItem::set_clip_rect(const Rect& canvas_rect)
{ clip_rect = canvas_rect; }

void LayoutItem::set_visible(bool visible_)
{
    if (this->visible != visible_)
    {
        this->visible = visible_;
        on_visibility_changed(visible_);
    }
}

bool LayoutItem::is_visible()
{ return visible; }

void LayoutItem::on_visibility_changed(bool visible_)
{
    for (auto child : get_children())
        child->on_visibility_changed(visible_);
}

bool LayoutItem::has_visible_key()
{
    return find_visible_item_if([&]( const Ptr& item) {
        return item->is_key();
    }) != nullptr;
}

bool LayoutItem::is_path_visible()
{
    auto item = getptr();
    while (item)
    {
        if (!item->visible)
            return false;
        item = item->get_parent();
    }
    return true;
}

bool LayoutItem::is_path_scannable()
{
    auto item = getptr();
    while (item)
    {
        if (!item->scannable)
            return false;
        item = item->get_parent();
    }
    return true;
}

int LayoutItem::get_path_scan_priority()
{
    auto item = getptr();
    while (item)
    {
        if (item->scan_priority > 0)
            return item->scan_priority;
        item = item->get_parent();
    }
    return 0;
}

LayoutItem::Ptr LayoutItem::get_layout_root()
{
    auto item = getptr();
    while (item)
    {
        auto parent_ = item->get_parent();
        if (!parent_)
            return item;
        item = parent_;
    }
    return {};
}

string LayoutItem::get_layer()
{
    std::string _layer_id;
    auto item = getptr();
    while (item)
    {
        if (!item->layer_id.empty())
            _layer_id = item->layer_id;
        item = item->get_parent();
    }
    return _layer_id;
}

string LayoutItem::layer_to_parent_id(string _layer_id) const
{
    if (_layer_id.empty())
        return {};

    string search = ".";
    auto it = find_end(_layer_id.begin(), _layer_id.end(),
                       search.begin(), search.end());
    if (it == _layer_id.end())
        return {};
    return string(_layer_id.begin(), it);
}

void LayoutItem::set_visible_layers(std::vector<string> layer_ids)
{
    if (!layer_id.empty() &&
        !is_key())
        set_visible(contains(layer_ids, layer_id));

    for (auto item : get_children())
        item->set_visible_layers(layer_ids);
}

void LayoutItem::get_key_groups(KeyGroups& key_groups_out)
{
    key_groups_out.clear();

    for_each([&](const Ptr& item)
    {
        if (item->is_key())
        {
            auto it = key_groups_out.find(item->group);
            if (it == key_groups_out.end())
                key_groups_out.emplace(item->group, std::vector<Ptr>{item});
            else
                it->second.emplace_back(item);
        }
    });
}

string LayoutItem::get_filename()
{
    if (!filename.empty())
        return filename;

    auto parent_ = get_parent();
    if (parent_)
        return parent_->get_filename();
    return {};
}

Noneable<bool> LayoutItem::can_unlatch_layer()
{
    if (!this->unlatch_layer.is_none())
        return this->unlatch_layer;

    auto parent_ = get_parent();
    if (parent_)
        return parent_->can_unlatch_layer();

    return {};
}

void LayoutItem::update_templates(const KeyTemplates& ts)
{
    update_map(this->templates, ts);
}

void LayoutItem::update_keysym_rules(const KeysymRules& rules)
{
    update_map(this->keysym_rules, rules);
}

void LayoutItem::append_sublayout(LayoutItem::Ptr& sublayout)
{
    if (sublayout)
        this->sublayouts.emplace_back(sublayout);
}

LayoutItem::Ptr LayoutItem::find_sublayout(const string& id_)
{
    Ptr sublayout;

    find_to_root_if([&](const Ptr& item)
    {
        auto sls = item->sublayouts;
        for (auto sl : sls)
            if (sl->id == id_)
            {
                sublayout = sl;
                return true;
            }
        return false;
    });

    return sublayout;
}

bool LayoutItem::dispatch_input_sequence_begin(const InputSequencePtr& sequence)
{
    if (this->visible && this->sensitive)
    {
        Point point = sequence->point;
        Rect rect = get_canvas_border_rect();
        if (rect.contains(point))
        {
            // allow self to handle it first
            if (on_input_sequence_begin(sequence))
                 return true;

            // then ask the children
            for (auto& item : get_children())
                if (item->dispatch_input_sequence_begin(sequence))
                    return true;
        }
    }
    return false;
}

bool LayoutItem::dispatch_input_sequence_update(const InputSequencePtr& sequence)
{
    if (sequence->active_item)
    {
        return sequence->active_item->on_input_sequence_update(sequence);
    }
    else
    {
        if (this->visible && this->sensitive)
        {
            Point point = sequence->point;
            Rect rect = get_canvas_border_rect();
            if (rect.contains(point))
            {
                if (on_input_sequence_update(sequence))
                    return true;

                for (auto& item : get_children())
                {
                    if (item->dispatch_input_sequence_update(sequence))
                        return true;
                }
            }
        }
    }
    return false;
}

bool LayoutItem::dispatch_input_sequence_end(const InputSequencePtr& sequence)
{
    if (sequence->active_item)
    {
        return sequence->active_item->on_input_sequence_end(sequence);
    }
    else
    {
        if (this->visible && this->sensitive)
        {
            Point point = sequence->point;
            Rect rect = get_canvas_border_rect();
            if (rect.contains(point))
            {
                // allow self to handle it first
                if (on_input_sequence_end(sequence))
                    return true;

                // then ask the children
                for (auto item : get_children())
                    if (item->dispatch_input_sequence_end(sequence))
                        return true;
            }
        }
    }
    return false;
}

bool LayoutItem::on_input_sequence_begin(const InputSequencePtr& sequence)
{
    (void)sequence;
    return false;
}

bool LayoutItem::on_input_sequence_update(const InputSequencePtr& sequence)
{
    (void)sequence;
    return false;
}

bool LayoutItem::on_input_sequence_end(const InputSequencePtr& sequence)
{
    (void)sequence;
    return false;
}


void LayoutItem::draw_tree(DrawingContext& dc)
{
    if (this->visible)
    {
        if (dc.draw_rect.intersects(get_canvas_border_rect()))
        {
            if (!this->clip_rect.empty())
            {
                auto cr = dc.get_cc();
                cr->save();
                cr->rectangle(this->clip_rect.floor());  // int clip is faster
                cr->clip();
            }

            draw_item(dc);

            for (auto& item  : get_children())
                item->draw_tree(dc);

            if (!this->clip_rect.empty())
            {
                dc.get_cc()->restore();
            }
        }
    }


}

void LayoutItem::draw_item(DrawingContext& dc)
{
    if (!this->layer_id.empty())
    {
        dc.draw_layer_background(getptr());
    }
}


std::vector<std::string> LayoutItem::get_layer_ids(const std::string& parent_layer_id)
{
    auto layer_ids = get_layer_ids();
    if (!parent_layer_id.empty())
    {
        std::vector<std::string> v;
        auto prefix = parent_layer_id + ".";
        for (auto& id_ : layer_ids)
            if (startswith(id_, prefix))
                v.emplace_back(id_);
        return v;
    }
    return layer_ids;
}

std::vector<string> LayoutItem::get_layer_ids()
{
    std::vector<std::string> layer_ids;
    get_layer_ids(layer_ids);
    return layer_ids;
}

void LayoutItem::get_layer_ids(vector<string>& layer_ids)
{
    if (!layer_id.empty() &&
        !contains(layer_ids, layer_id))
        layer_ids.emplace_back(layer_id);

    for (auto child : get_children())
        child->get_layer_ids(layer_ids);
}

LayoutItemPtr LayoutItem::get_item_at(const Point& point, const std::vector<std::string>& active_layer_ids)
{
    Ptr result;
    const auto& hit_rects = get_hit_rects(active_layer_ids);
    for (auto hit_rect : hit_rects)
    {
        // Inlined test, !using Rect.is_point_within for speed.
        if (hit_rect.r.contains(point))
        {
            LayoutItemPtr& item = hit_rect.item;
            if (item->is_key())
            {
                LayoutKeyPtr key = std::dynamic_pointer_cast<LayoutKey>(item);
                if (!key->geometry ||
                    key->get_hit_path()->is_point_within(point))
                {
                    result = item;
                    break;
                }
            }
        }
    }

    return result;
}

LayoutItem::HitRects& LayoutItem::get_hit_rects(const std::vector<string>& active_layer_ids)
{
    std::string key_str = join(active_layer_ids, " ");
    auto itrect = m_cached_hit_rects.find(key_str);
    if (itrect == m_cached_hit_rects.end())
    {
        // All visible && sensitive key items sorted by z-order.
        // Keys of the active layer have priority over non-layer keys
        // (layer switcher, hide, etc.).
        std::vector<LayoutItemPtr> items;
        for (auto& layer_id_ : active_layer_ids)
        {
            // insert items of layer layer_id in reverse order
            for_each_layer_key(
                [&](const LayoutItemPtr& item) {items.emplace_back(item);},
                layer_id_);
        }

        // insert items of the None layer in reverse order
        for_each_layer_key(
            [&](const LayoutItemPtr& item) {items.emplace_back(item);},
            {});

        HitRects hit_rects;
        for (auto it=items.rbegin(); it != items.rend(); ++it)
        {
            auto item = *it;
            Rect r = item->get_hit_rect();
            if (!r.empty())  // not clipped away?
                hit_rects.emplace_back(HitRect{r, item});
        }

        auto pair = m_cached_hit_rects.insert_or_assign(key_str, std::move(hit_rects));
        itrect = pair.first;
    }

    return itrect->second;
}

std::optional<Size> LayoutItem::get_average_key_canvas_border_size()
{
    size_t n = 0;
    std::optional<Size> avgsz;

    for_each_layer_item([&](const LayoutItemPtr& item)
    {
        LayoutKey* key = dynamic_cast<LayoutKey*>(item.get());
        if (key)
        {
            Size sz = key->get_canvas_border_rect().get_size();
            if (!avgsz)
                avgsz = sz;
            else
                avgsz.value() += sz;
            n++;
        }
    }, "", true);

    if (avgsz)
    {
        avgsz.value().w /= n;
        avgsz.value().h /= n;
    }
    return avgsz;
}


std::unique_ptr<LayoutBox> LayoutBox::make(const ContextBase& context)
{
    return std::make_unique<LayoutBox>(context);
}

void LayoutBox::dump(ostream& out) const
{
    out << "LayoutBox(";
    super::dump(out);
    out << " horizontal=" << repr(this->horizontal)
        << " spacing=" << this->spacing
        << " compact=" << repr(this->compact)
        << ")";
}

void LayoutBox::do_fit_inside_canvas(const Rect& canvas_border_rect)
{
    LayoutItem::do_fit_inside_canvas(canvas_border_rect);

    size_t axis = this->horizontal ? 0 : 1;
    auto& children = get_children();

    // get canvas rectangle without borders
    Rect canvas_rect = get_canvas_rect();

    // Find the combined length of all items, including
    // invisible ones (logical coordinates).
    double full_length = 0.0;
    int i = 0;
    for (auto child : children)
    {
        Rect rect = child->get_border_rect();
        if (!rect.empty())
            if (i)
                full_length += this->spacing;
        full_length += rect[axis + 2];
    }

    // Find the stretch factor, that fills the available canvas space with
    // evenly distributed, all visible items.
    double fully_visible_scale = full_length != 0.0 ?
                                               canvas_rect[axis + 2] / full_length : 1.0;
    double canvas_spacing = fully_visible_scale * this->spacing;

    // Transform items into preliminary canvas space, drop invisibles
    // and find the total lengths of expandable and non-expandable
    // items (preliminary canvas coordinates).
    double length_expandables = 0.0;
    int num_expandables = 0;
    double length_nonexpandables = 0.0;
    int num_nonexpandables = 0;

    for (auto child : children)
    {
        double length = child->get_border_rect()[axis + 2];
        if (length != 0.0 && child->has_visible_key())
        {
            length *= fully_visible_scale;
            if (child->m_expand)
            {
                length_expandables += length;
                num_expandables += 1;
            }
            else
            {
                length_nonexpandables += length;
                num_nonexpandables += 1;
            }
        }
    }

    // Calculate a second stretch factor for expandable and actually
    // visible items. This takes care of the part of the canvas_rect
    // that isn't covered by the first factor yet.
    // All calculation is done in preliminary canvas coordinates.
    double length_target = canvas_rect[axis + 2] - length_nonexpandables -
                           canvas_spacing * (num_nonexpandables + num_expandables - 1);
    double expandable_scale = length_expandables != 0.0 ?
                                                        length_target / length_expandables : 1.0;

    // Calculate the final canvas rectangles and traverse
    // the tree recursively.
    double position = 0.0;
    for (auto child : children)
    {
        Rect rect = child->get_border_rect();
        double spacing_;
        double length;
        if (child->has_visible_key())
        {
            length  = rect[axis + 2];
            spacing_ = canvas_spacing;
        }
        else
        {
            length  = 0.0;
            spacing_ = 0.0;
        }

        double scale = fully_visible_scale;
        if (child->m_expand)
            scale *= expandable_scale;
        double canvas_length = length * scale;

        // set the final canvas rect
        Rect r(canvas_rect);
        r[axis]   = canvas_rect[axis] + position;
        r[axis + 2] = canvas_length;
        child->do_fit_inside_canvas(r);

        position += canvas_length + spacing_;
    }
}

Size LayoutBox::get_log_extents() const
{
    Rect rect;
    bool rect_valid = false;

    for (auto child : get_children())
    {
        Rect r = child->get_border_rect();
        if (!rect_valid)
            rect = r;
        else
        {
            if (this->horizontal)
                rect.w += r.w;
            else
                rect.h += r.h;
        }
    }

    return rect.get_size();
}

Rect LayoutBox::calc_bounds()
{
    Noneable<Rect> bounds;
    for (auto item : get_children())
    {
        if (!this->compact || item->visible)
        {
            Rect rect = item->get_border_rect();
            if (!rect.empty())
            {
                if (bounds.is_none())
                    bounds = rect;
                else
                    bounds = rect.union_(bounds);
            }
        }
    }

    return bounds;
}


