#ifndef GLOBALKEYLISTENER_H
#define GLOBALKEYLISTENER_H

#include <functional>
#include <vector>

#include "signalling.h"

struct _OnboardOskEvent;
typedef struct _OnboardOskEvent OnboardOskEvent;

class GlobalKeyListener : public ContextBase
{
    public:
        using Super = ContextBase;
        GlobalKeyListener(const ContextBase& context);
        virtual ~GlobalKeyListener();

        static std::unique_ptr<GlobalKeyListener> make(const ContextBase& context);

        void stop() {}
        std::string get_key_event_string(const OnboardOskEvent* event) {(void)event; return {};}

    public:
        DEFINE_SIGNAL(<const OnboardOskEvent*>, key_press, this);
        DEFINE_SIGNAL(<const OnboardOskEvent*>, key_release, this);
        DEFINE_SIGNAL(<>, devices_updated, this);
};

#endif // GLOBALKEYLISTENER_H
