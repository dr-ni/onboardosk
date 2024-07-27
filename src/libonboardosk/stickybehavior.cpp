#include <array>
#include <map>

#include "tools/container_helpers.h"
#include "stickybehavior.h"


static std::map<std::string, StickyBehavior::Enum> StickyBehavior_names = {
    {"none",               StickyBehavior::NONE},
    {"cycle",              StickyBehavior::CYCLE},
    {"dblclick",           StickyBehavior::DOUBLE_CLICK},
    {"latch",              StickyBehavior::LATCH_ONLY},
    {"lock",               StickyBehavior::LOCK_ONLY},
    {"latch-lock-nocycle", StickyBehavior::LATCH_LOCK_NOCYCLE},
    {"dblclick-nocycle",   StickyBehavior::DOUBLE_CLICK_NOCYCLE},
    {"latch-nocycle",      StickyBehavior::LATCH_NOCYCLE},
    {"lock-nocycle",       StickyBehavior::LOCK_NOCYCLE},
    {"push",               StickyBehavior::PUSH_BUTTON},
};

StickyBehavior::Enum StickyBehavior::to_behavior(const std::string s)
{
    return get_value(StickyBehavior_names, s);
}

bool StickyBehavior::is_valid(StickyBehavior::Enum e)
{
    return contains(get_values(StickyBehavior_names), e);
}

bool StickyBehavior::can_latch(StickyBehavior::Enum e)
{
    static constexpr std::array<StickyBehavior::Enum, 6> a {
        StickyBehavior::CYCLE,
        StickyBehavior::DOUBLE_CLICK,
        StickyBehavior::LATCH_ONLY,
        StickyBehavior::LATCH_LOCK_NOCYCLE,
        StickyBehavior::DOUBLE_CLICK_NOCYCLE,
        StickyBehavior::LATCH_NOCYCLE,
    };
    return contains(a, e);
}

bool StickyBehavior::can_lock(StickyBehavior::Enum e)
{
    return StickyBehavior::can_lock_on_single_click(e) ||
           StickyBehavior::can_lock_on_double_click(e);
}

bool StickyBehavior::can_lock_on_single_click(StickyBehavior::Enum e)
{
    static constexpr std::array<StickyBehavior::Enum, 4> a {
        StickyBehavior::CYCLE,
        StickyBehavior::LOCK_ONLY,
        StickyBehavior::LATCH_LOCK_NOCYCLE,
        StickyBehavior::LATCH_NOCYCLE,
    };
    return contains(a, e);
}

bool StickyBehavior::can_lock_on_double_click(StickyBehavior::Enum e)
{
    return e == StickyBehavior::DOUBLE_CLICK ||
           e == StickyBehavior::DOUBLE_CLICK_NOCYCLE;
}

bool StickyBehavior::can_cycle(StickyBehavior::Enum e)
{
    static constexpr std::array<StickyBehavior::Enum, 4> a {
        StickyBehavior::CYCLE,
        StickyBehavior::DOUBLE_CLICK,
        StickyBehavior::LATCH_ONLY,
        StickyBehavior::LOCK_ONLY,
    };
    return contains(a, e);
}
