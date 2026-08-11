/*
    roundrobin pool : Add asynchrony to your apps

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
#ifndef ROUNDROBIN_POOL_HPP
#define ROUNDROBIN_POOL_HPP

#include <concepts>
#include <deque>
#include "simple_worker.hpp"


namespace siddiqsoft
{
    /**
     * @brief Implements a lock-free round-robin work distribution thread pool.
     *
     * This template class provides a thread pool that distributes work items across
     * multiple worker threads using a round-robin algorithm. Each worker thread has its own
     * queue, eliminating lock contention on a shared queue. Work items are distributed
     * sequentially to each worker in turn, ensuring balanced load distribution.
     *
     * @details
     * - Multiple simple_worker<T> instances stored in a deque
     * - Work items are distributed using round-robin indexing
     * - Each worker has its own queue, minimizing lock contention
     * - No locks needed for work distribution (uses atomic counter)
     * - Thread count is determined by N parameter or std::thread::hardware_concurrency()
     * - Uses std::deque to avoid element relocation on growth (safe for non-movable types)
     * - Provides JSON serialization for monitoring and diagnostics
     *
     * @tparam T The data type for work items (must be move-constructible)
     * @tparam N Number of threads in the pool. If 0 (default), uses std::thread::hardware_concurrency()
     *
     * @remarks The round-robin approach has several advantages:
     * - Eliminates lock contention on a shared queue
     * - Each thread only pops from its own queue (single consumer)
     * - Multiple producers can push to different queues concurrently
     * - Cost of modulo operation is cheaper than lock acquisition
     * - If one thread is blocked, other threads continue processing
     *
     * @example
     * @code
     * // Create a round-robin pool with default number of threads
     * siddiqsoft::roundrobin_pool<std::string> pool([](std::string&& item) {
     *     std::cout << "Processing: " << item << std::endl;
     * });
     *
     * // Queue work items - distributed round-robin across workers
     * pool.queue(std::string("task1"));
     * pool.queue(std::string("task2"));
     * pool.queue(std::string("task3"));
     *
     * // Pool automatically cleans up on destruction
     * @endcode
     */
    template <typename T, uint16_t N = 0>
        requires std::is_move_constructible_v<T>
    struct roundrobin_pool
    {
    public:
        /// @brief Move constructor (deleted - pools are not movable)
        roundrobin_pool(roundrobin_pool&&) = delete;

        /// @brief Move assignment operator (deleted - pools are not movable)
        auto operator=(roundrobin_pool&&) = delete;

        /// @brief Copy constructor (deleted - pools are not copyable)
        roundrobin_pool(roundrobin_pool&) = delete;

        /// @brief Copy assignment operator (deleted - pools are not copyable)
        auto operator=(roundrobin_pool&) = delete;


        /**
         * @brief Constructs a round-robin thread pool
         *
         * Creates a deque of simple_worker<T> instances that will process work items
         * using the provided callback function. Work items are distributed across workers
         * using a round-robin algorithm.
         *
         * @param c The worker callback function with signature void(T&&)
         *          Called for each item dequeued from a worker's queue
         *
         * @details
         * - Creates N worker threads (or hardware_concurrency() if N is 0)
         * - Uses std::deque to store workers (no relocation on growth)
         * - Caches the worker count for efficient round-robin calculation
         * - Each worker is initialized with the same callback
         * - Workers start immediately and wait for items
         */
        roundrobin_pool(std::function<void(T&&)> c)
        {
            // Calculate size first
            workersSize = (N > 0) ? N : std::thread::hardware_concurrency();

            // Then create workers
            for (unsigned i = 0; i < workersSize; i++) {
                workers.emplace_back(c);
            }
        }

        /**
         * @brief Queue a work item for processing
         *
         * Adds an item to one of the worker's queues using round-robin distribution.
         * The choice of worker is determined by the queue counter modulo the number of workers.
         *
         * @param item The work item to queue (must be move-constructible)
         *             Ownership is transferred to the selected worker
         *
         * @details
         * - Uses atomic fetch_add to get a unique index for each queued item
         * - Calculates worker index as (counter % workersSize)
         * - Distributes items evenly across all workers
         * - Thread-safe for concurrent calls from multiple producers
         * - Each worker only pops from its own queue (single consumer per queue)
         *
         * @remarks The round-robin approach ensures:
         * - Balanced load distribution across workers
         * - No lock contention on the shared queue (each worker has its own)
         * - Cost of modulo is cheaper than lock acquisition
         * - If one worker is blocked, others continue processing
         *
         * @note The item is moved into the selected worker's queue
         */
        void queue(T&& item)
        {
            // Add into the thread's internal queue using round-robin index
            // Atomic fetch_add to ensures each thread gets a unique index
            // and cast to size_t to avoid type mismatch issues
            size_t idx = nextWorkerIndex();
            workers.at(idx).queue(std::move(item));
            // Increment the queue counter with release semantics for visibility
            queueCounter.fetch_add(1, std::memory_order_release);
        }

#if defined(NLOHMANN_JSON_VERSION_MAJOR)
        /**
         * @brief Serialize pool state to JSON
         *
         * Returns a JSON object containing diagnostic information about the pool state.
         * Useful for monitoring and debugging.
         *
         * @return nlohmann::json object with pool statistics
         *
         * @details Includes:
         * - _typver: Version identifier for the pool type
         * - workersSize: Number of worker threads in the pool
         * - queueCounter: Total number of items queued (atomic counter)
         *
         * @note Thread-safe operation with acquire semantics
         */
        nlohmann::json to_json() const
        {
            return {{"_typver", "siddiqsoft.asynchrony.roundrobin_pool/2.3.3"},
                    {"workersSize", workersSize},
                    {"queueCounter", queueCounter.load(std::memory_order_acquire)}};
        }
#endif

#ifdef _DEBUG
    public:
        /// @brief Queue counter - tracks total items queued (public in debug builds)
        std::atomic_uint64_t queueCounter {0};
#else
    private:
        /// @brief Queue counter - tracks total items queued (private in release builds)
        std::atomic_uint64_t queueCounter {0};
#endif

    private:
        /**
         * @brief Deque of worker threads
         *
         * Uses std::deque instead of std::vector because:
         * - Deque does not relocate elements on growth
         * - Safe for non-movable types
         * - Provides random access via at()
         * - Allows efficient iteration
         */
        std::deque<simple_worker<T>> workers {};

        /// @brief Cached size of the workers deque for efficient round-robin calculation
        uint64_t workersSize {};

        /**
         * @brief Calculates the next worker index using round-robin distribution
         *
         * Computes the index into the workers array based on the queue counter
         * and the number of workers. Uses modulo arithmetic to wrap around.
         *
         * @return size_t index into the workers array (0 to workersSize-1)
         *
         * @details
         * - Uses atomic load with acquire semantics for consistency
         * - Ensures proper synchronization with queue() which uses release semantics
         * - Returns 0 if workersSize is 0 (safety check)
         * - Provides consistent round-robin distribution
         *
         * @note The acquire/release semantics ensure that the round-robin distribution
         *       is consistent across threads and matches the order of queue() calls
         */
        size_t nextWorkerIndex()
        {
            if (workersSize == 0) return 0;
            return static_cast<size_t>(queueCounter.load(std::memory_order_acquire) % workersSize);
        }
    };

#if defined(NLOHMANN_JSON_VERSION_MAJOR)
    /**
     * @brief JSON serialization adapter for roundrobin_pool
     *
     * Enables automatic JSON serialization of roundrobin_pool objects via nlohmann::json.
     *
     * @tparam T The item type stored in the pool
     * @tparam N The number of threads in the pool
     * @param dest Destination JSON object to populate
     * @param src Source roundrobin_pool object to serialize
     */
    template <typename T, uint16_t N = 0>
    static void to_json(nlohmann::json& dest, const siddiqsoft::roundrobin_pool<T, N>& src)
    {
        dest = src.to_json();
    }
#endif

} // namespace siddiqsoft
#endif // !ROUNDROBIN_POOL_HPP
