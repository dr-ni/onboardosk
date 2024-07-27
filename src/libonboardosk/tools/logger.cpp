
#include <array>
#include <chrono>
#include <iomanip>      // setw
#include <iostream>     // cout
#include <memory>
#include <mutex>
#include <sstream>
#include <time.h>

#include <unistd.h>     // isatty
#include <sys/stat.h>   // stat

#include "tools/container_helpers.h"
#include "tools/string_helpers.h"

#include "../exception.h"
#include "logger.h"
#include "process_helpers.h"
#include "time_helpers.h"


// Singleton class providing ANSI terminal color codes
class TermColor
{
    public:
        enum SequenceId {
            BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE,
            BOLD, RESET,
            NONE       // last entry this time, so we don't have to translate for tput
        };

        // Return ANSI character sequence for the given sequence id,
        // e.g. color index.
        static std::string get(SequenceId seq_id)
        {
            switch (seq_id)
            {
                case RESET: return "\x1b[0m";
                case BOLD: return "\x1b[1m";
                case NONE: return "";
                default:
                    char sequence[] = "\x1b[30m";
                    sequence[3] = static_cast<char>('0' + seq_id);
                    return sequence;
            }
        }
};

LogStream::LogStream(const Logger* logger, LogLevel level, const char* const src_location)
{
    if (logger->can_log(level))
    {
        m_logger = logger;

        std::string location = extract_src_location(src_location);
        m_logger->write_prefix(*this, level, location);
    }
}

LogStream::~LogStream()
{
    if (m_logger)
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> guard(mutex);
        m_logger->output(this->str());
    }
}

void Logger::set_level(LogLevel level)
{
    m_level = level;
}

std::shared_ptr<Logger> Logger::get_default() {
    static auto lg = std::make_shared<LoggerConsole>();
    return lg;
}

LoggerConsole::LoggerConsole()
{
    // running from an interactive terminal?
    if (isatty(fileno(stdout)))
    {
        m_use_colors = true;
    }
    else
    {
        // is stdout a FIFO?
        struct stat stdin_stats;
        int result = fstat(fileno(stdin), &stdin_stats);
        if (result == 0)
        {
            #if 0
            using namespace std;
            cout << "result=" << result << " ";
            cout << S_ISCHR(stdin_stats.st_mode);
            cout << S_ISREG(stdin_stats.st_mode);
            cout << S_ISFIFO(stdin_stats.st_mode);
            cout << " m_use_colors=" << m_use_colors;
            cout << endl;
            #endif

            struct stat stdout_stats;
            result = fstat(fileno(stdout), &stdout_stats);
            if (result == 0)
            {
                #if 0
                using namespace std;
                cout << "result=" << result << " ";
                cout << S_ISCHR(stdout_stats.st_mode);
                cout << S_ISREG(stdout_stats.st_mode);
                cout << S_ISFIFO(stdout_stats.st_mode);
                cout << " m_use_colors=" << m_use_colors;
                cout << endl;
                #endif

                // Input and output are FIFOs?
                // Then we're perhaps in qtcreator - turn colors on
                if (S_ISFIFO(stdin_stats.st_mode) &&
                    S_ISFIFO(stdout_stats.st_mode))
                    m_use_colors = true;
            }
        }
    }
}

void LoggerConsole::write_prefix(std::ostream& stream, LogLevel level, const std::string& src_location) const
{
    auto now = std::chrono::system_clock::now();
    stream << format_time_stamp(now) << " ";

    if (m_use_colors)
        stream << TermColor::get(static_cast<TermColor::SequenceId>(get_term_color_id(level)));
    stream << std::setw(7) << std::left << to_string(level);
    if (m_use_colors)
        stream << TermColor::get(TermColor::RESET);

    if (m_use_colors)   // no bolding in qtcreator
        stream << TermColor::get(TermColor::BOLD);
    stream << " " << std::setw(33) << std::left << (src_location + ": ");
    if (m_use_colors)
        stream << TermColor::get(TermColor::RESET);
}

void LoggerConsole::output(const std::string& s) const
{
    std::cout << s << std::endl;
}

int LoggerConsole::get_term_color_id(LogLevel level) const
{
    switch (level)
    {
        case LogLevel::CRITICAL: return TermColor::RED;
        case LogLevel::ERROR:    return TermColor::RED;
        case LogLevel::WARNING:  return TermColor::YELLOW;
        case LogLevel::INFO:     return TermColor::GREEN;
        case LogLevel::DEBUG:    return TermColor::BLUE;
        case LogLevel::TRACE:    return TermColor::BLUE;
        case LogLevel::ATSPI:    return TermColor::CYAN;
        case LogLevel::EVENT:    return TermColor::MAGENTA;
        default:
            return TermColor::NONE;
    }
}

std::string to_string(LogLevel e)
{
    static std::map<LogLevel, std::string> m = {
        {LogLevel::NONE, "NONE"},
        {LogLevel::CRITICAL, "CRITICAL"},
        {LogLevel::ERROR, "ERROR"},
        {LogLevel::WARNING, "WARNING"},
        {LogLevel::INFO, "INFO"},
        {LogLevel::DEBUG, "DEBUG"},
        {LogLevel::ATSPI, "ATSPI"},
        {LogLevel::TRACE, "TRACE"},
        {LogLevel::EVENT, "EVENT"},
    };
    return get_value(m, e);
}

// cut out class::function from src_location
std::string extract_src_location(const char* const src_location)
{
    auto fields = re_search(src_location, R"(((?:\w+::)*\w+)\()");
    return fields.empty() ? "" : fields[0];
}

