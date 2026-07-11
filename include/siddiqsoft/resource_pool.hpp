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
#include <cstdint>
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
     * @brief RAII wrapper for checked-out resources
     *
     * Automatically returns the resource to the pool when destroyed.
     * Provides pointer-like access to the underlying resource via operator* and operator&.
     *
     * @details
     * - Holds the resource and a callback function to return it to the pool
     * - Destructor automatically invokes the callback to ensure resource is returned
     * - Supports pointer-like access patterns for convenience
     */
    template <typename T>
        requires std::move_constructible<T>
    struct resource_wrap
    {
        /// @brief The actual resource being wrapped
        T rsrc {};

        /// @brief Callback function to return the resource to the pool
        std::function<void(T&&)> putbackCallback {};

        /// @brief Provides dereference access to the underlying resource
        auto operator*() -> T& { return rsrc; }

             operator T() { return rsrc; }

        /// @brief Destructor automatically returns the resource to the pool
        ~resource_wrap()
        {
            if (putbackCallback) putbackCallback(std::move(rsrc));
        }
    };

    /**
     * @brief Implements a thread-safe resource pool for managing reusable objects.
     *
     * This template class provides efficient resource pooling for managing expensive resources
     * like database connections, thread pools, or other reusable objects. Resources are checked
     * out from the pool and automatically returned when the wrapper goes out of scope (RAII pattern).
     *
     * @details
     * - Resources are stored in a deque and protected by a mutex for thread-safety
     * - The checkout() method returns a resource_wrap that automatically returns the resource
     *   to the pool when destroyed
     * - The capacity should ideally match std::thread::hardware_concurrency() for optimal
     *   performance in multi-threaded scenarios
     * - Resources must be move-constructible
     * - Uses FIFO (First-In-First-Out) ordering for resource retrieval
     * @note
     * - There is clear overhead when using the resource_wrap. The main benefit is from using
     *   the resource within a long-lived scope and not having to worry about cleanup.
     *
     * @tparam T The storage element type (e.g., shared_ptr or unique_ptr)
     *         The only requirement is that the underlying object is move-constructible
     * @tparam InitCapacity Initial capacity hint for the pool (default: 1 byte)
     *         Must not exceed the size of uint16_t
     *
     * @example
     * @code
     * // Create a pool of database connections
     * siddiqsoft::resource_pool<std::shared_ptr<DbConnection>> pool;
     *
     * // Check out a resource
     * auto wrapped = pool.checkout();
     * wrapped->executeQuery("SELECT * FROM users");
     * // Resource automatically returned to pool when wrapped goes out of scope
     * @endcode
     */
    template <typename T, uint16_t InitCapacity = sizeof(uint8_t)>
        requires((InitCapacity <= sizeof(uint16_t))) && std::move_constructible<T>
    class resource_pool
    {
    private:
        /// @brief Internal deque storing the pooled resources
        std::deque<T> _pool {};

        /// @brief Mutex protecting access to the resource pool
        /// Uses a regular mutex (not recursive) since no recursive locking is needed
        std::mutex _poolLock {};

    public:
        /// @brief Default constructor
        resource_pool() = default;

        /// @brief Copy constructor (deleted - pools are not copyable)
        resource_pool(resource_pool&) = delete;

        /// @brief Move constructor (defaulted)
        resource_pool(resource_pool&& src) = default;

        /// @brief Copy assignment operator (deleted - pools are not copyable)
        resource_pool& operator=(resource_pool&) = delete;

        /// @brief Move assignment operator (defaulted)
        resource_pool& operator=(resource_pool&& src) = default;

        /// @brief Destructor - clears all resources from the pool
        ~resource_pool() { clear(); }

        /**
         * @brief Clear all items from the pool
         *
         * Removes all resources from the pool. Thread-safe operation.
         * Safe to call on an empty pool.
         *
         * @note All resources are destroyed when cleared
         */
        void clear()
        {
            std::scoped_lock<std::mutex> l(_poolLock);
            _pool.clear();
        }

        /**
         * @brief Get the current size of the pool
         *
         * Returns the number of available resources in the pool.
         * Thread-safe operation.
         *
         * @return The number of resources currently in the pool
         *
         * @note This prevents TOCTOU (Time-of-Check-Time-of-Use) race conditions
         *       by returning the size directly without separate empty checks
         */
        auto size()
        {
            std::scoped_lock<std::mutex> l(_poolLock);
            return _pool.size();
        }

        /**
         * @brief Check out a resource from the pool
         *
         * Retrieves a resource from the pool and wraps it in a resource_wrap that
         * automatically returns the resource when destroyed. This implements the RAII pattern
         * to ensure resources are always returned to the pool.
         *
         * @return A resource_wrap containing the checked-out resource
         * @throws std::runtime_error if the pool is empty
         *
         * @note The returned resource_wrap uses RAII to ensure the resource is
         *       returned to the pool even if an exception occurs in the calling code
         * @note The [[nodiscard]] attribute encourages proper usage of the returned wrapper
         */
        [[nodiscard]] resource_wrap<T> checkout() /* throw() */
        {
            std::scoped_lock<std::mutex> l(_poolLock);

            if (!_pool.empty()) {
                RunOnEnd roe([&]() { _pool.pop_front(); });

                /// @brief Lambda that returns the resource back to the pool
                /// Captures 'this' to access the pool's checkin method
                /// Called by resource_wrap destructor to ensure automatic return
                /// even if an exception occurs
                auto autoReturnResource = [this](T&& rsrc) {
                    this->checkin(std::move(rsrc));
                };

                return resource_wrap<T> {std::move(_pool.front()), autoReturnResource};
            }

            throw std::runtime_error("Empty pool; add something first!");
        }

        /**
         * @brief Return a resource to the pool
         *
         * Adds a resource back to the pool, making it available for future checkout operations.
         * This is typically called automatically by the resource_wrap destructor.
         *
         * @param rsrc R-Value reference to the resource to return to the pool
         *             Can be a previously checked-out resource or a newly created one
         *
         * @note Thread-safe operation protected by mutex
         * @note Resources are added to the back of the deque and retrieved from the front (FIFO)
         * @note This method is typically not called directly; use checkout() instead
         */
        void checkin(T&& rsrc)
        {
            std::scoped_lock<std::mutex> l(_poolLock);
            _pool.push_back(std::move(rsrc));
        }
    };
} // namespace siddiqsoft
#endif
