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
#include <atomic>
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
#include "private/common.hpp"
#include "private/resource_wrap.hpp"

namespace siddiqsoft
{
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
    template <typename T, typename RW = resource_wrap<T>, uint8_t InitCapacity = resource_pool_limits::DefaultCapacity>
        requires((InitCapacity <= resource_pool_limits::MaxCapacity)) && NonNumericMoveConstructible<T> &&
                std::derived_from<RW, resource_wrap<T>>
    class resource_pool
    {
    private:
        uint8_t  _capacity {InitCapacity};
        uint16_t _resourcesCheckedout {0};
        uint16_t _invalidatedResources {0};

        /// @brief Internal deque storing the pooled resources
        /// Uses FIFO ordering: resources are added to back, retrieved from front
        std::deque<T> _pool {};

#if defined(DEBUG)
        /// @brief Mutex protecting access to the resource pool
        /// Uses a standard mutex
        /// @note Marked as mutable to allow usage within const methods
        mutable std::recursive_mutex _poolLock {};
#else
        /// @brief Mutex protecting access to the resource pool
        /// Uses a recursive mutex since debugging might use a recursive mutex
        /// @note Marked as mutable to allow usage within const methods
        mutable std::mutex _poolLock {};
#endif

        /// @brief This callback is invoked when a new resource is to be added to the pool.
        /// The client cannot add a resource to the pool and must instead craft a callback
        /// that the resource_pool will invoke when the pool needs a resource and is within
        /// the maximum capacity.
        std::function<RW && (resource_pool & pool)> _onNewResourceCallback {};
        /// @brief This callback is invoked when the resource is invalidated
        std::function<void(RW&&)> _onResourceInvalidatedCallback {};
        /// @brief This callback is invoked when the pool is about to shutdown
        std::function<void()> _onPoolShutdownCallback {};

    public:
        /// @brief Default constructor
        /// Creates an empty pool ready to accept resources
        resource_pool() = default;

        resource_pool(std::function<RW && (resource_pool & pool)>&& new_resource_callback)
            : _onNewResourceCallback(std::move(new_resource_callback))
        {
        }

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
        ~resource_pool()
        {
            if (_onPoolShutdownCallback) _onPoolShutdownCallback();
            clear();
        }

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
            std::scoped_lock l(_poolLock);
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
            std::scoped_lock l(_poolLock);
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
        [[nodiscard]] auto checkout() -> RW&& /* throw() */
        {
            auto _ = RunOnEnd {[this]() {
#if defined(DEBUG) && defined(NLOHMANN_JSON_VERSION_MAJOR)
                std::cerr << std::format("checkout - completed..{}\n", this->toJson().dump(2));
#endif
            }};

#if defined(DEBUG) && defined(NLOHMANN_JSON_VERSION_MAJOR)
            std::cerr << std::format("checkout - begin..{}\n", this->toJson().dump(2));
#endif

            try {
                // @note We use a unique_lock vs a scoped_lock to allow ourselves
                // to create the resource outside the lock!
                std::unique_lock l(_poolLock);

                if (!_pool.empty()) {
                    RunOnEnd roe([&]() {
                        _pool.pop_front();
#if defined(DEBUG) && defined(NLOHMANN_JSON_VERSION_MAJOR)
                        std::cerr << std::format("checkout - completed..from pool..{}\n", this->toJson().dump(2));
#endif
                    });

                    _resourcesCheckedout++;

#if defined(DEBUG) && defined(NLOHMANN_JSON_VERSION_MAJOR)
                    std::cerr << std::format("checkout - satisfy from pool.. {}\n", this->toJson().dump(2));
#endif

                    return wrapResource(std::move(_pool.front()));
                    // The pop_front() happens within this scope and
                    // within the lock!
                }
                else if (_capacity > _pool.size() + _resourcesCheckedout && _onNewResourceCallback) {
                    // We have no more items in the pool (we're starting up or everything is checked out)
                    // but we have not reached the limit.
                    // The limit is number of resourcesCheckedout + pool.size() < _capacity
                    // We are under-capacity.. so we can return to the caller an new item..
                    _resourcesCheckedout++;
                    // We should unlock the resource and ..
                    l.unlock();
#if defined(DEBUG) && defined(NLOHMANN_JSON_VERSION_MAJOR)
                    std::cerr << std::format("checkout - under-capacity asking provider! {}\n", this->toJson().dump(2));
#endif

                    // ..delegate the new resource acquisition
                    // outside the lock.
                    return wrapResource(std::move(_onNewResourceCallback(*this)));
                }
                else if (_capacity > _pool.size() + _resourcesCheckedout) {
                    // We're under-capacity.. but no dynamic resource provider
#if defined(DEBUG) && defined(NLOHMANN_JSON_VERSION_MAJOR)
                    std::cerr << std::format("checkout - under-capacity but no provider! {}\n", this->toJson().dump(2));
#endif
                }
            } // scope end
            catch (std::exception& ex) {
                std::cerr << ex.what();
            }

            auto msg = std::format("Pool Size:{}  checkedout:{}  capacity:{}", _pool.size(), _resourcesCheckedout, _capacity);
            throw std::runtime_error(msg);
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
        [[nodiscard]] auto wrapResource(T&& src) -> RW&&
        {
            // We return the resource back to the caller as a wrapper that has
            // the auto-checkin wired up to our pool.
            // For derived classes, we need to handle the case where the derived class
            // has a different constructor signature than the base class.
            // We construct the derived class first, then set the callback.
            return wrapResource(RW(std::move(src)));
        }


        [[nodiscard]] auto wrapResource(RW&& src) -> RW&&
        {
            /// @brief Lambda that returns the resource back to the pool
            /// Captures 'this' to access the pool's checkin method
            /// Called by resource_wrap destructor to ensure automatic return
            /// even if an exception occurs
            /// Set the callback and validity on the base class members
            /// resource_pool is a friend of resource_wrap, so we can access protected members
            src._putbackCallback = [this](T&& src) {
                this->checkin(std::move(src));
            };
            src._isValid = true;

            return std::move(src);
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
            std::unique_lock l(_poolLock);

            _pool.push_back(std::move(rsrc));
            if (_resourcesCheckedout > 0) _resourcesCheckedout--;

            /*
             * This is not valid for the current implementation
            if (rsrc._isValid) {
                _pool.push_back(std::move(rsrc));
                _resourcesCheckedout--;
            }
            else if (rsrc._isValid == false && _onResourceInvalidatedCallback) {
                _invalidatedResources++;
                l.unlock();
                // Delegate is called outside the lock
                _onResourceInvalidatedCallback(std::move(rsrc));
            }
             */
        }

#if defined(NLOHMANN_JSON_VERSION_MAJOR)
        /**
         * @brief Serialize pool state to JSON
         *
         * Returns a JSON object containing diagnostic information about the pool state.
         * Useful for monitoring and debugging.
         *
         * @return nlohmann::json object with pool statistics
         *
         * @note Thread-safe operation with acquire semantics
         */
        nlohmann::json toJson() const
        {
            std::scoped_lock l(_poolLock);

            return {{"_typver", "siddiqsoft.asynchrony-lib.resource_pool/0.10"},
                    {"capacity", _capacity},
                    {"poolSize", _pool.size()},
                    {"invalidatedResources", _invalidatedResources},
                    {"resourcesCheckedout", _resourcesCheckedout}};
        }
#endif
    };


#if defined(NLOHMANN_JSON_VERSION_MAJOR)
    /**
     * @brief JSON serialization adapter for resource_pool
     *
     * Enables automatic JSON serialization of resource_pool objects via nlohmann::json.
     *
     * @param dest Destination JSON object to populate
     * @param src Source resource_pool object to serialize
     */
    template <typename T, typename RW = resource_wrap<T>, uint8_t InitCapacity = resource_pool_limits::DefaultCapacity>
        requires((InitCapacity <= resource_pool_limits::MaxCapacity)) && NonNumericMoveConstructible<T> &&
                std::derived_from<RW, resource_wrap<T>>
    static void to_json(nlohmann::json& dest, const siddiqsoft::resource_pool<T, RW, InitCapacity>& src)
    {
        dest = src.toJson();
    }
#endif

} // namespace siddiqsoft
#endif
