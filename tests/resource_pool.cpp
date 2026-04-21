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
#include <vector>
#include <barrier>


#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/resource_pool.hpp"


TEST(resource_pool, T_int)
{
    bool passTest {false};

    EXPECT_NO_THROW({
        siddiqsoft::resource_pool<int> rp {};
        std::cerr << std::format("{} - Capacity:{}\n", __func__, rp.size());
        passTest = true;
    });

    EXPECT_TRUE(passTest);
}

TEST(resource_pool, T_shared_ptr_string)
{
    bool passTest {false};

    EXPECT_NO_THROW({
        siddiqsoft::resource_pool<std::shared_ptr<std::string>> rp {};

        EXPECT_EQ(0, rp.size());
        rp.checkin(std::shared_ptr<std::string>(new std::string(__TIME__)));
        EXPECT_EQ(1, rp.size());

        auto item = rp.checkout();
        EXPECT_EQ(0, rp.size());
        EXPECT_EQ(__TIME__, *item);
        (*item).append("-ok");

        rp.checkin(std::move(item));
        EXPECT_EQ(1, rp.size());

        auto item2 = rp.checkout();
        EXPECT_EQ(0, rp.size());
        EXPECT_TRUE(item2->ends_with("-ok"));

        rp.checkin(std::move(item2));
        EXPECT_EQ(1, rp.size());

        passTest = true;
    });

    EXPECT_TRUE(passTest);
}


TEST(resource_pool, T_unique_ptr_string)
{
    bool passTest {false};

    EXPECT_NO_THROW({
        siddiqsoft::resource_pool<std::unique_ptr<std::string>> rp {};

        EXPECT_EQ(0, rp.size());
        rp.checkin(std::unique_ptr<std::string>(new std::string(__TIME__)));
        EXPECT_EQ(1, rp.size());

        auto item = rp.checkout();
        EXPECT_EQ(0, rp.size());
        EXPECT_EQ(__TIME__, *item);
        (*item).append("-ok");

        rp.checkin(std::move(item));
        EXPECT_EQ(1, rp.size());

        auto item2 = rp.checkout();
        EXPECT_EQ(0, rp.size());
        EXPECT_TRUE(item2->ends_with("-ok"));

        rp.checkin(std::move(item2));
        EXPECT_EQ(1, rp.size());

        passTest = true;
    });

    EXPECT_TRUE(passTest);
}

TEST(resource_pool, T_checkin_checkout_unique_ptr_string)
{
    bool passTest {false};

    EXPECT_NO_THROW({
        siddiqsoft::resource_pool<std::unique_ptr<std::string>> rp {};

        EXPECT_EQ(0, rp.size());
        rp.checkin(std::unique_ptr<std::string>(new std::string(__TIME__)));
        EXPECT_EQ(1, rp.size());

        // Immediately push back in.. we're testing to make sure that there is
        // no leakage!
        rp.checkin(rp.checkout());
        EXPECT_EQ(1, rp.size());

        auto item2 = rp.checkout();
        EXPECT_EQ(0, rp.size());
        EXPECT_EQ(__TIME__, *item2);

        passTest = true;
    });

    EXPECT_TRUE(passTest);
}



TEST(resource_pool, T_checkin_checkout_vector_string)
{
    bool passTest {false};

    EXPECT_NO_THROW({
        siddiqsoft::resource_pool<std::vector<std::string>> rp {};

        EXPECT_EQ(0, rp.size());
        rp.checkin({"A", "B", "C"});
        EXPECT_EQ(1, rp.size());

        // Immediately push back in.. we're testing to make sure that there is
        // no leakage!
        rp.checkin(rp.checkout());
        EXPECT_EQ(1, rp.size());

        auto item2 = rp.checkout();
        item2.push_back("1");
        item2.push_back("2");
        item2.push_back("3");
        EXPECT_EQ(0, rp.size());
        EXPECT_EQ(6, item2.size());

        rp.checkin(std::move(item2));
        // The resource is now empty; the checking moved now owns it.
        EXPECT_EQ(0, item2.size());

        passTest = true;
    });

    EXPECT_TRUE(passTest);
}


/// @brief Test that checkout on an empty pool throws
TEST(resource_pool, checkout_empty_throws)
{
    siddiqsoft::resource_pool<int> rp {};
    EXPECT_THROW({ [[maybe_unused]] auto v = rp.checkout(); }, std::runtime_error);
}


/// @brief Test clear empties the pool
TEST(resource_pool, clear)
{
    siddiqsoft::resource_pool<int> rp {};
    rp.checkin(1);
    rp.checkin(2);
    rp.checkin(3);
    EXPECT_EQ(3u, rp.size());

    rp.clear();
    EXPECT_EQ(0u, rp.size());

    // After clear, checkout should throw
    EXPECT_THROW({ [[maybe_unused]] auto v = rp.checkout(); }, std::runtime_error);
}


/// @brief Test multiple checkin/checkout cycles
TEST(resource_pool, multiple_items)
{
    siddiqsoft::resource_pool<int> rp {};

    for (int i = 0; i < 10; i++) {
        rp.checkin(std::move(i));
    }
    EXPECT_EQ(10u, rp.size());

    // Checkout all items (FIFO order)
    for (int i = 0; i < 10; i++) {
        auto item = rp.checkout();
        EXPECT_EQ(i, item);
    }
    EXPECT_EQ(0u, rp.size());
}


/// @brief Test that checkin after checkout preserves the resource
TEST(resource_pool, round_trip_preserves_value)
{
    siddiqsoft::resource_pool<std::string> rp {};

    rp.checkin(std::string("hello"));
    auto item = rp.checkout();
    EXPECT_EQ("hello", item);

    item += " world";
    rp.checkin(std::move(item));

    auto item2 = rp.checkout();
    EXPECT_EQ("hello world", item2);
}


/// @brief Test with nlohmann::json type
TEST(resource_pool, json_type)
{
    siddiqsoft::resource_pool<nlohmann::json> rp {};

    rp.checkin(nlohmann::json {{"key", "value"}});
    EXPECT_EQ(1u, rp.size());

    auto item = rp.checkout();
    EXPECT_EQ("value", item["key"].get<std::string>());
    EXPECT_EQ(0u, rp.size());
}


/// @brief Test concurrent checkin/checkout from multiple threads
TEST(resource_pool, concurrent_access)
{
    siddiqsoft::resource_pool<int> rp {};
    constexpr int                  ITERATIONS = 100;
    std::atomic_int                checkoutCount {0};

    // Pre-fill the pool
    for (int i = 0; i < ITERATIONS; i++) {
        rp.checkin(std::move(i));
    }

    std::vector<std::jthread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < ITERATIONS / 4; i++) {
                try {
                    auto item = rp.checkout();
                    checkoutCount++;
                    // Simulate some work
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    rp.checkin(std::move(item));
                }
                catch (const std::runtime_error&) {
                    // Pool was empty — that's OK in concurrent scenario
                }
            }
        });
    }

    // Wait for threads to finish (jthread joins automatically)
    threads.clear();

    // All items should be back in the pool
    EXPECT_EQ(ITERATIONS, static_cast<int>(rp.size()));
}


/// @brief Test that double clear is safe
TEST(resource_pool, double_clear)
{
    siddiqsoft::resource_pool<int> rp {};
    rp.checkin(42);
    rp.clear();
    EXPECT_EQ(0u, rp.size());

    // Second clear on empty pool should be safe
    EXPECT_NO_THROW(rp.clear());
    EXPECT_EQ(0u, rp.size());
}


/// @brief Starvation test: many threads compete for a small pool.
/// Some threads will get resources, others will fail with exceptions.
/// All resources must be returned to the pool at the end.
TEST(resource_pool, starvation_under_contention)
{
    constexpr int POOL_SIZE    = 3;
    constexpr int THREAD_COUNT = 8;
    constexpr int OPS_PER_THREAD = 50;

    siddiqsoft::resource_pool<int> rp {};
    for (int i = 0; i < POOL_SIZE; i++) {
        rp.checkin(int(i));
    }

    std::atomic_int successCount {0};
    std::atomic_int failCount {0};

    std::vector<std::jthread> threads;
    std::barrier              startBarrier {THREAD_COUNT};

    for (int t = 0; t < THREAD_COUNT; t++) {
        threads.emplace_back([&]() {
            startBarrier.arrive_and_wait();
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                try {
                    auto item = rp.checkout();
                    successCount++;
                    // Simulate work
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                    rp.checkin(std::move(item));
                }
                catch (const std::runtime_error&) {
                    failCount++;
                }
            }
        });
    }

    threads.clear();

    // All resources should be back in the pool
    EXPECT_EQ(static_cast<size_t>(POOL_SIZE), rp.size());
    // At least some operations should have succeeded
    EXPECT_GT(successCount.load(), 0);
    // Total operations = successes + failures
    EXPECT_EQ(THREAD_COUNT * OPS_PER_THREAD, successCount.load() + failCount.load());
}


/// @brief Test concurrent clear racing with checkout/checkin operations.
/// One thread clears the pool while others are actively checking in/out.
/// No crashes or deadlocks should occur.
TEST(resource_pool, concurrent_clear_with_operations)
{
    siddiqsoft::resource_pool<int> rp {};
    constexpr int INITIAL_SIZE = 20;

    for (int i = 0; i < INITIAL_SIZE; i++) {
        rp.checkin(int(i));
    }

    std::atomic_bool done {false};
    std::atomic_int  clearCount {0};

    // Thread that periodically clears the pool
    std::jthread clearer([&](std::stop_token st) {
        while (!st.stop_requested() && !done.load()) {
            rp.clear();
            clearCount++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            // Re-populate
            for (int i = 0; i < 5; i++) {
                rp.checkin(int(i));
            }
        }
    });

    // Threads that checkout/checkin
    std::vector<std::jthread> workers;
    for (int t = 0; t < 4; t++) {
        workers.emplace_back([&]() {
            for (int i = 0; i < 100; i++) {
                try {
                    auto item = rp.checkout();
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    rp.checkin(std::move(item));
                }
                catch (const std::runtime_error&) {
                    // Pool was empty — expected during clear
                }
            }
        });
    }

    workers.clear();
    done = true;
    clearer.request_stop();
    if (clearer.joinable()) clearer.join();

    EXPECT_GT(clearCount.load(), 0);
}


/// @brief Test high-throughput checkin/checkout cycling from many threads.
/// Each thread does many rapid checkout-then-checkin cycles. This exercises
/// the recursive_mutex under high contention.
TEST(resource_pool, high_throughput_cycling)
{
    constexpr int POOL_SIZE    = 8;
    constexpr int THREAD_COUNT = 8;
    constexpr int CYCLES       = 200;

    siddiqsoft::resource_pool<std::string> rp {};
    for (int i = 0; i < POOL_SIZE; i++) {
        rp.checkin(std::format("resource-{}", i));
    }

    std::atomic_int totalCheckouts {0};
    std::barrier    startBarrier {THREAD_COUNT};

    std::vector<std::jthread> threads;
    for (int t = 0; t < THREAD_COUNT; t++) {
        threads.emplace_back([&]() {
            startBarrier.arrive_and_wait();
            for (int c = 0; c < CYCLES; c++) {
                try {
                    auto item = rp.checkout();
                    totalCheckouts++;
                    // Immediately return
                    rp.checkin(std::move(item));
                }
                catch (const std::runtime_error&) {
                    // Pool was momentarily empty
                }
            }
        });
    }

    threads.clear();

    // All resources should be back
    EXPECT_EQ(static_cast<size_t>(POOL_SIZE), rp.size());
    EXPECT_GT(totalCheckouts.load(), 0);
}


/// @brief Test that checkout on a pool that was populated then fully drained throws.
/// Validates the exception path after legitimate use, not just on a fresh empty pool.
TEST(resource_pool, checkout_after_drain_throws)
{
    siddiqsoft::resource_pool<int> rp {};
    rp.checkin(1);
    rp.checkin(2);

    [[maybe_unused]] auto a = rp.checkout();
    [[maybe_unused]] auto b = rp.checkout();
    EXPECT_EQ(0u, rp.size());

    EXPECT_THROW({ [[maybe_unused]] auto v = rp.checkout(); }, std::runtime_error);
}


/// @brief Test that size() is accurate under concurrent modifications.
/// Multiple threads checkin while the main thread polls size().
TEST(resource_pool, size_accuracy_under_concurrency)
{
    siddiqsoft::resource_pool<int> rp {};
    constexpr int ITEMS_PER_THREAD = 50;
    constexpr int THREAD_COUNT     = 4;

    std::barrier              startBarrier {THREAD_COUNT};
    std::vector<std::jthread> threads;

    for (int t = 0; t < THREAD_COUNT; t++) {
        threads.emplace_back([&, t]() {
            startBarrier.arrive_and_wait();
            for (int i = 0; i < ITEMS_PER_THREAD; i++) {
                int val = t * ITEMS_PER_THREAD + i;
                rp.checkin(std::move(val));
            }
        });
    }

    threads.clear();

    EXPECT_EQ(static_cast<size_t>(THREAD_COUNT * ITEMS_PER_THREAD), rp.size());
}


/// @brief Test resource_pool with unique_ptr under concurrent access.
/// unique_ptr is move-only, so this validates that the pool correctly handles
/// move semantics under thread contention.
TEST(resource_pool, concurrent_unique_ptr)
{
    constexpr int POOL_SIZE    = 4;
    constexpr int THREAD_COUNT = 4;
    constexpr int CYCLES       = 100;

    siddiqsoft::resource_pool<std::unique_ptr<std::string>> rp {};
    for (int i = 0; i < POOL_SIZE; i++) {
        rp.checkin(std::make_unique<std::string>(std::format("resource-{}", i)));
    }

    std::atomic_int totalCheckouts {0};
    std::barrier    startBarrier {THREAD_COUNT};

    std::vector<std::jthread> threads;
    for (int t = 0; t < THREAD_COUNT; t++) {
        threads.emplace_back([&]() {
            startBarrier.arrive_and_wait();
            for (int c = 0; c < CYCLES; c++) {
                try {
                    auto item = rp.checkout();
                    EXPECT_NE(nullptr, item);
                    totalCheckouts++;
                    rp.checkin(std::move(item));
                }
                catch (const std::runtime_error&) {
                    // Pool was momentarily empty
                }
            }
        });
    }

    threads.clear();

    EXPECT_EQ(static_cast<size_t>(POOL_SIZE), rp.size());
    EXPECT_GT(totalCheckouts.load(), 0);
}
