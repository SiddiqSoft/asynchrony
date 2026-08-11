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
#include <siddiqsoft/RunOnEnd.hpp>
#ifndef PERIODIC_WORKER_HPP
#define PERIODIC_WORKER_HPP


#include <iostream>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <deque>
#include <semaphore>
#include <stop_token>
#include <utility>
#include <exception>
#include <source_location>
#include <atomic>

#if defined(_Linux_) || defined(__linux__) || defined(__linux) || (defined(__APPLE__) && defined(__MACH__))
#include <pthread.h>
#elif defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#include <windows.h>
#include <processthreadsapi.h>
#endif

#include "private/common.hpp"

namespace siddiqsoft
{
    /**
     * @brief Implements a periodic worker thread that invokes a callback at regular intervals.
     *
     * This template class provides a single worker thread that invokes a callback function
     * periodically at a specified interval. The thread waits on a semaphore for the specified
     * duration and then invokes the callback. This is useful for periodic tasks like health checks,
     * cleanup operations, or status updates.
     *
     * @details
     * - Single dedicated worker thread per instance
     * - Callback is invoked at regular intervals specified by the constructor
     * - Thread waits on a counting_semaphore with the specified interval as timeout
     * - Exceptions in callbacks are caught and logged to prevent thread termination
     * - Uses jthread for automatic cleanup on destruction
     * - Supports optional thread priority adjustment (Windows and POSIX systems)
     * - Provides JSON serialization for monitoring and diagnostics
     * - Thread-safe interval modification via atomic operations
     *
     * @tparam Pri Optional thread priority level (-10 to 10, default: 0 for normal priority)
     *         On Windows: passed to SetThreadPriority()
     *         On POSIX: can be used for custom priority handling
     *
     * @example
     * @code
     * // Create a periodic worker that runs every 5 seconds
     * siddiqsoft::periodic_worker<> worker(
     *     []() { std::cout << "Periodic task running" << std::endl; },
     *     std::chrono::seconds(5),
     *     "health-check-worker"
     * );
     * 
     * // Worker automatically cleans up on destruction
     * @endcode
     */
    template <int Pri = 0>
        requires((Pri >= -10) && (Pri <= 10))
    struct periodic_worker
    {
        /// @brief Default wait interval for the worker thread
        static constexpr std::chrono::milliseconds DEFAULT_WAIT_FOR_NEXT_ITEM_MS {1500};

    public:
        /// @brief Copy constructor (deleted - workers are not copyable)
        periodic_worker(periodic_worker&) = delete;
        
        /// @brief Copy assignment operator (deleted - workers are not copyable)
        auto& operator=(periodic_worker&) = delete;
        
        /// @brief Move constructor (deleted - workers are not movable)
        periodic_worker(periodic_worker&&) = delete;
        
        /// @brief Move assignment operator (deleted - workers are not movable)
        auto& operator=(periodic_worker&&) = delete;


        /**
         * @brief Destructor - gracefully shuts down the periodic worker thread
         *
         * Performs the following cleanup steps:
         * 1. Sets the invoke period to 0 microseconds to wake up the waiting thread immediately
         * 2. Releases the semaphore to signal the thread
         * 3. Requests the worker thread to stop via stop_token
         * 4. Waits briefly for the thread to respond
         * 5. Allows jthread to join automatically
         *
         * @remarks This approach is critical because:
         * - Without reducing the interval, the thread might wait for the full period before shutting down
         * - Setting interval to 0 ensures immediate wakeup from the semaphore wait
         * - Saves approximately 100ms+ of idle time compared to default timeout
         *
         * @note In debug builds, logs shutdown progress and statistics
         */
        ~periodic_worker()
        {
#if defined(DEBUG) || defined(_DEBUG)
            std::cerr << std::format(
                    "Shutting down periodic worker [{}] with outstanding callbacks [{}] and total invoke count [{}]\n",
                    threadName,
                    outstandingCallback.load(std::memory_order_acquire),
                    invokeCounter.load(std::memory_order_acquire));
#endif

            // This is critical step since we wait on the semaphore for a long time (keeps threads suspended) and if we do not
            // decrease this interval then the shutdown will be quite delayed.
            // Use atomic store with release semantics to safely modify invokePeriod from destructor
            invokePeriod.store(std::chrono::microseconds(0), std::memory_order_release);
            // Empty signal to get our thread to wake up
            signal.release();

#if defined(DEBUG) || defined(_DEBUG)
            std::cerr << std::format("Signaled shutdown for periodic worker [{}], waiting for thread to join...\n", threadName);
#endif

            try {
                // Notify the thread to stop.. and wait for a bit.. and then instead of joining we should just let the jthread
                // destroy. Ask thread to shutdown and if joinable.. join.
                processor.request_stop();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                // if (processor.joinable()) processor.join();
            }
            catch (const std::exception& ex) {
                std::cerr << std::format("Exception while shutting down periodic worker [{}]: {}", threadName, ex.what());
            }

#if defined(DEBUG) || defined(_DEBUG)
            std::cerr << std::format("End of destructor for periodic worker [{}], waiting for thread to join...\n", threadName);
#endif
        }

        /**
         * @brief Force immediate termination of the worker thread
         *
         * This method should only be used during application shutdown when the callback
         * cannot be guaranteed to be "clean" or respect the stop_token. It forcefully
         * terminates the thread using platform-specific APIs.
         *
         * @param sl Source location for logging purposes (automatically captured)
         *
         * @warning This is a last-resort cleanup method and should only be called when
         *          normal shutdown has failed. Using this during normal operation can
         *          lead to resource leaks and undefined behavior.
         *
         * @details
         * - On POSIX systems: calls pthread_cancel() and detaches the thread
         * - On Windows: calls TerminateThread() and detaches the thread
         * - Uses std::call_once to ensure this is only called once
         * - Logs a warning message with the source location
         */
        void forceCleanupTerminate(const std::source_location& sl = std::source_location::current())
        {
            std::call_once(flag_forceCleanupTerminate, [&]() {
                try {
                    // Notify the thread to stop.. and wait a bit before forceful termination
                    processor.request_stop();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
#if defined(_Linux_) || defined(__linux__) || defined(__linux) || (defined(__APPLE__) && defined(__MACH__))
                    auto nativeHandle = processor.native_handle();
                    std::cerr << std::format(
                            "forceCleanupTerminate - WARNING!! Calling native thread shutdown; only perform this when app is "
                            "ending! from: {}:{}",
                            sl.file_name(),
                            sl.line());
                    pthread_cancel(nativeHandle);
                    processor.detach();
#elif defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
                auto nativeHandle = processor.native_handle();
                std::cerr << std::format(
                             "forceCleanupTerminate - WARNING!! Calling native thread shutdown; only perform this when app is "
                             "ending! from: {}:{}",
                             sl.file_name(),
                             sl.line());
                TerminateThread(nativeHandle, 0);
                processor.detach();
#endif
                }
                catch (const std::exception& ex) {
                    std::cerr << std::format("forceCleanupTerminate - Exception while shutting down worker: {}", ex.what());
                }
            });
        }

        /**
         * @brief Constructs a periodic worker thread
         *
         * Creates a single worker thread that will invoke the callback at the specified interval.
         *
         * @param c The worker callback function with signature void()
         *          Called periodically at the specified interval
         * @param interval The time interval between callback invocations
         * @param name Optional name for the worker thread (useful for debugging)
         *
         * @details
         * - The callback is stored and invoked by the worker thread
         * - The worker thread starts immediately and waits for the first interval
         * - Thread priority is set if Pri != 0 (Windows only)
         * - Exceptions in callbacks are caught and logged to prevent thread termination
         * - The interval can be modified at runtime via the invokePeriod atomic variable
         */
        periodic_worker(std::function<void()>     c,
                        std::chrono::microseconds interval,
                        std::string               name = {"anonymous-periodic-worker"})
            : callback(std::move(c))
            , invokePeriod(interval)
            , threadName(std::move(name))
        {
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
         * - threadName: Name of the worker thread
         * - outstandingCallbacks: Number of callbacks currently executing
         * - invokeCounter: Total number of times the callback has been invoked
         * - threadPriority: Thread priority level
         * - waitInterval: Current wait interval in microseconds
         *
         * @note Thread-safe operation with acquire semantics
         */
        nlohmann::json to_json() const
        {
            using namespace std;

            return {{"_typver"s, "siddiqsoft.asynchrony.periodic_worker/2.3.3"s},
                    {"threadName", threadName},
                    {"outstandingCallbacks", outstandingCallback.load(std::memory_order_acquire)},
                    {"invokeCounter"s, invokeCounter.load(std::memory_order_acquire)},
                    {"threadPriority"s, Pri},
                    {"waitInterval"s, invokePeriod.load(std::memory_order_acquire).count()}};
        }
#endif

    private:
        /// @brief Flag to ensure forceCleanupTerminate is called only once
        std::once_flag flag_forceCleanupTerminate {};

        /// @brief Tracks the number of callbacks currently executing
        /// Uses acquire/release semantics for proper synchronization
        std::atomic_uint outstandingCallback {0};
        
        /// @brief Internal name of the worker thread (displays in debugger when supported)
        std::string threadName {"anonymous-periodic-worker"};
        
        /// @brief Track total number of times the callback has been invoked
        std::atomic_uint64_t invokeCounter {0};
        
        /// @brief Counting semaphore for signaling the worker thread
        /// Initial count of 1 allows one immediate signal
        std::counting_semaphore<1> signal {0};
        
        /**
         * @brief The interval between callback invocations
         *
         * This is made atomic to allow safe modification from the destructor
         * and other threads. Uses acquire/release semantics for proper synchronization.
         * Set to 0 microseconds during shutdown to wake up the thread immediately.
         */
        std::atomic<std::chrono::microseconds> invokePeriod {std::chrono::milliseconds(1500)};
        
        /// @brief The callback function invoked periodically
        std::function<void()> callback;
        
        /**
         * @brief Worker thread that invokes the callback periodically
         *
         * This jthread runs the main worker loop:
         * 1. Waits on the semaphore for the specified interval
         * 2. If stop not requested, increments outstanding callback counter
         * 3. Invokes the callback
         * 4. Decrements outstanding callback counter
         * 5. Continues until stop_token is signaled
         *
         * @details
         * - Captures 'this' to access the callback and interval
         * - Sets thread priority if Pri != 0 (Windows only)
         * - Exceptions are caught at two levels (inner and outer) for robustness
         * - Uses acquire/release semantics for thread-safe operations
         * - Loads invokePeriod atomically to allow runtime modification
         * - Uses RunOnEnd to ensure outstanding callback counter is decremented
         */
        std::jthread processor {[&](std::stop_token st) {
#if defined(WIN64) || defined(_WIN64) || defined(WIN32) || defined(_WIN32)
            // Set the thread priority if possible
            if constexpr (Pri != 0) SetThreadPriority(GetCurrentThread(), Pri);
#endif

            while (!st.stop_requested()) {
                try {
                    // This will wait until our period and return.
                    // We do not care about the return from try_acquire_for..
                    // We're using it as an efficient "wait" facility for period.
                    // Load invokePeriod atomically with acquire semantics
                    auto _ = signal.try_acquire_for(invokePeriod.load(std::memory_order_acquire));

                    if (!st.stop_requested()) {
                        auto decrementOutstandingCallback = siddiqsoft::RunOnEnd {[&] {
                            // Decrement outstanding callback
                            outstandingCallback.fetch_sub(1, std::memory_order_release);
                        }};

                        // Increment outstanding callback with release semantics
                        outstandingCallback.fetch_add(1, std::memory_order_release);
                        try {
                            // Delegate to the callback outside the lock
                            if (callback) callback();
                            invokeCounter.fetch_add(1, std::memory_order_release);
                        }
                        catch (const std::exception& ex) {
                            // We swallow exceptions from the callback to avoid thread termination and log it if needed.
                            std::cerr << std::format("Ignoring Exception (inner) in periodic_worker callback: {}", ex.what());
                        }
                    }
                }
                catch (const std::exception& ex) {
                    // We swallow exceptions from the callback to avoid thread termination and log it if needed.
                    std::cerr << std::format("Ignoring Exception (outer) in periodic_worker callback: {}", ex.what());
                }
            } // while ..continue until we're asked to stop
        }};
    };

#if defined(NLOHMANN_JSON_VERSION_MAJOR)
    /**
     * @brief JSON serialization adapter for periodic_worker
     *
     * Enables automatic JSON serialization of periodic_worker objects via nlohmann::json.
     *
     * @tparam Pri The thread priority level
     * @param dest Destination JSON object to populate
     * @param src Source periodic_worker object to serialize
     */
    template <int Pri = 0>
    static void to_json(nlohmann::json& dest, const siddiqsoft::periodic_worker<Pri>& src)
    {
        dest = src.to_json();
    }
#endif

} // namespace siddiqsoft
#endif // PERIODIC_WORKER_HPP
