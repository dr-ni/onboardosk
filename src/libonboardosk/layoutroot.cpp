#include "layoutroot.h"


std::unique_ptr<LayoutRoot> LayoutRoot::make(const ContextBase& context)
{
    return std::make_unique<This>(context);
}

LayoutRoot::LayoutRoot(const ContextBase& context) :
    Super(context)
{
}

void LayoutRoot::dump(std::ostream& out) const
{
    out << "LayoutRootPanel(";
    Super::dump(out);
    out << ")";
}

void LayoutRoot::set_item_visible(LayoutItemPtr item, bool visible_)
{
    if (item->visible != visible_)
    {
        item->set_visible(visible_);
        invalidate_tree();
    }
}

