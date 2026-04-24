#pragma once

#include <chrono>
#include <ctime>
#include <string>

namespace Common
{
    typedef int64_t Nanos;

    constexpr Nanos NANOS_TO_MICROS = 1000;
    constexpr Nanos MICROS_TO_MILLIS = 1000;
    constexpr Nanos MILLIS_TO_SECS = 1000;
    constexpr Nanos NANOS_TO_MILLIS = NANOS_TO_MICROS * MICROS_TO_MILLIS;
    constexpr Nanos NANOS_TO_SECS = NANOS_TO_MILLIS * MILLIS_TO_SECS;

    inline auto getCurrentNanos() noexcept -> Nanos
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    inline auto &getCurrentTimeStr(std::string *time_str)
    {
        const auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        time_str->assign(std::ctime(&time));
        if (!time_str->empty())
        {
            // ctime() appends a trailing newline, which is awkward inside structured log lines.
            time_str->at(time_str->size() - 1) = '\0'; // Remove the newline character added by ctime
        }
        return *time_str;
    }
}
