/**
 * @page quick_reference Quick Reference
 *
 * @section qr_components Components at a Glance
 *
 * | Component | Use Case | Threads | Queue Type |
 * |-----------|----------|---------|-----------|
 * | `simple_worker` | Single async task | 1 | Shared |
 * | `simple_pool` | Parallel processing | N | Shared |
 * | `roundrobin_pool` | Load balancing | N | Per-thread |
 * | `periodic_worker` | Scheduled tasks | 1 | N/A |
 * | `resource_pool` | Resource management | N/A | N/A |
 *
 * @section qr_includes Include Files
 *
 * ```cpp
 * #include "siddiqsoft/simple_worker.hpp"      // Single-threaded worker
 * #include "siddiqsoft/simple_pool.hpp"        // Multi-threaded pool
 * #include "siddiqsoft/roundrobin_pool.hpp"    // Round-robin pool
 * #include "siddiqsoft/periodic_worker.hpp"    // Periodic executor
 * #include "siddiqsoft/resource_pool.hpp"      // Resource pool
 * ```
 *
 * @section qr_basic_patterns Basic Patterns
 *
 * @subsection qr_pattern_worker Single Worker
 *
 * ```cpp
 * siddiqsoft::simple_worker<Task> worker{
 *     [](auto&& task) { task.execute(); }
 * };
 * worker.queue(std::move(task));
 * ```
 *
 * @subsection qr_pattern_pool Thread Pool
 *
 * ```cpp
 * siddiqsoft::simple_pool<Task> pool{
 *     [](auto&& task) { task.execute(); }
 * };
 * for (auto& t : tasks) pool.queue(std::move(t));
 * ```
 *
 * @subsection qr_pattern_roundrobin Round-Robin Pool
 *
 * ```cpp
 * siddiqsoft::roundrobin_pool<Task> pool{
 *     [](auto&& task) { task.execute(); }
 * };
 * for (auto& t : tasks) pool.queue(std::move(t));
 * ```
 *
 * @subsection qr_pattern_periodic Periodic Task
 *
 * ```cpp
 * siddiqsoft::periodic_worker<> timer{
 *     []() { /* do something */ },
 *     std::chrono::milliseconds(1000)
 * };
 * ```
 *
 * @section qr_template_params Template Parameters
 *
 * ### simple_worker<T, Pri>
 * - `T`: Data type (must be move-constructible)
 * - `Pri`: Thread priority (-10 to +10, default 0)
 *
 * ### simple_pool<T, N>
 * - `T`: Data type (must be move-constructible)
 * - `N`: Number of threads (0 = hardware_concurrency)
 *
 * ### roundrobin_pool<T, N>
 * - `T`: Data type (must be move-constructible)
 * - `N`: Number of threads (0 = hardware_concurrency)
 *
 * ### periodic_worker<Pri>
 * - `Pri`: Thread priority (-10 to +10, default 0)
 *
 * ### resource_pool<T>
 * - `T`: Resource type
 *
 * @section qr_methods Common Methods
 *
 * ### queue(T&& item)
 * Queue an item for processing
 * ```cpp
 * worker.queue(std::move(item));
 * ```
 *
 * ### toJson()
 * Get JSON representation (requires nlohmann/json)
 * ```cpp
 * auto json = worker.toJson();
 * ```
 *
 * ### checkout() / checkin()
 * Resource pool operations
 * ```cpp
 * auto resource = pool.checkout();
 * // use resource
 * pool.checkin(std::move(resource));
 * ```
 *
 * @section qr_requirements Requirements
 *
 * - **C++23** or later
 * - **Compiler**: GCC 14+, MSVC 17+, Clang 18+
 * - **Platform**: Windows, Linux, macOS
 * - **Dependencies**: None (header-only for core functionality)
 *
 * @section qr_compilation Compilation
 *
 * **GCC/Clang:**
 * ```bash
 * g++ -std=c++23 -pthread your_file.cpp
 * ```
 *
 * **MSVC:**
 * ```bash
 * cl /std:c++latest your_file.cpp
 * ```
 *
 * **CMake:**
 * ```cmake
 * target_link_libraries(your_target PRIVATE asynchrony::asynchrony)
 * ```
 *
 * @section qr_tips Tips & Tricks
 *
 * 1. **Keep callbacks lightweight** - Avoid blocking operations
 * 2. **Use move semantics** - Always move items into the queue
 * 3. **Handle exceptions** - Catch exceptions in callbacks
 * 4. **Monitor queue depth** - Check for bottlenecks
 * 5. **Choose right pool type** - Use roundrobin for variable-duration tasks
 * 6. **Lifetime management** - Keep worker/pool alive while queuing
 * 7. **Thread priority** - Use carefully, may affect system performance
 *
 * @section qr_troubleshooting Common Issues
 *
 * | Issue | Solution |
 * |-------|----------|
 * | Compilation error: `jthread not found` | Use C++23 or later |
 * | Tasks not executing | Ensure worker/pool is not destroyed |
 * | High CPU usage | Increase wait timeout or reduce threads |
 * | Deadlock | Avoid circular dependencies in callbacks |
 * | Memory leak | Ensure proper RAII cleanup |
 *
 * @section qr_performance Performance Tips
 *
 * - **CPU-bound tasks**: Use threads = hardware_concurrency()
 * - **I/O-bound tasks**: Use threads > hardware_concurrency()
 * - **Batch small tasks**: Reduce overhead
 * - **Use roundrobin**: For variable-duration tasks
 * - **Monitor metrics**: Use toJson() to track queue depth
 *
 * @section qr_examples Quick Examples
 *
 * ### Example 1: Simple Async Task
 * ```cpp
 * siddiqsoft::simple_worker<std::string> worker{
 *     [](auto&& msg) { std::cout << msg << std::endl; }
 * };
 * worker.queue("Hello, World!");
 * std::this_thread::sleep_for(std::chrono::milliseconds(100));
 * ```
 *
 * ### Example 2: Parallel Processing
 * ```cpp
 * siddiqsoft::simple_pool<int> pool{
 *     [](auto&& num) { std::cout << num * 2 << std::endl; }
 * };
 * for (int i = 0; i < 100; ++i) pool.queue(i);
 * std::this_thread::sleep_for(std::chrono::seconds(1));
 * ```
 *
 * ### Example 3: Periodic Monitoring
 * ```cpp
 * siddiqsoft::periodic_worker<> monitor{
 *     []() { std::cout << "Tick!" << std::endl; },
 *     std::chrono::seconds(1)
 * };
 * std::this_thread::sleep_for(std::chrono::seconds(10));
 * ```
 */
