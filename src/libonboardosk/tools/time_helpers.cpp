

#include <iomanip>      // put_time, setw
#include <iostream>

#include "time_helpers.h"

std::string format_time_stamp(std::chrono::system_clock::time_point tp)
{
    using namespace std::chrono;

    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    auto t = system_clock::to_time_t(tp);
    auto tm = *std::localtime(&t);

    std::ostringstream ss;
    ss << std::put_time(&tm, "%H:%M:%S");
    ss << '.';
    ss << std::setw(3) << std::setfill('0') << ms.count();

    return ss.str();
}

std::string format_time(const std::string& format, std::chrono::system_clock::time_point tp)
{
    using namespace std::chrono;

    auto t = system_clock::to_time_t(tp);
    auto tm = *std::localtime(&t);

    std::ostringstream ss;
    ss << std::put_time(&tm, format.c_str());
    return ss.str();
}

int64_t milliseconds_since_epoch()
{
    using namespace std::chrono;
    auto now = steady_clock::now();
    return duration_cast<milliseconds>(now.time_since_epoch()).count();
}

int64_t seconds_since_epoch(std::chrono::time_point<std::chrono::steady_clock> tp)
{
    using namespace std::chrono;
    return duration_cast<seconds>(tp.time_since_epoch()).count();
}

