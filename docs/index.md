# Asynchrony

<div class="badge-container">
  <a href="https://dev.azure.com/siddiqsoft/siddiqsoft/_build/latest?definitionId=17&branchName=main"><img src="https://dev.azure.com/siddiqsoft/siddiqsoft/_apis/build/status/SiddiqSoft.asynchrony?branchName=main" alt="Build Status"></a>
  <img src="https://img.shields.io/nuget/v/SiddiqSoft.asynchrony" alt="NuGet Version">
  <img src="https://img.shields.io/github/v/tag/SiddiqSoft/asynchrony" alt="GitHub Tag">
  <img src="https://img.shields.io/azure-devops/tests/siddiqsoft/siddiqsoft/17" alt="Azure DevOps Tests">
</div>

The **asynchrony** library provides a comprehensive set of modern **C++23** header-only utilities for building asynchronous and multi-threaded applications. It leverages standard C++ library features like `std::jthread`, `std::semaphore`, `std::deque`, and C++ concepts to provide clean, efficient, and type-safe abstractions for common concurrency patterns.

---

## Key Features & Components

<div class="card-grid">
  <div class="card">
    <h3>Single-threaded Worker</h3>
    <p>Process items asynchronously in a dedicated worker thread with low overhead and full move semantics.</p>
    <a href="features/simple_worker/">Read Guide &rarr;</a>
  </div>
  <div class="card">
    <h3>Shared Thread Pool</h3>
    <p>Distribute work across a pool of threads consuming from a single shared queue with automatic load balancing.</p>
    <a href="features/simple_pool/">Read Guide &rarr;</a>
  </div>
  <div class="card">
    <h3>Round-Robin Pool</h3>
    <p>Minimize lock contention in high-throughput applications using per-thread queues and round-robin dispatch.</p>
    <a href="features/roundrobin_pool/">Read Guide &rarr;</a>
  </div>
  <div class="card">
    <h3>Periodic Worker</h3>
    <p>Execute recurring background tasks or timers at regular intervals cleanly using RAII lifecycle management.</p>
    <a href="features/periodic_worker/">Read Guide &rarr;</a>
  </div>
</div>

---

## Quick Start Examples

=== "Simple Worker"

    ```cpp
    #include "siddiqsoft/simple_worker.hpp"
    #include <iostream>
    #include <chrono>

    struct MyTask {
        std::string data;
        void operator()() { 
            std::cout << "Processing: " << data << std::endl;
        }
    };

    int main() {
        siddiqsoft::simple_worker<MyTask> worker{[](auto& task) {
            task();  // Execute the task
        }};

        // Queue work items
        for (int i = 0; i < 100; ++i) {
            worker.queue(MyTask{"data-" + std::to_string(i)});
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        return 0;
    }
    ```

=== "Thread Pool"

    ```cpp
    #include "siddiqsoft/simple_pool.hpp"
    #include <iostream>
    #include <chrono>

    struct MyTask {
        std::string data;
        void operator()() { 
            std::cout << "Thread " << std::this_thread::get_id()
                      << " processing " << data << std::endl;
        }
    };

    int main() {
        // Pool of worker threads sharing a single queue
        siddiqsoft::simple_pool<MyTask> pool{[](auto& task) {
            task();  // Execute task
        }};

        for (int i = 0; i < 1000; ++i) {
            pool.queue(MyTask{"item-" + std::to_string(i)});
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 0;
    }
    ```

=== "Round-Robin Pool"

    ```cpp
    #include "siddiqsoft/roundrobin_pool.hpp"
    #include <iostream>
    #include <chrono>

    struct MyTask {
        std::string data;
        void operator()() { 
            std::cout << "Round-robin item: " << data << std::endl;
        }
    };

    int main() {
        // Multi-threaded pool with per-thread queues
        siddiqsoft::roundrobin_pool<MyTask> pool{[](auto& task) {
            task();
        }};

        for (int i = 0; i < 1000; ++i) {
            pool.queue(MyTask{"item-" + std::to_string(i)});
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 0;
    }
    ```

=== "Periodic Worker"

    ```cpp
    #include "siddiqsoft/periodic_worker.hpp"
    #include <iostream>
    #include <chrono>

    int main() {
        // Execute callback every 500ms
        siddiqsoft::periodic_worker<> timer{
            []() {
                std::cout << "Tick!" << std::endl;
            },
            std::chrono::milliseconds(500)
        };

        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 0;
    }
    ```

---

## Requirements & Compatibility

| Requirement | Details |
| :--- | :--- |
| **Language Standard** | C++23 (requires `std::jthread` and `std::stop_token`) |
| **GCC** | GCC 10+ (`-std=c++23 -pthread`) |
| **MSVC** | MSVC 16.11+ / Visual Studio 2019+ (`/std:c++23`) |
| **Clang** | Clang 10+ (`-std=c++23 -fexperimental-library -pthread`) |
| **Platforms** | Windows, Linux, macOS |
| **Dependencies** | Core library is header-only with zero required external dependencies. Optional: `nlohmann/json` for JSON serialization. |

---

## Design Goals

- **Zero External Core Dependencies**: Built entirely on C++23 standard library primitives.
- **Move Semantics & Zero Copy**: Full support for move-only types and perfect forwarding.
- **RAII Lifecycle**: Thread lifetimes and resource cleanups are managed via deterministic destructors.
- **Exception Safety**: Callbacks handle exceptions without aborting worker threads.
- **Type Safety**: Concepts ensure compile-time verification of task types.

---

## Documentation Quick Links

- [Integration & Installation Guide](integration/index.md)
- [Features Overview](features/index.md)
- [Quick Reference](features/quick_reference.md)
- [API Reference](api/index.md)
- [Security Guide](features/security.md)
- [License](license.md)
