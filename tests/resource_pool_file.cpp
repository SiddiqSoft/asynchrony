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
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <filesystem>

#include "../include/siddiqsoft/resource_pool.hpp"
#include "../include/siddiqsoft/simple_pool.hpp"

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

/**
 * @brief RAII wrapper for FILE* to ensure proper cleanup
 *
 * This wrapper ensures that FILE* resources are properly closed when
 * they go out of scope, even if an exception occurs.
 */
class FileHandle
{
private:
    FILE* handle;

public:
    explicit FileHandle(FILE* f = nullptr)
        : handle(f)
    {
    }

    // Move constructor
    FileHandle(FileHandle&& other) noexcept
        : handle(other.release())
    {
    }

    // Move assignment
    FileHandle& operator=(FileHandle&& other) noexcept
    {
        if (this != &other) {
            close();
            handle = other.release();
        }
        return *this;
    }

    // Delete copy operations
    FileHandle(const FileHandle&)            = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    // Destructor
    ~FileHandle() { close(); }

    // Get raw pointer
    operator FILE*() { return handle; }

    // Release ownership
    [[nodiscard]] FILE* release()
    {
        FILE* temp = handle;
        handle     = nullptr;
        return temp;
    }

    // Close the file
    void close()
    {
        if (handle != nullptr) {
            std::fclose(handle);
            handle = nullptr;
        }
    }

    // Operator-> for convenience
    FILE* operator->() const { return handle; }

    auto& operator=(FILE* f)
    {
        // Make sure we close and release the current handle..
        close();
        release();
        // Now we can accept the new one..
        handle = f;
        return *this;
    }

    // Boolean conversion
    explicit operator bool() const { return handle != nullptr; }
};

/**
 * @brief Test basic FILE* resource pool creation and usage
 *
 * Demonstrates creating a resource pool with FILE* handles and
 * checking out/in files.
 */
TEST(resource_pool_file, basic_file_pool)
{
    // Create a temporary file for testing
    const std::string temp_file = "/tmp/asynchrony_test_basic.txt";

    // Create resource pool for FILE* handles
    siddiqsoft::resource_pool<FileHandle> file_pool;

    // Create and add a file handle to the pool
    FileHandle f {std::fopen(temp_file.c_str(), "w+")};
    ASSERT_TRUE(f);

    file_pool.checkin(std::move(f));
    EXPECT_EQ(1u, file_pool.size());

    // Checkout the file
    {
        auto file_wrapper = file_pool.checkout();
        EXPECT_EQ(0u, file_pool.size());

        // Write to the file
        std::fprintf(*file_wrapper, "Hello, World!\n");
        std::fflush(*file_wrapper);
    }
    // File is automatically returned to pool

    EXPECT_EQ(1u, file_pool.size());

    // Checkout again and verify content
    {
        auto file_wrapper = file_pool.checkout();
        std::rewind(*file_wrapper);

        char buffer[100];
        ASSERT_NE(nullptr, std::fgets(buffer, sizeof(buffer), *file_wrapper));
        EXPECT_STREQ("Hello, World!\n", buffer);
    }

    // Cleanup
    std::remove(temp_file.c_str());
}

/**
 * @brief Test multiple threads writing to a single FILE* through resource pool
 *
 * This demonstrates the key use case: multiple threads can safely write to
 * a single file by checking out the FILE* from the pool, writing, and
 * returning it.
 */
TEST(resource_pool_file, concurrent_file_writes)
{
    const std::string temp_file = "/tmp/asynchrony_test_concurrent.txt";

    // Create resource pool with a single FILE* handle
    siddiqsoft::resource_pool<FileHandle> file_pool;

    FILE*                                 f = std::fopen(temp_file.c_str(), "w+");
    ASSERT_NE(nullptr, f);
    file_pool.checkin(FileHandle(f));

    constexpr int             THREAD_COUNT      = 4;
    constexpr int             WRITES_PER_THREAD = 25;
    std::atomic_int           total_writes {0};

    std::vector<std::jthread> threads;
    std::barrier              start_barrier {THREAD_COUNT};

    // Create worker threads that write to the file
    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&, t](std::stop_token st) {
            start_barrier.arrive_and_wait();

            for (int i = 0; i < WRITES_PER_THREAD; ++i) {
                if (st.stop_requested()) break;

                try {
                    // Checkout the file from the pool
                    {
                        auto file_wrapper = file_pool.checkout();

                        // Write thread-specific data
                        std::fprintf(*file_wrapper, "Thread %d: Write %d\n", t, i);
                        std::fflush(*file_wrapper);

                        total_writes++;
                    }
                    // File is automatically returned to pool
                }
                catch (const std::runtime_error& ex) {
                    std::cerr << "Error: " << ex.what() << std::endl;
                }
            }
        });
    }

    // Wait for all threads to complete
    threads.clear();

    // Verify all writes completed
    EXPECT_EQ(THREAD_COUNT * WRITES_PER_THREAD, total_writes.load());

    // Verify file content
    {
        auto file_wrapper = file_pool.checkout();
        std::rewind(*file_wrapper);

        int  line_count = 0;
        char buffer[100];
        while (std::fgets(buffer, sizeof(buffer), *file_wrapper) != nullptr) {
            line_count++;
        }

        EXPECT_EQ(THREAD_COUNT * WRITES_PER_THREAD, line_count);
    }

    // Cleanup
    std::remove(temp_file.c_str());
}

/**
 * @brief Test resource pool with multiple FILE* handles for parallel writes
 *
 * Demonstrates using a pool with multiple FILE* handles to allow
 * concurrent writes without contention.
 */
TEST(resource_pool_file, multiple_file_handles)
{
    const std::string temp_dir = "/tmp/asynchrony_test_multi/";
    std::filesystem::create_directories(temp_dir);

    constexpr int POOL_SIZE         = 3;
    constexpr int THREAD_COUNT      = 6;
    constexpr int WRITES_PER_THREAD = 20;

    // Create resource pool with multiple FILE* handles
    siddiqsoft::resource_pool<FileHandle> file_pool;

    // Create multiple temporary files and add to pool
    std::vector<std::string> temp_files;
    for (int i = 0; i < POOL_SIZE; ++i) {
        std::string filename = temp_dir + std::format("file_{}.txt", i);
        FILE*       f        = std::fopen(filename.c_str(), "w+");
        ASSERT_NE(nullptr, f);
        file_pool.checkin(FileHandle(f));
        temp_files.push_back(filename);
    }

    EXPECT_EQ(POOL_SIZE, static_cast<int>(file_pool.size()));

    std::atomic_int           successful_writes {0};
    std::atomic_int           failed_writes {0};

    std::vector<std::jthread> threads;
    std::barrier              start_barrier {THREAD_COUNT};

    // Create worker threads
    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&, t](std::stop_token st) {
            start_barrier.arrive_and_wait();

            for (int i = 0; i < WRITES_PER_THREAD; ++i) {
                if (st.stop_requested()) break;

                try {
                    // Checkout a file from the pool
                    {
                        auto file_wrapper = file_pool.checkout();

                        // Write data
                        std::fprintf(*file_wrapper, "Thread %d: Message %d\n", t, i);
                        std::fflush(*file_wrapper);

                        successful_writes++;
                    }
                    // File is automatically returned to pool
                }
                catch (const std::runtime_error&) {
                    failed_writes++;
                }
            }
        });
    }

    // Wait for completion
    threads.clear();

    // Verify results
    EXPECT_EQ(THREAD_COUNT * WRITES_PER_THREAD, successful_writes.load());
    EXPECT_EQ(0, failed_writes.load());

    // Verify all files have content
    for (const auto& filename : temp_files) {
        FILE* f = std::fopen(filename.c_str(), "r");
        ASSERT_NE(nullptr, f);

        int  line_count = 0;
        char buffer[100];
        while (std::fgets(buffer, sizeof(buffer), f) != nullptr) {
            line_count++;
        }

        std::fclose(f);
        EXPECT_GT(line_count, 0);
    }

    // Cleanup
    for (const auto& filename : temp_files) {
        std::remove(filename.c_str());
    }
    std::filesystem::remove_all(temp_dir);
}

/**
 * @brief Test FILE* resource pool with simple_pool for work distribution
 *
 * Demonstrates using resource_pool with simple_pool to distribute
 * write tasks across multiple threads while sharing FILE* resources.
 */
TEST(resource_pool_file, file_pool_with_simple_pool)
{
    const std::string temp_file = "/tmp/asynchrony_test_with_pool.txt";

    // Create resource pool with a single FILE* handle
    auto  file_pool = std::make_shared<siddiqsoft::resource_pool<FileHandle>>();

    FILE* f         = std::fopen(temp_file.c_str(), "w+");
    ASSERT_NE(nullptr, f);
    file_pool->checkin(FileHandle(f));

    // Create a simple_pool that uses the file_pool
    struct WriteTask
    {
        int         thread_id;
        int         message_id;
        std::string message;
    };

    std::atomic_int                    total_writes {0};

    siddiqsoft::simple_pool<WriteTask> write_pool {[file_pool, &total_writes](auto&& task) {
        try {
            // Checkout file from resource pool
            auto file_wrapper = file_pool->checkout();

            // Write the task data
            std::fprintf(*file_wrapper, "[Thread %d] Message %d: %s\n", task.thread_id, task.message_id, task.message.c_str());
            std::fflush(*file_wrapper);

            total_writes++;
        }
        catch (const std::runtime_error& ex) {
            std::cerr << "Error writing: " << ex.what() << std::endl;
        }
    }};

    // Queue write tasks from multiple threads
    constexpr int TASKS = 100;
    for (int i = 0; i < TASKS; ++i) {
        write_pool.queue(WriteTask {i % 4, i, std::format("Message number {}", i)});
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify all writes completed
    EXPECT_EQ(TASKS, total_writes.load());

    // Verify file content
    {
        auto file_wrapper = file_pool->checkout();
        std::rewind(*file_wrapper);

        int  line_count = 0;
        char buffer[200];
        while (std::fgets(buffer, sizeof(buffer), *file_wrapper) != nullptr) {
            line_count++;
        }

        EXPECT_EQ(TASKS, line_count);
    }

    // Cleanup
    std::remove(temp_file.c_str());
}

/**
 * @brief Test FILE* resource pool with high contention
 *
 * Stress test with many threads competing for a single FILE* resource.
 */
TEST(resource_pool_file, high_contention_file_writes)
{
    const std::string                     temp_file = "/tmp/asynchrony_test_contention.txt";

    siddiqsoft::resource_pool<FileHandle> file_pool;

    FILE*                                 f = std::fopen(temp_file.c_str(), "w+");
    ASSERT_NE(nullptr, f);
    file_pool.checkin(FileHandle(f));

    constexpr int             THREAD_COUNT      = 8;
    constexpr int             WRITES_PER_THREAD = 50;

    std::atomic_int           successful_writes {0};
    std::barrier              start_barrier {THREAD_COUNT};

    std::vector<std::jthread> threads;

    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&, t](std::stop_token st) {
            start_barrier.arrive_and_wait();

            for (int i = 0; i < WRITES_PER_THREAD; ++i) {
                if (st.stop_requested()) break;

                try {
                    auto file_wrapper = file_pool.checkout();

                    // Write with timestamp-like data
                    std::fprintf(*file_wrapper, "T%d-W%d\n", t, i);
                    std::fflush(*file_wrapper);

                    successful_writes++;
                }
                catch (const std::runtime_error&) {
                    // Expected under high contention
                }
            }
        });
    }

    threads.clear();

    // Verify writes
    EXPECT_EQ(THREAD_COUNT * WRITES_PER_THREAD, successful_writes.load());

    // Verify file integrity
    {
        auto file_wrapper = file_pool.checkout();
        std::rewind(*file_wrapper);

        int  line_count = 0;
        char buffer[50];
        while (std::fgets(buffer, sizeof(buffer), *file_wrapper) != nullptr) {
            line_count++;
        }

        EXPECT_EQ(THREAD_COUNT * WRITES_PER_THREAD, line_count);
    }

    // Cleanup
    std::remove(temp_file.c_str());
}

/**
 * @brief Test FILE* resource pool with append mode
 *
 * Demonstrates using FILE* in append mode for concurrent logging.
 */
TEST(resource_pool_file, append_mode_logging)
{
    const std::string                     temp_file = "/tmp/asynchrony_test_append.log";

    siddiqsoft::resource_pool<FileHandle> log_pool;

    // Open file in append mode
    FILE* f = std::fopen(temp_file.c_str(), "a+");
    ASSERT_NE(nullptr, f);
    log_pool.checkin(FileHandle(f));

    constexpr int             THREAD_COUNT           = 4;
    constexpr int             LOG_ENTRIES_PER_THREAD = 25;

    std::atomic_int           total_logs {0};
    std::barrier              start_barrier {THREAD_COUNT};

    std::vector<std::jthread> threads;

    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&, t](std::stop_token st) {
            start_barrier.arrive_and_wait();

            for (int i = 0; i < LOG_ENTRIES_PER_THREAD; ++i) {
                if (st.stop_requested()) break;

                try {
                    auto log_file = log_pool.checkout();

                    // Log entry
                    std::fprintf(*log_file, "[Thread %d] Log entry %d\n", t, i);
                    std::fflush(*log_file);

                    total_logs++;
                }
                catch (const std::runtime_error&) {
                    // Handle error
                }
            }
        });
    }

    threads.clear();

    // Verify all logs were written
    EXPECT_EQ(THREAD_COUNT * LOG_ENTRIES_PER_THREAD, total_logs.load());

    // Verify file content
    {
        auto log_file = log_pool.checkout();
        std::rewind(*log_file);

        int  line_count = 0;
        char buffer[100];
        while (std::fgets(buffer, sizeof(buffer), *log_file) != nullptr) {
            line_count++;
        }

        EXPECT_EQ(THREAD_COUNT * LOG_ENTRIES_PER_THREAD, line_count);
    }

    // Cleanup
    std::remove(temp_file.c_str());
}

/**
 * @brief Test FILE* resource pool with binary mode
 *
 * Demonstrates using FILE* in binary mode for concurrent data writes.
 */
TEST(resource_pool_file, binary_mode_writes)
{
    const std::string                     temp_file = "/tmp/asynchrony_test_binary.bin";

    siddiqsoft::resource_pool<FileHandle> bin_pool;

    FileHandle                            f {std::fopen(temp_file.c_str(), "w+b")};
    ASSERT_TRUE(f);
    bin_pool.checkin(std::move(f));

    constexpr int             THREAD_COUNT      = 4;
    constexpr int             WRITES_PER_THREAD = 20;

    std::atomic_int           total_writes {0};
    std::barrier              start_barrier {THREAD_COUNT};

    std::vector<std::jthread> threads;

    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&, t](std::stop_token st) {
            start_barrier.arrive_and_wait();

            for (int i = 0; i < WRITES_PER_THREAD; ++i) {
                if (st.stop_requested()) break;

                try {
                    auto bin_file = bin_pool.checkout();

                    // Write binary data
                    uint32_t data = (t << 16) | i;
                    std::fwrite(&data, sizeof(data), 1, *bin_file);
                    std::fflush(*bin_file);

                    total_writes++;
                }
                catch (const std::runtime_error&) {
                    // Handle error
                }
            }
        });
    }

    threads.clear();

    // Verify all writes completed
    EXPECT_EQ(THREAD_COUNT * WRITES_PER_THREAD, total_writes.load());

    // Verify file size
    {
        auto bin_file = bin_pool.checkout();
        std::fseek(*bin_file, 0, SEEK_END);
        long file_size = std::ftell(*bin_file);

        // Each write is 4 bytes (uint32_t)
        EXPECT_EQ(static_cast<long>(THREAD_COUNT * WRITES_PER_THREAD * sizeof(uint32_t)), file_size);
    }

    // Cleanup
    std::remove(temp_file.c_str());
}

// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
