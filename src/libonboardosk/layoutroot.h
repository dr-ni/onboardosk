#ifndef LAYOUTROOT_H
#define LAYOUTROOT_H

#include <string>
#include <vector>

#include "layoutdrawingitem.h"


class LayoutRoot : public LayoutPanel
{
    public:
        using Super = LayoutPanel;
        using This = LayoutRoot;
        using Ptr = std::shared_ptr<This>;

        LayoutRoot(const ContextBase& context);
        virtual void dump(std::ostream& out) const override;
        static std::unique_ptr<This> make(const ContextBase& context);
/*
        void invalidate_font_sizes()
        { m_font_sizes_valid = false; }

        bool get_font_sizes_valid()
        { return m_font_sizes_valid; }

        void set_font_sizes_valid(bool valid)
        { m_font_sizes_valid = valid; }
*/
        void set_item_visible(LayoutItemPtr item, bool visible);

    private:
        bool m_font_sizes_valid{false};
};


inline LayoutRoot::Ptr get_layout_root(LayoutItem::Ptr item)
{
    if (item)
        return std::dynamic_pointer_cast<LayoutRoot>(item->get_layout_root());
    return {};
}

#endif // LAYOUTROOT_H
