/*
    asynchrony-lib - Additional Bug Detection Tests
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
#include <barrier>
#include <vector>
#include <mutex>
#include <chrono>

#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/simple_worker.hpp"
#include "../include/siddiqsoft/simple_pool.hpp"
#include "../include/siddiqsoft/roundrobin_pool.hpp"
#include "../include/siddiqsoft/periodic_worker.hpp"
#include "../include/siddiqsoft/resource_pool.hpp"

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

/// @brief BUG #1: TOCTOU Race Condition in resource_pool::size()
/// The size() method checks if pool is empty, then returns size.
/// Between the check and the return, another thread could modify the pool.
TEST(additional_bugs, resource_pool_size_toctou_race)
{
    struct dummy_resource
    {
        int value;
    };
    siddiqsoft::resource_pool<dummy_resource> pool {};
    std::atomic_int                           sizeReadCount {0};
    std::atomic_int                           sizeInconsistency {0};

    // Pre-populate
    for (int i = 0; i < 10; i++) {
        pool.checkin(dummy_resource {i});
    }

    std::atomic_bool          done {false};
    std::vector<std::jthread> threads;

    // Thread that repeatedly reads size
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&]() {
            while (!done.load()) {
                size_t sz = pool.size();
                sizeReadCount++;
                // If size is 0, but we can still checkout, there's a race
                if (sz == 0) {
                    try {
                        auto item = pool.checkout();
                        sizeInconsistency++;
                    }
                    catch (const std::runtime_error&) {
                        // Expected when pool is actually empty
                    }
                }
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    // Thread that modifies the pool
    std::jthread modifier([&]() {
        for (int i = 0; i < 100; i++) {
            pool.checkin(dummy_resource {i});
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            try {
                auto _ = pool.checkout();
            }
            catch (const std::runtime_error&) {
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    done = true;
    threads.clear();
    modifier.request_stop();
    if (modifier.joinable()) modifier.join();

    // If there were inconsistencies, the race condition was triggered
    std::cerr << "Size reads: " << sizeReadCount.load() << ", Inconsistencies: " << sizeInconsistency.load() << std::endl;
}


/// @brief BUG #3: Bare catch(...) Swallows Exceptions
/// Tests that exceptions in callbacks are silently swallowed
TEST(additional_bugs, simple_worker_bare_catch_swallows_exceptions)
{
    std::atomic_uint                          exceptionCount {0};
    std::atomic_uint                          processedCount {0};

    siddiqsoft::simple_worker<nlohmann::json> worker {[&](auto&& item) {
        int idx = item["index"].template get<int>();
        if (idx % 3 == 0) {
            exceptionCount++;
            throw std::runtime_error("test exception");
        }
        processedCount++;
    }};

    for (int i = 0; i < 30; i++) {
        worker.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify exceptions were caught (not propagated)
    EXPECT_EQ(10u, exceptionCount.load());
    EXPECT_EQ(20u, processedCount.load());
    // The bare catch(...) silently swallows the exceptions
}


/// @brief BUG #9: Uninitialized workersSize in roundrobin_pool
/// If constructor throws before workersSize is set, it remains 0
TEST(additional_bugs, roundrobin_pool_uninitialized_workers_size)
{
    // This is hard to trigger without modifying the code
    // But we can test the edge case where workersSize is 0

    // Create a pool with 1 worker
    siddiqsoft::roundrobin_pool<std::string, 1> pool {[](auto&&) { }};

    // Queue items - should work fine
    for (int i = 0; i < 10; i++) {
        EXPECT_NO_THROW(pool.queue(std::format("item-{}", i)));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}


/// @brief BUG #2: Unnecessary Condition in resource_pool::clear()
/// The condition !_pool.empty() is unnecessary and could cause issues
TEST(additional_bugs, resource_pool_clear_semantics)
{
    siddiqsoft::resource_pool<int> pool {};

    // Add items
    pool.checkin(1);
    pool.checkin(2);
    pool.checkin(3);
    EXPECT_EQ(3u, pool.size());

    // Clear
    pool.clear();
    EXPECT_EQ(0u, pool.size());

    // Clear again (should be safe)
    EXPECT_NO_THROW(pool.clear());
    EXPECT_EQ(0u, pool.size());

    // Clear on empty pool (should be safe)
    EXPECT_NO_THROW(pool.clear());
}


/// @brief BUG #4: Integer Overflow in queueCounter
/// After 2^64 items, the counter wraps around
/// This test documents the behavior (not a failure, just a limitation)
TEST(additional_bugs, roundrobin_pool_queue_counter_overflow_documentation)
{
    struct dummy_task
    {
        int value;
    };

    siddiqsoft::roundrobin_pool<dummy_task, 1> pool {[](auto&&) { }};

    // We can't actually test overflow in a reasonable time
    // But we can document that it's a known limitation

    // Queue a large number of items
    for (int i = 0; i < 10000; i++) {
        pool.queue(dummy_task {i});
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // The counter should be around 10000
    auto     j       = pool.toJson();
    uint64_t counter = j["queueCounter"].get<uint64_t>();
    EXPECT_EQ(10000u, counter);

    // After 2^64 items, the counter would wrap to 0
    // This is documented as a known limitation
}


/// @brief BUG #12: Bounds Check in roundrobin_pool::queue()
/// If workersSize is 0, nextWorkerIndex() returns 0, then workers.at(0) throws
TEST(additional_bugs, roundrobin_pool_bounds_check)
{
    // Create a pool with 1 worker
    siddiqsoft::roundrobin_pool<int, 1> pool {[](auto&&) { }};

    // Queue should work fine
    EXPECT_NO_THROW(pool.queue(1));
    EXPECT_NO_THROW(pool.queue(2));
    EXPECT_NO_THROW(pool.queue(3));

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}


/// @brief BUG #10: Bare catch in Destructor
/// Tests that exceptions in destructor are silently swallowed
TEST(additional_bugs, simple_worker_destructor_exception_handling)
{
    // This is hard to trigger without modifying the code
    // But we can test that destruction is safe even with pending items

    EXPECT_NO_THROW({
        siddiqsoft::simple_worker<std::string> worker {[](auto&&) { std::this_thread::sleep_for(std::chrono::seconds(5)); }};

        // Queue items that will be pending when destructor runs
        for (int i = 0; i < 10; i++) {
            worker.queue(std::format("item-{}", i));
        }

        // Destructor fires - should not hang or crash
    });
}


/// @brief BUG #1 (Extended): Concurrent size() and checkout() Race
/// More aggressive test for the TOCTOU race in size()
TEST(additional_bugs, resource_pool_size_checkout_race_aggressive)
{
    struct dummy_task
    {
        int value;
    };
    siddiqsoft::resource_pool<dummy_task> pool {};

    // Pre-populate with many items
    for (int i = 0; i < 100; i++) {
        pool.checkin(dummy_task {i});
    }

    std::atomic_bool done {false};
    std::atomic_int  racesDetected {0};

    // Thread that repeatedly checks size then tries to checkout
    std::jthread reader([&]() {
        while (!done.load()) {
            size_t sz = pool.size();
            if (sz > 0) {
                try {
                    auto item = pool.checkout();
                }
                catch (const std::runtime_error&) {
                    // Race detected: size said > 0, but checkout failed
                    racesDetected++;
                }
            }
        }
    });

    // Thread that repeatedly adds and removes items
    std::jthread writer([&]() {
        for (int i = 0; i < 1000; i++) {
            pool.checkin(dummy_task {i});
            try {
                auto _ = pool.checkout();
            }
            catch (const std::runtime_error&) {
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    done = true;
    reader.request_stop();
    writer.request_stop();
    if (reader.joinable()) reader.join();
    if (writer.joinable()) writer.join();

    // If races were detected, the TOCTOU bug was triggered
    std::cerr << "Races detected: " << racesDetected.load() << std::endl;
}


/// @brief BUG #6 (Extended): Lambda Capture Safety with Concurrent Operations
/// More aggressive test for lambda capture safety
TEST(additional_bugs, simple_pool_lambda_capture_safety)
{
    struct dummy_task
    {
        int value;
    };
    std::atomic_uint processedCount {0};

    // simple_pool is not move-constructible or move-assignable by design
    for (int cycle = 0; cycle < 50; cycle++) {
        {
            siddiqsoft::simple_pool<dummy_task, 2> pool {[&](auto&&) { processedCount++; }};

            // Queue items
            for (int i = 0; i < 20; i++) {
                pool.queue(dummy_task {i});
            }

            // Pool is destroyed here, threads are joined
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    // Verify all items were processed
    EXPECT_EQ(50u * 20u, processedCount.load());
}

/// @brief BUG #8 (Extended): periodic_worker Lambda Capture Safety
/// Tests lambda capture safety in periodic_worker
TEST(additional_bugs, periodic_worker_lambda_capture_safety)
{
    std::atomic_uint invokeCount {0};

    // Create and destroy multiple periodic workers
    for (int cycle = 0; cycle < 50; cycle++) {
        {
            siddiqsoft::periodic_worker worker {[&]() { invokeCount++; }, std::chrono::milliseconds(10)};

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    // If we got here without a crash, the lambda capture might be safe
    EXPECT_GT(invokeCount.load(), 0u);
}
// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)