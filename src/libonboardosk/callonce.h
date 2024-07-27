#ifndef CALLONCE_H
#define CALLONCE_H

#include <chrono>
#include <functional>
#include <memory>

#include "onboardoskglobals.h"


class Timer;

// Call each <callback> during <delay> only once
// Useful to reduce a storm of config notifications
// to just a single (or a few) update(s) of onboards state.
class CallOnceBase : public ContextBase
{
    public:
        using Super = ContextBase;
        CallOnceBase(const ContextBase& context);

    protected:
        virtual void flush_calls() = 0;
        virtual void stop() = 0;

        void start_timer(const std::chrono::milliseconds delay);
        void stop_timer();
        bool is_timer_running();

        bool on_timer();

    private:
        std::unique_ptr<Timer> m_timer;
};


template <typename ...TArgs>
class CallOnce : public CallOnceBase
{
    public:
        using Super = CallOnceBase;
        using Callback = std::function<void(TArgs...)>;
        using Params = std::tuple<TArgs...>;

        CallOnce(const ContextBase& context,
                 const Callback& callback,
                 std::chrono::milliseconds delay=std::chrono::milliseconds{20},
                 bool delay_forever=false) :
            Super(context),
            m_callback(callback),
            m_delay(delay),
            m_delay_forever(delay_forever)
        {
        }

        bool is_running()
        {
            return is_timer_running();
        }

        void enqueue(const TArgs&... params)
        {
            bool empty_before = m_params_queue.empty();
            m_params_queue.emplace_back(params...);

            if (m_delay_forever && is_timer_running())
            {
                stop_timer();
            }

            if (!is_timer_running() &&
                empty_before)
            {
                start_timer(m_delay);
            }
        }

        virtual void flush_calls()
        {
            size_t n = m_params_queue.size();
            for (size_t i=0; i<n; i++)
            {
                auto& params = m_params_queue[i];
                call_with_tuple(params,
                                std::index_sequence_for<TArgs...>());
            }
            clear();
        }

        template<std::size_t... Is>
        void call_with_tuple(const Params& tuple,
                             std::index_sequence<Is...>)
        {
            call(std::get<Is>(tuple)...);
        }

        void call(const TArgs&... params)
        {
            m_callback(params...);
        }

        void clear()
        {
            m_params_queue.clear();
        }

        void stop()
        {
            stop_timer();
            clear();
        }

    private:
        Callback m_callback;
        std::chrono::milliseconds m_delay;
        bool m_delay_forever{false};
        std::vector<Params> m_params_queue;
};

#endif // CALLONCE_H
