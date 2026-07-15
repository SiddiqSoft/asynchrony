/*
    asynchrony : Add asynchrony to your apps

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

#pragma once
#include <type_traits>
#ifndef RESOURCE_WRAP_HPP
#define RESOURCE_WRAP_HPP

#include <cstdint>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <deque>
#include <format>
#include <concepts>

#include "common.hpp"

namespace siddiqsoft
{
    template <typename T>
    concept NonNumericMoveConstructible = std::move_constructible<T> && !std::is_arithmetic_v<T>;

    /**
     * @brief RAII wrapper for checked-out resources with validity tracking
     * Use the resource_pool as a sole owner of the resources/objects
     *
     * @warning CRITICAL SAFETY FEATURE: This wrapper tracks resource validity to prevent
     * returning uninitialized or moved-out resources to the pool. Only valid resources are
     * returned to the pool on destruction. This prevents pool corruption.
     * @note When using shared_ptr remember that you have to std::move into the resource_pool
     * and the original variable would be empty. You must no share ownership of the object/resource
     * resource_pool. The move semantics ensure that the resource you're using is returned to the
     * pool once the resource_wrap goes out of scope.
     * The caller is responsible for tracking the validity of the resource (example closed or aborted connection.)
     *
     * @details
     * The resource_wrap class provides automatic resource management through RAII (Resource
     * Acquisition Is Initialization). When a resource is checked out from a resource_pool,
     * it is wrapped in a resource_wrap that automatically returns it to the pool when destroyed.
     *
     * Key Features:
     * - Automatic resource return via RAII pattern
     * - Validity tracking prevents pool corruption
     * - Pointer-like access to underlying resource
     * - Move-only semantics (no copying)
     * - Thread-safe when used with resource_pool
     * - Support for derived classes with custom behavior
     *
     * Validity Tracking:
     * - Resources are marked as valid when constructed
     * - Destructor only returns valid resources to the pool
     * - Invalid resources are discarded (not returned)
     * - Use invalidate() to prevent automatic return
     *
     * @tparam T The resource type (must be move-constructible)
     *
     * @section derived_classes Creating Derived Classes
     *
     * You can create custom wrapper classes by deriving from resource_wrap to add
     * domain-specific functionality. This is useful for resources that need special
     * handling, cleanup, or convenience methods.
     *
     * @subsection derived_example Example: FileHandle Wrapper
     *
     * Here's a complete example of a derived class for FILE* resources:
     *
     * @code
     * class FileHandle : public siddiqsoft::resource_wrap<FILE*>
     * {
     * public:
     *     // Delete default constructor
     *     FileHandle() = delete;
     *
     *     // Constructor from FILE*
     *     explicit FileHandle(FILE*&& f) noexcept
     *         : resource_wrap(std::move(f))
     *     {
     *     }
     *
     *     // Move constructor
     *     FileHandle(FileHandle&& other) noexcept
     *         : resource_wrap(std::move(other))
     *     {
     *     }
     *
     *     // Move assignment
     *     FileHandle& operator=(FileHandle&& other) noexcept
     *     {
     *         if (this != &other) {
     *             close();
     *             _rsrc = std::move(other._rsrc);
     *         }
     *         return *this;
     *     }
     *
     *     // Delete copy operations
     *     FileHandle(const FileHandle&)            = delete;
     *     FileHandle& operator=(const FileHandle&) = delete;
     *
     *     // Custom methods
     *     void close()
     *     {
     *         if (_rsrc != nullptr) {
     *             std::fclose(_rsrc);
     *             _rsrc = nullptr;
     *         }
     *     }
     *
     *     FILE* operator->() const { return _rsrc; }
     *     explicit operator bool() const { return _rsrc != nullptr; }
     * };
     * @endcode
     *
     * @subsection derived_usage Using Derived Classes with resource_pool
     *
     * @code
     * // Create a pool with the derived wrapper type
     * siddiqsoft::resource_pool<FILE*, FileHandle> file_pool;
     *
     * // Create and wrap a resource
     * auto wrapped = file_pool.wrapResource(std::fopen("file.txt", "r"));
     * // wrapped is now a FileHandle with auto-checkin configured
     *
     * // When wrapped goes out of scope, it automatically returns to pool
     * @endcode
     *
     * @subsection derived_guidelines Guidelines for Derived Classes
     *
     * 1. **Constructor Pattern**: Derived classes should have their own constructor
     *    that takes `T&&` (the resource type). Call the base constructor with the resource.
     *    The base class constructor has an optional callback parameter that defaults to empty.
     *
     * 2. **Move Semantics**: Implement move constructor and move assignment operator.
     *    Delete copy constructor and copy assignment operator to maintain move-only semantics.
     *
     * 3. **Protected Members**: Access protected members (_rsrc, _isValid, _putbackCallback)
     *    directly in your derived class for custom behavior. These are accessible because
     *    resource_pool is declared as a friend class.
     *
     * 4. **Custom Methods**: Add domain-specific methods (e.g., close(), flush(), etc.)
     *    to provide a convenient interface for your resource type.
     *
     * 5. **Destructor**: If you need custom cleanup, implement a destructor. The base
     *    class destructor will still handle returning the resource to the pool if valid.
     *
     * 6. **Compatibility**: Derived classes work seamlessly with resource_pool::wrapResource()
     *    which handles setting up the auto-checkin callback through friend access.
     *
     * 7. **Constructor Flexibility**: Unlike the base class which requires a callback parameter,
     *    derived classes can have custom constructors that only take the resource type.
     *    The pool will set up the callback after construction.
     *
     * @example
     * @code
     * // Typical usage (automatic return)
     * {
     *     auto resource = pool.checkout();
     *     resource->doSomething();
     *     // Resource automatically returned to pool when scope exits
     * }
     *
     * // Advanced usage (prevent return)
     * {
     *     auto resource = pool.checkout();
     *     auto ptr = std::move(*resource);
     *     resource.invalidate();  // Don't return the moved-out resource
     *     // Resource is NOT returned to pool
     * }
     *
     * // Using derived class
     * {
     *     siddiqsoft::resource_pool<FILE*, FileHandle> file_pool;
     *     auto file = file_pool.wrapResource(std::fopen("data.txt", "r"));
     *     // Use file with custom FileHandle methods
     *     file.close();  // Custom method
     * }
     * @endcode
     *
     * @see resource_pool
     */
    template <typename T>
        requires NonNumericMoveConstructible<T>
    class resource_wrap
    {
        // Allow resource_pool to access protected members
        template <typename U, typename RW, uint8_t IC>
            requires((IC <= resource_pool_limits::MaxCapacity)) && NonNumericMoveConstructible<U> && std::derived_from<RW, resource_wrap<U>>
        friend class resource_pool;

    protected:
        /// @brief The actual resource being wrapped
        T _rsrc {};

        /// @brief Debug identifier for tracking (used in DEBUG builds)
        uint64_t _debugId {static_cast<uint64_t>(std::rand())};

        /// @brief Callback function to return the resource to the pool
        /// Called by destructor when resource is valid
        std::function<void(T&&)> _putbackCallback {};

        /// @brief Tracks whether the resource is valid and should be returned to pool
        /// Prevents returning uninitialized or moved-out resources
        /// - true: resource will be returned to pool on destruction
        /// - false: resource will NOT be returned to pool on destruction
        bool _isValid {false};

    public:
        /// @brief Default constructor is deleted
        /// Resources must be explicitly constructed with a valid resource
        resource_wrap() = delete;

        /**
         * @brief Construct a resource_wrap with a resource and optional callback
         *
         * @param src R-value reference to the resource to wrap
         * @param f Optional callback function to return resource to pool
         *
         * @details
         * The resource is marked as valid upon construction. The callback is typically
         * provided by resource_pool::wrapResource() to automatically return the resource.
         *
         * For derived classes, the callback parameter can be omitted and will be set
         * by resource_pool::wrapResource() through friend access to protected members.
         *
         * @note This constructor is typically called by resource_pool::wrapResource()
         */
        explicit resource_wrap(T&& src, std::function<void(T&&)>&& f = {})
            : _rsrc(std::move(src))
            , _putbackCallback(std::move(f))
            , _isValid(true)
        {
        }


        /// @brief Copy constructor is deleted
        /// Resources are move-only to maintain clear ownership semantics
        explicit resource_wrap(const T&) = delete;

        /**
         * @brief Move constructor
         *
         * @param src R-value reference to another resource_wrap to move from
         *
         * @details
         * Moves the resource and callback from another wrapper.
         * This is essential for returning wrapped resources from functions.
         * Derived classes should call this constructor in their move constructor.
         */
        resource_wrap(resource_wrap&& src) noexcept
            : _rsrc(std::move(src._rsrc))
            , _debugId(src._debugId)
            , _putbackCallback(std::move(src._putbackCallback))
            , _isValid(src._isValid)
        {
        }

        /**
         * @brief Move assignment operator
         *
         * @param src R-value reference to the resource to assign
         * @return Reference to this resource_wrap
         *
         * @details
         * Assigns a new resource to this wrapper and marks it as valid.
         * The previous resource (if any) is discarded.
         */
        resource_wrap& operator=(T&& src)
        {
#if defined(DEBUG)
            std::cerr << std::format("  - resource_wrap: move into debugId:{}\n", _debugId);
#endif
            _rsrc    = std::move(src);
            _isValid = true;
            return *this;
        };

        /// @brief Copy assignment is deleted
        resource_wrap& operator=(const resource_wrap&) = delete;

        /**
         * @brief Dereference operator to access the underlying resource
         *
         * @return Reference to the wrapped resource
         *
         * @example
         * @code
         * auto resource = pool.checkout();
         * (*resource)->doSomething();  // Access via dereference
         * @endcode
         */
        auto operator*() -> T& { return _rsrc; }

        /**
         * @brief Type conversion operator
         *
         * @return Copy of the wrapped resource
         *
         * @details
         * Allows implicit conversion to the resource type.
         * Useful for passing to functions expecting the resource type.
         */
        operator T() { return _rsrc; }

        /**
         * @brief Destructor - automatically returns resource to pool if valid
         *
         * @details
         * The destructor implements the RAII pattern:
         * - If isValid is true and putbackCallback exists: returns resource to pool
         * - If isValid is false: resource is discarded (not returned)
         *
         * This ensures resources are always properly managed, even if an exception occurs.
         *
         * @note This is called automatically when the resource_wrap goes out of scope
         */
        ~resource_wrap()
        {
#if defined(DEBUG)
            std::cerr << std::format("  - ~resource_wrap: putback debugId:{}  isValid:{}\n", _debugId, _isValid);
#endif
            // Only return resource if it's valid and callback exists
            // This prevents returning uninitialized or moved-out resources to the pool
            if (_isValid && _putbackCallback) {
                _putbackCallback(std::move(_rsrc));
                _isValid = false;
            }
        }

        /**
         * @brief Invalidate the resource to prevent it from being returned to pool
         *
         * @details
         * Call this method when you've moved the resource out or want to prevent
         * automatic return to the pool. After calling this, the destructor will NOT
         * return the resource to the pool.
         *
         * Use Cases:
         * - You've moved the resource out and it's no longer valid
         * - You want to take ownership and prevent automatic return
         * - You're implementing custom resource management
         *
         * @note Safe to call multiple times
         * @note This is primarily for advanced scenarios; normal usage doesn't need this
         *
         * @example
         * @code
         * auto resource = pool.checkout();
         * auto ptr = std::move(*resource);
         * resource.invalidate();  // Don't return the moved-out resource
         * // Resource is NOT returned to pool
         * @endcode
         */
        void invalidate() { _isValid = false; }
    };

} // namespace siddiqsoft

#endif
