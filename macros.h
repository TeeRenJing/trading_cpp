#include <iostream>
#include <cstdlib>
#include <string>

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

inline auto ASSERT(bool cond, const std::string &msg) noexcept -> void
{
    if (UNLIKELY(!cond))
    {
        if (UNLIKELY(!msg.empty()))
        {
            std::cerr << "Assertion failed: " << msg << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

inline auto FATAL(const std::string &msg) noexcept -> void
{
    std::cerr << "Fatal error: " << msg << std::endl;
    exit(EXIT_FAILURE);
}