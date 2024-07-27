#ifndef THREADHELPERS_H
#define THREADHELPERS_H

#include <thread>


class ThreadFd
{
    private:
        // exception/safe file descriptor wrapper
        class Fd
        {
            public:
                Fd(int fd=0) :
                    m_fd(fd) {}
                Fd(Fd& other) = delete;   // can only move
                Fd(Fd&& other) noexcept :
                    m_fd(other.m_fd)
                {
                    other.m_fd = 0;
                }
                ~Fd() {clear();}

                Fd& operator=(int fd)
                {
                    clear();
                    m_fd = fd;
                    return *this;
                }
                Fd& operator=(Fd&) = delete;  // can only move
                Fd& operator=(Fd&& other) noexcept
                {
                    clear();
                    m_fd = other.m_fd;
                    other.m_fd = 0;
                    return *this;
                }

                //operator int()
                int get()
                {
                    return m_fd;
                }

                void clear();

            private:
                int m_fd;
        };

    public:
        ThreadFd() = default;
        ~ThreadFd() = default;

        // throws std::runtime_error
        template<typename F>
        void start(int poll_fd, const F& thread_func)
        {
            init();
            m_fd = poll_fd;
            std::thread thread(thread_func);
            m_thread = std::move(thread);
        }

        bool stop();
        bool is_stop_requested() {return m_stopping;}

        // Call this from the thread function.
        // throws std::runtime_error
        bool wait_for_data();
        void set_name(const std::string& name);


    private:
        void init();
        void set_non_blocking(int fd);

    protected:
        Fd m_fd;

    private:
        std::thread m_thread;
        Fd m_exit_r;
        Fd m_exit_w;
        volatile bool m_stopping{false};
};

#endif // THREADHELPERS_H
