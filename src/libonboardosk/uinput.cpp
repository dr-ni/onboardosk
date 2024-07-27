/*
 * Copyright © 2012, 2016 marmuta <marmvta@gmail.com>
 *
 * This file is part of Onboard.
 *
 * Onboard is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Onboard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <cstring>

#include <linux/input.h>
#include <linux/uinput.h>

#include "exception.h"
#include "uinput.h"


class UInputSingleton
{
    public:
        void open(const std::string& device_name)
        {
            if (m_use_count == 0)
                do_open(device_name);
            m_use_count++;
        }

        void close()
        {
            m_use_count--;
            if (m_use_count == 0)
                do_close();
        }

        void send_key_event(uint16_t keycode, bool press)
        {
            if (is_open())
                do_send_key_event(keycode, press);
        }

    private:
        void do_open (const std::string& device_name)
        {
            int fd;
            int i;
            struct uinput_user_dev* uidev = &m_uidev;

            fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK);
            if(fd < 0)
                throw VirtKeyException(
                        "failed to open /dev/uinput; "
                        "write permission required.");

            if(::ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
                throw VirtKeyException(
                        "ioctl UI_SET_EVBIT failed");

            for (i=0; i < 256; i++)
            {
                if(::ioctl(fd, UI_SET_KEYBIT, i) < 0)
                    throw VirtKeyException(
                            "ioctl UI_SET_KEYBIT failed");
            }

            // init uinput device
            std::memset(uidev, 0, sizeof(*uidev));
            std::snprintf(uidev->name, UINPUT_MAX_NAME_SIZE, "%s", device_name.c_str());
            uidev->id.bustype = BUS_USB;
            uidev->id.vendor  = 0x1;
            uidev->id.product = 0x1;
            uidev->id.version = 1;

            if(::write(fd, uidev, sizeof(*uidev)) < 0)
                throw VirtKeyException(
                        "error writing uinput device struct");

            if(::ioctl(fd, UI_DEV_CREATE) < 0)
                throw VirtKeyException(
                    "ioctl UI_DEV_CREATE failed");

            m_fd = fd;
        }

        void do_close()
        {
            if (m_fd)
            {
                ::ioctl(m_fd, UI_DEV_DESTROY);
                ::close(m_fd);
                m_fd = 0;
            }
        }

        bool is_open()
        {
            return m_fd > 0;
        }

        void do_send_key_event(uint16_t keycode, bool press)
        {
            int fd = m_fd;
            uint16_t code = keycode - 8;
            struct input_event ev;

            //printf("send_key_event %d %d \n", keycode, press);
            memset(&ev, 0, sizeof(ev));
            ev.type = EV_KEY;
            ev.code = code;
            //ev.code = KEY_A;
            ev.value = press;
            if(write(fd, &ev, sizeof(ev)) < 0)
                throw VirtKeyException(
                    "writing key event failed");

            ev.type = EV_SYN;
            ev.code = 0;
            ev.value = 0;
            if(write(fd, &ev, sizeof(ev)) < 0)
                throw VirtKeyException(
                    "writing syn failed");
        }

    public:
        int m_use_count{0};
        int m_fd{0};
        struct uinput_user_dev m_uidev;
};

// one instance for all onboard instances (of the same process)
static UInputSingleton uinput_singleton;

UInput::UInput(const std::string& device_name)
{
    uinput_singleton.open(device_name);
}


UInput::~UInput()
{
    uinput_singleton.close();
}

void UInput::send_key_event(KeyCode keycode, bool press)
{
    uinput_singleton.send_key_event(keycode, press);
}

