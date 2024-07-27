#include "callonce.h"
#include "timer.h"


CallOnceBase::CallOnceBase(const ContextBase& context) :
    Super(context),
    m_timer(std::make_unique<Timer>(context))
{}

void CallOnceBase::start_timer(const std::chrono::milliseconds delay)
{
    m_timer->start(delay, [this](){return on_timer();});
}

void CallOnceBase::stop_timer()
{
    m_timer->stop();
}

bool CallOnceBase::is_timer_running()
{
    return m_timer->is_running();
}

bool CallOnceBase::on_timer()
{
    flush_calls();
    stop();
    return false;
}
