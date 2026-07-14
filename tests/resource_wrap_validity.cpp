/*
    Test cases for resource_wrap validity tracking fix

    These tests verify that the resource_wrap class properly tracks
    resource validity and prevents returning uninitialized or invalid
    resources to the pool.

    NOTE: Tests using invalidate() are only compiled in DEBUG builds
    since invalidate() is only available in DEBUG mode.
*/

#include "gtest/gtest.h"
#include <memory>
#include <thread>
#include <format>
#include <atomic>
#include <vector>

#include "../include/siddiqsoft/resource_pool.hpp"

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

/**
 * @brief Test that valid resources are still returned to the pool
 *
 * Ensures the fix doesn't break normal operation.
 * This test works in both DEBUG and RELEASE builds.
 */
TEST(resource_wrap_validity, valid_resource_returned)
{
    siddiqsoft::resource_pool<std::string> pool;
    pool.checkin(std::string("42"));

    EXPECT_EQ(1u, pool.size());

    {
        auto wrap = pool.checkout();
        EXPECT_EQ(0u, pool.size());
        // Don't invalidate - resource should be returned
    }

    // Pool should have the resource back
    EXPECT_EQ(1u, pool.size());

    auto item = pool.checkout();
    EXPECT_EQ("42", *item);
}

/**
 * @brief Test that assignment operator maintains validity
 */
TEST(resource_wrap_validity, assignment_maintains_validity)
{
    siddiqsoft::resource_pool<std::string> pool;
    pool.checkin(std::string("42"));
    pool.checkin(std::string("99"));

    {
        auto wrap1 = pool.checkout();
        auto wrap2 = pool.checkout();

        EXPECT_EQ(0u, pool.size());

        // Assign wrap2's resource to wrap1
        *wrap1 = std::move(*wrap2);

        // Both should still be valid and return their resources
    }

    // Both resources should be back in the pool
    EXPECT_EQ(2u, pool.size());
}

/**
 * @brief Test that destructor properly handles valid resources
 */
TEST(resource_wrap_validity, destructor_returns_valid_resource)
{
    siddiqsoft::resource_pool<std::string> pool;
    pool.checkin(std::string("100"));

    {
        auto wrap = pool.checkout();
        EXPECT_EQ(0u, pool.size());
        // Don't invalidate - destructor should return it
    }

    // Resource should be back in pool
    EXPECT_EQ(1u, pool.size());
    auto item = pool.checkout();
    EXPECT_EQ("100", *item);
}

#if defined(DEBUG)

/**
 * @brief Test that invalid resources are not returned to the pool
 *
 * This test verifies the fix for the critical issue where uninitialized
 * resources could be returned to the pool, corrupting it.
 *
 * When a resource is invalidated, it should NOT be returned to the pool.
 *
 * NOTE: This test is only available in DEBUG builds
 */
TEST(resource_wrap_validity, no_corruption_on_invalid_resource)
{
    siddiqsoft::resource_pool<std::string> pool;
    pool.checkin(std::string("42"));

    EXPECT_EQ(1u, pool.size());

    {
        auto wrap = pool.checkout();
        EXPECT_EQ(0u, pool.size());

        // Invalidate the resource to simulate it being moved out
        wrap.invalidate();
        // After invalidation, the resource is NOT returned to pool
    }

    // Pool should be empty because we invalidated the resource
    EXPECT_EQ(0u, pool.size());
}

/**
 * @brief Test with unique_ptr to ensure move-only types work correctly
 *
 * NOTE: This test is only available in DEBUG builds
 */
TEST(resource_wrap_validity, unique_ptr_invalidation)
{
    siddiqsoft::resource_pool<std::unique_ptr<std::string>> pool;
    pool.checkin(std::make_unique<std::string>("42"));

    EXPECT_EQ(1u, pool.size());

    {
        auto wrap = pool.checkout();
        EXPECT_EQ(0u, pool.size());

        // Invalidate to prevent returning the resource
        wrap.invalidate();
    }

    // Pool should be empty because we invalidated the resource
    EXPECT_EQ(0u, pool.size());
}

/**
 * @brief Test multiple invalidations don't cause issues
 *
 * NOTE: This test is only available in DEBUG builds
 */
TEST(resource_wrap_validity, multiple_invalidations)
{
    siddiqsoft::resource_pool<std::string> pool;
    pool.checkin(std::string("42"));

    {
        auto wrap = pool.checkout();
        wrap.invalidate();
        wrap.invalidate(); // Should be safe to call multiple times
    }

    // Pool should be empty because we invalidated
    EXPECT_EQ(0u, pool.size());
}

/**
 * @brief Test concurrent access with invalidation
 *
 * This test verifies that concurrent access with mixed valid/invalid
 * resources works correctly. The key insight is:
 * - Valid resources are returned to the pool
 * - Invalid resources are NOT returned to the pool
 * - Final pool size = initial size - invalidated count
 *
 * NOTE: This test is only available in DEBUG builds
 */
TEST(resource_wrap_validity, concurrent_with_invalidation)
{
    siddiqsoft::resource_pool<std::string> pool;

    // Pre-fill the pool with enough resources
    // We use 100 to ensure no contention
    for (int i = 0; i < 100; i++) {
        pool.checkin(std::format("resource-{}", i));
    }

    EXPECT_EQ(100u, pool.size());

    std::vector<std::jthread> threads;
    std::atomic_int           invalidated_count {0};
    std::atomic_int           returned_count {0};

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 10; i++) {
                try {
                    auto wrap = pool.checkout();

                    // Pattern: invalidate on i % 3 == 0
                    // i=0,3,6,9 → invalidate (4 times)
                    // i=1,2,4,5,7,8 → return (6 times)
                    if (i % 3 == 0) {
                        wrap.invalidate();
                        invalidated_count++;
                    }
                    else {
                        returned_count++;
                    }
                }
                catch (const std::runtime_error&) {
                    // Pool was empty - this shouldn't happen with 100 items
                    // but we don't fail the test if it does
                }
            }
        });
    }

    threads.clear();

    // Verify the behavior:
    // - All operations should succeed (we have 100 items)
    // - Total operations = invalidated + returned
    // - Pool size = 100 - invalidated (because invalidated ones are not returned)

    int total_operations = invalidated_count.load() + returned_count.load();
    EXPECT_EQ(40, total_operations);  // 4 threads * 10 iterations
    EXPECT_GT(invalidated_count.load(), 0);  // Some should be invalidated
    EXPECT_GT(returned_count.load(), 0);  // Some should be returned

    // Pool size should be: initial (100) - invalidated + returned
    // But since invalidated ones are NOT returned, it's:
    // = 100 - invalidated
    size_t expected_pool_size = 100u - invalidated_count.load();
    EXPECT_EQ(expected_pool_size, pool.size());
}

/**
 * @brief Test that destructor does NOT return invalidated resources
 *
 * NOTE: This test is only available in DEBUG builds
 */
TEST(resource_wrap_validity, destructor_skips_invalid_resource)
{
    siddiqsoft::resource_pool<std::string> pool;
    pool.checkin(std::string("200"));

    {
        auto wrap = pool.checkout();
        EXPECT_EQ(0u, pool.size());
        wrap.invalidate();
        // Destructor should NOT return it
    }

    // Resource should NOT be in pool
    EXPECT_EQ(0u, pool.size());
}

/**
 * @brief Test mixed valid and invalid resources in concurrent scenario
 *
 * NOTE: This test is only available in DEBUG builds
 */
TEST(resource_wrap_validity, mixed_valid_invalid_concurrent)
{
    siddiqsoft::resource_pool<std::string> pool;

    // Pre-fill with 20 items
    for (int i = 0; i < 20; i++) {
        pool.checkin(std::format("resource-{}", i));
    }

    std::atomic_int           valid_count {0};
    std::atomic_int           invalid_count {0};

    std::vector<std::jthread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 5; i++) {
                try {
                    auto wrap = pool.checkout();

                    // Alternate between valid and invalid
                    if (i % 2 == 0) {
                        valid_count++;
                        // Let it return normally
                    }
                    else {
                        invalid_count++;
                        wrap.invalidate();
                    }
                }
                catch (const std::runtime_error&) {
                    // Pool empty
                }
            }
        });
    }

    threads.clear();

    // Pool should have exactly the valid count
    EXPECT_EQ(static_cast<size_t>(valid_count.load()), pool.size());
    EXPECT_GT(invalid_count.load(), 0);
}

#endif  // defined(DEBUG)

// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
