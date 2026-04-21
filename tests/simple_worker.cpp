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
#include <barrier>
#include <vector>
#include <mutex>

#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/simple_worker.hpp"


TEST(simple_worker, test1)
{
    bool                                      passTest {false};

    siddiqsoft::simple_worker<nlohmann::json> worker {[&](auto&& item) {
        std::cerr << std::format("Got object: {}\n", item.dump());
        passTest = true;
    }};

    worker.queue({{"hello", "world"}});

    // This is important otherwise the destructor will kill the thread before it has a chance to process anything!
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_TRUE(passTest);

    std::cerr << worker.toJson().dump() << std::endl;
}


TEST(simple_worker, test2)
{
    bool                                                       passTest {false};

    siddiqsoft::simple_worker<std::shared_ptr<nlohmann::json>> worker {[&](auto&& item) {
        std::cerr << std::format("Got object: {}\n", item->dump());
        passTest = true;
    }};

    worker.queue(std::make_shared<nlohmann::json>(nlohmann::json {{"hello", "world"}}));

    // This is important otherwise the destructor will kill the thread before it has a chance to process anything!
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_TRUE(passTest);
}


TEST(simple_worker, test3)
{
    bool passTest {false};

    struct nonCopyableObject
    {
        std::string Data {};

        nonCopyableObject(const std::string& s)
            : Data(s)
        {
        }

        nonCopyableObject(nonCopyableObject&) = delete;
        nonCopyableObject& operator=(nonCopyableObject&) = delete;

        // Move constructors
        nonCopyableObject(nonCopyableObject&&) = default;
        nonCopyableObject& operator=(nonCopyableObject&&) = default;
    };

    siddiqsoft::simple_worker<nonCopyableObject> worker {[&](auto&& item) {
        std::cerr << std::format("Got object: {}\n", item.Data);
        passTest = true;
    }};

    worker.queue(nonCopyableObject {"Hello world!"});

    // This is important otherwise the destructor will kill the thread before it has a chance to process anything!
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_TRUE(passTest);
}


/// @brief Test queuing multiple items and verifying all are processed
TEST(simple_worker, multiple_items)
{
    constexpr unsigned                        ITEM_COUNT = 50;
    std::atomic_uint                          processedCount {0};

    siddiqsoft::simple_worker<nlohmann::json> worker {[&](auto&& item) {
        processedCount++;
    }};

    for (unsigned i = 0; i < ITEM_COUNT; i++) {
        worker.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(ITEM_COUNT, processedCount.load());

    auto j = worker.toJson();
    EXPECT_EQ(ITEM_COUNT, j["queueCounter"].get<uint64_t>());
}


/// @brief Test that toJson returns expected fields
TEST(simple_worker, toJson_fields)
{
    siddiqsoft::simple_worker<nlohmann::json> worker {[](auto&&) {}};

    worker.queue({{"test", true}});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto j = worker.toJson();
    EXPECT_TRUE(j.contains("_typver"));
    EXPECT_TRUE(j.contains("dequeSize"));
    EXPECT_TRUE(j.contains("queueCounter"));
    EXPECT_TRUE(j.contains("threadPriority"));
    EXPECT_TRUE(j.contains("outstandingCallback"));
    EXPECT_TRUE(j.contains("waitInterval"));
    EXPECT_EQ(0, j["threadPriority"].get<int>());
    EXPECT_EQ(1, j["queueCounter"].get<uint64_t>());
}


/// @brief Test that the worker processes items in FIFO order
TEST(simple_worker, fifo_order)
{
    constexpr unsigned                        ITEM_COUNT = 20;
    std::atomic_uint                          nextExpected {0};
    std::atomic_bool                          orderCorrect {true};

    siddiqsoft::simple_worker<nlohmann::json> worker {[&](auto&& item) {
        unsigned idx = item["index"].template get<unsigned>();
        if (idx != nextExpected.load()) {
            orderCorrect = false;
        }
        nextExpected++;
    }};

    for (unsigned i = 0; i < ITEM_COUNT; i++) {
        worker.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(ITEM_COUNT, nextExpected.load());
    EXPECT_TRUE(orderCorrect.load());
}


/// @brief Test that the worker handles string type items
TEST(simple_worker, string_type)
{
    std::atomic_uint                       processedCount {0};
    std::string                            lastValue;
    std::mutex                             mtx;

    siddiqsoft::simple_worker<std::string> worker {[&](auto&& item) {
        std::lock_guard<std::mutex> lk(mtx);
        lastValue = item;
        processedCount++;
    }};

    worker.queue(std::string("hello"));
    worker.queue(std::string("world"));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_EQ(2u, processedCount.load());
    {
        std::lock_guard<std::mutex> lk(mtx);
        EXPECT_EQ("world", lastValue);
    }
}


/// @brief Test that destroying the worker without processing is safe
TEST(simple_worker, destroy_with_pending_items)
{
    EXPECT_NO_THROW({
        siddiqsoft::simple_worker<nlohmann::json> worker {[](auto&&) {
            // Slow callback — items will be pending when destructor runs
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }};

        for (unsigned i = 0; i < 10; i++) {
            worker.queue({{"index", i}});
        }
        // Destructor fires immediately — should not hang or crash
    });
}


/// @brief Verify the worker continues processing after a callback throws an exception.
/// The worker thread catches all exceptions internally; subsequent items must still be processed.
TEST(simple_worker, callback_exception_resilience)
{
    constexpr unsigned ITEM_COUNT = 20;
    std::atomic_uint   processedCount {0};
    std::atomic_uint   exceptionCount {0};

    siddiqsoft::simple_worker<nlohmann::json> worker {[&](auto&& item) {
        unsigned idx = item["index"].template get<unsigned>();
        if (idx % 3 == 0) {
            exceptionCount++;
            throw std::runtime_error("deliberate test exception");
        }
        processedCount++;
    }};

    for (unsigned i = 0; i < ITEM_COUNT; i++) {
        worker.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Items at indices 0,3,6,9,12,15,18 throw => 7 exceptions
    // Remaining 13 items should be processed successfully
    EXPECT_EQ(7u, exceptionCount.load());
    EXPECT_EQ(ITEM_COUNT - 7u, processedCount.load());
}


/// @brief Stress test: rapidly create and destroy workers in a tight loop.
/// Validates that thread lifecycle management (jthread start/stop/join) is robust
/// under rapid construction/destruction cycles.
TEST(simple_worker, rapid_create_destroy_cycles)
{
    constexpr unsigned CYCLES = 50;

    EXPECT_NO_THROW({
        for (unsigned c = 0; c < CYCLES; c++) {
            siddiqsoft::simple_worker<std::string> worker {[](auto&&) {}};
            worker.queue(std::string("ping"));
            // Immediate destruction — no sleep
        }
    });
}


/// @brief Test queuing from within the callback (re-entrant queue).
/// The callback queues additional work into the same worker. This exercises
/// the lock ordering: the callback runs outside the items_mutex lock, so
/// re-entrant queuing should not deadlock.
TEST(simple_worker, reentrant_queue_from_callback)
{
    std::atomic_uint processedCount {0};
    constexpr unsigned INITIAL_ITEMS = 5;
    // Each initial item spawns one child => total = INITIAL_ITEMS + INITIAL_ITEMS = 10
    constexpr unsigned EXPECTED_TOTAL = INITIAL_ITEMS * 2;

    siddiqsoft::simple_worker<nlohmann::json>* workerPtr = nullptr;

    siddiqsoft::simple_worker<nlohmann::json> worker {[&](auto&& item) {
        processedCount++;
        // If this is a "parent" item, queue a "child" item from within the callback
        if (item.contains("spawn") && item["spawn"].template get<bool>()) {
            workerPtr->queue({{"spawn", false}, {"child_of", item["index"]}});
        }
    }};
    workerPtr = &worker;

    for (unsigned i = 0; i < INITIAL_ITEMS; i++) {
        worker.queue({{"index", i}, {"spawn", true}});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(EXPECTED_TOTAL, processedCount.load());
}


/// @brief High-contention concurrent queue flooding from multiple producer threads.
/// Uses a barrier to synchronize all producers so they start simultaneously,
/// maximizing contention on the internal mutex and semaphore.
TEST(simple_worker, concurrent_producer_flood)
{
    constexpr int  PRODUCER_COUNT    = 8;
    constexpr int  ITEMS_PER_PRODUCER = 100;
    constexpr int  TOTAL_ITEMS       = PRODUCER_COUNT * ITEMS_PER_PRODUCER;
    std::atomic_uint processedCount {0};

    siddiqsoft::simple_worker<nlohmann::json> worker {[&](auto&&) {
        processedCount++;
    }};

    std::barrier              startBarrier {PRODUCER_COUNT};
    std::vector<std::jthread> producers;
    producers.reserve(PRODUCER_COUNT);

    for (int p = 0; p < PRODUCER_COUNT; p++) {
        producers.emplace_back([&, p]() {
            startBarrier.arrive_and_wait();
            for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
                worker.queue({{"producer", p}, {"item", i}});
            }
        });
    }

    // Wait for producers to finish
    producers.clear();
    // Wait for worker to drain
    std::this_thread::sleep_for(std::chrono::seconds(3));

    EXPECT_EQ(static_cast<unsigned>(TOTAL_ITEMS), processedCount.load());
}


/// @brief Burst-queue a large number of items then immediately destroy the worker.
/// Validates that the destructor doesn't hang or crash when the internal deque
/// is full of unprocessed items and the semaphore has a large count.
TEST(simple_worker, burst_queue_then_destroy)
{
    constexpr unsigned BURST_SIZE = 500;
    std::atomic_uint   processedCount {0};

    EXPECT_NO_THROW({
        siddiqsoft::simple_worker<std::string> worker {[&](auto&&) {
            processedCount++;
        }};

        for (unsigned i = 0; i < BURST_SIZE; i++) {
            worker.queue(std::format("burst-{}", i));
        }
        // Immediate destruction — some items will be processed, many won't
    });

    // We just verify it didn't crash/hang. Some items may have been processed.
    EXPECT_LE(processedCount.load(), BURST_SIZE);
}


/// @brief Worker that never receives any items should shut down cleanly.
/// Tests the idle path where the semaphore times out repeatedly until destruction.
TEST(simple_worker, idle_worker_clean_shutdown)
{
    EXPECT_NO_THROW({
        siddiqsoft::simple_worker<std::string> worker {[](auto&&) {
            FAIL() << "Callback should never be invoked on an idle worker";
        }};
        // Let it sit idle for a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        // Destructor fires — should not hang
    });
}
