#ifndef TREEITEM_H
#define TREEITEM_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "tools/container_helpers.h"


// Abstract base class of tree nodes.
// Base class of nodes in  layout- and color scheme tree.
template <class T>
class TreeItem : public std::enable_shared_from_this<T>
{
    public:
        using Ptr = std::shared_ptr<T>;
        using Items = std::vector<Ptr>;

        TreeItem() {}
        TreeItem(const std::string& id_) : id(id_) {}
        virtual ~TreeItem()
        {
            // Don't leave dangling parent pointers when freeing this.
            // -> Leftover references of closing LayoutPopups in UnpressTimers
            // simply fail to traverse to their parent items (and don't crash).
            for (auto& child : get_children())
                child->m_parent = nullptr;
        }

        Ptr getptr() {
            return std::enable_shared_from_this<T>::shared_from_this();
        }

        // Recursively dump the layout (sub-) tree starting from this.
        void dump_tree(std::ostream& out, size_t level=0) const
        {
            out << std::string(level * 3, ' ');
            dump(out);
            out << std::endl;

            for (auto child : get_children())
                child->dump_tree(out, level + 1);
        }
        virtual void dump(std::ostream& out) const
        {
            out << "id='" << id << "'";
        }

        std::string get_id() const { return id; }
        virtual void set_id(const std::string& id_) { this->id = id_; }

        Ptr get_parent() const
        {
            if (m_parent)
                return m_parent->getptr();
            return {};
        }
        Items& get_children() { return m_children; }
        const Items& get_children() const { return m_children; }

        template<class TDerivedPtr>
        void set_children(const std::vector<TDerivedPtr>& items)
        {
            m_children.assign(items.begin(), items.end());
            for (auto& item : m_children)
                item->m_parent = reinterpret_cast<T*>(this);
        }

        void clear_children()
        {
            m_children.clear();
        }

        void append_child(const Ptr& item)
        {
            m_children.insert(m_children.end(), item);
            item->m_parent = reinterpret_cast<T*>(this);
        }

        template<class TDerivedPtr>
        void append_children(const std::vector<TDerivedPtr>& items)
        {
            m_children.insert(m_children.end(), items.begin(), items.end());

            for (auto& item : m_children)
                item->m_parent = reinterpret_cast<T*>(this);
        }

        void insert_child(size_t index, const Ptr& item)
        {
            m_children.insert(m_children.end()+index, item);
            item->parent = this;
        }

        void remove_child(const Ptr& item)
        {
            std::remove(m_children.begin(), m_children.end(), item);
        }

        // lower this to the bottom of its siblings
        void lower_to_bottom()
        {
            auto parent_ = get_parent();
            if (parent_)
            {
                auto tmp = getptr();
                parent_->remove_child(tmp);
                parent_->insert_child(0, tmp);
            }
        }

        // raise self to the top of its siblings
        void raise_to_top()
        {
            auto parent_ = get_parent();
            if (parent_)
            {
                auto tmp = getptr();
                parent_->remove_child(tmp);
                parent_->append_child(tmp);
            }
        }

        // find the first item with matching id
        Ptr find_id(const std::string& id_)
        {
            return find_if([&id_](const Ptr& item) -> bool
            {
                return item->id == id_;
            });
        }

        // find all items with matching id
        Items find_ids(const std::vector<std::string>& ids)
        {
            Items v;
            for_each ([&v, &ids](const Ptr& item) -> void
            {
                if (contains(ids, item->id))
                    v.emplace_back(item);
            });
            return v;
        }

        // find all items with matching class
        Items find_classes(const std::vector<std::string>& class_names)
        {
            Items v;
            for_each ([&v, &class_names](const Ptr& item) -> void
            {
                if (contains(class_names, item->get_class_name()))
                    v.emplace_back(item);
            });
            return v;
        }

        // Traverse tree depth-first, pre-order.
        template< typename F >
        void for_each(const F& func)
        {
            func(getptr());

            for (auto child : get_children())
                child->for_each(func);
        }

        // Traverse tree depth-first, pre-order.
        template< typename F >
        Ptr find_if(const F& predicate)
        {
            auto item = getptr();
            if (predicate(item))
                return item;

            for (auto& child : get_children())
                if (child->find_post_order_if(predicate))
                    return child;

            return {};
        }

        // Traverse tree depth-first, post-order.
        template< typename F >
        Ptr find_post_order_if(const F& predicate)
        {
            for (auto& child : get_children())
                if (child->find_post_order_if(predicate))
                    return child;

            auto item = getptr();
            if (predicate(item))
                return item;

            return {};
        }

        template< typename F >
        void for_each_post_order(const F& func)
        {
            for (auto& child : get_children())
                child->for_each_post_order(func);

            func(getptr());
        }

        template< typename F >
        Ptr find_to_root_if(const F& predicate)
        {
            auto item = getptr();
            while (item)
            {
                if(predicate(item))
                    return item;
                item = item->get_parent();
            }
            return {};
        }

        template< typename F >
        void for_each_to_root(const F& func)
        {
            auto item = getptr();
            while (item)
            {
                func(item);
                item = item->get_parent();
            }
        }

        bool is_parent_of(const Ptr& child_item)
        {
            auto item = child_item;
            while (item)
            {
                if (item.get() == this)
                    return true;
                item = item->get_parent();
            }
            return false;
        }

    public:
        // id string of the item
        std::string id;

        // parent item in the tree
        T* m_parent{nullptr};

        // child items
        Items m_children;
};


#endif // TREEITEM_H
