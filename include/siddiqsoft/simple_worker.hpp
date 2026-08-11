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
#include <chrono>
#ifndef SIMPLE_WORKER_HPP
#define SIMPLE_WORKER_HPP


#include <iostream>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <deque>
#include <semaphore>
#include <stop_token>
#include <exception>
#include <source_location>
#include <atomic>

#if defined(_Linux_) || defined(__linux__) || defined(__linux) || (defined(__APPLE__) && defined(__MACH__))
#include <pthread.h>
#elif defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#include <windows.h>
#include <processthreadsapi.h>
#endif

#include "siddiqsoft/WaitableQueue.hpp"
#include "siddiqsoft/RunOnEnd.hpp"
#include "private/common.hpp"

namespace siddiqsoft
{
    /**
     * @brief Implements a simple asynchronous worker thread with queue-based processing.
     *
     * This template class provides a single worker thread that processes items from a queue
     * asynchronously. Items are queued by the main thread and processed by the worker thread
     * via a callback function. The worker thread waits on a semaphore for items and processes
     * them as they become available.
     *
     * @details
     * - Single dedicated worker thread per instance
     * - Items are stored in a WaitableQueue protected by internal synchronization
     * - Thread waits for up to DEFAULT_WAIT_FOR_NEXT_ITEM_MS (1500ms) for the next item
     * - Exceptions in callbacks are caught and logged to prevent thread termination
     * - Uses jthread for automatic cleanup on destruction
     * - Supports optional thread priority adjustment (Windows and POSIX systems)
     * - Provides JSON serialization for monitoring and diagnostics
     *
     * @tparam T The data type for work items (must be move-constructible)
     * @tparam Pri Optional thread priority level (-10 to 10, default: 0 for normal priority)
     *         On Windows: passed to SetThreadPriority()
     *         On POSIX: can be used for custom priority handling
     *
     * @example
     * @code
     * // Create a worker that processes strings
     * siddiqsoft::simple_worker<std::string> worker([](std::string&& item) {
     *     std::cout << "Processing: " << item << std::endl;
     * });
     *
     * // Queue work items
     * worker.queue(std::string("task1"));
     * worker.queue(std::string("task2"));
     *
     * // Worker automatically cleans up on destruction
     * @endcode
     */
    template <typename T, int Pri = 0>
        requires((Pri >= -10) && (Pri <= 10)) && std::move_constructible<T>
    struct simple_worker
    {
        std::atomic<bool> accepting_items {true};
        std::atomic<bool> shutdown_initiated {false};
        std::once_flag    shutdown_invoked;

        /// @brief Default wait interval for the worker thread waiting on items
        static constexpr std::chrono::milliseconds DEFAULT_WAIT_FOR_NEXT_ITEM_MS {1500};
        static constexpr std::chrono::milliseconds DEFAULT_SHUTDOWN_DRAIN_MS {1000};

    public:
        /// @brief Copy constructor (deleted - workers are not copyable)
        simple_worker(const simple_worker&) = delete;

        /// @brief Copy assignment operator (deleted - workers are not copyable)
        simple_worker& operator=(const simple_worker&) = delete;


        /**
         * @brief Destructor - gracefully shuts down the worker thread
         *
         * Performs the following cleanup steps:
         * 1. Waits for the queue to be empty (all pending items processed)
         * 2. Requests the worker thread to stop via stop_token
         * 3. Joins the thread to ensure clean shutdown
         *
         * @remarks In debug builds, logs the queue state before shutdown
         */
        ~simple_worker()
        {
            // Performs a graceful shutdown (drains and kills the threads.)
            shutdown();
        }

        bool shutdown(std::chrono::milliseconds timeout = DEFAULT_SHUTDOWN_DRAIN_MS)
        {
            bool shutdown_status {false};

            std::call_once(
                    shutdown_invoked,
                    [&](bool& status, std::chrono::milliseconds& t) {
                        accepting_items.store(false, std::memory_order_release);
#if defined(DEBUG)
                        std::cerr << std::format("worker shutdown started inside call_once.. asking for waitUntilEmpty...for {}ms\n", t.count());
#endif

                        // Drain existing items and wait for the queue to be empty.
                        // Add a total deadline with buffer of 500ms extra..
                        auto deadline  = std::chrono::steady_clock::now() + t + std::chrono::milliseconds(500);
                        auto isDrained = items.waitUntilEmpty(t);

#if defined(DEBUG)
                        std::cerr << std::format("worker shutdown possible; isDrained: {}. size:{}\n", isDrained, items.size());
#endif

                        // Notify the processor to shutdown (we should have no outstanding items.)
                        processor.request_stop();
#if defined(DEBUG)
                        std::cerr << std::format("worker shutdown started inside call_once\n");
#endif

                        if (processor.joinable()) {
                            processor.join();
                            status = isDrained;
#if defined(DEBUG)
                            std::cerr << std::format("worker shutdown ok; isDrained: {}. size:{}\n", isDrained, items.size());
#endif
                        }
#if defined(DEBUG)
                        else {
                            std::cerr << std::format("worker shutdown failed; isDrained: {}. size:{}\n", isDrained, items.size());
                        }

                        std::cerr << "WARNING: Graceful shutdown timeout exceeded\n";
#endif

                        status = isDrained; // Timeout occurred
                    },
                    shutdown_status,
                    timeout);
            return shutdown_status;
        }

        /// @brief Move constructor (deleted - workers are not movable)
        simple_worker(simple_worker&&) = delete;

        /// @brief Move assignment operator (deleted - workers are not movable)
        simple_worker& operator=(simple_worker&&) = delete;

        /**
         * @brief Constructs a worker thread with the given callback
         *
         * Creates a single worker thread that will process items from the queue
         * using the provided callback function.
         *
         * @param c The worker callback function with signature void(T&&)
         *          Called for each item dequeued from the worker's queue
         *
         * @details
         * - The callback is stored and invoked by the worker thread
         * - The worker thread starts immediately and waits for items
         * - Thread priority is set if Pri != 0 (Windows only)
         * - Exceptions in callbacks are caught and logged to prevent thread termination
         */
        simple_worker(std::function<void(T&&)> c)
            : callback(c)
        {
        }


        /**
         * @brief Queue a work item for processing
         *
         * Adds an item to the worker's queue for asynchronous processing.
         * The worker thread will process this item as soon as it becomes available.
         *
         * @param item The work item to queue (must be move-constructible)
         *             Ownership is transferred to the worker
         *
         * @details
         * - Thread-safe operation
         * - Increments the queue counter with release semantics
         * - Item is moved into the internal queue
         * - Worker thread is signaled to wake up if waiting
         *
         * @note The item is moved into the queue, so the original is no longer valid
         */
        void queue(T&& item) noexcept(false)
        {
            if (!accepting_items.load(std::memory_order_acquire)) {
                throw std::runtime_error("Worker is shutting down, cannot queue new items");
            }

            items.emplace(std::move(item));
            queueCounter.fetch_add(1, std::memory_order_release);
        }

#if defined(NLOHMANN_JSON_VERSION_MAJOR)
        /**
         * @brief Serialize worker state to JSON
         *
         * Returns a JSON object containing diagnostic information about the worker state.
         * Useful for monitoring and debugging.
         *
         * @return nlohmann::json object with worker statistics
         *
         * @details Includes:
         * - _typver: Version identifier for the worker type
         * - itemsSize: Current number of items in the queue
         * - queueCounter: Total number of items queued (atomic counter)
         * - itemsQueued: Total items added to queue
         * - itemsPopped: Total items removed from queue
         * - itemsOutstanding: Items queued but not yet processed
         * - threadPriority: Thread priority level
         * - outstandingCallback: Number of callbacks currently executing
         * - waitInterval: Default wait interval in milliseconds
         *
         * @note Thread-safe operation with acquire semantics
         */
        auto to_json() const -> nlohmann::json
        {
            auto itemsSize        = items.size();
            auto itemsQueued      = items.addCounter();
            auto itemsPopped      = items.removeCounter();
            auto itemsOutstanding = itemsQueued - itemsPopped;

            return {{"_typver", "siddiqsoft.asynchrony.simple_worker/2.3.3"},
                    {"itemsSize", itemsSize},
                    {"queueCounter", queueCounter.load(std::memory_order_acquire)},
                    {"itemsQueued", itemsQueued},
                    {"itemsPopped", itemsPopped},
                    {"itemsOutstanding", itemsOutstanding},
                    {"threadPriority", Pri},
                    {"outstandingCallback", outstandingCallback.load(std::memory_order_acquire)},
                    {"waitInterval", DEFAULT_WAIT_FOR_NEXT_ITEM_MS.count()}};
        }
#endif

    private:
        /// @brief Flag to ensure forceCleanupTerminate is called only once
        std::once_flag flag_forceCleanupTerminate {};

        /// @brief Tracks the number of callbacks currently executing
        /// Uses acquire/release semantics for proper synchronization
        std::atomic_uint outstandingCallback {0};

        /// @brief Track number of times items have been added to the queue
        std::atomic_uint64_t queueCounter {0};

        /// @brief The internal queue for work items
        siddiqsoft::WaitableQueue<T> items {};

        /// @brief The callback function invoked for each dequeued item
        std::function<void(T&&)> callback;

        /**
         * @brief Worker thread that processes items from the queue
         *
         * This jthread runs the main worker loop:
         * 1. Waits on the queue for the next item (with timeout)
         * 2. If an item is available and stop not requested, invokes the callback
         * 3. Catches and logs any exceptions to prevent thread termination
         * 4. Continues until stop_token is signaled
         *
         * @details
         * - Captures 'this' to access the queue and callback
         * - Sets thread priority if Pri != 0 (Windows only)
         * - Exceptions are caught at two levels (inner and outer) for robustness
         * - Uses acquire/release semantics for thread-safe operations
         * - Waits up to DEFAULT_WAIT_FOR_NEXT_ITEM_MS for each item
         */
        std::jthread processor {[&](std::stop_token st) {
#if defined(WIN64) || defined(_WIN64) || defined(WIN32) || defined(_WIN32)
            // Set the thread priority if possible
            if constexpr (Pri != 0) SetThreadPriority(GetCurrentThread(), Pri);
#endif

            while (!st.stop_requested()) {
                try {
                    // The getNextItem performs the wait on the signal and if it expires, returns empty.
                    // If there is an item, it will get that item (minimizing move) and performs the pop
                    // and returns the item so we can invoke the callback outside the lock.
                    // We must ensure that the callback is nonempty!
                    if (auto item = items.tryWaitItem(DEFAULT_WAIT_FOR_NEXT_ITEM_MS); item && !st.stop_requested() && callback) {
                        // Delegate to the callback outside the lock
                        try {
                            // We get an optional<> and thus the use of the * to get the value if present..
                            callback(std::move(*item));
                        }
                        catch (const std::exception& ex) {
                            // We swallow exceptions from the callback to avoid thread termination and log it if needed.
                            std::cerr << std::format("Ignoring Exception in simple_worker callback: {} - inner\n", ex.what());
                        }
                    }
                }
                catch (const std::exception& ex) {
                    // We swallow exceptions from the callback to avoid thread termination and log it if needed.
                    std::cerr << std::format("Ignoring Exception in simple_worker callback: {} - outer\n", ex.what());
                }
            } // while ..continue until we're asked to stop
#if defined(DEBUG)
            std::cerr << std::format("WARNING: Abandon {} items processing due to stop request!\n", items.size());
#endif
        }};
    };

#if defined(NLOHMANN_JSON_VERSION_MAJOR)
    /**
     * @brief JSON serialization adapter for simple_worker
     *
     * Enables automatic JSON serialization of simple_worker objects via nlohmann::json.
     *
     * @tparam T The item type processed by the worker
     * @tparam Pri The thread priority level
     * @param dest Destination JSON object to populate
     * @param src Source simple_worker object to serialize
     */
    template <typename T, int Pri = 0>
    static void to_json(nlohmann::json& dest, const siddiqsoft::simple_worker<T, Pri>& src)
    {
        dest = src.to_json();
    }
#endif

} // namespace siddiqsoft
#endif // !SIMPLE_WORKER_HPP
