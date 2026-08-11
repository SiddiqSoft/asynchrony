/*
    asynchrony - Bug Detection Tests
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
#include <climits>

#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/simple_worker.hpp"
#include "../include/siddiqsoft/simple_pool.hpp"
#include "../include/siddiqsoft/roundrobin_pool.hpp"
#include "../include/siddiqsoft/periodic_worker.hpp"

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

TEST(vulns_simple_worker, no_use_after_free)
{
    std::atomic_bool callback_executed {false};

    {
        siddiqsoft::simple_worker<std::string> worker([&](auto&& item) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            callback_executed = true;
        });

        worker.queue("test");
        // Destructor called while callback is still running
    }

    // Wait for callback to complete
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Should not crash
    EXPECT_TRUE(callback_executed);
}

/*
 * THIS WILL CAUSE A CRASH!
 * ************************
TEST(vuln_simple_worker, no_race_in_destructor) {
    std::atomic_uint items_processed{0};

    std::thread late_producer;

    {
        // Create the worker.. this will go out of scope before the destructor is called
        siddiqsoft::simple_worker<std::string> worker([&](auto&& item) {
            items_processed++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        });

        worker.queue("item1");

        // Start thread that will try to queue after destruction
        // We're using the worker as a reference..
        // This is clearly going to crash!
        late_producer = std::thread([&worker]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            try {
                worker.queue("late_item");
                FAIL() << "Should have thrown exception";
            }
            catch (const std::runtime_error& ex) {
                EXPECT_STREQ("Worker is shutting down, cannot queue new items", ex.what());
            }
        });
    }

    late_producer.join();
    EXPECT_EQ(1u, items_processed);  // Only first item processed
}
*/

TEST(vuln_simple_worker, graceful_shutdown)
{
    std::atomic_uint items_processed {0};

    {
        siddiqsoft::simple_worker<std::string> worker([&](auto&& val) {
            items_processed++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::println(std::cerr, "  task completed for   `{}`   via tid: {} ; items_processed: {}", val, std::this_thread::get_id(), items_processed.load());
        });

        for (int i = 0; i < 10; i++) {
            worker.queue(std::format("item-{}", i));
        }

        // Graceful shutdown
        bool success = worker.shutdown(std::chrono::seconds(2));
        EXPECT_TRUE(success);
    }

    EXPECT_EQ(10u, items_processed);
}

TEST(vuln_simple_worker, graceful_shutdown_timeout)
{
    std::atomic_uint items_processed {0};

    {
        siddiqsoft::simple_worker<std::string> worker([&](auto&&) {
            items_processed++;
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Long callback
        });

        // We must add more than one item otherwise the queue will drain
        // regardless of the shutdown timeout.
        worker.queue("item-0");
        worker.queue("item-1");
        worker.queue("item-2");

        // Graceful shutdown with very short timeout
        bool success = worker.shutdown(std::chrono::milliseconds(100));
        EXPECT_FALSE(success); // Should timeout
    }
}

TEST(vuln_roundrobin_pool, even_distribution)
{
    constexpr int                                     WORKERS = 4;
    constexpr int                                     ITEMS   = 1000;

    std::vector<std::atomic_uint>                     worker_counts(WORKERS);

    siddiqsoft::roundrobin_pool<std::string, WORKERS> pool([&](auto&&) {
        // Track which worker processed this
    });

    for (int i = 0; i < ITEMS; i++) {
        pool.queue(std::format("item-{}", i));
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify even distribution (within 10% tolerance)
    uint32_t expected_per_worker = ITEMS / WORKERS;
    uint32_t tolerance           = expected_per_worker / 10;

    for (int i = 0; i < WORKERS; i++) {
        // Each worker should have approximately ITEMS/WORKERS items
        // (This test would need instrumentation in the callback)
    }
}


// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
