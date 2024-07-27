#ifndef INPUTEVENTSOURCE_H
#define INPUTEVENTSOURCE_H

#include <deque>
#include <memory>
#include <mutex>

#include "onboardoskglobals.h"
#include "signalling.h"

struct _OnboardOskEvent;
typedef struct _OnboardOskEvent OnboardOskEvent;

class InputEventSourceSink
{
    public:
        virtual void on_input_source_event(OnboardOskEvent* ooe) = 0;
};


template<class T>
class Locker
{
    public:
        Locker(T* obj) :
            m_obj(obj)
        {
            obj->lock();
        }
        ~Locker()
        {
            m_obj->unlock();
        }

    private:
        T* m_obj;
};


class InputEventSource : public ContextBase
{
    public:
        using Super = ContextBase;
        InputEventSource(const ContextBase& context, InputEventSourceSink* sink);
        virtual ~InputEventSource();

        void queue_event(ViewBase* view, OnboardOskEvent* event);
        virtual bool is_native() {return false;}

        virtual void lock() = 0;
        virtual void unlock() = 0;

    public:
        static std::unique_ptr<InputEventSource>
            make_xinput(const ContextBase& context,
                        InputEventSourceSink* sink);
        static std::unique_ptr<InputEventSource>
            make_native(const ContextBase& context,
                        InputEventSourceSink* sink);

    protected:
        virtual void on_input_source_event(OnboardOskEvent* event) = 0;
        void on_toplevel_event(ViewBase* toplevel, OnboardOskEvent* event);

        virtual void grab_pointer(ViewBase* view);
        virtual void realize_pointer_grab();
        virtual void ungrab_pointer();

    private:
        void idle_process_event_queue();
        void dispatch_event(ViewBase* toplevel, OnboardOskEvent* event);
        void send_event_to_view(ViewBase* view, OnboardOskEvent* event);

    protected:
        InputEventSourceSink* m_sink{};
        std::deque<OnboardOskEvent> m_event_queue;

        ViewBase* m_last_entered_toplevel{};
        ViewBase* m_pointer_grab_view{};
        ViewBase* m_last_entered_view{};

        SignalConnections m_connections;
};


// Pass through events from e.g. GNOME Shell, but still
// queue and idle process them.
class InputEventSourceNative : public InputEventSource
{
    public:
        using Super = InputEventSource;
        InputEventSourceNative(const ContextBase& context,
                               InputEventSourceSink* sink);
        virtual ~InputEventSourceNative();

        virtual void lock() override {}
        virtual void unlock() override {}

        virtual bool is_native() override {return true;}

    protected:
        // Process event coming from the event queue.
        virtual void on_input_source_event(OnboardOskEvent* event) override;

        virtual void realize_pointer_grab() override;
        virtual void ungrab_pointer() override;

    private:
        bool m_pointer_grab_realized{false};
};

#endif // INPUTEVENTSOURCE_H
