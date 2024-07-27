#include <chrono>

#include "dwellprogresscontroller.h"

DwellProgressController::DwellProgressController()
{}

DwellProgressController::~DwellProgressController()
{}

bool DwellProgressController::is_dwelling()
{
    return m_dwell_start_time != TimePoint::min();
}

bool DwellProgressController::is_done()
{
    return Clock::now() > m_dwell_start_time + m_dwell_delay;
}

double DwellProgressController::get_done_ratio()
{
    using namespace std::chrono;
    double dt = duration_cast<duration<double>>(Clock::now() - m_dwell_start_time).count();
    return dt / duration_cast<duration<double>>(m_dwell_delay).count();
}

void DwellProgressController::start_dwelling(std::chrono::milliseconds delay)
{
    m_dwell_delay = delay;
    m_dwell_start_time = Clock::now();
}

void DwellProgressController::stop_dwelling()
{
    m_dwell_start_time = TimePoint::min();
}
