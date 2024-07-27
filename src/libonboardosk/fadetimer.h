#ifndef FADETIMER_H
#define FADETIMER_H

#include <chrono>

#include "timer.h"


// Sine-interpolated fade between two values, e.g. opacities.
class FadeTimer : public Timer
{
    public:
        using Super = Timer;
        using Clock = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;
        using FadeCallback = std::function<void(double, bool)>;

        FadeTimer(const ContextBase& context);
        ~FadeTimer();

        // Start value fade.
        // duration: fade time in milliseconds, 0 for immediate change
        void fade_to(double start_value, double target_value,
                     std::chrono::milliseconds duration,
                     FadeCallback func={});

        void start(std::chrono::milliseconds delay);

        void stop();

        virtual bool on_timer() override;

    public:
        double target_value;
        std::chrono::milliseconds time_step{50};

    private:
        double value;
        double start_value;
        int iteration = 0;   // just a counter of on_timer calls since start

        TimePoint m_start_time;
        std::chrono::milliseconds m_duration;
        FadeCallback m_func;
};



#endif // FADETIMER_H
