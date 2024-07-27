
#include <iomanip>

#include "tools/logger.h"

#include "onboardoskcallbacks.h"
#include "timer.h"


static const uint32_t s_cookie{0xa6a18ced};
static ContextBase s_last_context{nullptr};
static Logger* logger() {return s_last_context.logger();}

static int on_all_timers(void* user_data)
{
    Timer* timer = static_cast<Timer*>(user_data);
    if (timer->cookie == s_cookie)
        return static_cast<int>(timer->on_timer_event());

    LOG_ERROR << "bad timer event: timer cookie "
              << "0x" << std::setfill('0') << std::setw(2) << std::hex
              << timer->cookie << " invalid";
    return 0;
}

bool Timer::is_running()
{
    return m_running;
}

Timer::Timer(const ContextBase& context) :
    ContextBase(context),
    cookie(s_cookie)
{
    s_last_context = context;
}

Timer::~Timer()
{
    cookie = 0;
    stop();
}

void Timer::start(double seconds, const Callback& func)
{
    using namespace std::chrono;
    start(duration_cast<milliseconds>(duration<double>(seconds)),
          func);
}

void Timer::start(std::chrono::milliseconds delay,
                  Timer::Callback func)
{
    stop();

    m_running = true;
    m_callback = func;
    int milliseconds = static_cast<int>(delay.count());
    auto callbacks = get_global_callbacks();
    if (callbacks->start_timer)
        m_timer_id = callbacks->start_timer(milliseconds,
                                            on_all_timers,
                                            static_cast<void*>(this));
}

void Timer::stop()
{
    do_stop();
    m_callback = {};
}

void Timer::do_stop()
{
    if (m_running)
    {
        auto callbacks = get_global_callbacks();
        if (callbacks->stop_timer)
            callbacks->stop_timer(m_timer_id);
        m_timer_id = 0;
        m_running = false;
    }
}

void Timer::finish()
{
    if (is_running())
    {
        do_stop();
        m_callback();
    }
}

bool Timer::on_timer_event()
{
    // LOG_DEBUG << this;
    if (m_running)
    {
        if (m_callback)
        {
            if (m_callback())
                return true;
        }
        else if (on_timer())
        {
            return true;
        }
    }

    stop();
    return false;
}


void _idle_call_impl(ContextBase* context, IdleCallback func, void* user_data)
{
    auto callbacks = context->get_global_callbacks();
    if (callbacks->idle_run)
        callbacks->idle_run(func, user_data);
}
