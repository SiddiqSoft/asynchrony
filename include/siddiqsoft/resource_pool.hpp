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
#ifndef RESOURCE_POOL_HPP
#define RESOURCE_POOL_HPP

#include <cstdint>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <deque>
#include <format>
#include <concepts>

#include "siddiqsoft/RunOnEnd.hpp"

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
     *
     * Validity Tracking:
     * - Resources are marked as valid when constructed
     * - Destructor only returns valid resources to the pool
     * - Invalid resources are discarded (not returned)
     * - Use invalidate() to prevent automatic return
     *
     * @tparam T The resource type (must be move-constructible)
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
     * @endcode
     *
     * @see resource_pool
     */
    template <typename T>
        requires NonNumericMoveConstructible<T>
    class resource_wrap
    {
        // Allow resource_pool to access protected members
        template <typename U, typename RW, uint16_t IC>
            requires((IC <= sizeof(uint16_t))) && NonNumericMoveConstructible<U> && std::derived_from<RW, resource_wrap<U>>
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
         * provided by resource_pool::checkout() to automatically return the resource.
         *
         * @note This constructor is typically called by resource_pool::checkout()
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

    /**
     * @brief Thread-safe resource pool for managing reusable objects
     *
     * @details
     * The resource_pool class provides efficient resource pooling for managing expensive
     * resources like database connections, file handles, thread pools, or other reusable
     * objects. Resources are checked out from the pool and automatically returned when
     * the wrapper goes out of scope (RAII pattern).
     *
     * Key Features:
     * - Thread-safe operations protected by mutex
     * - RAII pattern ensures resources are always returned
     * - FIFO (First-In-First-Out) ordering for resource retrieval
     * - Automatic resource cleanup on pool destruction
     * - Validity tracking prevents pool corruption
     * - Support for derived wrapper classes with custom constructors
     *
     * Thread Safety:
     * - All public methods are thread-safe
     * - Uses std::mutex to protect internal state
     * - Safe for concurrent checkout/checkin operations
     *
     * Performance Considerations:
     * - Ideal capacity should match std::thread::hardware_concurrency()
     * - Each checkout/checkin operation acquires a lock
     * - Resources are stored in a deque for efficient FIFO access
     * - There is overhead from the resource_wrap wrapper
     *
     * @tparam T The resource type (must be move-constructible)
     *           Examples: std::shared_ptr<Connection>, std::unique_ptr<Buffer>, FILE*
     * @tparam RW The resource wrapper type (default: resource_wrap<T>)
     *            Can be a derived class with custom behavior
     * @tparam InitCapacity Initial capacity hint (default: 1 byte, max: 65535)
     *
     * @example
     * @code
     * // Create a pool of database connections
     * siddiqsoft::resource_pool<std::shared_ptr<DbConnection>> pool;
     *
     * // Add resources to the pool
     * // IMPORTANT: Use std::move when checking in shared_ptr to transfer ownership
     * auto conn1 = std::make_shared<DbConnection>("localhost");
     * pool.checkin(std::move(conn1));  // conn1 is now empty
     *
     * auto conn2 = std::make_shared<DbConnection>("localhost");
     * pool.checkin(std::move(conn2));  // conn2 is now empty
     *
     * // Check out and use a resource
     * {
     *     auto connection = pool.checkout();
     *     (*connection)->executeQuery("SELECT * FROM users");
     *     // Connection automatically returned to pool when scope exits
     * }
     *
     * // Handle empty pool
     * try {
     *     auto connection = pool.checkout();
     * }
     * catch (const std::runtime_error& e) {
     *     std::cerr << "Pool is empty: " << e.what() << std::endl;
     * }
     *
     * // IMPORTANT: When using shared_ptr, the pool becomes the sole owner
     * // Do NOT keep external references to the pooled objects:
     * // WRONG:
     * // auto conn = std::make_shared<DbConnection>("localhost");
     * // pool.checkin(conn);  // BAD: conn still holds a reference!
     * // This creates a reference cycle and prevents proper resource reuse
     *
     * // CORRECT:
     * // auto conn = std::make_shared<DbConnection>("localhost");
     * // pool.checkin(std::move(conn));  // GOOD: pool is sole owner
     * @endcode
     *
     * @see resource_wrap
     */
    template <typename T, typename RW = resource_wrap<T>, uint16_t InitCapacity = sizeof(uint8_t)>
        requires((InitCapacity <= sizeof(uint16_t))) && NonNumericMoveConstructible<T> && std::derived_from<RW, resource_wrap<T>>
    class resource_pool
    {
    private:
        /// @brief Internal deque storing the pooled resources
        /// Uses FIFO ordering: resources are added to back, retrieved from front
        std::deque<T> _pool {};

        /// @brief Mutex protecting access to the resource pool
        /// Uses a regular mutex (not recursive) since no recursive locking is needed
        std::mutex _poolLock {};

    public:
        /// @brief Default constructor
        /// Creates an empty pool ready to accept resources
        resource_pool() = default;

        /// @brief Copy constructor (deleted - pools are not copyable)
        /// Each pool manages its own resources independently
        resource_pool(resource_pool&) = delete;

        /// @brief Move constructor (defaulted)
        /// Allows moving a pool to a new location
        resource_pool(resource_pool&& src) = default;

        /// @brief Copy assignment operator (deleted - pools are not copyable)
        resource_pool& operator=(resource_pool&) = delete;

        /// @brief Move assignment operator (defaulted)
        resource_pool& operator=(resource_pool&& src) = default;

        /// @brief Destructor - clears all resources from the pool
        /// All remaining resources are destroyed
        ~resource_pool() { clear(); }

        /**
         * @brief Clear all items from the pool
         *
         * @details
         * Removes and destroys all resources currently in the pool.
         * Thread-safe operation. Safe to call on an empty pool.
         *
         * @note All resources are destroyed when cleared
         * @note Any checked-out resources are NOT affected
         */
        void clear()
        {
            std::scoped_lock<std::mutex> l(_poolLock);
            _pool.clear();
        }

        /**
         * @brief Get the current size of the pool
         *
         * @return The number of available resources currently in the pool
         *
         * @details
         * Returns the number of resources available for checkout.
         * Thread-safe operation.
         *
         * @note This prevents TOCTOU (Time-of-Check-Time-of-Use) race conditions
         *       by returning the size directly without separate empty checks
         * @note Size may change immediately after this call due to concurrent access
         */
        auto size()
        {
            std::scoped_lock<std::mutex> l(_poolLock);
            return _pool.size();
        }

        /**
         * @brief Check out a resource from the pool
         *
         * @return A resource_wrap containing the checked-out resource
         * @throws std::runtime_error if the pool is empty
         *
         * @details
         * Retrieves a resource from the pool and wraps it in a resource_wrap that
         * automatically returns the resource when destroyed. This implements the RAII
         * pattern to ensure resources are always returned to the pool.
         *
         * The returned resource_wrap:
         * - Provides pointer-like access to the resource
         * - Automatically returns the resource to the pool on destruction
         * - Ensures resource return even if an exception occurs
         * - Can be invalidated to prevent automatic return
         *
         * Thread Safety:
         * - Thread-safe operation protected by mutex
         * - Multiple threads can safely checkout simultaneously
         * - Resources are retrieved in FIFO order
         *
         * @note The [[nodiscard]] attribute encourages proper usage of the returned wrapper
         * @note If the pool is empty, throws std::runtime_error
         *
         * @example
         * @code
         * try {
         *     auto resource = pool.checkout();
         *     // Use resource...
         *     // Automatically returned to pool when scope exits
         * }
         * catch (const std::runtime_error& e) {
         *     std::cerr << "No resources available: " << e.what() << std::endl;
         * }
         * @endcode
         */
        [[nodiscard]] auto checkout() -> RW /* throw() */
        {
            {
                std::scoped_lock<std::mutex> l(_poolLock);

                if (!_pool.empty()) {
                    RunOnEnd roe([&]() { _pool.pop_front(); });

                    return wrapResource(std::move(_pool.front()));
                    // The pop_front() happens within this scope and
                    // within the lock!
                }
            } // scope end

            throw std::runtime_error("Empty pool; add something first!");
        }

        /**
         * @brief Make a resource_wrap from the src. It does not add to the pool.
         *
         * @param src R-value reference to the resource to wrap
         * @return A resource wrapper with auto-checkin configured
         *
         * @details
         * The intention is to allow for creation of the resource and wire it up
         * to auto-checkin to the pool when the scope exits.
         *
         * This method supports both base resource_wrap and derived classes.
         * For derived classes with custom constructors, it:
         * 1. Constructs the derived class with just the resource
         * 2. Sets up the callback and validity through friend access
         * 3. Returns the fully configured wrapper
         *
         * @example
         * @code
         * // With base resource_wrap
         * auto wrapped = pool.wrapResource(std::move(resource));
         *
         * // With derived class (e.g., FileHandle)
         * auto file_wrapped = pool.wrapResource(std::fopen("file.txt", "r"));
         * @endcode
         */
        [[nodiscard]] auto wrapResource(T&& src) -> RW
        {
            /// @brief Lambda that returns the resource back to the pool
            /// Captures 'this' to access the pool's checkin method
            /// Called by resource_wrap destructor to ensure automatic return
            /// even if an exception occurs
            auto autoReturnResource = [this](T&& src) {
                this->checkin(std::move(src));
            };

            // We return the resource back to the caller as a wrapper that has
            // the auto-checkin wired up to our pool.
            // For derived classes, we need to handle the case where the derived class
            // has a different constructor signature than the base class.
            // We construct the derived class first, then set the callback.
            RW wrapper(std::move(src));

            // Set the callback and validity on the base class members
            // resource_pool is a friend of resource_wrap, so we can access protected members
            wrapper._putbackCallback = std::move(autoReturnResource);
            wrapper._isValid         = true;

            return wrapper;
        }

        /**
         * @brief Return a resource to the pool
         *
         * @param rsrc R-Value reference to the resource to return to the pool
         *
         * @details
         * Adds a resource back to the pool, making it available for future checkout
         * operations. This is typically called automatically by the resource_wrap
         * destructor, but can also be called manually.
         *
         * Thread Safety:
         * - Thread-safe operation protected by mutex
         * - Multiple threads can safely checkin simultaneously
         *
         * @note Thread-safe operation protected by mutex
         * @note Resources are added to the back of the deque (FIFO)
         * @note This method is typically not called directly; use checkout() instead
         * @note Only valid resources should be checked in (not moved-out or invalid)
         * @note When using shared_ptr, always use std::move to transfer ownership to the pool
         *
         * @example
         * @code
         * // Automatic return (typical usage)
         * {
         *     auto resource = pool.checkout();
         *     // Use resource...
         * }  // Automatically returned via resource_wrap destructor
         *
         * // Manual return (advanced usage)
         * auto resource = pool.checkout();
         * // ... use resource ...
         * pool.checkin(std::move(*resource));
         *
         * // For shared_ptr: Always use std::move
         * auto conn = std::make_shared<DbConnection>("localhost");
         * pool.checkin(std::move(conn));  // Transfer ownership to pool
         * @endcode
         */
        void checkin(T&& rsrc)
        {
            std::scoped_lock<std::mutex> l(_poolLock);
            _pool.push_back(std::move(rsrc));
        }
    };
} // namespace siddiqsoft
#endif
