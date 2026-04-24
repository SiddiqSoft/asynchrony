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
#include <barrier>
#include <vector>
#include <atomic>

#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/roundrobin_pool.hpp"


TEST(roundrobin_pool, test1)
{
    std::atomic_uint                            passTest {0};

    siddiqsoft::roundrobin_pool<nlohmann::json> workers {[&passTest](auto&& item) {
        std::cerr << std::format("Item:{} .. Got object: {}\n", passTest.load(), item.dump());
        passTest++;
    }};

    for (unsigned i = 0; i < std::thread::hardware_concurrency(); i++) {
        workers.queue({{"test", "roundrobin_pool"}, {"hello", "world"}, {"i", i}});
    }

    // This is important otherwise the destructor will kill the thread before it has a chance to process anything!
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(passTest.load(), std::thread::hardware_concurrency());
    std::cerr << nlohmann::json(workers).dump() << std::endl;
}


TEST(roundrobin_pool, test2)
{
    constexpr auto            FEEDER_COUNT = 8;
    std::barrier              startFeeding {FEEDER_COUNT};
    constexpr auto            WORKER_POOLSIZE = 8 * FEEDER_COUNT;
    std::atomic_uint          passTest {0};
    std::vector<std::jthread> feeders {};

    // The target is our workers
    siddiqsoft::roundrobin_pool<nlohmann::json, WORKER_POOLSIZE> workers {[&passTest](auto&& item) {
        std::cerr << std::this_thread::get_id() << std::format("..Item {:03} .. Got object: {}\n", passTest.load(), item.dump());
        passTest++;
    }};

    // This is our thread pool that will try to inject into the workers all at the same time
    for (auto f = 0; f < FEEDER_COUNT; f++) {
        feeders.emplace_back([&]() {
            // Each thread waits
            startFeeding.arrive_and_wait();
            // ..until everyone's "arrived" and then they all simultaneously should feed into the workers
            // with the objective that we excercise the correctness and shake out any potential race condition
            for (auto j = 0; j < WORKER_POOLSIZE; j++) {
                workers.queue({{"test", "roundrobin_pool"}, {"hello", "world"}, {"j", j}});
            }
        });
    }

    // This is important otherwise the destructor will kill the thread before it has a chance to process anything!
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(passTest.load(), FEEDER_COUNT * WORKER_POOLSIZE);
}


/// @brief Test roundrobin_pool with a fixed small pool size
TEST(roundrobin_pool, fixed_pool_size)
{
    constexpr uint16_t POOL_SIZE  = 4;
    constexpr unsigned ITEM_COUNT = 40;
    std::atomic_uint   passTest {0};

    siddiqsoft::roundrobin_pool<nlohmann::json, POOL_SIZE> workers {[&passTest](auto&& item) {
        passTest++;
    }};

    for (unsigned i = 0; i < ITEM_COUNT; i++) {
        workers.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(ITEM_COUNT, passTest.load());
}


/// @brief Test toJson returns expected fields
TEST(roundrobin_pool, toJson_fields)
{
    siddiqsoft::roundrobin_pool<nlohmann::json, 2> workers {[](auto&&) {}};

    workers.queue({{"test", true}});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto j = workers.toJson();
    EXPECT_TRUE(j.contains("_typver"));
    EXPECT_TRUE(j.contains("workersSize"));
    EXPECT_TRUE(j.contains("queueCounter"));
    EXPECT_EQ(2u, j["workersSize"].get<unsigned>());
    EXPECT_EQ(1u, j["queueCounter"].get<uint64_t>());
}


/// @brief Test that destroying the pool with no items queued is safe
TEST(roundrobin_pool, destroy_empty)
{
    using pool_t = siddiqsoft::roundrobin_pool<nlohmann::json, 2>;
    EXPECT_NO_THROW({
        pool_t workers {[](auto&&) {}};
    });
}


/// @brief Test that destroying the pool with pending items is safe
TEST(roundrobin_pool, destroy_with_pending)
{
    using pool_t = siddiqsoft::roundrobin_pool<nlohmann::json, 2>;
    EXPECT_NO_THROW({
        pool_t workers {[](auto&&) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }};

        for (unsigned i = 0; i < 10; i++) {
            workers.queue({{"index", i}});
        }
    });
}


/// @brief Test with string type items
TEST(roundrobin_pool, string_type)
{
    constexpr unsigned ITEM_COUNT = 30;
    std::atomic_uint   passTest {0};

    siddiqsoft::roundrobin_pool<std::string, 4> workers {[&passTest](auto&& item) {
        EXPECT_FALSE(item.empty());
        passTest++;
    }};

    for (unsigned i = 0; i < ITEM_COUNT; i++) {
        workers.queue(std::format("item-{}", i));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(ITEM_COUNT, passTest.load());
}


/// @brief Test that a single worker pool processes all items
TEST(roundrobin_pool, single_worker)
{
    constexpr unsigned ITEM_COUNT = 10;
    std::atomic_uint   passTest {0};

    siddiqsoft::roundrobin_pool<nlohmann::json, 1> workers {[&passTest](auto&& item) {
        passTest++;
    }};

    for (unsigned i = 0; i < ITEM_COUNT; i++) {
        workers.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(ITEM_COUNT, passTest.load());
}


/// @brief Verify the roundrobin pool continues processing after callbacks throw.
/// Each worker thread catches exceptions independently; items routed to non-throwing
/// workers should still be processed.
TEST(roundrobin_pool, callback_exception_resilience)
{
    constexpr unsigned ITEM_COUNT = 40;
    std::atomic_uint   processedCount {0};
    std::atomic_uint   exceptionCount {0};

    siddiqsoft::roundrobin_pool<nlohmann::json, 4> workers {[&](auto&& item) {
        unsigned idx = item["index"].template get<unsigned>();
        if (idx % 7 == 0) {
            exceptionCount++;
            throw std::runtime_error("deliberate roundrobin exception");
        }
        processedCount++;
    }};

    for (unsigned i = 0; i < ITEM_COUNT; i++) {
        workers.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Indices 0,7,14,21,28,35 throw => 6 exceptions
    // Remaining 34 items should be processed
    EXPECT_EQ(6u, exceptionCount.load());
    EXPECT_EQ(ITEM_COUNT - 6u, processedCount.load());
}


/// @brief Stress test: rapidly create and destroy roundrobin pools.
/// Validates that the vector of simple_worker lifecycle management is robust
/// under rapid construction/destruction cycles.
TEST(roundrobin_pool, rapid_create_destroy_cycles)
{
    using pool_t = siddiqsoft::roundrobin_pool<std::string, 4>;
    constexpr unsigned CYCLES = 30;

    EXPECT_NO_THROW({
        for (unsigned c = 0; c < CYCLES; c++) {
            pool_t workers {[](auto&&) {}};
            workers.queue(std::string("ping"));
            // Immediate destruction — no sleep
        }
    });
}


/// @brief High-contention concurrent queue flooding from many producer threads.
/// The round-robin distribution means each producer's items may go to different
/// workers, exercising the atomic counter and per-worker queue concurrently.
TEST(roundrobin_pool, concurrent_producer_flood)
{
    constexpr int  PRODUCER_COUNT     = 8;
    constexpr int  ITEMS_PER_PRODUCER = 100;
    constexpr int  TOTAL_ITEMS        = PRODUCER_COUNT * ITEMS_PER_PRODUCER;
    constexpr int  POOL_SIZE          = 4;
    std::atomic_uint processedCount {0};

    siddiqsoft::roundrobin_pool<nlohmann::json, POOL_SIZE> workers {[&](auto&&) {
        processedCount++;
    }};

    std::barrier              startBarrier {PRODUCER_COUNT};
    std::vector<std::jthread> producers;
    producers.reserve(PRODUCER_COUNT);

    for (int p = 0; p < PRODUCER_COUNT; p++) {
        producers.emplace_back([&, p]() {
            startBarrier.arrive_and_wait();
            for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
                workers.queue({{"producer", p}, {"item", i}});
            }
        });
    }

    // Wait for producers to finish
    producers.clear();
    // Wait for workers to drain
    std::this_thread::sleep_for(std::chrono::seconds(3));

    EXPECT_EQ(static_cast<unsigned>(TOTAL_ITEMS), processedCount.load());
}


/// @brief Burst-queue a large number of items then immediately destroy.
/// Validates that the destructor properly shuts down all underlying simple_workers
/// even when their deques are full.
TEST(roundrobin_pool, burst_queue_then_destroy)
{
    using pool_t = siddiqsoft::roundrobin_pool<std::string, 4>;
    constexpr unsigned BURST_SIZE = 500;
    std::atomic_uint   processedCount {0};

    EXPECT_NO_THROW({
        pool_t workers {[&](auto&&) {
            processedCount++;
        }};

        for (unsigned i = 0; i < BURST_SIZE; i++) {
            workers.queue(std::format("burst-{}", i));
        }
        // Immediate destruction
    });

    EXPECT_LE(processedCount.load(), BURST_SIZE);
}


/// @brief Test that a slow callback on one worker doesn't block items routed
/// to other workers. Round-robin ensures items are distributed, so other workers
/// should continue processing while one is blocked.
TEST(roundrobin_pool, slow_worker_doesnt_block_others)
{
    // With 4 workers and round-robin, item 0 goes to worker 0, item 1 to worker 1, etc.
    std::atomic_uint fastCount {0};
    std::atomic_bool slowStarted {false};

    siddiqsoft::roundrobin_pool<nlohmann::json, 4> workers {[&](auto&& item) {
        if (item.contains("slow") && item["slow"].template get<bool>()) {
            slowStarted = true;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } else {
            fastCount++;
        }
    }};

    // First item (index 0) goes to worker 0 — make it slow
    workers.queue({{"slow", true}, {"index", 0}});

    // Next items go to workers 1,2,3,0,1,2,3,... — make them fast
    // Items at indices 4,8,12,... also go to worker 0 and will be queued behind the slow one
    // But items at indices 1,2,3,5,6,7,9,10,11 go to workers 1,2,3 and should process quickly
    for (unsigned i = 1; i <= 12; i++) {
        workers.queue({{"slow", false}, {"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_TRUE(slowStarted.load());
    // Workers 1,2,3 should have processed their items (indices 1,2,3,5,6,7,9,10,11 = 9 items)
    // Worker 0 is blocked on the slow item, so items 4,8,12 are queued behind it
    EXPECT_GE(fastCount.load(), 9u);
}


/// @brief Pool that never receives any items should shut down cleanly.
TEST(roundrobin_pool, idle_pool_clean_shutdown)
{
    using pool_t = siddiqsoft::roundrobin_pool<std::string, 4>;
    EXPECT_NO_THROW({
        pool_t workers {[](auto&&) {
            FAIL() << "Callback should never be invoked on an idle pool";
        }};
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    });
}


/// @brief Test the ADL to_json free function for roundrobin_pool.
/// This exercises the nlohmann::json serialization path via `nlohmann::json j = pool;`
/// which invokes the free `to_json(json&, const roundrobin_pool<T, N>&)` function.
TEST(roundrobin_pool, adl_to_json)
{
    siddiqsoft::roundrobin_pool<nlohmann::json, 2> workers {[](auto&&) {}};

    workers.queue({{"test", "adl"}});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // This uses the to_json free function, not the member toJson()
    nlohmann::json j;
    siddiqsoft::to_json(j, workers);
    EXPECT_TRUE(j.contains("_typver"));
    EXPECT_TRUE(j.contains("workersSize"));
    EXPECT_TRUE(j.contains("queueCounter"));
    EXPECT_EQ(2u, j["workersSize"].get<unsigned>());
    EXPECT_EQ(1u, j["queueCounter"].get<uint64_t>());
    std::cerr << "to_json result: " << j.dump() << std::endl;
}


/// @brief Test queuing from within the callback (re-entrant queue).
/// Each simple_worker's callback runs outside its items_mutex lock, so
/// re-entrant queuing into the parent roundrobin_pool should not deadlock.
TEST(roundrobin_pool, reentrant_queue_from_callback)
{
    std::atomic_uint processedCount {0};
    constexpr unsigned INITIAL_ITEMS = 8;
    constexpr unsigned EXPECTED_TOTAL = INITIAL_ITEMS * 2;

    siddiqsoft::roundrobin_pool<nlohmann::json, 4>* poolPtr = nullptr;

    siddiqsoft::roundrobin_pool<nlohmann::json, 4> workers {[&](auto&& item) {
        processedCount++;
        if (item.contains("spawn") && item["spawn"].template get<bool>()) {
            poolPtr->queue({{"spawn", false}, {"child_of", item["index"]}});
        }
    }};
    poolPtr = &workers;

    for (unsigned i = 0; i < INITIAL_ITEMS; i++) {
        workers.queue({{"index", i}, {"spawn", true}});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(EXPECTED_TOTAL, processedCount.load());
}
