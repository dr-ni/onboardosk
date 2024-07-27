#include "tools/logger.h"
#include "tools/point.h"

#include "inputeventsource.h"
#include "onboardoskevent.h"
#include "timer.h"
#include "keyboardview.h"
#include "layoutview.h"


InputEventSource::InputEventSource(const ContextBase& context,
                                   InputEventSourceSink* sink) :
    Super(context),
    m_sink(sink)
{
}

InputEventSource::~InputEventSource()
{
}

void InputEventSource::queue_event(ViewBase* view, OnboardOskEvent* event)
{
    Locker<InputEventSource> locker(this);

    event->toplevel = view;

    if (m_event_queue.empty())
    {
        idle_call<InputEventSource>(this, [](InputEventSource* this_) {
            this_->idle_process_event_queue();
            return 0;  // one-shot
        });
    }

    // Discard pending elements of the same type, e.g. to clear
    // motion event congestion (LP: #1210665).
    bool discard_pending = event->type == ONBOARD_OSK_EVENT_MOTION ||
                           event->type == ONBOARD_OSK_EVENT_TOUCH_UPDATE;
    if (discard_pending)
    {
        m_event_queue.erase(std::remove_if(m_event_queue.begin(),
                                           m_event_queue.end(),
                                           [&](const OnboardOskEvent& e)
            {
                return e.device_id == event->device_id &&
                       e.type == event->type;
            }), m_event_queue.end());
    }

    // Enqueue the event.
    m_event_queue.emplace_back(*event);
}

void InputEventSource::idle_process_event_queue()
{
    Locker<InputEventSource> locker(this);

    for (auto& event : m_event_queue)
        on_input_source_event(&event);
    m_event_queue.clear();
}

void InputEventSource::on_toplevel_event(ViewBase* toplevel, OnboardOskEvent* event)
{
    if (!toplevel)
        return;

    // grab in progress?
    if (m_pointer_grab_view)
    {
        send_event_to_view(m_pointer_grab_view, event);

        // Leave event means the grab wasn't actually realized yet.
        // Do it now, at the last moment. -> No need to initiate costly
        // grabs (e.g. showing stage filling actor) while dragging
        // or pressing buttons inside the view only.
        if (event->type == ONBOARD_OSK_EVENT_LEAVE)
        {
            realize_pointer_grab();
        }
        // release grab?
        else if (event->type == ONBOARD_OSK_EVENT_BUTTON_RELEASE ||
                 event->type == ONBOARD_OSK_EVENT_TOUCH_END) // || !is_leaf_view(m_pointer_grab_view))
        {
            ungrab_pointer();
        }
    }

    // leave event?
    else if (event->type == ONBOARD_OSK_EVENT_LEAVE &&
             m_last_entered_view)
    {
        m_connections.disconnect(m_last_entered_view->destroy_notify);
        send_event_to_view(m_last_entered_view, event);
        m_last_entered_view = nullptr;
    }

    // regular pointer event
    else
    {
        // keyboard invisible?
        auto keyboard_view = get_keyboard_view();
        if (!keyboard_view->is_visible())
            return;

        // Send the event to the final leaf view.
        Point point{event->x, event->y};
        ViewBase* view = toplevel->find_view_at_point(point);
        if (view)
        {
            // transform from toplevel view to leaf view coordinates
            Point root_point = toplevel->canvas_to_root(point);
            Point pt = view->root_to_canvas(root_point);
            event->x = pt.x;
            event->y = pt.y;

            // Remember which view we are currently entering.
            // The leave event doesn't come with a point
            // we can search for.
            if (event->type == ONBOARD_OSK_EVENT_ENTER)
            {
                m_last_entered_view = view;

                // Connect closure in case the view is
                // destroyed before the leave event.
                m_connections.connect(view->destroy_notify,
                                      [&]{m_last_entered_view = nullptr;});
            }

            // "grab" pointer. Toolkit-dependent code has to
            // make sure there is some actual grab in place, so
            // we can receive events from outside the view.
            if (event->type == ONBOARD_OSK_EVENT_BUTTON_PRESS ||
                event->type == ONBOARD_OSK_EVENT_TOUCH_BEGIN)
            {
                grab_pointer(view);
            }
            send_event_to_view(view, event);
        }
    }
}

void InputEventSource::send_event_to_view(ViewBase* view, OnboardOskEvent* event)
{
    LOG_EVENT << view
                << " type=" << event->type
                << " point=" << Point{event->x, event->y}
                << " root_point=" << Point{event->x_root, event->y_root}
                << " button=" << event->button
                << " state=" << event->state
                << " device_id=" << event->device_id
                << " source_id=" << event->source_device_id;

    view->on_event(event);
}

void InputEventSource::grab_pointer(ViewBase* view)
{
    LOG_EVENT << "grab begin";
    m_pointer_grab_view = view;
}

void InputEventSource::realize_pointer_grab()
{
    LOG_EVENT << "realize grab";
}

void InputEventSource::ungrab_pointer()
{
    LOG_EVENT << "grab end";
    m_pointer_grab_view = nullptr;
}


std::unique_ptr<InputEventSource> InputEventSource::make_native(
    const ContextBase& context,
    InputEventSourceSink* sink)
{
    return std::make_unique<InputEventSourceNative>(context, sink);
}

InputEventSourceNative::InputEventSourceNative(const ContextBase& context,
                                               InputEventSourceSink* sink) :
    Super(context, sink)
{
}

InputEventSourceNative::~InputEventSourceNative()
{
}

void InputEventSourceNative::on_input_source_event(OnboardOskEvent* event)
{
    ViewBase* toplevel = static_cast<ViewBase*>(event->toplevel);
    if (is_toplevel_view(toplevel))
    {
        on_toplevel_event(toplevel, event);
    }
    else
    {
        LOG_WARNING << "0x" << std::hex << toplevel << " is not a toplevel view"
                    << ", ignoring event " << event->type;
    }
}

void InputEventSourceNative::realize_pointer_grab()
{
    Super::realize_pointer_grab();

    if (m_pointer_grab_view &&
        !m_pointer_grab_realized)
    {
        ViewBase* toplevel =
                m_pointer_grab_view->find_toplevel_view_from_leaf();
        if (toplevel)
        {
            auto callbacks = get_global_callbacks();
            if (callbacks->view_grab_pointer)
                callbacks->view_grab_pointer(toplevel);

            m_pointer_grab_realized = true;
        }
    }
}

void InputEventSourceNative::ungrab_pointer()
{
    if (m_pointer_grab_view &&
        m_pointer_grab_realized)
    {
        ViewBase* toplevel =
                m_pointer_grab_view->find_toplevel_view_from_leaf();
        if (toplevel)
        {
            auto callbacks = get_global_callbacks();
            if (callbacks->view_ungrab_pointer)
                callbacks->view_ungrab_pointer(toplevel);
        }
        m_pointer_grab_realized = false;
    }

    Super::ungrab_pointer();
}

