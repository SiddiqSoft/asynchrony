/*
    basic-pool : Add asynchrony to your apps

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
#ifndef SIMPLE_POOL_HPP
#define SIMPLE_POOL_HPP

#include "simple_worker.hpp"
#include <optional>
#include <latch>
#include <exception>
#include <atomic>
#include <utility>

#include "siddiqsoft/WaitableQueue.hpp"
#include "siddiqsoft/RunOnEnd.hpp"

namespace siddiqsoft
{
    /**
     * @brief Implements a thread pool with a single deque-based queue of worker threads.
     *
     * This template class provides a simple thread pool implementation where all threads
     * wait on items from a shared deque via a counting semaphore. When an item is available,
     * the next available thread invokes the callback on that item. This design minimizes
     * lock contention by using a single shared queue with semaphore signaling.
     *
     * @details
     * - All threads share a single WaitableQueue for work items
     * - Threads are signaled via a counting_semaphore when items are available
     * - Each thread waits for up to DEFAULT_WAIT_FOR_NEXT_ITEM_MS (1500ms) for the next item
     * - Exceptions in callbacks are caught and logged to prevent thread termination
     * - Uses jthread for automatic cleanup on destruction
     * - Thread count is determined by N parameter or std::thread::hardware_concurrency()
     *
     * @tparam T The data type for work items (must be move-constructible)
     * @tparam N Number of threads in the pool. If 0 (default), uses std::thread::hardware_concurrency()
     *
     * @remarks The number of threads in the pool should be chosen based on the nature of your work:
     * - For CPU-bound work: use std::thread::hardware_concurrency()
     * - For I/O-bound work (e.g., database queries): consider using more threads as individual
     *   operations might take time and block the thread
     *
     * @example
     * @code
     * // Create a pool with default number of threads
     * siddiqsoft::simple_pool<std::string> pool([](std::string&& item) {
     *     std::cout << "Processing: " << item << std::endl;
     * });
     *
     * // Queue work items
     * pool.queue(std::string("task1"));
     * pool.queue(std::string("task2"));
     *
     * // Pool automatically cleans up on destruction
     * @endcode
     */
    template <typename T, uint16_t N = 0>
        requires std::is_move_constructible_v<T>
    struct simple_pool
    {
        /// @brief Default wait interval for threads waiting on the semaphore
        static constexpr std::chrono::milliseconds DEFAULT_WAIT_FOR_NEXT_ITEM_MS {1500};

        /// @brief Move constructor (deleted - pools are not movable)
        simple_pool(simple_pool&&) = delete;

        /// @brief Move assignment operator (deleted - pools are not movable)
        simple_pool& operator=(simple_pool&&) = delete;

        /// @brief Copy constructor (deleted - pools are not copyable)
        simple_pool(simple_pool&) = delete;

        /// @brief Copy assignment operator (deleted - pools are not copyable)
        simple_pool& operator=(simple_pool&) = delete;


        /**
         * @brief Destructor - gracefully shuts down all worker threads
         *
         * Performs the following cleanup steps:
         * 1. Releases the semaphore to wake up waiting threads
         * 2. Requests all threads to stop via stop_token
         * 3. Joins all threads to ensure clean shutdown
         *
         * @remarks This saves approximately 100ms of idle time compared to letting threads
         *          timeout naturally on the default 1500ms wait interval
         */
        ~simple_pool()
        {
            // Compared to skipping the following code, we save at least about 100ms
            // of idle time waiting for the threads to be signalled by default.
            // Request all threads to stop first
            for (auto& t : workers) {
                t.request_stop();
            }

            // Then wake them up and join
            for (auto& t : workers) {
                signal.release();
                if (t.joinable()) t.join();
            }
        }

        /**
         * @brief Constructs a thread pool with N worker threads
         *
         * Creates a pool of worker threads that process items from a shared queue.
         * Each thread runs a loop that waits for items and invokes the callback.
         *
         * @param c The worker callback function with signature void(T&&)
         *          Called for each item dequeued from the pool
         *
         * @details
         * - Reserves space for all threads upfront to avoid relocations
         * - Creates N threads (or hardware_concurrency() if N is 0)
         * - Each thread waits on the semaphore with a 1500ms timeout
         * - Exceptions in callbacks are caught and logged to stderr
         * - Threads continue running until stop_requested() returns true
         */
        simple_pool(std::function<void(T&&)> c)
            : callback(std::move(c))
        {
            // *CRITICAL*
            // This is step is *critical* otherwise we will end up moving threads as we add elements to the vector.
            workers.reserve((N > 0) ? N : std::thread::hardware_concurrency());

            // Create as many threads as reported by the system..
            for (unsigned i = 0; i < ((N > 0) ? N : std::thread::hardware_concurrency()); i++) {
                // Add the thread with the main driver
                workers.emplace_back([&](std::stop_token st) {
                    // The driver runs forever until signalled to stop
                    // Tries to get next item ready in the queue (for max 1s cycle)
                    // If we have an item, invoke the callback with the item
                    while (!st.stop_requested()) {
                        try {
                            // The getNextItem performs the wait on the signal and if it expires, returns empty.
                            // If there is an item, it will get that item (minimizing move) and performs the pop
                            // and returns the item so we can invoke the callback outside the lock.
                            if (auto item = getNextItem(); item.has_value() && !st.stop_requested() && callback) {
                                // Delegate to the callback outside the lock
                                callback(std::move(*item));
                            }
                        }
                        catch (const std::exception& ex) {
                            // We swallow exceptions from the callback to avoid thread termination and log it if needed.
                            std::cerr << std::format("Ignoring Exception in simple_worker callback: {}", ex.what());
                        }
                    } // while ..continue until we're asked to stop
                });
            }
        }

        /**
         * @brief Queue a work item for processing
         *
         * Adds an item to the shared queue and signals a waiting thread to process it.
         * This method is thread-safe and can be called from multiple threads concurrently.
         *
         * @param item The work item to queue (must be move-constructible)
         *             Ownership is transferred to the pool
         *
         * @details
         * - Uses perfect forwarding to minimize copies
         * - Increments the queue counter with release semantics for visibility
         * - Releases the semaphore to wake up a waiting thread
         * - Thread-safe for concurrent calls from multiple producers
         *
         * @note The item is moved into the internal queue, so the original is no longer valid
         */
        void queue(T&& item)
        {
            // With this interface, we can peform a perfect forward of the r-value from the caller into the
            // items internal container without the complexity of lambda capture forwards.
            items.emplace(std::forward<T>(item));

            // Use atomic fetch_add with release semantics to ensure thread-safe updates
            queueCounter.fetch_add(1, std::memory_order_release);
            signal.release();
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
         * - workersSize: Number of worker threads
         * - dequeSize: Current number of items in the queue
         * - queueCounter: Total number of items queued (atomic counter)
         * - waitInterval: Default wait interval in milliseconds
         *
         * @note Thread-safe operation
         */
        auto to_json() const -> nlohmann::json
        {
            const auto sz = items.size();
            return nlohmann::json {{"_typver", "siddiqsoft.asynchrony.simple_pool/2.3.3"},
                                   {"workersSize", workers.size()},
                                   {"dequeSize", sz},
                                   {"queueCounter", queueCounter.load(std::memory_order_acquire)},
                                   {"waitInterval", DEFAULT_WAIT_FOR_NEXT_ITEM_MS.count()}};
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
        /// @brief Vector of worker threads
        std::vector<std::jthread> workers {};

        /// @brief Callback function invoked for each dequeued item
        std::function<void(T&&)> callback;

        /// @brief Counting semaphore for signaling available work
        std::counting_semaphore<> signal {0};

        /// @brief Thread-safe queue for work items
        siddiqsoft::WaitableQueue<T> items {};

        /**
         * @brief Attempts to retrieve the next item from the queue
         *
         * Performs a timed wait on the semaphore and if successful, attempts to lock
         * and retrieve the item from the front of the deque.
         *
         * @param delta Wait timeout duration (default: DEFAULT_WAIT_FOR_NEXT_ITEM_MS)
         * @return An optional containing the item if available, or empty if timeout occurred
         *
         * @details
         * - Waits on the semaphore for up to 'delta' milliseconds
         * - If semaphore acquire succeeds, retrieves and removes the front item
         * - Returns empty optional if timeout occurs or queue is empty
         * - Thread-safe operation
         */
        std::optional<T> getNextItem(const std::chrono::milliseconds& delta = DEFAULT_WAIT_FOR_NEXT_ITEM_MS)
        {
            return items.tryWaitItem(delta);
        }
    };

#if defined(NLOHMANN_JSON_VERSION_MAJOR)
    /**
     * @brief JSON serialization adapter for simple_pool
     *
     * Enables automatic JSON serialization of simple_pool objects via nlohmann::json.
     *
     * @tparam T The item type stored in the pool
     * @tparam N The number of threads in the pool
     * @param dest Destination JSON object to populate
     * @param src Source simple_pool object to serialize
     */
    template <typename T, uint16_t N = 0>
    static auto to_json(nlohmann::json& dest, const siddiqsoft::simple_pool<T, N>& src) -> void const
    {
        dest = src.to_json();
    }
#endif

} // namespace siddiqsoft
#endif // !SIMPLE_POOL_HPP
