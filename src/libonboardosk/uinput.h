#ifndef UINPUT_H
#define UINPUT_H

#include <memory>

#include "tools/keydecls.h"

class UInput
{
    public:
        UInput(const std::string& device_name);
        ~UInput();

        void send_key_event(KeyCode keycode, bool press);
};

#endif // UINPUT_H
