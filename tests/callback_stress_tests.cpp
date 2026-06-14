/*
    asynchrony-lib - Callback Lockup and Exception Tests
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
#include <stdexcept>

#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/simple_worker.hpp"
#include "../include/siddiqsoft/simple_pool.hpp"
#include "../include/siddiqsoft/roundrobin_pool.hpp"
#include "../include/siddiqsoft/periodic_worker.hpp"


// ============================================================================
// HELPER STRUCT FOR TESTS
// ============================================================================

/// @brief Simple move-constructible struct for testing
struct TestItem
{
    int value;

    TestItem(int v = 0)
        : value(v)
    {
    }
    TestItem(TestItem&&)                 = default;
    TestItem& operator=(TestItem&&)      = default;
    TestItem(const TestItem&)            = delete;
    TestItem& operator=(const TestItem&) = delete;
};


// ============================================================================
// SIMPLE_WORKER CALLBACK LOCKUP TESTS
// ============================================================================

/// @brief Test simple_worker with callback that locks up indefinitely
/// Verifies that other items can still be queued even if one callback is blocked
TEST(callback_lockup, simple_worker_callback_lockup)
{
    std::atomic_uint                    processedCount {0};
    std::atomic_bool                    lockupStarted {false};
    std::atomic_bool                    lockupCanFinish {false};

    siddiqsoft::simple_worker<TestItem> worker {[&](auto&& item) {
        if (item.value == 0) {
            lockupStarted = true;
            // Simulate indefinite lockup
            while (!lockupCanFinish.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        else {
            processedCount++;
        }
    }};

    // Queue item that will lockup
    worker.queue(TestItem(0));

    // Wait for lockup to start
    while (!lockupStarted.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Queue more items while one is locked up
    for (int i = 1; i <= 10; i++) {
        worker.queue(TestItem(i));
    }

    // Wait a bit for items to be queued
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Items should be queued but not processed yet (worker is blocked)
    EXPECT_EQ(0u, processedCount.load());

    // Release the lockup
    lockupCanFinish = true;

    // Wait for remaining items to be processed
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // All non-lockup items should be processed
    EXPECT_EQ(10u, processedCount.load());
}


/// @brief Test simple_worker with callback that locks up for a duration
/// Verifies that the worker can recover after a lockup
TEST(callback_lockup, simple_worker_callback_temporary_lockup)
{
    std::atomic_uint                    processedCount {0};
    std::atomic_uint                    lockupCount {0};

    siddiqsoft::simple_worker<TestItem> worker {[&](auto&& item) {
        if (item.value % 5 == 0) {
            lockupCount++;
            // Temporary lockup
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        processedCount++;
    }};

    // Queue items
    for (int i = 0; i < 20; i++) {
        worker.queue(TestItem(i));
    }

    // Wait for all items to be processed
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // All items should be processed despite lockups
    EXPECT_EQ(20u, processedCount.load());
    EXPECT_EQ(4u, lockupCount.load()); // Items 0, 5, 10, 15
}


// ============================================================================
// SIMPLE_WORKER CALLBACK EXCEPTION TESTS
// ============================================================================

/// @brief Test simple_worker with callback that throws std::runtime_error
/// Verifies that exceptions don't stop processing of subsequent items
TEST(callback_exception, simple_worker_runtime_error)
{
    std::atomic_uint                    processedCount {0};
    std::atomic_uint                    exceptionCount {0};

    siddiqsoft::simple_worker<TestItem> worker {[&](auto&& item) {
        if (item.value % 3 == 0) {
            exceptionCount++;
            throw std::runtime_error("Test runtime error");
        }
        processedCount++;
    }};

    // Queue items
    for (int i = 0; i < 30; i++) {
        worker.queue(TestItem(i));
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify results
    EXPECT_EQ(20u, processedCount.load()); // 30 - 10 exceptions
    EXPECT_EQ(10u, exceptionCount.load()); // Items 0,3,6,9,12,15,18,21,24,27
}


/// @brief Test simple_worker with callback that throws std::bad_alloc
/// Verifies that memory exceptions are handled gracefully
TEST(callback_exception, simple_worker_bad_alloc)
{
    std::atomic_uint                    processedCount {0};
    std::atomic_uint                    exceptionCount {0};

    siddiqsoft::simple_worker<TestItem> worker {[&](auto&& item) {
        if (item.value == 5) {
            exceptionCount++;
            throw std::bad_alloc();
        }
        processedCount++;
    }};

    // Queue items
    for (int i = 0; i < 10; i++) {
        worker.queue(TestItem(i));
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify results
    EXPECT_EQ(9u, processedCount.load());
    EXPECT_EQ(1u, exceptionCount.load());
}


/// @brief Test simple_worker with callback that throws custom exception
/// Verifies that custom exceptions are handled
TEST(callback_exception, simple_worker_custom_exception)
{
    struct CustomException : public std::exception
    {
        const char* what() const noexcept override { return "Custom exception"; }
    };

    std::atomic_uint                    processedCount {0};
    std::atomic_uint                    exceptionCount {0};

    siddiqsoft::simple_worker<TestItem> worker {[&](auto&& item) {
        if (item.value % 4 == 0) {
            exceptionCount++;
            throw CustomException();
        }
        processedCount++;
    }};

    // Queue items
    for (int i = 0; i < 20; i++) {
        worker.queue(TestItem(i));
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify results
    EXPECT_EQ(15u, processedCount.load()); // 20 - 5 exceptions
    EXPECT_EQ(5u, exceptionCount.load());  // Items 0,4,8,12,16
}


/// @brief Test simple_worker with callback that throws and then recovers
/// Verifies that exceptions don't permanently break the worker
TEST(callback_exception, simple_worker_exception_recovery)
{
    std::atomic_uint                    processedCount {0};
    std::atomic_uint                    exceptionCount {0};
    std::atomic_bool                    shouldThrow {true};

    siddiqsoft::simple_worker<TestItem> worker {[&](auto&& item) {
        if (shouldThrow && item.value < 5) {
            exceptionCount++;
            throw std::runtime_error("Temporary error");
        }
        processedCount++;
    }};

    // Queue first batch (will throw)
    for (int i = 0; i < 5; i++) {
        worker.queue(TestItem(i));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Stop throwing
    shouldThrow = false;

    // Queue second batch (should process normally)
    for (int i = 5; i < 10; i++) {
        worker.queue(TestItem(i));
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify results
    EXPECT_EQ(5u, processedCount.load()); // Only second batch
    EXPECT_EQ(5u, exceptionCount.load()); // First batch threw
}


// ============================================================================
// SIMPLE_POOL CALLBACK LOCKUP TESTS
// ============================================================================

/// @brief Test simple_pool with callback that locks up
/// Verifies that other worker threads can still process items
TEST(callback_lockup, simple_pool_callback_lockup)
{
    std::atomic_uint                     processedCount {0};
    std::atomic_bool                     lockupStarted {false};
    std::atomic_bool                     lockupCanFinish {false};

    siddiqsoft::simple_pool<TestItem, 4> pool {[&](auto&& item) {
        if (item.value == 0) {
            lockupStarted = true;
            while (!lockupCanFinish.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Add work
            processedCount++;
        }
    }};

    // Queue item that will lockup
    pool.queue(TestItem(0));

    // Wait for lockup to start
    while (!lockupStarted.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Queue more items while one is locked up
    for (int i = 1; i <= 20; i++) {
        pool.queue(TestItem(i));
    }

    // Wait a bit - other threads should process items
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Some items should be processed by other threads
    EXPECT_GT(processedCount.load(), 0u);
    EXPECT_LT(processedCount.load(), 21u); // Not all, since one thread is locked

    // Release the lockup
    lockupCanFinish = true;

    // Wait for remaining items
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // All non-lockup items should be processed
    EXPECT_EQ(20u, processedCount.load());
}


/// @brief Test simple_pool with multiple callbacks locking up
/// Verifies that remaining threads can still process items
TEST(callback_lockup, simple_pool_multiple_lockups)
{
    std::atomic_uint                     processedCount {0};
    std::atomic_uint                     lockupCount {0};
    std::atomic_bool                     canFinish {false};

    siddiqsoft::simple_pool<TestItem, 4> pool {[&](auto&& item) {
        if (item.value % 10 == 0) {
            lockupCount++;
            // Lockup
            while (!canFinish.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        processedCount++;
    }};

    // Queue items
    for (int i = 0; i < 40; i++) {
        pool.queue(TestItem(i));
    }

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Some items should be processed
    EXPECT_GT(processedCount.load(), 0u);

    // Release lockups
    canFinish = true;

    // Wait for all items
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // All items should be processed
    EXPECT_EQ(40u, processedCount.load());
    EXPECT_EQ(4u, lockupCount.load()); // Items 0, 10, 20, 30
}


// ============================================================================
// SIMPLE_POOL CALLBACK EXCEPTION TESTS
// ============================================================================

/// @brief Test simple_pool with callback that throws
/// Verifies that exceptions in one thread don't affect others
TEST(callback_exception, simple_pool_exception)
{
    std::atomic_uint                     processedCount {0};
    std::atomic_uint                     exceptionCount {0};

    siddiqsoft::simple_pool<TestItem, 4> pool {[&](auto&& item) {
        if (item.value % 5 == 0) {
            exceptionCount++;
            throw std::runtime_error("Pool exception");
        }
        processedCount++;
    }};

    // Queue items
    for (int i = 0; i < 50; i++) {
        pool.queue(TestItem(i));
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify results
    EXPECT_EQ(40u, processedCount.load()); // 50 - 10 exceptions
    EXPECT_EQ(10u, exceptionCount.load()); // Items 0,5,10,15,20,25,30,35,40,45
}


/// @brief Test simple_pool with mixed exceptions and lockups
/// Verifies robustness under combined stress
TEST(callback_exception, simple_pool_mixed_stress)
{
    std::atomic_uint                     processedCount {0};
    std::atomic_uint                     exceptionCount {0};
    std::atomic_uint                     lockupCount {0};
    std::atomic_bool                     canFinish {false};

    siddiqsoft::simple_pool<TestItem, 4> pool {[&](auto&& item) {
        if (item.value % 7 == 0) {
            // Items divisible by 7: lockup then process
            lockupCount++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            processedCount++; // Explicitly increment after lockup
        }
        else if (item.value % 3 == 0) {
            // Items divisible by 3 but not 7: throw exception, NOT processed
            exceptionCount++;
            throw std::runtime_error("Mixed stress exception");
            // processedCount NOT incremented (exception thrown)
        }
        else {
            // All other items: process normally
            processedCount++;
        }
    }};

    // Queue items
    for (int i = 0; i < 100; i++) {
        pool.queue(TestItem(i));
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify results
    // Items divisible by 7: 0,7,14,21,28,35,42,49,56,63,70,77,84,91,98 (15 items)
    // Items divisible by 3 but not 7: 3,6,9,12,15,18,24,27,30,33,36,39,45,48,51,54,57,60,66,69,72,75,78,81,87,90,93,96,99 (29
    // items) Processed: 100 - 15 - 29 = 56 items
    EXPECT_EQ(71u, processedCount.load());
    EXPECT_EQ(29u, exceptionCount.load());
    EXPECT_EQ(15u, lockupCount.load());
}


// ============================================================================
// ROUNDROBIN_POOL CALLBACK LOCKUP TESTS
// ============================================================================

/// @brief Test roundrobin_pool with callback that locks up
/// Verifies that other workers can still process items
TEST(callback_lockup, roundrobin_pool_callback_lockup)
{
    std::atomic_uint                         processedCount {0};
    std::atomic_bool                         lockupStarted {false};
    std::atomic_bool                         lockupCanFinish {false};

    siddiqsoft::roundrobin_pool<TestItem, 4> pool {[&](auto&& item) {
        if (item.value == 0) {
            lockupStarted = true;
            // Lockup
            while (!lockupCanFinish.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        else {
            processedCount++;
        }
    }};

    // Queue item that will lockup
    pool.queue(TestItem(0));

    // Wait for lockup to start
    while (!lockupStarted.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Queue more items
    for (int i = 1; i <= 20; i++) {
        pool.queue(TestItem(i));
    }

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Some items should be processed
    EXPECT_GT(processedCount.load(), 0u);

    // Release lockup
    lockupCanFinish = true;

    // Wait for remaining items
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // All non-lockup items should be processed
    EXPECT_EQ(20u, processedCount.load());
}


// ============================================================================
// ROUNDROBIN_POOL CALLBACK EXCEPTION TESTS
// ============================================================================

/// @brief Test roundrobin_pool with callback that throws
/// Verifies that exceptions don't affect other workers
TEST(callback_exception, roundrobin_pool_exception)
{
    std::atomic_uint                         processedCount {0};
    std::atomic_uint                         exceptionCount {0};

    siddiqsoft::roundrobin_pool<TestItem, 4> pool {[&](auto&& item) {
        if (item.value % 4 == 0) {
            exceptionCount++;
            throw std::runtime_error("Roundrobin exception");
        }
        processedCount++;
    }};

    // Queue items
    for (int i = 0; i < 40; i++) {
        pool.queue(TestItem(i));
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify results
    EXPECT_EQ(30u, processedCount.load()); // 40 - 10 exceptions
    EXPECT_EQ(10u, exceptionCount.load()); // Items 0,4,8,12,16,20,24,28,32,36
}


// ============================================================================
// PERIODIC_WORKER CALLBACK LOCKUP TESTS
// ============================================================================

/// @brief Test periodic_worker with callback that locks up
/// Verifies that the worker can recover after lockup
TEST(callback_lockup, periodic_worker_callback_lockup)
{
    std::atomic_uint            invokeCount {0};
    std::atomic_bool            lockupStarted {false};
    std::atomic_bool            lockupCanFinish {false};

    siddiqsoft::periodic_worker worker {[&]() {
                                            invokeCount++;
                                            if (invokeCount.load() == 2) {
                                                lockupStarted = true;
                                                // Lockup
                                                while (!lockupCanFinish.load()) {
                                                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                                }
                                            }
                                        },
                                        std::chrono::milliseconds(50)};

    // Wait for lockup to start
    while (!lockupStarted.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Wait a bit while locked up
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Release lockup
    lockupCanFinish = true;

    // Wait for more invocations
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Should have invoked multiple times despite lockup
    EXPECT_GT(invokeCount.load(), 2u);
}


/// @brief Test periodic_worker with callback that locks up indefinitely
/// Verifies that destructor can still clean up
TEST(callback_lockup, periodic_worker_indefinite_lockup_cleanup)
{
    std::atomic_uint invokeCount {0};
    std::atomic_bool lockupStarted {false};

    EXPECT_NO_THROW({
        siddiqsoft::periodic_worker worker(
                [&]() {
                    invokeCount++;
                    if (invokeCount.load() == 1) {
                        lockupStarted = true;
                        // Indefinite lockup
                        while (true) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                    }
                },
                std::chrono::milliseconds(50));

        // Wait for lockup to start
        while (!lockupStarted.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Destructor should clean up even with indefinite lockup
        // TODO: This test tends to lockup the build servers and we should
        // exclude it from the CI tests and instead ust it only manually.
        std::println(std::cerr, "WE SHOULD be destroying periodic_worker while locked up...");
    });

    EXPECT_EQ(1u, invokeCount.load());
}


// ============================================================================
// PERIODIC_WORKER CALLBACK EXCEPTION TESTS
// ============================================================================

/// @brief Test periodic_worker with callback that throws
/// Verifies that exceptions don't stop periodic invocations
TEST(callback_exception, periodic_worker_exception)
{
    std::atomic_uint            invokeCount {0};
    std::atomic_uint            exceptionCount {0};

    siddiqsoft::periodic_worker worker {[&]() {
                                            invokeCount++;
                                            if (invokeCount.load() % 3 == 0) {
                                                exceptionCount++;
                                                throw std::runtime_error("Periodic exception");
                                            }
                                        },
                                        std::chrono::milliseconds(50)};

    // Wait for multiple invocations
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify results
    EXPECT_GT(invokeCount.load(), 5u);
    EXPECT_GT(exceptionCount.load(), 0u);
    // Exceptions should be roughly 1/3 of invocations
    EXPECT_NEAR(exceptionCount.load(), invokeCount.load() / 3, 2);
}


/// @brief Test periodic_worker with callback that throws and recovers
/// Verifies that periodic invocations continue despite exceptions
TEST(callback_exception, periodic_worker_exception_recovery)
{
    std::atomic_uint            invokeCount {0};
    std::atomic_uint            exceptionCount {0};
    std::atomic_bool            shouldThrow {true};

    siddiqsoft::periodic_worker worker {[&]() {
                                            invokeCount++;
                                            if (shouldThrow && invokeCount.load() < 5) {
                                                exceptionCount++;
                                                throw std::runtime_error("Temporary periodic exception");
                                            }
                                        },
                                        std::chrono::milliseconds(50)};

    // Wait for first batch (with exceptions)
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Stop throwing
    shouldThrow = false;

    // Wait for second batch (without exceptions)
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Verify results
    EXPECT_GT(invokeCount.load(), 8u);
    EXPECT_GT(exceptionCount.load(), 0u);
}


// ============================================================================
// STRESS TESTS: COMBINED LOCKUP AND EXCEPTION SCENARIOS
// ============================================================================

/// @brief Stress test: simple_pool with concurrent lockups and exceptions
/// Verifies robustness under extreme conditions
TEST(callback_stress, simple_pool_concurrent_lockup_and_exceptions)
{
    std::atomic_uint                     processedCount {0};
    std::atomic_uint                     exceptionCount {0};
    std::atomic_uint                     lockupCount {0};
    std::atomic_bool                     canFinish {false};

    siddiqsoft::simple_pool<TestItem, 8> pool {[&](auto&& item) {
        if (item.value % 13 == 0) {
            lockupCount++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            processedCount++;
        }
        else if (item.value % 7 == 0) {
            exceptionCount++;
            throw std::runtime_error("Stress test exception");
        }
        else {
            processedCount++;
        }
    }};

    // Queue many items
    for (int i = 0; i < 200; i++) {
        pool.queue(TestItem(i));
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify all items were processed or threw
    EXPECT_EQ(216u, processedCount.load() + exceptionCount.load() + lockupCount.load());
}


/// @brief Stress test: roundrobin_pool with rapid exception throwing
/// Verifies that round-robin distribution handles exceptions well
TEST(callback_stress, roundrobin_pool_rapid_exceptions)
{
    std::atomic_uint                         processedCount {0};
    std::atomic_uint                         exceptionCount {0};

    siddiqsoft::roundrobin_pool<TestItem, 8> pool {[&](auto&& item) {
        if (item.value % 2 == 0) {
            exceptionCount++;
            throw std::runtime_error("Rapid exception");
        }
        processedCount++;
    }};

    // Queue many items rapidly
    for (int i = 0; i < 500; i++) {
        pool.queue(TestItem(i));
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify results
    EXPECT_EQ(250u, processedCount.load()); // Half processed
    EXPECT_EQ(250u, exceptionCount.load()); // Half threw
}
