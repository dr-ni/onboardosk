#ifndef STICKYBEHAVIOR_H
#define STICKYBEHAVIOR_H

#include <string>

class StickyBehavior
{
    public:
        enum Enum
        {
            NONE,
            CYCLE,
            DOUBLE_CLICK,
            LATCH_ONLY,
            LOCK_ONLY,
            LATCH_LOCK_NOCYCLE,
            DOUBLE_CLICK_NOCYCLE,
            LATCH_NOCYCLE,
            LOCK_NOCYCLE,
            PUSH_BUTTON,
        };

        // translate modifier names traditionally used by Onboards's layouts
        static Enum to_behavior(const std::string s);

        static bool is_valid(StickyBehavior::Enum e);

        // Can sticky key enter latched state?
        // Latched keys are automatically released when a
        // non-sticky key is pressed.
        static bool can_latch(StickyBehavior::Enum e);

        static bool can_lock(StickyBehavior::Enum e);

        // Can sticky key enter locked state?
        // Locked keys stay active until they are pressed again.
        static bool can_lock_on_single_click(StickyBehavior::Enum e);

        // Can sticky key enter locked state on double click?
        // Locked keys stay active until they are pressed again.
        static bool can_lock_on_double_click(StickyBehavior::Enum e);

        // Can sticky key return to normal state?
        // Latched keys are still automatically released when a
        // non-sticky key is pressed.
        static bool can_cycle(StickyBehavior::Enum e);
};

#endif // STICKYBEHAVIOR_H
