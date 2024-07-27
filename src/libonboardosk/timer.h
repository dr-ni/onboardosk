
#ifndef TIMER_H
#define TIMER_H

#include "chrono"
#include "functional"

#include "onboardoskglobals.h"


class Timer : public ContextBase
{
    public:
        using Callback = std::function<bool()>;

        Timer(const ContextBase& context);
        Timer(const Timer& other) = delete;
        Timer(const Timer&& other) noexcept = delete;
        Timer& operator=(const Timer& other) = delete;
        Timer& operator=(Timer&& other) noexcept = delete;
        virtual ~Timer();

        void start(double seconds, const Callback& func={});
        void start(std::chrono::milliseconds delay, Callback func={});
        void stop();

        // Run callback one last time and stop.
        void finish();

        bool is_running();

        virtual bool on_timer()
        {return false;}

        bool on_timer_event();

    protected:
        Callback m_callback;

    private:
        void do_stop();

    private:
        int m_timer_id{0};
        bool m_running{false};

    public:
        u_int32_t cookie;
};


using IdleCallback = int (*)(void*);
void _idle_call_impl(ContextBase* context, IdleCallback func, void* user_data);

template<class T>
using TIdleCallback = int (*)(T*);
template<class T>
void idle_call( T* this_, TIdleCallback<T> func)
{
    _idle_call_impl(this_, reinterpret_cast<IdleCallback>(func), this_);
}

#endif // TIMER_H
