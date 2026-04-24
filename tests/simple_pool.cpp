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
#include <barrier>
#include <atomic>

#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/simple_pool.hpp"

TEST(simple_pool, test1)
{
    std::atomic_uint                        passTest {0};

    siddiqsoft::simple_pool<nlohmann::json> workers {[&passTest](auto&& item) {
        std::cerr << std::format("Item:{} .. Got object: {}\n", passTest.load(), item.dump());
        passTest++;
    }};

    for (unsigned i = 0; i < std::thread::hardware_concurrency(); i++) {
        workers.queue({{"test", "simple_pool"}, {"hello", "world"}, {"i", i}});
    }

    // This is important otherwise the destructor will kill the thread before it has a chance to process anything!
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(passTest.load(), std::thread::hardware_concurrency());
    std::cerr << nlohmann::json(workers).dump() << std::endl;
}


TEST(simple_pool, test2)
{
    const auto                FEEDER_COUNT = 2;
    std::barrier              startFeeding {FEEDER_COUNT};
    constexpr auto            WORKER_POOLSIZE = 2 * FEEDER_COUNT;
    std::atomic_uint          passTest {0};
    std::vector<std::jthread> feeders {};

    // The target is our workers
    siddiqsoft::simple_pool<nlohmann::json, WORKER_POOLSIZE> workers {[&passTest](auto&& item) {
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
                workers.queue({{"test", "simple_pool"}, {"hello", "world"}, {"p2j", std::to_string(j)}});
            }
        });
    }

    // This is important otherwise the destructor will kill the thread before it has a chance to process anything!
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(passTest.load(), FEEDER_COUNT * WORKER_POOLSIZE);
}


/*
 * The test is to check if we have the proper capability to handle default copy/move constructors/operators
 * for the simple_pool object. See the issue: https://github.com/SiddiqSoft/asynchrony-lib/issues/3
 */


class meow_type : public nlohmann::json
{
public:
    static const inline std::string L1 = "meow_type";

    meow_type(const nlohmann::json& src)
    {
        this->update(src);
    }

    meow_type(nlohmann::json&& src)
    {
        this->swap(src);
        // static_cast<nlohmann::json>(*this).operator=(std::move(src));
    }

    meow_type(meow_type&&) = default;


    meow_type(const meow_type& src)
    {
        this->update(nlohmann::json(src));
    }

    meow_type& operator=(meow_type&& src) = default;


    meow_type& operator                   =(nlohmann::json&& src)
    {
        this->swap(src);
        return *this;
    }
};


struct cat_type
{
    cat_type(nlohmann::json&& src, std::string&& n)
        : meow(std::move(src))
        , name(std::move(n))
    {
    }
    meow_type   meow;
    std::string name {};
};


TEST(simple_pool, test3)
{
    std::atomic_uint                  passTest {0};


    siddiqsoft::simple_pool<cat_type> workers {[&passTest](auto&& item) {
        std::cerr << std::format("Item:{} .. Got object: >{} -- {}<\n", passTest.load(), item.meow.dump(), item.name);
        passTest++;
    }};

    for (unsigned i = 0; i < std::thread::hardware_concurrency(); i++) {
        try {
            workers.queue(
                    cat_type(nlohmann::json {{"test", "simple_pool"}, {"hello", "world"}, {"i", i}}, std::format("test3..{}", i)));
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    // This is important otherwise the destructor will kill the thread before it has a chance to process anything!
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    EXPECT_EQ(passTest.load(), std::thread::hardware_concurrency());
    std::cerr << nlohmann::json(workers).dump() << std::endl;
}


/// @brief Test simple_pool with a fixed small pool size
TEST(simple_pool, fixed_pool_size)
{
    constexpr uint16_t POOL_SIZE  = 2;
    constexpr unsigned ITEM_COUNT = 20;
    std::atomic_uint   passTest {0};

    siddiqsoft::simple_pool<nlohmann::json, POOL_SIZE> workers {[&passTest](auto&& item) {
        passTest++;
    }};

    for (unsigned i = 0; i < ITEM_COUNT; i++) {
        workers.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(ITEM_COUNT, passTest.load());
}


/// @brief Test toJson returns expected fields
TEST(simple_pool, toJson_fields)
{
    siddiqsoft::simple_pool<nlohmann::json, 2> workers {[](auto&&) {}};

    workers.queue({{"test", true}});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto j = workers.toJson();
    EXPECT_TRUE(j.contains("_typver"));
    EXPECT_TRUE(j.contains("workersSize"));
    EXPECT_TRUE(j.contains("dequeSize"));
    EXPECT_TRUE(j.contains("queueCounter"));
    EXPECT_TRUE(j.contains("waitInterval"));
    EXPECT_EQ(2u, j["workersSize"].get<unsigned>());
    EXPECT_EQ(1u, j["queueCounter"].get<uint64_t>());
}


/// @brief Test that destroying the pool with no items queued is safe
TEST(simple_pool, destroy_empty)
{
    using pool_t = siddiqsoft::simple_pool<nlohmann::json, 2>;
    EXPECT_NO_THROW({
        pool_t workers {[](auto&&) {}};
        // Destructor fires immediately — should not hang or crash
    });
}


/// @brief Test that destroying the pool with pending items is safe
TEST(simple_pool, destroy_with_pending)
{
    using pool_t = siddiqsoft::simple_pool<nlohmann::json, 2>;
    EXPECT_NO_THROW({
        pool_t workers {[](auto&&) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }};

        for (unsigned i = 0; i < 10; i++) {
            workers.queue({{"index", i}});
        }
        // Destructor fires — should not hang indefinitely or crash
    });
}


/// @brief Test with string type items
TEST(simple_pool, string_type)
{
    constexpr unsigned ITEM_COUNT = 30;
    std::atomic_uint   passTest {0};

    siddiqsoft::simple_pool<std::string, 4> workers {[&passTest](auto&& item) {
        EXPECT_FALSE(item.empty());
        passTest++;
    }};

    for (unsigned i = 0; i < ITEM_COUNT; i++) {
        workers.queue(std::format("item-{}", i));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(ITEM_COUNT, passTest.load());
}


/// @brief Verify the pool continues processing after callbacks throw exceptions.
/// All pool threads catch exceptions internally; items that don't throw should
/// still be processed even when interleaved with throwing items.
TEST(simple_pool, callback_exception_resilience)
{
    constexpr unsigned ITEM_COUNT = 40;
    std::atomic_uint   processedCount {0};
    std::atomic_uint   exceptionCount {0};

    siddiqsoft::simple_pool<nlohmann::json, 4> workers {[&](auto&& item) {
        unsigned idx = item["index"].template get<unsigned>();
        if (idx % 5 == 0) {
            exceptionCount++;
            throw std::runtime_error("deliberate pool exception");
        }
        processedCount++;
    }};

    for (unsigned i = 0; i < ITEM_COUNT; i++) {
        workers.queue({{"index", i}});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Indices 0,5,10,15,20,25,30,35 throw => 8 exceptions
    // Remaining 32 items should be processed
    EXPECT_EQ(8u, exceptionCount.load());
    EXPECT_EQ(ITEM_COUNT - 8u, processedCount.load());
}


/// @brief Stress test: rapidly create and destroy pools in a tight loop.
/// Validates that multiple jthread lifecycle management is robust under
/// rapid construction/destruction cycles.
TEST(simple_pool, rapid_create_destroy_cycles)
{
    using pool_t = siddiqsoft::simple_pool<std::string, 4>;
    constexpr unsigned CYCLES = 30;

    EXPECT_NO_THROW({
        for (unsigned c = 0; c < CYCLES; c++) {
            pool_t workers {[](auto&&) {}};
            workers.queue(std::string("ping"));
            // Immediate destruction — no sleep
        }
    });
}


/// @brief High-contention concurrent queue flooding from many producer threads
/// into a pool with multiple consumer threads. Uses a barrier for maximum contention.
TEST(simple_pool, concurrent_producer_flood)
{
    constexpr int  PRODUCER_COUNT     = 8;
    constexpr int  ITEMS_PER_PRODUCER = 100;
    constexpr int  TOTAL_ITEMS        = PRODUCER_COUNT * ITEMS_PER_PRODUCER;
    constexpr int  POOL_SIZE          = 4;
    std::atomic_uint processedCount {0};

    siddiqsoft::simple_pool<nlohmann::json, POOL_SIZE> workers {[&](auto&&) {
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
    // Wait for pool to drain
    std::this_thread::sleep_for(std::chrono::seconds(3));

    EXPECT_EQ(static_cast<unsigned>(TOTAL_ITEMS), processedCount.load());
}


/// @brief Burst-queue a large number of items then immediately destroy the pool.
/// Validates that the destructor properly shuts down all threads even when
/// the deque is full and the semaphore has a large count.
TEST(simple_pool, burst_queue_then_destroy)
{
    using pool_t = siddiqsoft::simple_pool<std::string, 4>;
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


/// @brief Test that a slow callback on one thread doesn't block other threads
/// in the pool from processing their items.
TEST(simple_pool, slow_callback_doesnt_block_others)
{
    std::atomic_uint fastCount {0};
    std::atomic_bool slowStarted {false};

    siddiqsoft::simple_pool<nlohmann::json, 4> workers {[&](auto&& item) {
        if (item.contains("slow") && item["slow"].template get<bool>()) {
            slowStarted = true;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } else {
            fastCount++;
        }
    }};

    // Queue one slow item first
    workers.queue({{"slow", true}});
    // Then queue many fast items
    for (unsigned i = 0; i < 20; i++) {
        workers.queue({{"slow", false}, {"index", i}});
    }

    // Wait enough for fast items but not for the slow one to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // The slow item should have started, and the fast items should have been
    // processed by other threads in the pool
    EXPECT_TRUE(slowStarted.load());
    EXPECT_EQ(20u, fastCount.load());
}


/// @brief Pool that never receives any items should shut down cleanly.
TEST(simple_pool, idle_pool_clean_shutdown)
{
    using pool_t = siddiqsoft::simple_pool<std::string, 4>;
    EXPECT_NO_THROW({
        pool_t workers {[](auto&&) {
            FAIL() << "Callback should never be invoked on an idle pool";
        }};
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    });
}


/// @brief Test the ADL to_json free function for simple_pool.
/// This exercises the nlohmann::json serialization path via `nlohmann::json j = pool;`
/// which invokes the free `to_json(json&, const simple_pool<T, N>&)` function.
TEST(simple_pool, adl_to_json)
{
    siddiqsoft::simple_pool<nlohmann::json, 2> workers {[](auto&&) {}};

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


/// @brief Test queuing from within the callback (re-entrant queue into the pool).
/// The callback runs outside the items_mutex lock, so re-entrant queuing
/// should not deadlock.
TEST(simple_pool, reentrant_queue_from_callback)
{
    std::atomic_uint processedCount {0};
    constexpr unsigned INITIAL_ITEMS = 10;
    constexpr unsigned EXPECTED_TOTAL = INITIAL_ITEMS * 2;

    siddiqsoft::simple_pool<nlohmann::json, 4>* poolPtr = nullptr;

    siddiqsoft::simple_pool<nlohmann::json, 4> workers {[&](auto&& item) {
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
