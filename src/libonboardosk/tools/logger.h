#ifndef LOGGER_H
#define LOGGER_H

#include <memory>
#include <sstream>

#include "loggerdecls.h"


class Logger
{
    public:
        static std::shared_ptr<Logger> get_default();

        virtual ~Logger() = default;

        void set_level(LogLevel level);
        bool can_log(LogLevel level) const
        { return level <= m_level; }

        virtual void write_prefix(std::ostream& stream, LogLevel level, const std::string& src_location) const = 0;
        virtual void output(const std::string& s) const = 0;

    private:
        LogLevel m_level;
};


class LoggerConsole : public Logger
{
    public:
        LoggerConsole();

        virtual void write_prefix(std::ostream& stream, LogLevel level, const std::string& src_location) const override;
        virtual void output(const std::string& s) const override;

    private:
        int get_term_color_id(LogLevel level) const;

    private:
        bool m_use_colors{false};
};

class LogStream : public std::stringstream
{
    public:
        LogStream(const Logger* logger, LogLevel level, const char* const src_location);
        ~LogStream();
    private:
        mutable const Logger* m_logger{};
};

//#define LOG_IF(level, on) if (on) ThreadSafeLogStream(level)
#define LOG_SRC_LOCATION __PRETTY_FUNCTION__

#define LOG_ERROR   LogStream(logger(), LogLevel::ERROR, LOG_SRC_LOCATION)
#define LOG_WARNING LogStream(logger(), LogLevel::WARNING, LOG_SRC_LOCATION)
#define LOG_INFO    LogStream(logger(), LogLevel::INFO, LOG_SRC_LOCATION)
#define LOG_DEBUG   LogStream(logger(), LogLevel::DEBUG, LOG_SRC_LOCATION)
#define LOG_TRACE   LogStream(logger(), LogLevel::TRACE, LOG_SRC_LOCATION)
#define LOG_ATSPI   LogStream(logger(), LogLevel::ATSPI, LOG_SRC_LOCATION)
#define LOG_EVENT   LogStream(logger(), LogLevel::EVENT, LOG_SRC_LOCATION)


// get class::function from src_location
std::string extract_src_location(const char* const src_location);

#endif // LOGGER_H
