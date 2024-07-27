#ifndef BUTTONCONTROLLER_H
#define BUTTONCONTROLLER_H

#include "keyboarddecls.h"
#include "layoutdecls.h"
#include "onboardoskglobals.h"


// MVC inspired controller that handles events and the resulting
// state changes of buttons.
class ButtonController : public ContextBase
{
    public:
        ButtonController(const ContextBase& context, const LayoutItemPtr& key);
        virtual ~ButtonController() = default;

        static std::unique_ptr<ButtonController> make_for(const LayoutItemPtr& item,
                                                          const ContextBase& context);

        LayoutItemPtr get_item();

        LayoutKeyPtr get_key();

        // button pressed
        virtual void press(LayoutView* view, MouseButton::Enum button, EventType::Enum event_type);

        // button pressed long
        virtual void long_press(LayoutView* view, MouseButton::Enum button);

        // button released
        virtual void release(LayoutView* view, MouseButton::Enum button, EventType::Enum event_type);

        // asynchronous ui update
        virtual void update();

        // after suggestions have been updated
        virtual void update_late();

        // can start dwelling?
        virtual bool can_dwell();

        // Cannot cancel already called press() without consequences?
        virtual bool is_activated_on_press();

        void set_items_to_invalidate_container(ItemsToInvalidate& items);

    protected:
        void set_visible(bool visible);

        void set_sensitive(bool sensitive);

        void set_active(bool active);

        void set_locked(bool locked);

    protected:
        std::weak_ptr<LayoutItem> m_item;
        std::string m_id;
        ItemsToInvalidate* m_items_to_invalidate{0};
};



#endif // BUTTONCONTROLLER_H
