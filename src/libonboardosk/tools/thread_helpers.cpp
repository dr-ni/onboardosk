

#include <string.h>  // strerror
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>

#include "tools/string_helpers.h"

#include "thread_helpers.h"


void ThreadFd::Fd::clear()
{
    if (m_fd)
    {
        //printf("ThreadFd::Fd::clear(): closing fd %d\n", m_fd);
        ::close(m_fd);
        m_fd = 0;
    }
}

bool ThreadFd::stop()
{
    if (m_exit_w.get())
    {
        if (m_thread.joinable())
        {
            m_stopping = true;
            if (::write(m_exit_w.get(), "x", 1) != 1)
                return false;
            m_thread.join();
        }
    }
    return true;
}

void ThreadFd::init()
{
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1)
    {
        throw std::runtime_error(sstr()
                                 << "Failed to create pipe: "
                                 << strerror(errno) << " (" << errno << ")");
    }
    m_exit_r = pipe_fds[0];
    m_exit_w = pipe_fds[1];

    set_non_blocking(m_exit_r.get());
    set_non_blocking(m_exit_w.get());

    m_stopping = false;
}

void ThreadFd::set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool ThreadFd::wait_for_data()
{
    while (true)
    {
        int nfds = 0;
        fd_set readfds;
        timeval tv;
        FD_ZERO(&readfds);
        FD_SET(m_fd.get(), &readfds);
        nfds = std::max(nfds, m_fd.get() + 1);

        FD_SET(m_exit_r.get(), &readfds);
        nfds = std::max(nfds, m_exit_r.get() + 1);

        tv.tv_sec = 5;
        tv.tv_usec = 0;
        int ret = select(nfds, &readfds, NULL, NULL, &tv);
        if (ret > 0)
        {
            if (FD_ISSET(m_fd.get(), &readfds))
            {
                return true;  // data ready
            }
            else if (FD_ISSET(m_exit_r.get(), &readfds))
            {
                return false;  // exit thread
            }
        }
        else if (ret < 0)
        {
            throw std::runtime_error(sstr()
                                     << "select failed: "
                                     << strerror(errno) << " (" << errno << ")");
        }
    }

}

void ThreadFd::set_name(const std::string& name)
{
    auto handle = m_thread.native_handle();
    if (handle)
        pthread_setname_np(handle, name.c_str());
}

