#pragma once
#include <string>
#include <fstream>
#include <cstdio>

#include "macros.h"
#include "time_utils.h"
#include "lf_queue.h"
#include "thread_utils.h"

namespace Common
{
    constexpr size_t LOG_QUEUE_SIZE = 8 * 1024 * 1024; // 8 MiB
    enum class LogType : int8_t
    {
        CHAR = 0,
        INTEGER = 1,
        LONG_INTEGER = 2,
        LONG_LONG_INTEGER = 3,
        UNSIGNED_INTEGER = 4,
        UNSIGNED_LONG_INTEGER = 5,
        UNSIGNED_LONG_LONG_INTEGER = 6,
        FLOAT = 7,
        DOUBLE = 8,
    };

    struct LogElement
    {
        LogType type_ = LogType::CHAR;
        union
        {
            char c;
            int i;
            long l;
            long long ll;
            unsigned u;
            unsigned long ul;
            unsigned long long ull;
            float f;
            double d;
        } u_;
    };

    class Logger final
    {
    public:
        explicit Logger(const std::string &log_file_path) : log_file_path_(log_file_path), log_queue_(LOG_QUEUE_SIZE)
        {
            log_file_.open(log_file_path_, std::ios::out | std::ios::app);
            ASSERT(log_file_.is_open(), "Logger: Failed to open log file: " + log_file_path_);
            logger_thread_ = createAndStartThread(-1, "Common/Logger", [this]()
                                                  { flushQueue(); });
            ASSERT(logger_thread_ != nullptr, "Logger: Failed to create logger thread");
        }

        ~Logger()
        {
            // Wait for the background thread to drain any queued log records before shutdown.
            while (log_queue_.size())
            {
                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(1ms);
            }
            is_running_ = false;
            if (logger_thread_ != nullptr)
            {
                logger_thread_->join();
                delete logger_thread_;
            }
            log_file_.close();
        }
        Logger() = delete;
        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;
        Logger(Logger &&) = delete;
        Logger &operator=(Logger &&) = delete;

        auto pushValue(const LogElement &log_element) noexcept -> void
        {
            // Producers publish fully-formed log elements with a single SPSC queue operation.
            ASSERT(log_queue_.tryPush(log_element), "Logger: Log queue is full");
        }

        auto pushValue(const char value) noexcept -> void
        {
            pushValue(LogElement{.type_ = LogType::CHAR, .u_ = {.c = value}});
        }

        auto pushValue(const char *value) noexcept -> void
        {
            for (size_t i = 0; value[i] != '\0'; i++)
            {
                pushValue(value[i]);
            }
        }

        auto pushValue(const std::string &value) noexcept -> void
        {
            pushValue(value.c_str());
        }

        auto pushValue(const int value) noexcept -> void
        {
            pushValue(LogElement{.type_ = LogType::INTEGER, .u_ = {.i = value}});
        }
        auto pushValue(const long value) noexcept -> void
        {
            pushValue(LogElement{.type_ = LogType::LONG_INTEGER, .u_ = {.l = value}});
        }
        auto pushValue(const long long value) noexcept -> void
        {
            pushValue(LogElement{.type_ = LogType::LONG_LONG_INTEGER, .u_ = {.ll = value}});
        }
        auto pushValue(const unsigned value) noexcept -> void
        {
            pushValue(LogElement{.type_ = LogType::UNSIGNED_INTEGER, .u_ = {.u = value}});
        }
        auto pushValue(const unsigned long value) noexcept -> void
        {
            pushValue(LogElement{.type_ = LogType::UNSIGNED_LONG_INTEGER, .u_ = {.ul = value}});
        }
        auto pushValue(const unsigned long long value) noexcept -> void
        {
            pushValue(LogElement{.type_ = LogType::UNSIGNED_LONG_LONG_INTEGER, .u_ = {.ull = value}});
        }
        auto pushValue(const float value) noexcept -> void
        {
            pushValue(LogElement{.type_ = LogType::FLOAT, .u_ = {.f = value}});
        }
        auto pushValue(const double value) noexcept -> void
        {
            pushValue(LogElement{.type_ = LogType::DOUBLE, .u_ = {.d = value}});
        }
        auto flushQueue() noexcept -> void
        {
            while (is_running_ || log_queue_.size())
            {
                // Drain all currently available elements in one batch, then yield briefly.
                for (auto next_to_read = log_queue_.front(); next_to_read != nullptr; log_queue_.pop(), next_to_read = log_queue_.front())
                {
                    const auto &log_element = *next_to_read;
                    switch (log_element.type_)
                    {
                    case LogType::CHAR:
                        log_file_ << log_element.u_.c;
                        break;
                    case LogType::INTEGER:
                        log_file_ << log_element.u_.i;
                        break;
                    case LogType::LONG_INTEGER:
                        log_file_ << log_element.u_.l;
                        break;
                    case LogType::LONG_LONG_INTEGER:
                        log_file_ << log_element.u_.ll;
                        break;
                    case LogType::UNSIGNED_INTEGER:
                        log_file_ << log_element.u_.u;
                        break;
                    case LogType::UNSIGNED_LONG_INTEGER:
                        log_file_ << log_element.u_.ul;
                        break;
                    case LogType::UNSIGNED_LONG_LONG_INTEGER:
                        log_file_ << log_element.u_.ull;
                        break;
                    case LogType::FLOAT:
                        log_file_ << log_element.u_.f;
                        break;
                    case LogType::DOUBLE:
                        log_file_ << log_element.u_.d;
                        break;
                    default:
                        ASSERT(false, "Logger: Invalid LogType in log element");
                    }
                }

                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(1ms);
            }
        }

        template <typename T, typename... Args>
        auto log(const char *s, const T &value, Args... args) noexcept -> void
        {
            // '%' is this logger's placeholder marker; '%%' escapes a literal percent sign.
            while (*s != '\0')
            {
                if (*s == '%')
                {
                    if (UNLIKELY(*(s + 1) == '%'))
                    {
                        s++;
                    }
                    else
                    {
                        pushValue(value);
                        log(s + 1, args...);
                        return;
                    }
                }
                pushValue(*s);
                s++;
            }
            FATAL("Logger: Too many arguments provided for format string: " + std::string(s));
        }

        auto log(const char *s) noexcept -> void
        {
            while (*s != '\0')
            {
                if (*s == '%')
                {
                    if (UNLIKELY(*(s + 1) == '%'))
                    {
                        s++;
                    }
                    else
                    {
                        FATAL("Logger: Not enough arguments provided for format string: " + std::string(s));
                    }
                }
                pushValue(*s);
                s++;
            }
        }

    private:
        const std::string log_file_path_;
        std::ofstream log_file_;
        LFQueue<LogElement> log_queue_;
        std::atomic<bool> is_running_ = true;
        std::thread *logger_thread_ = nullptr;
    };
}
