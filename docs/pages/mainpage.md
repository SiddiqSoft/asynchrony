/**
 * @mainpage asynchrony - Add Asynchrony to Your C++ Applications
 *
 * @section intro Introduction
 *
 * The **asynchrony** library provides a set of modern C++23 utilities for building asynchronous and multi-threaded applications.
 * It leverages standard library features like `std::jthread`, `std::semaphore`, `std::deque`, and `std::barrier` to provide
 * clean, efficient abstractions for common asynchronous patterns.
 *
 * @section features Key Features
 *
 * - **Single-threaded Worker**: Process items asynchronously in a dedicated thread
 * - **Thread Pool**: Distribute work across multiple threads with a shared queue
 * - **Round-Robin Pool**: Minimize contention with per-thread queues
 * - **Periodic Worker**: Execute functions at regular intervals
 * - **Resource Pool**: Manage a pool of reusable resources
 * - **Modern C++23**: Uses only standard library features (no external dependencies for core functionality)
 * - **Type-Safe**: Leverages C++ concepts for compile-time type checking
 * - **Exception Safe**: Handles exceptions gracefully without thread termination
 *
 * @section requirements Requirements
 *
 * - **C++23 Support**: Requires `std::jthread` and `std::stop_token`
 * - **Compiler Support**:
 *   - GCC 14+
 *   - MSVC 17+ (Visual Studio 2022)
 *   - Clang 18+ (with `-fexperimental-library` flag)
 * - **Platform Support**: Windows, Linux, macOS
 *
 * @section components Main Components
 *
 * | Component | Description |
 * |-----------|-------------|
 * | @ref siddiqsoft::simple_worker | Single-threaded asynchronous processor |
 * | @ref siddiqsoft::simple_pool | Multi-threaded pool with shared queue |
 * | @ref siddiqsoft::roundrobin_pool | Multi-threaded pool with per-thread queues |
 * | @ref siddiqsoft::periodic_worker | Periodic task executor |
 * | @ref siddiqsoft::resource_pool | Resource pool manager |
 *
 * @section quickstart Quick Start
 *
 * @subsection simple_worker_example Simple Worker Example
 *
 * ```cpp
 * #include "siddiqsoft/simple_worker.hpp"
 *
 * struct MyTask {
 *     std::string data;
 *     void operator()() { /* process data */ }
 * };
 *
 * int main() {
 *     siddiqsoft::simple_worker<MyTask> worker{[](auto& task) {
 *         task();  // Execute the task
 *     }};
 *
 *     // Queue work
 *     for (int i = 0; i < 100; ++i) {
 *         worker.queue(MyTask{"data-" + std::to_string(i)});
 *     }
 *
 *     std::this_thread::sleep_for(std::chrono::seconds(1));
 *     return 0;
 * }
 * ```
 *
 * @subsection pool_example Thread Pool Example
 *
 * ```cpp
 * #include "siddiqsoft/simple_pool.hpp"
 *
 * int main() {
 *     siddiqsoft::simple_pool<MyTask> pool{[](auto& task) {
 *         task();  // Execute the task
 *     }};
 *
 *     // Queue work across multiple threads
 *     for (int i = 0; i < 1000; ++i) {
 *         pool.queue(MyTask{"data-" + std::to_string(i)});
 *     }
 *
 *     std::this_thread::sleep_for(std::chrono::seconds(2));
 *     return 0;
 * }
 * ```
 *
 * @section design Design Principles
 *
 * - **Move Semantics**: All components use move semantics for efficient resource transfer
 * - **RAII**: Proper resource management through constructors and destructors
 * - **Exception Safety**: Exceptions in callbacks are caught and logged, not propagated
 * - **Thread Safety**: Internal synchronization using mutexes and semaphores
 * - **Zero-Copy**: Minimal data copying through perfect forwarding
 *
 * @section license License
 *
 * BSD 3-Clause License - See LICENSE file for details
 *
 * @section copyright Copyright
 *
 * Copyright (c) 2021, Siddiq Software LLC. All rights reserved.
 *
 * @see https://github.com/SiddiqSoft/asynchrony
 */
