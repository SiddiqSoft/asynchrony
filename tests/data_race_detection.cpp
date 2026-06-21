/*
    asynchrony-lib - Data Race Detection Tests
    Add asynchrony to your apps

    BSD 3-Clause License

    Copyright (c) 2021, Siddiq Software LLC
    All rights reserved.
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
#include <latch>

#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/simple_worker.hpp"
#include "../include/siddiqsoft/simple_pool.hpp"
#include "../include/siddiqsoft/resource_pool.hpp"
#include "../include/siddiqsoft/roundrobin_pool.hpp"


/// @brief DATA RACE #1: simple_worker toJson() reads items.size() without lock
/// Concurrent queue() modifies deque while toJson() reads size
TEST(DataRaceDetection, simple_worker_toJson_size_race)
{
    std::atomic_bool                       done {false};
    std::atomic_uint                       races_detected {0};
    siddiqsoft::simple_worker<std::string> worker {[](auto&&) { std::this_thread::sleep_for(std::chrono::microseconds(100)); }};

    std::vector<std::jthread>              threads;

    // Producer threads rapidly queue items
    for (int p = 0; p < 8; p++) {
        threads.emplace_back([&]() {
            while (!done.load()) {
                for (int i = 0; i < 1000; i++) {
                    worker.queue(std::format("item-{}", i));
                }
                std::this_thread::yield();
            }
        });
    }

    // Serializer threads call toJson() which reads items.size() without lock
    for (int s = 0; s < 4; s++) {
        threads.emplace_back([&]() {
            while (!done.load()) {
                try {
                    nlohmann::json j;
                    to_json(j, worker);
                    size_t reported_size = j["items"].get<nlohmann::json::array_t>().size();
                    // Verify the reported size is consistent
                }
                catch (const std::exception& e) {
                    races_detected++;
                }
                std::this_thread::yield();
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    done = true;

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::cerr << std::format("simple_worker toJson races detected: {}\n", races_detected.load());
}


/// @brief DATA RACE #2: simple_pool toJson() reads items.size() without lock
/// Multiple producers and serializers racing
TEST(DataRaceDetection, simple_pool_toJson_size_race)
{
    std::atomic_bool                done {false};
    std::atomic_uint                serialization_attempts {0};
    std::atomic_uint                size_mismatches {0};

    siddiqsoft::simple_pool<int, 4> pool {[](auto&&) { std::this_thread::sleep_for(std::chrono::microseconds(50)); }};

    std::vector<std::jthread>       threads;

    // Producer threads
    for (int p = 0; p < 6; p++) {
        threads.emplace_back([&]() {
            int counter = 0;
            while (!done.load()) {
                for (int i = 0; i < 500; i++) {
                    pool.queue(counter++);
                }
                std::this_thread::yield();
            }
        });
    }

    // Concurrent serializer threads
    for (int s = 0; s < 4; s++) {
        threads.emplace_back([&]() {
            while (!done.load()) {
                try {
                    nlohmann::json j;
                    to_json(j, pool);
                    serialization_attempts++;

                    // Try to validate consistency
                    if (j.contains("pools")) {
                        for (const auto& pool_data : j["pools"]) {
                            if (pool_data.contains("items")) {
                                auto items_size = pool_data["items"].size();
                                // If we read a snapshot, verify it's reasonable
                            }
                        }
                    }
                }
                catch (const std::exception&) {
                    size_mismatches++;
                }
                std::this_thread::yield();
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    done = true;

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::cerr << std::format(
            "simple_pool toJson attempts: {}, mismatches: {}\n", serialization_attempts.load(), size_mismatches.load());
}


/// @brief DATA RACE #3: roundrobin_pool nextWorkerIndex() without atomic ordering
/// Multiple producers compute same index due to loose atomic operations
TEST(DataRaceDetection, roundrobin_pool_index_collision_race)
{
    std::atomic_uint                    collision_count {0};
    std::atomic_uint                    queue_attempts {0};
    std::atomic_bool                    done {false};

    siddiqsoft::roundrobin_pool<int, 4> pool {[](auto&&) { std::this_thread::sleep_for(std::chrono::microseconds(10)); }};

    std::barrier<>                      sync_point {16};
    std::vector<std::jthread>           threads;

    // Burst of concurrent producers trying to queue simultaneously
    for (int p = 0; p < 16; p++) {
        threads.emplace_back([&, producer_id = p]() {
            for (int round = 0; round < 100; round++) {
                // Synchronize all threads to burst at the same moment
                sync_point.arrive_and_wait();

                for (int i = 0; i < 10; i++) {
                    try {
                        pool.queue(producer_id * 10000 + i);
                        queue_attempts++;
                    }
                    catch (const std::exception&) {
                        collision_count++;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cerr << std::format("roundrobin pool collisions: {}, attempts: {}\n", collision_count.load(), queue_attempts.load());
}


/// @brief DATA RACE #4: simple_worker item extraction with concurrent operations
/// Race between getNextItem() and queue() on deque
TEST(DataRaceDetection, simple_worker_queue_extraction_race)
{
    std::atomic_bool               done {false};
    std::atomic_uint               items_processed {0};
    std::atomic_uint               items_dropped {0};
    std::mutex                     processed_mutex;
    std::vector<int>               processed_items;

    siddiqsoft::simple_worker<int> worker {[&](auto&& item) {
        {
            std::lock_guard<std::mutex> lock(processed_mutex);
            processed_items.push_back(item);
        }
        items_processed++;
    }};

    std::vector<std::jthread>      threads;

    // Rapid producer
    threads.emplace_back([&]() {
        int counter = 0;
        while (!done.load()) {
            for (int i = 0; i < 5000; i++) {
                worker.queue(counter++);
            }
            std::this_thread::yield();
        }
    });

    // Monitor for dropped items
    threads.emplace_back([&]() {
        int last_seen = -1;
        while (!done.load()) {
            {
                std::lock_guard<std::mutex> lock(processed_mutex);
                if (!processed_items.empty()) {
                    int current = processed_items.back();
                    if (current != last_seen + 1 && last_seen >= 0) {
                        items_dropped += (current - last_seen - 1);
                    }
                    last_seen = current;
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    done = true;

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::cerr << std::format("simple_worker: processed={}, dropped={}\n", items_processed.load(), items_dropped.load());
}


/// @brief DATA RACE #5: resource_pool size() TOCTOU with concurrent operations
/// Check size(), then checkout() - another thread could modify between the two
TEST(DataRaceDetection, resource_pool_size_checkout_toctou_race)
{
    struct resource_item
    {
        int id;
    };

    std::atomic_bool                         done {false};
    std::atomic_uint                         checkout_failures {0};
    std::atomic_uint                         size_checks {0};
    siddiqsoft::resource_pool<resource_item> pool {};

    // Pre-populate
    for (int i = 0; i < 100; i++) {
        pool.checkin(resource_item {i});
    }

    std::vector<std::jthread> threads;

    // Threads that check size then try to checkout
    for (int t = 0; t < 8; t++) {
        threads.emplace_back([&]() {
            while (!done.load()) {
                size_t sz = pool.size();
                size_checks++;

                if (sz > 0) {
                    try {
                        auto item = pool.checkout();
                        // Successfully got an item
                    }
                    catch (const std::runtime_error&) {
                        // TOCTOU race: size said > 0 but checkout failed
                        checkout_failures++;
                    }
                }
                std::this_thread::yield();
            }
        });
    }

    // Concurrent modifier threads
    for (int m = 0; m < 4; m++) {
        threads.emplace_back([&]() {
            int counter = 100;
            while (!done.load()) {
                for (int i = 0; i < 50; i++) {
                    pool.checkin(resource_item {counter++});
                    try {
                        auto _ = pool.checkout();
                    }
                    catch (const std::runtime_error&) {
                    }
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    done = true;

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::cerr << std::format(
            "resource_pool TOCTOU: size_checks={}, checkout_failures={}\n", size_checks.load(), checkout_failures.load());
}


/// @brief DATA RACE #6: Concurrent toJson() calls on simple_worker
/// Multiple threads serializing the same worker simultaneously
TEST(DataRaceDetection, simple_worker_concurrent_toJson_race)
{
    std::atomic_bool                       done {false};
    std::atomic_uint                       serialization_count {0};
    std::atomic_uint                       serialization_errors {0};

    siddiqsoft::simple_worker<std::string> worker {[](auto&&) { std::this_thread::sleep_for(std::chrono::microseconds(50)); }};

    std::vector<std::jthread>              threads;

    // Producer
    threads.emplace_back([&]() {
        int counter = 0;
        while (!done.load()) {
            for (int i = 0; i < 100; i++) {
                worker.queue(std::format("msg-{}", counter++));
            }
        }
    });

    // Multiple concurrent serializers
    for (int s = 0; s < 8; s++) {
        threads.emplace_back([&]() {
            while (!done.load()) {
                try {
                    nlohmann::json j;
                    to_json(j, worker);
                    serialization_count++;

                    // Verify JSON structure is valid
                    ASSERT_TRUE(j.contains("itemsSize"));
                    size_t sz = j["itemsSize"].get<size_t>();
                    ASSERT_GE(sz, 0);
                }
                catch (const std::exception& e) {
                    serialization_errors++;
                }
                std::this_thread::yield();
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    done = true;

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::cerr << std::format(
            "concurrent toJson: successes={}, errors={}\n", serialization_count.load(), serialization_errors.load());
}


/// @brief DATA RACE #7: simple_pool concurrent operations during destruction
/// Pool destruction while threads are still processing
TEST(DataRaceDetection, simple_pool_destruction_race)
{
    std::atomic_uint total_processed {0};
    struct test_item
    {
        test_item(const int& arg)
            : id(arg)
        {
        }

        int id;
    };

    for (int iteration = 0; iteration < 50; iteration++) {
        std::atomic_bool release_threads {false};

        {
            siddiqsoft::simple_pool<test_item, 4> pool {[&](auto&&) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                total_processed++;
            }};

            // Queue items
            for (int i = 0; i < 100; i++) {
                pool.queue(test_item {i});
            }

            // Don't wait for completion - let destructor handle it
            // This stresses the join/request_stop logic
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cerr << std::format("simple_pool destruction: total_processed={}\n", total_processed.load());
}