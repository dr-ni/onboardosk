
#include <fstream>
#include <thread>

#include <string.h>  // strerror
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>


#include "tools/file_helpers.h"
#include "tools/logger.h"
#include "tools/container_helpers.h"

#include "configuration.h"
#include "globalkeylistener.h"
#include "hardwaresensortracker.h"
#include "onboardoskevent.h"

using std::string;
using std::vector;


// Listen to events aggregated by acpid.
class AcpidListener : ContextBase
{
    public:
        AcpidListener(HardwareSensorTracker* sensor_tracker) :
            ContextBase(*sensor_tracker),
            m_sensor_tracker(sensor_tracker)
        {
            start();
        }

        ~AcpidListener()
        {
            cleanup();
        }

        void start()
        {
            try
            {
                m_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
                if (m_socket_fd < 0)
                {
                    LOG_WARNING << "Failed to create acpid socket,"
                                << " SW_TABLET_MODE detection disabled:"
                                << " " << strerror(errno) << " (" << errno << ")";
                    throw false;
                }

                string fn = "/var/run/acpid.socket";
                struct sockaddr_un remote;
                remote.sun_family = AF_UNIX;
                strcpy(remote.sun_path, fn.c_str());
                size_t len = strlen(remote.sun_path) + sizeof(remote.sun_family);

                if (connect(m_socket_fd, reinterpret_cast<struct sockaddr *>(&remote), len) < 0)
                {
                    LOG_WARNING << "Failed to connect to acpid at " << fn
                                << ", SW_TABLET_MODE detection disabled:"
                                << " " << strerror(errno) << " (" << errno << ")";
                    throw false;
                }

                if (pipe(m_pipe_fds) == -1)
                {
                    LOG_WARNING << "Failed to create pipe"
                                << ", SW_TABLET_MODE detection disabled:"
                                << " " << strerror(errno) << " (" << errno << ")";
                    throw false;
                }

                set_non_blocking(m_socket_fd);
                set_non_blocking(m_exit_r);
                set_non_blocking(m_exit_w);

                std::thread thread([&](){on_thread();});
                m_thread = std::move(thread);
                auto handle = m_thread.native_handle();
                pthread_setname_np(handle, "AcpidListener");
            }
            catch (...)
            {
                cleanup();
            }
        }

        void stop()
        {
            if (m_exit_w)
            {
                if (m_thread.joinable())
                {
                    if (::write(m_exit_w, "x", 1) != 1)
                    {
                        LOG_ERROR << "error writing to pipe: can't end thread";
                        return;
                    }
                    m_thread.join();
                    LOG_INFO << "AcpidListener: thread stopped, joinable="
                             << repr(m_thread.joinable());
                }
            }
        }

    private:
        void cleanup()
        {
            if (m_socket_fd)
                ::close(m_socket_fd);
            m_socket_fd = 0;

            if (m_exit_r)
                ::close(m_exit_r);
            m_exit_r = 0;

            if (m_exit_w)
                ::close(m_exit_w);
            m_exit_w = 0;
        }

        void set_non_blocking(int fd)
        {
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        void on_thread()
        {
            LOG_INFO << "AcpidListener: thread start";

            while (true)
            {
                int nfds = 0;
                fd_set readfds;
                timeval tv;
                FD_ZERO(&readfds);
                FD_SET(m_socket_fd, &readfds);
                nfds = std::max(nfds, m_socket_fd + 1);

                FD_SET(m_exit_r, &readfds);
                nfds = std::max(nfds, m_exit_r + 1);

                tv.tv_sec = 1;
                tv.tv_usec = 0;

                int ret = select(nfds, &readfds, NULL, NULL, &tv);
                if (ret > 0)
                {
                    string data;
                    if (FD_ISSET(m_socket_fd, &readfds))
                    {
                        constexpr const size_t nmax = 4096;
                        char str[nmax+1];
                        ssize_t n;
                        if ((n = recv(m_socket_fd, str, nmax, 0)) > 0)
                        {
                             str[n] = '\0';
                             data = str;
                        }
                        else
                        {
                            LOG_ERROR << "recv failed: ret=" << ret
                                      << " " << strerror(errno) << " (" << errno << ")";
                            break;
                        }
                    }
                    else
                    if (FD_ISSET(m_exit_r, &readfds))
                    {
                        break;
                    }

                    for (auto event : split(data, '\n'))
                    {
                        LOG_INFO << "AcpidListener: ACPI event: " << repr(event);

                        if (event == "button/power PBTN 00000080 00000000")
                        {
                            LOG_INFO << "AcpidListener: power button";
                            m_sensor_tracker->power_button_pressed.emit_async(true);
                        }

                        else
                        if (event == "video/tabletmode TBLT 0000008A 00000001")
                        {
                            LOG_INFO << "AcpidListener: tablet_mode true";
                            m_sensor_tracker->set_tablet_mode(true);
                        }

                        else
                        if (event == "video/tabletmode TBLT 0000008A 00000000")
                        {
                            LOG_INFO << "AcpidListener: tablet_mode false";
                            m_sensor_tracker->set_tablet_mode(false);
                        }
                    }
                }
            }

            cleanup();

            LOG_INFO << "AcpidListener: thread exit";
        }

    private:
        HardwareSensorTracker* m_sensor_tracker{};
        std::thread m_thread;
        int m_socket_fd{0};

        int m_pipe_fds[2];
        int& m_exit_r{m_pipe_fds[0]};
        int& m_exit_w{m_pipe_fds[1]};
};

std::unique_ptr<HardwareSensorTracker> HardwareSensorTracker::make(const ContextBase& context)
{
    return std::make_unique<HardwareSensorTracker>(context);
}

// Filenames && search patterns to determine convertible tablet-mode.
// Only some of the drivers that send SW_TABLET_MODE evdev events
// also provide sysfs attributes to read the current tablet-mode state.
typedef vector<std::pair<string, string>> StateFileEntries;
static const StateFileEntries tablet_mode_state_files =
{
    // classmate-laptop.c
    // nothing

    // fujitsu-tablet.c
    // nothing

    // hp-wmi.c
    {"/sys/devices/platform/hp-wmi/tablet",
     "1"},

    // ideapad-laptop.c, only debugfs which requires root
    // ("/sys/kernel/debug/ideapad/status",
    // re.compile("Touchpad status:Off(0)")),

    // thinkpad_acpi.c
    {"/sys/devices/platform/thinkpad_acpi/hotkey_tablet_mode",
     "1"},

    // xo15-ebook.c
    // nothing
};

HardwareSensorTracker::HardwareSensorTracker(const ContextBase& context) :
    Super(context)
{
}

HardwareSensorTracker::~HardwareSensorTracker()
{
    register_listeners(false);
}

void HardwareSensorTracker::on_listeners_changed()
{
    update_sensor_sources();
    if (!has_listeners())
        LOG_INFO << "all listeners disconnected";
}

bool HardwareSensorTracker::has_listeners()
{
    return tablet_mode_changed.has_listeners() ||
           power_button_pressed.has_listeners();
}

void HardwareSensorTracker::update_sensor_sources()
{
    bool register_ = has_listeners();
    register_acpid_listeners(register_);

    register_ = tablet_mode_changed.has_listeners();
    register_hotkey_listeners(register_);
}

void HardwareSensorTracker::register_listeners(bool register_)
{
    register_acpid_listeners(register_);
    register_hotkey_listeners(register_);
}

void HardwareSensorTracker::register_acpid_listeners(bool register_)
{
    if ((m_acpid_listener != nullptr) != register_)
    {
        if (register_)
        {
            m_acpid_listener = std::make_unique<AcpidListener>(this);
        }
        else
        {
            m_acpid_listener->stop();
            m_acpid_listener = nullptr;
        }
    }
}

void HardwareSensorTracker::register_hotkey_listeners(bool register_)
{
    int enter_key = config()->auto_show->tablet_mode_enter_key;
    int leave_key = config()->auto_show->tablet_mode_leave_key;
    if (!enter_key && !leave_key)
    {
        register_ = false;
    }

    if (register_)
    {
        if (!m_key_listener)
        {
            m_key_listener = get_global_key_listener();
            if (m_key_listener)
            {
                m_key_listener->key_press.connect(this, [&](const OnboardOskEvent* event) {on_key_press(event);});
            }
        }
    }
    else
    {
        if (m_key_listener)
        {
            m_key_listener->key_press.disconnect(this);
        }
        m_key_listener = nullptr;
    }
}

void HardwareSensorTracker::set_tablet_mode(bool activ)
{
    m_tablet_mode = activ;
    tablet_mode_changed.emit_async(activ);
}

Noneable<bool> HardwareSensorTracker::get_tablet_mode()
{
    Noneable<bool> state = get_tablet_mode_state();
    if (state.is_none())
        return m_tablet_mode;
    return state;
}

Noneable<bool> HardwareSensorTracker::get_tablet_mode_state()
{
    string custom_state_file = config()->auto_show->tablet_mode_state_file;
    string custom_pattern = config()->auto_show->tablet_mode_state_file_pattern;

    StateFileEntries custom_entries;
    const StateFileEntries* candidates = &custom_entries;
    if (!custom_state_file.empty())
        custom_entries.emplace_back(custom_state_file, custom_pattern);
    else
        candidates = &tablet_mode_state_files;

    for (auto& e : *candidates)
    {
        string fn = e.first;
        string pattern = e.second;
        string content;
        string error;
        if (!read_file(fn, content, 4096, error))
            LOG_DEBUG << "Reading " << repr(fn) << "failed: " << error;

        if (!content.empty())
        {
            bool active = !pattern.empty() && content.find(pattern) != string::npos;
            LOG_INFO << "read tablet_mode=" << repr(active)
                     << " from " << repr(fn)
                     << " with pattern " << repr(pattern);
            return active;
        }
    }

    return {};
}

void HardwareSensorTracker::on_key_press(const OnboardOskEvent* event)
{
    int enter_keycode = config()->auto_show->tablet_mode_enter_key;
    int leave_keycode = config()->auto_show->tablet_mode_leave_key;

    if (logger()->can_log(LogLevel::INFO))
    {
        string s = m_key_listener->get_key_event_string(event);
        LOG_INFO << "_on_key_press(): " << s
                 << ", enter_keycode=" << enter_keycode
                 << " leave_keycode=" << leave_keycode;
    }

    if (enter_keycode && event->keycode == enter_keycode)
    {
        LOG_INFO << "hotkey tablet_mode_enter_key " << enter_keycode
                 << " received";
        set_tablet_mode(true);
    }

    if (leave_keycode && event->keycode == leave_keycode)
    {
        LOG_INFO << "hotkey tablet_mode_leave_key " << leave_keycode
                 << " received";
        set_tablet_mode(false);
    }
}


