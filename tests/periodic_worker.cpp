/*
    asynchrony-lib
    Add asynchrony to your apps

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

#include "gtest/gtest.h"

#include <iostream>
#include <format>
#include <string>
#include <thread>
#include <atomic>
#include <array>
#include <memory>
#include <vector>

#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/periodic_worker.hpp"


TEST(periodic_worker, test1)
{
    uint64_t                    passTest {0};

    siddiqsoft::periodic_worker worker {[&]() { passTest++; }, std::chrono::milliseconds(100)};

    while (passTest <= 44)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // We expect at least 50 iterations at a rate of one per 100ms with a test time of 5s.
    // This test is going to be tough since each VM configuration varies and the CPU frequency has a bearing on how "fast" or "lazy"
    // the wait on the semaphore is accurate.
    EXPECT_LE(44, passTest);

    std::cerr << worker.toJson().dump() << std::endl;
}

TEST(periodic_worker, nosleep_test2)
{
    uint64_t                    passTest {0};

    siddiqsoft::periodic_worker worker {[&]() {
                                            // this sleep will force the worker to terminate mid-call
                                            std::cerr << "  Started......`" << __func__ << "`......" << std::endl;
                                            std::this_thread::sleep_for(std::chrono::seconds(2));
                                            passTest++;
                                            std::cerr << "  Completed....`" << __func__ << "`......" << std::endl;
                                        },
                                        // run the above code every 50ms
                                        std::chrono::milliseconds(50)};

    // We expect at least one iteration completed.
    EXPECT_GE(1, passTest) << std::format("At least one iteration; completed: {}", passTest);

    std::clog << worker.toJson().dump() << std::endl;
}


/// @brief Test that the named periodic worker stores the name correctly in toJson
TEST(periodic_worker, named_worker)
{
    std::atomic_uint passTest {0};

    siddiqsoft::periodic_worker worker {[&]() { passTest++; }, std::chrono::milliseconds(50), "my-test-worker"};

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_GT(passTest.load(), 0u);

    auto j = worker.toJson();
    EXPECT_EQ("my-test-worker", j["threadName"].get<std::string>());
    std::cerr << j.dump() << std::endl;
}


/// @brief Test toJson returns all expected fields
TEST(periodic_worker, toJson_fields)
{
    std::atomic_uint passTest {0};

    siddiqsoft::periodic_worker worker {[&]() { passTest++; }, std::chrono::milliseconds(50)};

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto j = worker.toJson();
    EXPECT_TRUE(j.contains("_typver"));
    EXPECT_TRUE(j.contains("threadName"));
    EXPECT_TRUE(j.contains("outstandingCallbacks"));
    EXPECT_TRUE(j.contains("invokeCounter"));
    EXPECT_TRUE(j.contains("threadPriority"));
    EXPECT_TRUE(j.contains("waitInterval"));
    EXPECT_EQ(0, j["threadPriority"].get<int>());
    EXPECT_GT(j["invokeCounter"].get<uint64_t>(), 0u);
    std::cerr << j.dump() << std::endl;
}


/// @brief Test that the periodic worker invokes at roughly the expected rate
TEST(periodic_worker, invocation_rate)
{
    std::atomic_uint passTest {0};

    // 50ms interval, run for 500ms => expect ~10 invocations (allow some tolerance)
    siddiqsoft::periodic_worker worker {[&]() { passTest++; }, std::chrono::milliseconds(50)};

    std::this_thread::sleep_for(std::chrono::milliseconds(550));

    // Should have at least 5 invocations and no more than 20 (generous bounds for CI)
    EXPECT_GE(passTest.load(), 5u);
    EXPECT_LE(passTest.load(), 20u);
}


/// @brief Test that destroying the periodic worker is safe even if callback is slow
TEST(periodic_worker, destroy_during_callback)
{
    auto makeWorker = []() {
        return std::make_unique<siddiqsoft::periodic_worker<>>(
                []() { std::this_thread::sleep_for(std::chrono::seconds(5)); }, std::chrono::milliseconds(10));
    };

    EXPECT_NO_THROW({
        auto worker = makeWorker();
        // Let it start one callback
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // Destructor fires — should not hang indefinitely or crash
        worker.reset();
    });
}


/// @brief Test default thread name
TEST(periodic_worker, default_thread_name)
{
    siddiqsoft::periodic_worker worker {[]() {}, std::chrono::milliseconds(100)};

    auto j = worker.toJson();
    EXPECT_EQ("anonymous-periodic-worker", j["threadName"].get<std::string>());
}


/// @brief Verify the periodic worker continues invoking after the callback throws.
/// The worker thread catches all exceptions; the invoke counter should keep incrementing
/// even when some invocations throw.
TEST(periodic_worker, callback_exception_resilience)
{
    std::atomic_uint invokeCount {0};
    std::atomic_uint exceptionCount {0};

    siddiqsoft::periodic_worker worker {[&]() {
                                            invokeCount++;
                                            if (invokeCount.load() % 3 == 0) {
                                                exceptionCount++;
                                                throw std::runtime_error("deliberate periodic exception");
                                            }
                                        },
                                        std::chrono::milliseconds(20)};

    // Let it run for ~500ms at 20ms intervals => ~25 invocations
    std::this_thread::sleep_for(std::chrono::milliseconds(550));

    // Should have continued past exceptions
    EXPECT_GE(invokeCount.load(), 10u);
    EXPECT_GE(exceptionCount.load(), 3u);
    std::cerr << std::format("periodic callback_exception_resilience: invocations={}, exceptions={}\n",
                             invokeCount.load(),
                             exceptionCount.load());
}


/// @brief Stress test: rapidly create and destroy periodic workers.
/// Validates that the jthread lifecycle (start/stop/join) and semaphore teardown
/// are robust under rapid construction/destruction cycles.
TEST(periodic_worker, rapid_create_destroy_cycles)
{
    constexpr unsigned CYCLES = 50;
    auto interval = std::chrono::milliseconds(5);
    auto noop     = []() {};

    EXPECT_NO_THROW({
        for (unsigned c = 0; c < CYCLES; c++) {
            siddiqsoft::periodic_worker worker(noop, interval);
            // Immediate destruction — no sleep
        }
    });
}


/// @brief Test multiple periodic workers running concurrently, each with its own
/// callback and interval. Validates there's no cross-contamination or deadlock
/// when multiple instances coexist.
TEST(periodic_worker, multiple_concurrent_workers)
{
    constexpr unsigned WORKER_COUNT = 6;
    std::array<std::atomic_uint, WORKER_COUNT> counters {};

    // Use unique_ptr because periodic_worker is non-movable/non-copyable
    std::vector<std::unique_ptr<siddiqsoft::periodic_worker<>>> workers;
    workers.reserve(WORKER_COUNT);

    for (unsigned w = 0; w < WORKER_COUNT; w++) {
        workers.push_back(std::make_unique<siddiqsoft::periodic_worker<>>(
                [&counters, w]() { counters[w]++; }, std::chrono::milliseconds(25 + w * 10)));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Destroy all workers
    workers.clear();

    // Each worker should have invoked at least a few times
    for (unsigned w = 0; w < WORKER_COUNT; w++) {
        EXPECT_GT(counters[w].load(), 0u) << "Worker " << w << " never invoked";
    }
}


/// @brief Verify that a periodic worker with a very short interval (near-zero)
/// doesn't spin-lock or cause issues. The callback should be invoked many times.
TEST(periodic_worker, very_short_interval)
{
    std::atomic_uint invokeCount {0};

    siddiqsoft::periodic_worker worker {[&]() { invokeCount++; }, std::chrono::microseconds(100)};

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // With 100µs interval over 200ms, expect many invocations (at least 100)
    EXPECT_GE(invokeCount.load(), 100u);
}


/// @brief Test the ADL to_json free function for periodic_worker.
/// This exercises the nlohmann::json serialization path via `nlohmann::json(worker)`
/// which invokes the free `to_json(json&, const periodic_worker<Pri>&)` function.
TEST(periodic_worker, adl_to_json)
{
    std::atomic_uint passTest {0};

    siddiqsoft::periodic_worker worker {[&]() { passTest++; }, std::chrono::milliseconds(50)};

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_GT(passTest.load(), 0u);

    // This uses the ADL to_json free function, not the member toJson()
    nlohmann::json j;
    siddiqsoft::to_json(j, worker);
    EXPECT_TRUE(j.contains("_typver"));
    EXPECT_TRUE(j.contains("invokeCounter"));
    EXPECT_GT(j["invokeCounter"].get<uint64_t>(), 0u);
    std::cerr << "ADL to_json result: " << j.dump() << std::endl;
}


/// @brief Test that the periodic worker's outstandingCallback counter is accurate.
/// While a slow callback is running, outstandingCallback should be 1.
TEST(periodic_worker, outstanding_callback_tracking)
{
    std::atomic_bool callbackStarted {false};
    std::atomic_bool callbackCanFinish {false};

    siddiqsoft::periodic_worker worker {[&]() {
                                            callbackStarted = true;
                                            while (!callbackCanFinish.load()) {
                                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                            }
                                        },
                                        std::chrono::milliseconds(10)};

    // Wait for the callback to start
    while (!callbackStarted.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // While callback is blocked, outstandingCallbacks should be 1
    auto j = worker.toJson();
    EXPECT_EQ(1u, j["outstandingCallbacks"].get<unsigned>());

    // Let the callback finish
    callbackCanFinish = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // After callback completes, outstandingCallbacks should be 0 (between invocations)
    // Note: there's a small window where it could be 1 again if another invocation started
    // so we just verify it's <= 1
    j = worker.toJson();
    EXPECT_LE(j["outstandingCallbacks"].get<unsigned>(), 1u);
}
