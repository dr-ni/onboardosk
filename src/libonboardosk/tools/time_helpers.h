#ifndef TIME_HELPERS_H
#define TIME_HELPERS_H

#include <chrono>
#include <string>


std::string format_time_stamp(std::chrono::system_clock::time_point tp
                              =std::chrono::system_clock::now());
std::string format_time(const std::string& format,
                        std::chrono::system_clock::time_point tp=
                            std::chrono::system_clock::now());

int64_t milliseconds_since_epoch();
int64_t seconds_since_epoch(std::chrono::time_point<std::chrono::steady_clock> tp);

#endif // TIME_HELPERS_H
