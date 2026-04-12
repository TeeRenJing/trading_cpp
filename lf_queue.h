#pragma once

#include <iostream>
#include <atomic>
#include <vector>

namespace Common
{
    template <typename T>
    class LFQueue final
    {
    public:
        LFQueue(std::size_t num_elements) : store_(num_elements, T()), num_elements_(num_elements)
        {
        }
        LFQueue() = delete;
        LFQueue(const LFQueue &) = delete;
        LFQueue &operator=(const LFQueue &) = delete;
        LFQueue(LFQueue &&) = delete;
        LFQueue &operator=(LFQueue &&) = delete;

        auto getNextToWriteTo() noexcept
        {
            return &store_[next_write_index_];
        }

        auto updateNextWriteIndex() noexcept
        {
            next_write_index_ = (next_write_index_ + 1) % num_elements_;
            num_elements_++;
        }

        auto getNextToRead() const noexcept -> const T *
        {
            return (next_read_index_ == next_write_index_) ? nullptr : &store_[next_read_index_];
        }

        auto updateReadIndex() noexcept
        {
            next_read_index_ = (next_read_index_ + 1) % num_elements_;
            ASSERT(num_elements_ > 0, "LFQueue: No elements to read in:" + std::to_string(pthread_self()));
            num_elements_--;
        }

        auto size() const noexcept
        {
            return num_elements_.load();
        }

    private:
        std::vector<T> store_;
        std::atomic<size_t> next_write_index_ = 0;
        std::atomic<size_t> next_read_index_ = 0;

        std::atomic<size_t> num_elements_ = 0;
    };
}