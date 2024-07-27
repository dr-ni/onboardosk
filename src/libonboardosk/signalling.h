#ifndef EVENTSOURCE_H
#define EVENTSOURCE_H

#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <iostream>

#include "onboardoskglobals.h"

//#define DEBUG_SIGNALLING 1

class SignalBase : public ContextBase
{
    public:
        using CallbackId = void*;
        using ListenersChangedFunc = std::function<void()>;
        using FinalizeCallback = std::function<void(const SignalBase*)>;
        using FinalizeListenerEntry = std::pair<CallbackId, FinalizeCallback>;

        SignalBase(const ContextBase* context,
                   ListenersChangedFunc listeners_changed_func={});

        virtual ~SignalBase();

        virtual void disconnect(CallbackId callback_id) = 0;
        virtual bool has_listeners() = 0;

        void connect_finalize(CallbackId callback_id, FinalizeCallback callback);
        void disconnect_finalize(CallbackId callback_id);
        void emit_finalize();


    protected:
        void idle_flush();
        virtual void flush_events() = 0;

    protected:
        std::string m_name;
        ListenersChangedFunc m_listeners_changed_func{};
        std::vector<FinalizeListenerEntry> m_finalize_listeners;

        #ifdef DEBUG_SIGNALLING
        int m_count{0};
        #endif
};


template <typename ...TArgs>
class Signal : public SignalBase
{
    public:
        using Callback = std::function<void(TArgs...)>;
        using ListenerEntry = std::pair<CallbackId, Callback>;
        using SignalParams = std::tuple<TArgs...>;

/*
        Signal<TArgs...>(const ContextBase* context,
                       ListenersChangedFunc listeners_changed_func={}) :
            SignalBase(context, listeners_changed_func)
        {}
*/
        Signal<TArgs...>(const char* name, const ContextBase* context,
                       ListenersChangedFunc listeners_changed_func={}) :
            SignalBase(context, listeners_changed_func)
        {
            m_name = name;
        }

        void connect(CallbackId callback_id, Callback callback)
        {
            m_listeners.emplace_back(callback_id, callback);
            #if 0
            std::cout << "connect " << callback_id << std::endl;
            for (auto& l : m_listeners)
                std::cout << l.first << " " << callback_id << std::endl;
            #endif
            if (m_listeners_changed_func)
                m_listeners_changed_func();
        }

        void disconnect(CallbackId callback_id)
        {
            #if 0
            std::cout << "disconnect1 " << callback_id << std::endl;
            for (auto& l : m_listeners)
                std::cout << l.first << " " << callback_id << std::endl;
            #endif
            m_listeners.erase(std::remove_if(
                m_listeners.begin(), m_listeners.end(),
                [&](const ListenerEntry& e){return e.first == callback_id;}),
                m_listeners.end());
            #if 0
            std::cout << "disconnect2 " << callback_id << std::endl;
            for (auto& l : m_listeners)
                std::cout << l.first << " " << callback_id << std::endl;
            #endif
            if (m_listeners_changed_func)
                m_listeners_changed_func();
        }

        virtual bool has_listeners() override
        {
            return !m_listeners.empty();
        }

        void emit(const TArgs&... params)
        {
            #ifdef DEBUG_SIGNALLING
            std::cout << m_name << ".emit() " << std::endl;
            #endif
            for (auto& e : m_listeners)
                e.second(params...);
        }

        template<std::size_t... Is>
        void emit_with_tuple(const std::tuple<TArgs...>& tuple,
                             std::index_sequence<Is...>)
        {
            emit(std::get<Is>(tuple)...);
        }

        void emit_async(const TArgs&... params)
        {
            #ifdef DEBUG_SIGNALLING
            std::cout << m_name << ".emit_async() " << std::endl;
            #endif
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_event_queue.emplace_back(params...);
            if (m_event_queue.size() == 1)
                idle_flush();
        }

        virtual void flush_events()
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            #ifdef DEBUG_SIGNALLING
            std::cout << m_name << ".flush_events(): " << "count=" << m_count << std::endl;
            m_count++;
            #endif

            //for (auto& params : m_event_queue)
            size_t n = m_event_queue.size();
            for (size_t i=0; i<n; i++)
            {
                auto& params = m_event_queue[i];
                emit_with_tuple(params,
                                std::index_sequence_for<TArgs...>());
            }
            clear_events();

            #ifdef DEBUG_SIGNALLING
            m_count--;
            #endif
        }

        void clear_events()
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_event_queue.clear();
        }

    private:
        std::recursive_mutex m_mutex;
        std::vector<ListenerEntry> m_listeners;
        std::vector<SignalParams> m_event_queue;  // for optional async delivery
};


// Auto-disconnect from signals on destruction
class SignalConnections
{
    public:
        ~SignalConnections();

        template<class S, typename F>
        void connect(S& signal, const F& func)
        {
            signal.connect(this, func);
            signal.connect_finalize(this,
                [this](const SignalBase* s){remove_connection(s);});
            m_connections.emplace_back(&signal);
        }

        void disconnect_all();
        void disconnect(SignalBase& signal);

    private:
        void remove_connection(const SignalBase* signal);

    private:
        std::vector<SignalBase*> m_connections;
};

#define DEFINE_SIGNAL(template_params, name, ...) \
              Signal template_params name{#name, __VA_ARGS__}

#endif // EVENTSOURCE_H


