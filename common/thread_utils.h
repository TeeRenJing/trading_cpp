#pragma once

#include <iostream>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <sys/syscall.h>

inline auto setThreadCore(int core_id) noexcept
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
}
template <typename T, typename... Args>
inline auto createAndStartThread(int core_id, const std::string &thread_name, T &&func, Args &&...args) noexcept
{
    std::atomic<bool> running{false}, failed{false};

    auto thread_body = [&]
    {
        // Affinity is optional; a negative core_id means "leave scheduling to the OS".
        if (core_id >= 0 && !setThreadCore(core_id))
        {
            std::cerr << "Failed to set thread affinity for thread: " << thread_name << " " << pthread_self() << " to core " << core_id << std::endl;
            failed = true;
            return;
        }
        std::cout << "Starting thread: " << thread_name << " " << pthread_self() << " on core " << core_id << std::endl;
        running = true;
        std::forward<T>(func)(std::forward<Args>(args)...);
    };

    auto t = new std::thread(thread_body);

    // Spin until the child thread reports either successful startup or immediate failure.
    while (!running && !failed)
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1ms);
    }

    if (failed)
    {
        t.join();
        delete t;
        return nullptr;
    }

    return t;
}
