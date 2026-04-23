#pragma once

#include <iostream>
#include <atomic>
#include <vector>

#include "macros.h"

namespace Common
{
    template <typename T>
    class LFQueue final
    {
    public:
        LFQueue(std::size_t num_elements) : store_(num_elements, T()), capacity_(num_elements)
        {
            ASSERT(capacity_ > 1, "LFQueue: capacity must be greater than 1 for SPSC operation");
        }
        LFQueue() = delete;
        LFQueue(const LFQueue &) = delete;
        LFQueue &operator=(const LFQueue &) = delete;
        LFQueue(LFQueue &&) = delete;
        LFQueue &operator=(LFQueue &&) = delete;

        auto tryPush(const T &value) noexcept -> bool
        {
            const auto write_index = next_write_index_.load(std::memory_order_relaxed);
            const auto next_write_index = (write_index + 1) % capacity_;
            // In SPSC mode the producer publishes by advancing write_index_ after the
            // element payload is fully written into the reserved slot.
            if (next_write_index == next_read_index_.load(std::memory_order_acquire))
            {
                return false;
            }

            store_[write_index] = value;
            next_write_index_.store(next_write_index, std::memory_order_release);
            return true;
        }

        auto front() const noexcept -> const T *
        {
            const auto read_index = next_read_index_.load(std::memory_order_relaxed);
            const auto write_index = next_write_index_.load(std::memory_order_acquire);
            return (read_index == write_index) ? nullptr : &store_[read_index];
        }

        auto pop() noexcept -> void
        {
            const auto read_index = next_read_index_.load(std::memory_order_relaxed);
            ASSERT(read_index != next_write_index_.load(std::memory_order_acquire), "LFQueue: No elements available to read");
            next_read_index_.store((read_index + 1) % capacity_, std::memory_order_release);
        }

        auto size() const noexcept -> size_t
        {
            const auto read_index = next_read_index_.load(std::memory_order_acquire);
            const auto write_index = next_write_index_.load(std::memory_order_acquire);
            return (write_index >= read_index) ? (write_index - read_index) : (capacity_ - read_index + write_index);
        }

    private:
        std::vector<T> store_;
        const size_t capacity_;
        std::atomic<size_t> next_write_index_ = 0;
        std::atomic<size_t> next_read_index_ = 0;
    };
}
