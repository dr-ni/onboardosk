#ifndef UIELEMENT_H
#define UIELEMENT_H

#include <memory>

#include "tools/noneable.h"
#include "tools/rect_fwd.h"
#include "tools/textdecls.h"

#include "onboardoskglobals.h"
#include "textchangesdecls.h"

// Abstract base class of accessibles
class UIElement : public ContextBase
{
    public:
        using Ptr = std::shared_ptr<UIElement>;
        UIElement(const ContextBase& context) :
            ContextBase(context)
        {}
        virtual ~UIElement()
        {}

        virtual std::ostream& dump(std::ostream& s,
                          const std::string& prefix,
                          const std::string& suffix) const = 0;

    private:
        virtual bool is_same(const Ptr& other) const = 0;

    public:
        virtual bool is_text_entry() const = 0;
        virtual bool is_password_entry() const = 0;
        virtual bool is_urlbar() const = 0;
        virtual bool is_byobu() const = 0;
        virtual bool is_terminal() const = 0;
        virtual bool is_single_line() const = 0;
        virtual void invalidate_extents() const = 0;
        virtual bool can_insert_text() const = 0;
        virtual Noneable<Rect> get_extents() const = 0;
        virtual Noneable<Rect> get_character_extents(int offset) = 0;
        virtual Noneable<Rect> get_frame_extents() const = 0; // return empty rect if there is no frame
        virtual Noneable<Span> get_selection(int selection_num=0) const = 0;
        virtual TextLength get_character_count() const = 0;
        virtual TextPos get_caret_offset() const = 0;
        virtual TextSpanPtr get_line_at_offset(TextPos offset) const = 0;
        virtual TextSpanPtr get_line_before_offset(TextPos offset) const  = 0;
        virtual TextSpanPtr get_text(const Span& span) const = 0;
        virtual void set_caret_offset(int offset) = 0;
        virtual bool insert_text(int position, const std::string& text) = 0;
        virtual bool delete_text(int start_pos, int end_pos) = 0;

        friend bool is_same(const Ptr& a, const Ptr& b);
};
typedef UIElement::Ptr UIElementPtr;


inline bool is_same(const UIElementPtr& a, const UIElementPtr& b)
{
    return a == b ||       // both nullptr?
           (a && b && a->is_same(b));  // both wrap the same accessible?
}

inline std::ostream& operator<<(std::ostream& s, const UIElement& e)
{
    e.dump(s, {}, "");
    return s;
}


#endif // UIELEMENT_H
