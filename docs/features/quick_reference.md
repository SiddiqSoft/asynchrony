# Quick Reference Cheat Sheet

This page provides a quick lookup for common headers, template declarations, methods, compilation flags, and usage snippets.

---

## Headers & Includes

```cpp
#include "siddiqsoft/simple_worker.hpp"   // Single-threaded worker
#include "siddiqsoft/simple_pool.hpp"     // Multi-threaded shared-queue pool
#include "siddiqsoft/roundrobin_pool.hpp" // Multi-threaded per-thread queue pool
#include "siddiqsoft/periodic_worker.hpp" // Periodic timer executor
```

---

## Component Overview Table

| Class | Template Signature | Thread Count | Queue Structure | Use Case |
| :--- | :--- | :--- | :--- | :--- |
| `simple_worker` | `simple_worker<T, Priority = 0>` | 1 | Single `std::deque` | Sequential async background task |
| `simple_pool` | `simple_pool<T, Threads = 0>` | N (`hardware_concurrency`) | Single `std::deque` | Parallel execution with shared queue |
| `roundrobin_pool` | `roundrobin_pool<T, Threads = 0>` | N (`hardware_concurrency`) | Per-thread `std::deque` | Parallel execution with reduced lock contention |
| `periodic_worker` | `periodic_worker<Priority = 0>` | 1 | N/A (loop) | Recurring interval task |

---

## Common Operations

### Queueing Tasks
```cpp
// Rvalue move into worker/pool queue
worker.queue(std::move(task));
```

### Checking Queue Size
```cpp
size_t pending = worker.size();
```

### Counters & Metrics
```cpp
uint64_t totalAdded = worker.addCounter();
uint64_t totalProcessed = worker.removeCounter();
```

### Diagnostics JSON Output
```cpp
#include <nlohmann/json.hpp>

nlohmann::json info = worker.toJson();
std::cout << info.dump(2) << std::endl;
```

---

## Compiler Flags & Configuration

=== "CMake (Recommended)"

    ```cmake
    include(FetchContent)
    FetchContent_Declare(asynchrony
        GIT_REPOSITORY https://github.com/SiddiqSoft/asynchrony.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(asynchrony)

    target_link_libraries(your_target PRIVATE asynchrony::asynchrony)
    ```

=== "GCC 10+"

    ```bash
    g++ -std=c++23 -pthread main.cpp -o main
    ```

=== "Clang 10+"

    ```bash
    clang++ -std=c++23 -fexperimental-library -pthread main.cpp -o main
    ```

=== "MSVC (Visual Studio 2019+)"

    ```cmd
    cl /std:c++23 main.cpp
    ```
