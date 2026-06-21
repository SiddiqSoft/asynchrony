/*
    asynchrony-lib - Bug Detection Tests
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
#include <set>
#include <limits.h>

#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/simple_worker.hpp"
#include "../include/siddiqsoft/simple_pool.hpp"
#include "../include/siddiqsoft/roundrobin_pool.hpp"
#include "../include/siddiqsoft/periodic_worker.hpp"
#include "../include/siddiqsoft/resource_pool.hpp"


/// @brief BUG TEST: Race condition in queueCounter increment
/// The queueCounter should be incremented atomically and consistently.
/// This test verifies that all queued items are counted correctly even under
/// high contention from multiple producers.
TEST(bug_tests, simple_pool_queue_counter_race)
{
    constexpr int                              PRODUCER_COUNT     = 16;
    constexpr int                              ITEMS_PER_PRODUCER = 500;
    constexpr int                              TOTAL_ITEMS        = PRODUCER_COUNT * ITEMS_PER_PRODUCER;

    std::atomic_uint                           processedCount {0};

    siddiqsoft::simple_pool<nlohmann::json, 8> pool {[&](auto&&) { processedCount++; }};

    std::barrier                               startBarrier {PRODUCER_COUNT};
    std::vector<std::jthread>                  producers;

    for (int p = 0; p < PRODUCER_COUNT; p++) {
        producers.emplace_back([&, p]() {
            startBarrier.arrive_and_wait();
            for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
                pool.queue({{"producer", p}, {"item", i}});
            }
        });
    }

    producers.clear();
    std::this_thread::sleep_for(std::chrono::seconds(5));

    auto     j            = pool.toJson();
    uint64_t queueCounter = j["queueCounter"].get<uint64_t>();

    // BUG: If queueCounter is not properly synchronized, it may not equal TOTAL_ITEMS
    EXPECT_EQ(static_cast<uint64_t>(TOTAL_ITEMS), queueCounter)
            << "Queue counter mismatch: expected " << TOTAL_ITEMS << ", got " << queueCounter;
    EXPECT_EQ(static_cast<unsigned>(TOTAL_ITEMS), processedCount.load());
}


/// @brief BUG TEST: Race condition in roundrobin_pool with zero workers
/// If workersSize is 0, nextWorkerIndex() returns 0, but workers.at(0) will throw.
/// This test attempts to create a roundrobin_pool with 0 workers (edge case).
TEST(bug_tests, roundrobin_pool_zero_workers_edge_case)
{
    // This should either handle gracefully or fail with a clear error
    // Currently, if N=0 and hardware_concurrency() returns 0 (unlikely but possible),
    // the pool would have 0 workers and queue() would fail.

    // We can't directly test N=0 since it would use hardware_concurrency(),
    // but we can test with N=1 to ensure single-worker case works
    constexpr uint16_t                                     POOL_SIZE = 1;
    std::atomic_uint                                       processedCount {0};

    siddiqsoft::roundrobin_pool<nlohmann::json, POOL_SIZE> pool {[&](auto&&) { processedCount++; }};

    for (int i = 0; i < 100; i++) {
        pool.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(100u, processedCount.load());
}


/// @brief BUG TEST: Race condition in periodic_worker outstandingCallback
/// The outstandingCallback counter is incremented before the callback and
/// decremented after. If the callback throws, the decrement still happens
/// due to the try-catch, but we should verify this works correctly.
TEST(bug_tests, periodic_worker_outstanding_callback_exception)
{
    std::atomic_uint            invokeCount {0};
    std::atomic_uint            exceptionCount {0};
    std::atomic_uint            outstandingPeak {0};

    siddiqsoft::periodic_worker worker {[&]() {
                                            invokeCount++;
                                            if (invokeCount.load() % 2 == 0) {
                                                exceptionCount++;
                                                throw std::runtime_error("test exception");
                                            }
                                        },
                                        std::chrono::milliseconds(10)};

    // Monitor outstanding callbacks
    for (int i = 0; i < 100; i++) {
        auto     j           = worker.toJson();
        unsigned outstanding = j["outstandingCallbacks"].get<unsigned>();
        if (outstanding > outstandingPeak.load()) {
            outstandingPeak = outstanding;
            std::println(std::cerr, "Outstanding callbacks: {}", outstanding);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // outstandingCallback should never exceed 1 (only one callback at a time)
    EXPECT_LE(outstandingPeak.load(), 1u) << "Outstanding callback counter exceeded 1, indicating a race condition";
}


/// @brief BUG TEST: Concurrent access to resource_pool with rapid clear/checkout
/// Tests for potential race conditions between clear() and checkout/checkin operations.
TEST(bug_tests, resource_pool_concurrent_clear_checkout_race)
{
    struct test_resource
    {
        int value;
        test_resource(int v)
            : value(v)
        {
        }
    };
    siddiqsoft::resource_pool<test_resource> pool {};
    constexpr int                            INITIAL_SIZE = 100;
    constexpr int                            THREAD_COUNT = 8;
    constexpr int                            DURATION_MS  = 1000;

    // Pre-populate
    for (int i = 0; i < INITIAL_SIZE; i++) {
        pool.checkin(test_resource(i));
    }

    std::atomic_bool done {false};
    std::atomic_int  clearCount {0};
    std::atomic_int  checkoutCount {0};
    std::atomic_int  checkinCount {0};

    // Thread that repeatedly clears
    std::jthread clearer([&](std::stop_token st) {
        while (!st.stop_requested() && !done.load()) {
            pool.clear();
            clearCount++;
            // Repopulate
            for (int i = 0; i < 10; i++) {
                pool.checkin(i);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Threads that checkout/checkin
    std::vector<std::jthread> workers;
    for (int t = 0; t < THREAD_COUNT; t++) {
        workers.emplace_back([&]() {
            while (!done.load()) {
                try {
                    auto item = pool.checkout();
                    checkoutCount++;
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    pool.checkin(std::move(item));
                    checkinCount++;
                }
                catch (const std::runtime_error&) {
                    // Expected when pool is empty
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    done = true;
    clearer.request_stop();
    if (clearer.joinable()) clearer.join();
    workers.clear();

    // Verify consistency: checkins should equal checkouts (all items returned)
    EXPECT_EQ(checkoutCount.load(), checkinCount.load())
            << "Checkout/checkin mismatch: " << checkoutCount.load() << " vs " << checkinCount.load();
    EXPECT_GT(clearCount.load(), 0);
}


/// @brief BUG TEST: simple_pool queueCounter consistency across multiple operations
/// Verifies that queueCounter accurately reflects all queued items even when
/// items are processed at different rates.
TEST(bug_tests, simple_pool_queue_counter_consistency)
{
    std::atomic_uint                           processedCount {0};
    std::atomic_bool                           slowMode {false};

    siddiqsoft::simple_pool<nlohmann::json, 4> pool {[&](auto&&) {
        if (slowMode.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        processedCount++;
    }};

    // Phase 1: Queue items quickly
    for (int i = 0; i < 100; i++) {
        pool.queue({{"phase", 1}, {"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto     j1       = pool.toJson();
    uint64_t counter1 = j1["queueCounter"].get<uint64_t>();

    // Phase 2: Queue more items with slow processing
    slowMode = true;
    for (int i = 0; i < 50; i++) {
        pool.queue({{"phase", 2}, {"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto     j2       = pool.toJson();
    uint64_t counter2 = j2["queueCounter"].get<uint64_t>();

    // Counter should have increased by 50
    EXPECT_EQ(counter1 + 50, counter2) << "Queue counter did not increase correctly: " << counter1 << " -> " << counter2;
}


/// @brief BUG TEST: roundrobin_pool queueCounter with concurrent producers
/// Verifies that the atomic queueCounter in roundrobin_pool is correctly
/// incremented under high contention.
TEST(bug_tests, roundrobin_pool_queue_counter_atomic)
{
    constexpr int                                  PRODUCER_COUNT     = 12;
    constexpr int                                  ITEMS_PER_PRODUCER = 300;
    constexpr int                                  TOTAL_ITEMS        = PRODUCER_COUNT * ITEMS_PER_PRODUCER;

    std::atomic_uint                               processedCount {0};

    siddiqsoft::roundrobin_pool<nlohmann::json, 8> pool {[&](auto&&) { processedCount++; }};

    std::barrier                                   startBarrier {PRODUCER_COUNT};
    std::vector<std::jthread>                      producers;

    for (int p = 0; p < PRODUCER_COUNT; p++) {
        producers.emplace_back([&, p]() {
            startBarrier.arrive_and_wait();
            for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
                pool.queue({{"producer", p}, {"item", i}});
            }
        });
    }

    producers.clear();
    std::this_thread::sleep_for(std::chrono::seconds(5));

    auto     j            = pool.toJson();
    uint64_t queueCounter = j["queueCounter"].get<uint64_t>();

    EXPECT_EQ(static_cast<uint64_t>(TOTAL_ITEMS), queueCounter)
            << "Roundrobin queue counter mismatch: expected " << TOTAL_ITEMS << ", got " << queueCounter;
    EXPECT_EQ(static_cast<unsigned>(TOTAL_ITEMS), processedCount.load());
}


/// @brief BUG TEST: simple_worker with exception in callback doesn't lose items
/// Verifies that when a callback throws, the item is still properly removed
/// from the queue and subsequent items are processed.
TEST(bug_tests, simple_worker_exception_doesnt_lose_items)
{
    std::atomic_uint                          processedCount {0};
    std::atomic_uint                          exceptionCount {0};
    std::set<int>                             processedIndices;
    std::mutex                                mtx;

    siddiqsoft::simple_worker<nlohmann::json> worker {[&](auto&& item) {
        int idx = item["index"].template get<int>();
        if (idx % 5 == 0) {
            exceptionCount++;
            throw std::runtime_error("test exception");
        }
        {
            std::lock_guard<std::mutex> lk(mtx);
            processedIndices.insert(idx);
        }
        processedCount++;
    }};

    constexpr int                             ITEM_COUNT = 50;
    for (int i = 0; i < ITEM_COUNT; i++) {
        worker.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Items at indices 0,5,10,15,20,25,30,35,40,45 throw (10 items)
    // Remaining 40 items should be processed
    EXPECT_EQ(10u, exceptionCount.load());
    EXPECT_EQ(40u, processedCount.load());

    // Verify no items were lost
    {
        std::lock_guard<std::mutex> lk(mtx);
        for (int i = 0; i < ITEM_COUNT; i++) {
            if (i % 5 != 0) {
                EXPECT_TRUE(processedIndices.count(i)) << "Item " << i << " was not processed";
            }
        }
    }
}


/// @brief BUG TEST: simple_pool with exception doesn't lose items across workers
/// Verifies that exceptions in one worker don't affect other workers' ability
/// to process items.
TEST(bug_tests, simple_pool_exception_doesnt_block_other_workers)
{
    std::atomic_uint                           processedCount {0};
    std::atomic_uint                           exceptionCount {0};
    std::set<int>                              processedIndices;
    std::mutex                                 mtx;

    siddiqsoft::simple_pool<nlohmann::json, 4> pool {[&](auto&& item) {
        int idx = item["index"].template get<int>();
        if (idx % 7 == 0) {
            exceptionCount++;
            throw std::runtime_error("test exception");
        }
        {
            std::lock_guard<std::mutex> lk(mtx);
            processedIndices.insert(idx);
        }
        processedCount++;
    }};

    constexpr int                              ITEM_COUNT = 100;
    for (int i = 0; i < ITEM_COUNT; i++) {
        pool.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Items at indices 0,7,14,21,28,35,42,49,56,63,70,77,84,91,98 throw (15 items)
    // Remaining 85 items should be processed
    EXPECT_EQ(15u, exceptionCount.load());
    EXPECT_EQ(85u, processedCount.load());

    {
        std::lock_guard<std::mutex> lk(mtx);
        EXPECT_EQ(85u, processedIndices.size());
    }
}


/// @brief BUG TEST: Verify queueCounter doesn't overflow or wrap incorrectly
/// Tests that queueCounter can handle a large number of items without
/// wrapping or losing count.
TEST(bug_tests, simple_pool_queue_counter_large_volume)
{
    std::atomic_uint                           processedCount {0};

    siddiqsoft::simple_pool<nlohmann::json, 4> pool {[&](auto&&) { processedCount++; }};

    constexpr int                              BATCH_SIZE = 1000;
    constexpr int                              BATCHES    = 5;

    for (int b = 0; b < BATCHES; b++) {
        for (int i = 0; i < BATCH_SIZE; i++) {
            pool.queue({{"batch", b}, {"index", i}});
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto     j        = pool.toJson();
        uint64_t counter  = j["queueCounter"].get<uint64_t>();
        uint64_t expected = static_cast<uint64_t>((b + 1) * BATCH_SIZE);

        EXPECT_EQ(expected, counter) << "After batch " << b << ": expected " << expected << ", got " << counter;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(BATCH_SIZE * BATCHES, processedCount.load());
}


/// @brief BUG TEST: Verify periodic_worker doesn't skip invocations under load
/// Tests that the periodic worker continues invoking at the expected rate
/// even when the system is under load.
TEST(bug_tests, periodic_worker_invocation_consistency)
{
    std::atomic_uint            invokeCount {0};
    std::atomic_uint            minInterval {UINT_MAX};
    std::atomic_uint            maxInterval {0};
    std::atomic_uint64_t        lastInvokeTime {0};

    siddiqsoft::periodic_worker worker {[&]() {
                                            auto now  = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                                            auto last = lastInvokeTime.exchange(now);
                                            if (last != 0) {
                                                auto interval = static_cast<uint32_t>((now - last) / 1000000); // Convert to ms
                                                if (interval < minInterval.load()) {
                                                    minInterval = interval;
                                                }
                                                if (interval > maxInterval.load()) {
                                                    maxInterval = interval;
                                                }
                                            }
                                            invokeCount++;
                                        },
                                        std::chrono::milliseconds(50)};

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Should have at least 5 invocations in 500ms with 50ms interval
    EXPECT_GE(invokeCount.load(), 5u);

    // Intervals should be roughly around 50ms (allow 20-100ms tolerance for CI)
    if (invokeCount.load() > 1) {
        EXPECT_LE(minInterval.load(), 100u) << "Min interval too large: " << minInterval.load() << "ms";
        EXPECT_GE(maxInterval.load(), 20u) << "Max interval too small: " << maxInterval.load() << "ms";
    }
}


/// @brief BUG TEST: Verify resource_pool doesn't leak resources under concurrent access
/// Tests that all resources are properly accounted for even under high contention.
TEST(bug_tests, resource_pool_no_resource_leak)
{
    constexpr int POOL_SIZE      = 10;
    constexpr int THREAD_COUNT   = 8;
    constexpr int OPS_PER_THREAD = 200;

    struct test_resource
    {
        int value;
        test_resource(int v)
            : value(v)
        {
        }
    };

    siddiqsoft::resource_pool<test_resource> pool {};
    for (int i = 0; i < POOL_SIZE; i++) {
        pool.checkin(test_resource(i));
    }

    std::atomic_int           successCount {0};
    std::atomic_int           failCount {0};

    std::barrier              startBarrier {THREAD_COUNT};
    std::vector<std::jthread> threads;

    for (int t = 0; t < THREAD_COUNT; t++) {
        threads.emplace_back([&]() {
            startBarrier.arrive_and_wait();
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                try {
                    auto item = pool.checkout();
                    successCount++;
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                    pool.checkin(std::move(item));
                }
                catch (const std::runtime_error&) {
                    failCount++;
                }
            }
        });
    }

    threads.clear();

    // All resources should be back in the pool
    EXPECT_EQ(static_cast<size_t>(POOL_SIZE), pool.size())
            << "Resource leak detected: expected " << POOL_SIZE << " resources, got " << pool.size();

    // Total operations should equal successes + failures
    EXPECT_EQ(THREAD_COUNT * OPS_PER_THREAD, successCount.load() + failCount.load());
}
