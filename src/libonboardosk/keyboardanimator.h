#ifndef KEYBOARDTRANSITION_H
#define KEYBOARDTRANSITION_H

#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

#include "tools/rect_fwd.h"

#include "keyboard.h"
#include "onboardoskglobals.h"

using std::vector;

class Timer;
class TransitionVariable;
class TransitionState;

using TransitionDuration = std::chrono::milliseconds;


class KeyboardAnimator : public ContextBase
{
    public:
        KeyboardAnimator(const ContextBase& context);
        ~KeyboardAnimator ();

        bool transition_visible_to(bool visible,
                Noneable<TransitionDuration> opacity_duration={},
                Noneable<TransitionDuration> slide_duration={});

        // Transition active state for inactivity timer.
        // This ramps up/down the window opacity.
        bool transition_active_to(bool active, Noneable<TransitionDuration> duration={});
        bool transition_position_to(const Point& pt);
        void commit_transition();

        // Update transition variables with the actual window position.
        // Necessary on user positioning.
        void sync_transition_position(const Rect& rect);

    private:
        bool init_opacity_transition(TransitionVariable& var,
                                     double target_value,
                                     TransitionDuration duration);
        bool init_transition(TransitionVariable& var,
                             double target_value,
                             TransitionDuration duration);
        bool on_transition_step();

    private:
        std::unique_ptr<TransitionState> m_transition_state;
        std::unique_ptr<Timer> m_transition_timer;
};

#endif // KEYBOARDTRANSITION_H
