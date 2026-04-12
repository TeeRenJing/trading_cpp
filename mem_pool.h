#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "macros.h"

namespace Common
{
    template <typename T>
    class MemPool final
    {
    private:
        struct ObjectBlock
        {
            T object_;
            bool is_free_ = true;
        };
        std::vector<ObjectBlock> store_;
        size_t next_free_index_ = 0;

        auto updateNextFreeIndex() noexcept
        {
            const auto initial_free_index = next_free_index_;
            while (!store_[next_free_index_].is_free_)
            {
                next_free_index_++;
                if (UNLIKELY(next_free_index_ == store_.size()))
                {
                    next_free_index_ = 0;
                }
                if (UNLIKELY(next_free_index_ == initial_free_index))
                {
                    ASSERT(initial_free_index != next_free_index_, "MemPool: No free objects available");
                }
            }
        }

    public:
        explicit MemPool(std::size_t num_elements) : store_(num_elements, {T(), true})
        {
            ASSERT(reinterpret_cast<const ObjectBlock *>(&(store_[0])) == &store_[0], "MemPool: ObjectBlock layout is not as expected");
        }
        MemPool() = delete;
        MemPool(const MemPool &) = delete;
        MemPool &operator=(const MemPool &) = delete;
        MemPool(MemPool &&) = delete;
        MemPool &operator=(MemPool &&) = delete;

        template <typename... Args>
        T *allocate(Args... args) noexcept
        {
            auto obj_block = &(store_[next_free_index_]);
            ASSERT(obj_block->is_free_, "MemPool: No free objects available for allocation");
            T *ret = &(obj_block->object_);
            ret = new (ret) T(std::forward<Args>(args)...);
            obj_block->is_free_ = false;

            updateNextFreeIndex();

            return ret;
        }

        auto deallocate(const T *elem) noexcept
        {
            const auto elem_index = (reinterpret_cast<const ObjectBlock *>(elem) - &(store_[0]));
            ASSERT(elem_index >= 0 && static_cast<size_t>(elem_index) < store_.size(), "MemPool: Attempt to deallocate invalid pointer");
            ASSERT(!store_[elem_index].is_free_, "MemPool: Attempt to deallocate already free object");

            store_[elem_index].is_free_ = true;
        }
    };
};