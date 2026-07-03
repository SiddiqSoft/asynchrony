/*
    asynchrony : Add asynchrony to your apps

    BSD 3-Clause License

    Copyright (c) 2021, Siddiq Software LLC
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    3. Neither the name of the copyright holder nor the names of its
       contributors may be used to endorse or promote products derived from
       this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#ifndef RESOURCE_POOL_HPP
#define RESOURCE_POOL_HPP

#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <deque>

#include "siddiqsoft/RunOnEnd.hpp"

namespace siddiqsoft
{
    /**
     * @brief Implements a resource pool that stores objects of type T.
     *        Said objects can be shared_ptr or unique_ptr
     *        Client "acquire" and "release" T from this pool.
     *        The capacity of this pool should be kept at
     *        the same value as std::thread::hardware_concurrency()
     * @tparam T The storage element type. Maybe shared_ptr or unique_ptr
     *         The only requirement is that the underlying object is move-constructible!
     * 
     */
    template <typename T>
        requires std::move_constructible<T>
    class resource_pool
    {
    private:
        std::deque<T>        _pool {};
        std::mutex           _poolLock {};  // FIX: Changed from recursive_mutex to mutex (no recursive locking needed)

    public:
        resource_pool()                               = default;
        resource_pool(resource_pool&)                 = delete;
        resource_pool(resource_pool&& src)            = default;
        resource_pool& operator=(resource_pool&)      = delete;
        resource_pool& operator=(resource_pool&& src) = default;

        ~resource_pool()
        {
            clear();
        }

        /// @brief Clear all items from the pool
        /// FIX: Removed unnecessary empty check - clear() is safe on empty deque
        void clear()
        {
            std::scoped_lock<std::mutex> l(_poolLock);
            _pool.clear();
        }

        /// @brief Get the current size of the pool
        /// FIX: Removed unnecessary empty check - return size unconditionally
        /// This prevents TOCTOU (Time-of-Check-Time-of-Use) race condition
        auto size()
        {
            std::scoped_lock<std::mutex> l(_poolLock);
            return _pool.size();
        }

        [[nodiscard]] T checkout() /* throw() */
        {
            std::scoped_lock<std::mutex> l(_poolLock);
            if (!_pool.empty()) {
                RunOnEnd roe([&]() { _pool.pop_front(); });
                return std::move(_pool.front());
            }

            throw std::runtime_error("Empty pool; add something first!");
        }

        /**
         * @brief Insert a new element or return a borrowed element
         *
         * @param rsrc R-Value for the item to return to the pool (previously checkout'd or create a new one!)
         */
        void checkin(T&& rsrc)
        {
            std::scoped_lock<std::mutex> l(_poolLock);
            _pool.push_back(std::move(rsrc));
        }
    };
} // namespace siddiqsoft
#endif
