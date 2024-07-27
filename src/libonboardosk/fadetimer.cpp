
#include <cmath>

#include "fadetimer.h"

using SteadyClock = std::chrono::steady_clock;
using SteadyTimePoint = std::chrono::time_point<SteadyClock>;

static double sin_int(double lin_progress, double start_value, double target_value)
{
    double sin_progress = (std::sin(lin_progress * M_PI - M_PI / 2.0) + 1.0) / 2.0;
    return sin_progress * (target_value - start_value) + start_value;
}

static std::tuple<double, bool> sin_fade(SteadyTimePoint start_time,
                                         std::chrono::milliseconds duration,
                                         double start_value, double target_value)
{
    auto elapsed = SteadyClock::now() - start_time;
    double lin_progress;
    if (duration == std::chrono::milliseconds(0))
    {
        double ratio = static_cast<double>(elapsed.count()) /
                       static_cast<double>(duration.count());
        lin_progress = std::min(1.0, ratio);
    }
    else
    {
        lin_progress = 1.0;
    }

    return {sin_int(lin_progress, start_value, target_value),
            lin_progress >= 1.0};
}


FadeTimer::FadeTimer(const ContextBase& context) :
    Super(context)
{}

FadeTimer::~FadeTimer()
{}

void FadeTimer::fade_to(double start_value_, double target_value_,
                        std::chrono::milliseconds duration, FadeCallback func)
{
    this->value = start_value_;
    this->start_value = start_value_;
    m_start_time = std::chrono::steady_clock::now();
    m_duration = duration;

    start(this->time_step);

    this->target_value = target_value_;
    m_func = func;
}

void FadeTimer::start(std::chrono::milliseconds delay)
{
    this->iteration = 0;
    Super::start(delay);
}

void FadeTimer::stop()
{
    Super::stop();
}

bool FadeTimer::on_timer()
{
    bool done;
    std::tie(this->value, done) = sin_fade(m_start_time, m_duration,
                                           this->start_value, this->target_value);

    if (m_func)
        m_func(this->value, done);

    this->iteration += 1;
    return !done;
}
