#ifndef DWELLPROGRESSCONTROLLER_H
#define DWELLPROGRESSCONTROLLER_H

#include <chrono>
#include <memory>


class DwellProgressController
{
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        DwellProgressController();
        ~DwellProgressController();

        bool is_dwelling();
        bool is_done();
        double get_done_ratio();
        void start_dwelling(std::chrono::milliseconds delay
                            =std::chrono::milliseconds(4000));
        void stop_dwelling();

    public:
        double opacity = 1.0;

    private:
        // dwell time in seconds
        std::chrono::milliseconds m_dwell_delay;

        // time of dwell start
        TimePoint m_dwell_start_time{TimePoint::min()};
};

#endif // DWELLPROGRESSCONTROLLER_H
