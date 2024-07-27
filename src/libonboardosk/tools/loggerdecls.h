#ifndef LOGGERDECLS_H
#define LOGGERDECLS_H

#include <string>

enum class LogLevel
{
    NONE,   // no logging
    CRITICAL,
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    ATSPI,
    TRACE,
    EVENT,
};

std::string to_string(LogLevel e);

#endif // LOGGERDECLS_H
