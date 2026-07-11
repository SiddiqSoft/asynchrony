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

#include <chrono>
#include <exception>
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

// Helper function to get a platform-independent temporary file path
inline std::string get_temp_file_path(const std::string& filename)
{
    auto temp_dir  = std::filesystem::temp_directory_path();
    auto temp_file = temp_dir / filename;
    return temp_file.string();
}

// Helper function to safely remove a file
inline void safe_remove_file(const std::string& filepath)
{
    std::error_code ec;
    std::filesystem::remove(filepath, ec);
}

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

/**
 * @brief RAII wrapper for FILE* to ensure proper cleanup
 *
 * This wrapper ensures that FILE* resources are properly closed when
 * they go out of scope, even if an exception occurs.
 */
class FileHandle : public siddiqsoft::resource_wrap<FILE*>
{
public:
    FileHandle() = delete;

    auto to_string() -> std::string const
    {
        return std::format("FileHandle - FILE* {:p} debugId:{}  isValid:{}\n", static_cast<void*>(rsrc), debugId, isValid);
    }

    // Constructor from FILE*
    explicit FileHandle(FILE*&& f) noexcept
        : resource_wrap {std::forward<FILE*>(f)}
    {
    }

    // Move constructor
    FileHandle(FileHandle&& other) noexcept
        : resource_wrap {other.release()}
    {
    }

    // Move assignment
    FileHandle& operator=(FileHandle&& other) noexcept
    {
        if (this != &other) {
            close();
            rsrc = std::move(other.release());
        }
        std::cerr << std::format("  Assigned: {}", to_string());
        return *this;
    }

    // Delete copy operations
    FileHandle(const FileHandle&)            = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    // Destructor
    ~FileHandle()
    {
        if (rsrc) {
#if defined(DEBUG)
            std::cerr << std::format(" ~FileHandle - {}",to_string());
#endif
            std::fflush(rsrc);
        }
        else {
            std::cerr << std::format("FileHandle - No/empty resource!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! {}\n", to_string());
        }
    }

    // Get raw pointer
    // operator FILE*() { return rsrc; }

    // Release ownership
    [[nodiscard]] FILE* release()
    {
        FILE* temp = rsrc;
        rsrc       = nullptr;
        return temp;
    }

    // Close the file
    void close()
    {
        if (rsrc != nullptr) {
            std::fclose(rsrc);
            rsrc = nullptr;
        }
    }

    // Operator-> for convenience
    FILE* operator->() const { return rsrc; }

    auto& operator=(FILE* f)
    {
        // Make sure we close and release the current handle..
        close();
        release();
        // Now we can accept the new one..
        rsrc = f;
        return *this;
    }

    // Boolean conversion
    explicit operator bool() const { return rsrc != nullptr; }
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
    const std::string temp_file = get_temp_file_path("asynchrony_test_basic.txt");

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
    }
    // File is automatically returned to pool

    EXPECT_EQ(1u, file_pool.size());

    // Checkout again and verify content
    {
        auto file_wrapper = file_pool.checkout();
        std::rewind(*file_wrapper);

        char buffer[100] = {};
        ASSERT_NE(nullptr, std::fgets(buffer, sizeof(buffer), *file_wrapper));
        EXPECT_STREQ("Hello, World!\n", buffer);
    }

    // Cleanup
    safe_remove_file(temp_file);
}

/**
 * @brief Test FileHandle explicit constructor with valid FILE*
 */
TEST(resource_pool_file, file_handle_explicit_constructor)
{
    const std::string temp_file = get_temp_file_path("asynchrony_test_explicit.txt");
    FileHandle        fh {std::fopen(temp_file.c_str(), "w")};
    EXPECT_TRUE(fh);

    fh.close();
    safe_remove_file(temp_file);
}

/**
 * @brief Test FileHandle move constructor
 */
TEST(resource_pool_file, file_handle_move_constructor)
{
    const std::string temp_file = get_temp_file_path("asynchrony_test_move_ctor.txt");
    FileHandle        fh1 {std::fopen(temp_file.c_str(), "w")};
    EXPECT_TRUE(fh1);

    FileHandle fh2(std::move(fh1));
    EXPECT_FALSE(fh1);
    EXPECT_TRUE(fh2);

    fh2.close();
    safe_remove_file(temp_file);
}

/**
 * @brief Test FileHandle move assignment operator
 */
TEST(resource_pool_file, file_handle_move_assignment)
{
    const std::string temp_file1 = get_temp_file_path("asynchrony_test_move_assign1.txt");
    const std::string temp_file2 = get_temp_file_path("asynchrony_test_move_assign2.txt");

    FileHandle        fh1 {std::fopen(temp_file1.c_str(), "w")};
    FileHandle        fh2 {std::fopen(temp_file2.c_str(), "w")};

    fh1 = std::move(fh2);

    EXPECT_FALSE(fh2);

    fh1.close();
    safe_remove_file(temp_file1);
    safe_remove_file(temp_file2);
}

/**
 * @brief Test FileHandle release method
 */
TEST(resource_pool_file, file_handle_release)
{
    const std::string temp_file = get_temp_file_path("asynchrony_test_release.txt");
    FileHandle        fh {std::fopen(temp_file.c_str(), "w")};
    EXPECT_TRUE(fh);

    FILE* released = fh.release();
    EXPECT_FALSE(fh);

    std::fclose(released);
    safe_remove_file(temp_file);
}

/**
 * @brief Test FileHandle close method
 */
TEST(resource_pool_file, file_handle_close)
{
    const std::string temp_file = get_temp_file_path("asynchrony_test_close.txt");
    FileHandle        fh {std::fopen(temp_file.c_str(), "w")};

    EXPECT_TRUE(fh);

    fh.close();
    EXPECT_FALSE(fh);

    safe_remove_file(temp_file);
}

/**
 * @brief Test FileHandle operator-> for pointer access
 */
TEST(resource_pool_file, file_handle_operator_arrow)
{
    const std::string temp_file = get_temp_file_path("asynchrony_test_arrow.txt");
    FileHandle        fh {std::fopen(temp_file.c_str(), "w")};
    EXPECT_TRUE(fh);

    // Use operator-> to write to file
    std::fprintf(fh.operator->(), "Test content\n");
    std::fflush(fh.operator->());

    fh.close();
    safe_remove_file(temp_file);
}

/**
 * @brief Test FileHandle operator FILE* for implicit conversion
 */
TEST(resource_pool_file, file_handle_operator_file_ptr)
{
    const std::string temp_file = get_temp_file_path("asynchrony_test_file_ptr.txt");
    FileHandle        fh {std::fopen(temp_file.c_str(), "w")};
    EXPECT_TRUE(fh);

    // Use implicit conversion to FILE*
    std::fprintf(fh, "Implicit conversion test\n");
    std::fflush(fh);

    fh.close();
    safe_remove_file(temp_file);
}

/**
 * @brief Test FileHandle assignment operator with FILE*
 */
TEST(resource_pool_file, file_handle_assignment_operator)
{
    const std::string temp_file1 = get_temp_file_path("asynchrony_test_assign1.txt");
    const std::string temp_file2 = get_temp_file_path("asynchrony_test_assign2.txt");

    FileHandle        fh {std::fopen(temp_file1.c_str(), "w")};
    FILE*             fp2 = std::fopen(temp_file2.c_str(), "w");
    ASSERT_NE(nullptr, fp2);

    fh = fp2;
    EXPECT_EQ(fp2, static_cast<FILE*>(fh));

    fh.close();
    safe_remove_file(temp_file1);
    safe_remove_file(temp_file2);
}

/**
 * @brief Test FileHandle boolean conversion operator
 */
TEST(resource_pool_file, file_handle_bool_conversion)
{
    const std::string temp_file = get_temp_file_path("asynchrony_test_bool.txt");
    FileHandle        fh {std::fopen(temp_file.c_str(), "w")};

    EXPECT_TRUE(static_cast<bool>(fh));

    fh.close();
    EXPECT_FALSE(static_cast<bool>(fh));

    safe_remove_file(temp_file);
}

/**
 * @brief Test multiple FileHandles in resource pool
 */
TEST(resource_pool_file, multiple_file_handles_in_pool)
{
    const std::string                     temp_file1 = get_temp_file_path("asynchrony_test_multi1.txt");
    const std::string                     temp_file2 = get_temp_file_path("asynchrony_test_multi2.txt");
    const std::string                     temp_file3 = get_temp_file_path("asynchrony_test_multi3.txt");

    siddiqsoft::resource_pool<FileHandle> file_pool;

    // Add three files to the pool
    FileHandle f1 {std::fopen(temp_file1.c_str(), "w+")};
    FileHandle f2 {std::fopen(temp_file2.c_str(), "w+")};
    FileHandle f3 {std::fopen(temp_file3.c_str(), "w+")};

    ASSERT_TRUE(f1);
    ASSERT_TRUE(f2);
    ASSERT_TRUE(f3);

    file_pool.checkin(std::move(f1));
    file_pool.checkin(std::move(f2));
    file_pool.checkin(std::move(f3));

    EXPECT_EQ(3u, file_pool.size());

    // Checkout and use each file
    {
        auto fw1 = file_pool.checkout();
        EXPECT_EQ(2u, file_pool.size());
        std::fprintf(*fw1, "File 1\n");
        std::fflush(*fw1);
    }

    EXPECT_EQ(3u, file_pool.size());

    {
        auto fw2 = file_pool.checkout();
        EXPECT_EQ(2u, file_pool.size());
        std::fprintf(*fw2, "File 2\n");
        std::fflush(*fw2);
    }

    EXPECT_EQ(3u, file_pool.size());

    {
        auto fw3 = file_pool.checkout();
        EXPECT_EQ(2u, file_pool.size());
        std::fprintf(*fw3, "File 3\n");
        std::fflush(*fw3);
    }

    EXPECT_EQ(3u, file_pool.size());

    // Cleanup
    safe_remove_file(temp_file1);
    safe_remove_file(temp_file2);
    safe_remove_file(temp_file3);
}

/**
 * @brief Test FileHandle with read and write operations
 */
TEST(resource_pool_file, file_handle_read_write)
{
    const std::string                     temp_file = get_temp_file_path("asynchrony_test_rw.txt");

    siddiqsoft::resource_pool<FileHandle> file_pool;

    // Write to file
    {
        FileHandle f {std::fopen(temp_file.c_str(), "w+")};
        ASSERT_TRUE(f);

        file_pool.checkin(std::move(f));
        EXPECT_EQ(1u, file_pool.size());

        auto fw = file_pool.checkout();
        std::fprintf(*fw, "Line 1\n");
        std::fprintf(*fw, "Line 2\n");
        std::fprintf(*fw, "Line 3\n");
        std::fflush(*fw);
    }

    EXPECT_EQ(1u, file_pool.size());

    // Read from file
    {
        auto fw = file_pool.checkout();
        std::rewind(*fw);

        char buffer[100] = {};
        ASSERT_NE(nullptr, std::fgets(buffer, sizeof(buffer), *fw));
        EXPECT_STREQ("Line 1\n", buffer);

        ASSERT_NE(nullptr, std::fgets(buffer, sizeof(buffer), *fw));
        EXPECT_STREQ("Line 2\n", buffer);

        ASSERT_NE(nullptr, std::fgets(buffer, sizeof(buffer), *fw));
        EXPECT_STREQ("Line 3\n", buffer);
    }

    // Cleanup
    safe_remove_file(temp_file);
}

/**
 * @brief Test FileHandle destructor cleanup
 */
TEST(resource_pool_file, file_handle_destructor_cleanup)
{
    const std::string temp_file = get_temp_file_path("asynchrony_test_dtor.txt");

    {
        FileHandle fh {std::fopen(temp_file.c_str(), "w")};
        ASSERT_TRUE(fh);
        std::fprintf(fh, "Destructor test\n");
        std::fflush(fh);
        // fh goes out of scope and destructor is called
    }

    // File should be closed and readable
    FILE* fp = std::fopen(temp_file.c_str(), "r");
    ASSERT_NE(nullptr, fp);

    char buffer[100] = {};
    ASSERT_NE(nullptr, std::fgets(buffer, sizeof(buffer), fp));
    EXPECT_STREQ("Destructor test\n", buffer);

    std::fclose(fp);
    safe_remove_file(temp_file);
}

/**
 * @brief Test FileHandle with concurrent access
 */
TEST(resource_pool_file, file_handle_concurrent_access)
{
    const std::string                     temp_file = get_temp_file_path("asynchrony_test_concurrent.txt");

    siddiqsoft::resource_pool<FileHandle> file_pool;

    std::cerr << std::format("About to add file {} to the pool..\n",  temp_file );
    // Add a file to the pool
    FileHandle f {std::fopen(temp_file.c_str(), "w+")};
    ASSERT_TRUE(f);
    std::cerr << std::format("About to checkin..{:p}\n", static_cast<void*>(*f));
    file_pool.checkin(std::move(f));
    EXPECT_EQ(1u, file_pool.size());

    std::atomic<int>         write_count {0};
    std::vector<std::thread> threads;

    std::cerr << std::format("About to kick off the threads to use pool with {} items.\n", file_pool.size());
    // Create multiple threads that write to the file
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&file_pool, &write_count, i]() {
            try {
                auto fw = file_pool.checkout();
                std::fprintf(*fw, "Thread %d\n", i);
                ++write_count;
            }
            catch (std::exception& ex) {
                // Skip when we're getting an empty pool message
                // if other threads are using up single resource!
                std::cerr << std::format("Ignoring: {}\n", ex.what());
            }
        });
    }

    // Critical to wait for a second otherwise terminating will stop processing
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    std::cerr << std::format("About to terminated threads {}.\n", threads.size());
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GE(write_count, 1);
    EXPECT_EQ(1u, file_pool.size());

    // Cleanup
    safe_remove_file(temp_file);
}

// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
