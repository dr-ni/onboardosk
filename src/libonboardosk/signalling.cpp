
#include "tools/container_helpers.h"
#include "signalling.h"
#include "onboardoskcallbacks.h"



SignalBase::SignalBase(const ContextBase* context,
                       SignalBase::ListenersChangedFunc listeners_changed_func) :
    ContextBase(*context),
    m_listeners_changed_func(listeners_changed_func)
{
}

SignalBase::~SignalBase()
{
    emit_finalize();
}

void SignalBase::connect_finalize(SignalBase::CallbackId callback_id,
                                  SignalBase::FinalizeCallback callback)
{
    m_finalize_listeners.emplace_back(callback_id, callback);
}

void SignalBase::disconnect_finalize(SignalBase::CallbackId callback_id)
{
    m_finalize_listeners.erase(std::remove_if(
                                   m_finalize_listeners.begin(), m_finalize_listeners.end(),
                                   [&](const FinalizeListenerEntry& e){return e.first == callback_id;}),
                               m_finalize_listeners.end());
}

void SignalBase::emit_finalize()
{
    for (auto& e : m_finalize_listeners)
        e.second(this);
}

void SignalBase::idle_flush()
{
    static auto on_idle_run = [](void* user_data)
    {
        //SignalBase* sig = static_cast<SignalBase*>(user_data);
        SignalBase* sig = reinterpret_cast<SignalBase*>(user_data);
        sig->flush_events();
        return 0;  // 0=one-shot
    };

    auto callbacks = get_global_callbacks();
    if (callbacks->idle_run)
        callbacks->idle_run(on_idle_run, this);
}



SignalConnections::~SignalConnections()
{
    disconnect_all();
}

    void SignalConnections::disconnect_all()
{
    for (SignalBase* signal : m_connections)
        disconnect(*signal);
}

void SignalConnections::disconnect(SignalBase& signal)
{
    signal.disconnect_finalize(this);
    signal.disconnect(this);
    remove_connection(&signal);
}

void SignalConnections::remove_connection(const SignalBase* signal)
{
    m_connections.erase(std::remove_if(
                            m_connections.begin(), m_connections.end(),
                            [&](const SignalBase* s){return s == signal;}),
                        m_connections.end());
}
