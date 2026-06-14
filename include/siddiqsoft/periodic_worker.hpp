/*
    asynchrony-lib : Add asynchrony to your apps

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

#if defined(_Linux_) || defined(__linux__) || defined(__linux) || (defined(__APPLE__) && defined(__MACH__))
#include <pthread.h>
#elif defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#include <windows.h>
#include <processthreadsapi.h>
#endif

#include "private/common.hpp"

namespace siddiqsoft
{
    /// @brief Implements a simple queue + semaphore driven asynchronous processor
    /// @tparam T The data type for this processor
    /// @tparam Pri Optional thread priority level. 0=Normal
    template <int Pri = 0>
        requires((Pri >= -10) && (Pri <= 10))
    struct periodic_worker
    {
    public:
        periodic_worker(periodic_worker&)  = delete;
        auto& operator=(periodic_worker&)  = delete;
        periodic_worker(periodic_worker&&) = delete;
        auto& operator=(periodic_worker&&) = delete;


        /// @brief Destructor
        /// Cancel the semaphore by first resetting the interval to zero.
        /// Signal the semaphore follow that by requesting thread to stop.
        /// This is slightly better than allowing the default destructors to kick in. If we do not reduce the interval time and
        /// signal a release then the timeout might be quite large!
        ~periodic_worker()
        {
#if defined(DEBUG) || defined(_DEBUG)
            std::println(std::cerr,
                         "Shutting down periodic worker [{}] with outstanding callbacks [{}] and total invoke count [{}]\n",
                         threadName,
                         outstandingCallback.load(std::memory_order_acquire),
                         invokeCounter.load(std::memory_order_acquire));
#endif

            // This is critical step since we wait on the semaphore for a long time (keeps threads suspended) and if we do not
            // decrease this interval then the shutdown will be quite delayed.
            invokePeriod = std::chrono::milliseconds(0);
            // Empty signal to get our thread to wake up
            signal.release();

#if defined(DEBUG) || defined(_DEBUG)
            std::println(std::cerr, "Signaled shutdown for periodic worker [{}], waiting for thread to join...\n", threadName);
#endif

            try {
                // Notify the thread to stop.. and wait for a bit.. and then instead of joining we should just let the jthread
                // destroy. Ask thread to shutdown and if joinable.. join.
                processor.request_stop();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
#if defined(_Linux_) || defined(__linux__) || defined(__linux) || (defined(__APPLE__) && defined(__MACH__))
                auto nativeHandle = processor.native_handle();
                pthread_cancel(nativeHandle);
                processor.detach();
#elif defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
                auto nativeHandle = processor.native_handle();
                TerminateThread(nativeHandle, 0);
                processor.detach();
#endif
            }
            catch (const std::exception& ex) {
                std::println(std::cerr, "Exception while shutting down periodic worker [{}]: {}", threadName, ex.what());
            }

#if defined(DEBUG) || defined(_DEBUG)
            std::println(std::cerr, "End of desstructor for periodic worker [{}], waiting for thread to join...\n", threadName);
#endif
        }


        /// @brief Constructor requires the callback for the thread
        /// @param c The callback which accepts the type T as reference and performs action.
        /// @param interval The interval between each invocation
        periodic_worker(std::function<void()>     c,
                        std::chrono::microseconds interval,
                        std::string               name = {"anonymous-periodic-worker"})
            : callback(std::move(c))
            , invokePeriod(interval)
            , threadName(std::move(name))
        {
        }


#if defined(NLOHMANN_JSON_VERSION_MAJOR)
        /// @brief Serializer for json
        /// @param  destination
        /// @param  this object
        /// @note The use of signal.max() is causing an issue where winmindef.h is defining the `max` as a macro and thus we end up
        /// with compiler error when the client application includes any of the windows headers! Disabled for now.
        nlohmann::json toJson() const
        {
            using namespace std;

            return {{"_typver"s, "siddiqsoft.asynchrony-lib.periodic_worker/0.10"s},
                    {"threadName", threadName},
                    {"outstandingCallbacks", outstandingCallback.load(std::memory_order_acquire)},
                    {"invokeCounter"s, invokeCounter.load(std::memory_order_acquire)},
                    {"threadPriority"s, Pri},
                    {"waitInterval"s, invokePeriod.count()}};
        }
#endif

    private:
        /// @brief Tracks the outstanding callback invocations so we can ensure that they are completed
        ///        neatly prior to pool shutdown. Uses acquire/release semantics for proper synchronization.
        std::atomic_uint outstandingCallback {0};
        /// @brief Internal name of the worker thread (when supported the thread name displays in the debugger)
        std::string threadName {"anonymous-periodic-worker"};
        /// @brief Track number of times we've invoked the callback
        std::atomic_uint64_t invokeCounter {0};
        /// @brief Semaphore with initial max of 128 items (backlog)
        std::counting_semaphore<1> signal {0};
        /// @brief This is the interval we wait on the signal. It starts off with 500ms and when the thread is to shutdown, it is
        /// set to 1ms.
        /// Initialize to a sensible default (1500ms) to avoid confusion about zero initialization
        std::chrono::microseconds invokePeriod {std::chrono::milliseconds(1500)};
        /// @brief The callback is invoked whenever there is an item in the queue
        std::function<void()> callback;
        /// @brief Processor thread
        /// The driver runs forever until signalled to stop
        /// Tries to get next item ready in the queue (for max 500ms cycle)
        /// If we have an item, invoke the callback with the item
        /// @note
        /// The processor thread captures `this` and access the signal and callback
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
                    auto _ = signal.try_acquire_for(invokePeriod);

                    if (!st.stop_requested()) {
                        auto decrementOutstandingCallback = siddiqsoft::RunOnEnd {[&] {
                            // Decrement outstanding callback
                            outstandingCallback.fetch_sub(1, std::memory_order_release);
                        }};

                        // Increment outstanding callback with acquire semantics
                        outstandingCallback.fetch_add(1, std::memory_order_acquire);
                        try {
                            // Delegate to the callback outside the lock
                            callback();
                            invokeCounter.fetch_add(1, std::memory_order_release);
                        }
                        catch (const std::exception& ex) {
                            // We swallow exceptions from the callback to avoid thread termination and log it if needed.
                            std::println(std::cerr, "Ignoring Exception (inner) in simple_worker callback: {}", ex.what());
                        }
                    }
                }
                catch (const std::exception& ex) {
                    // We swallow exceptions from the callback to avoid thread termination and log it if needed.
                    std::println(std::cerr, "Ignoring Exception (outer) in simple_worker callback: {}", ex.what());
                }
            } // while ..continue until we're asked to stop
        }};
    };

#if defined(NLOHMANN_JSON_VERSION_MAJOR)
    /// @brief Serializer for the periodic_worker
    /// @tparam T base typename
    /// @param dest destination json object
    /// @param src source object
    template <int Pri = 0>
    static void to_json(nlohmann::json& dest, const siddiqsoft::periodic_worker<Pri>& src)
    {
        dest = src.toJson();
    }
#endif

} // namespace siddiqsoft
#endif
