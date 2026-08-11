@mainpage Asynchrony - Add Asynchrony to Your C++ Applications

@section intro Introduction

The **asynchrony** library provides a comprehensive set of modern C++23 utilities for building asynchronous and multi-threaded applications. It leverages standard library features like `std::jthread`, `std::semaphore`, `std::deque`, and `std::concepts` to provide clean, efficient abstractions for common asynchronous patterns.

This header-only library eliminates boilerplate synchronization code and provides a simple, type-safe API for concurrent programming scenarios.

@section features Key Features

- **Single-threaded Worker**: Process items asynchronously in a dedicated thread
- **Thread Pool**: Distribute work across multiple threads with a shared queue
- **Round-Robin Pool**: Minimize contention with per-thread queues
- **Periodic Worker**: Execute functions at regular intervals
- **Modern C++23**: Uses only standard library features (no external dependencies for core functionality)
- **Type-Safe**: Leverages C++ concepts for compile-time type checking
- **Exception Safe**: Handles exceptions gracefully without thread termination
- **Move Semantics**: Efficient resource transfer with perfect forwarding
- **RAII**: Proper resource management through constructors and destructors

@section requirements Requirements

- **C++23 Support**: Requires `std::jthread` and `std::stop_token`
- **Compiler Support**:
  - GCC 10+
  - MSVC 16.11+ (Visual Studio 2019 or later)
  - Clang 10+ (with `-fexperimental-library` flag)
- **Platform Support**: Windows, Linux, macOS
- **Optional**: nlohmann/json for JSON serialization support

@section components Main Components

| Component | Description | Use Case |
|-----------|-------------|----------|
| @ref siddiqsoft::simple_worker | Single-threaded asynchronous processor | Sequential async processing |
| @ref siddiqsoft::simple_pool | Multi-threaded pool with shared queue | Parallel processing with load balancing |
| @ref siddiqsoft::roundrobin_pool | Multi-threaded pool with per-thread queues | Parallel processing with reduced contention |
| @ref siddiqsoft::periodic_worker | Periodic task executor | Scheduled/recurring tasks |


@section design Design Principles

- **Move Semantics**: All components use move semantics for efficient resource transfer
- **RAII**: Proper resource management through constructors and destructors
- **Exception Safety**: Exceptions in callbacks are caught and logged, not propagated
- **Thread Safety**: Internal synchronization using mutexes and semaphores
- **Zero-Copy**: Minimal data copying through perfect forwarding
- **Type Safety**: C++23 concepts ensure compile-time type checking
- **Simplicity**: Clean API that hides complexity of thread management

@section documentation Documentation

- @ref getting_started - Installation and setup guide
- @ref usage_guide - Detailed usage examples and best practices
- @ref examples - Real-world code examples
- @ref quick_reference - Quick lookup guide for common tasks
- @ref api - Complete API reference

@section quickstart Quick Start

@subsection simple_worker_example Simple Worker Example

```cpp
#include "siddiqsoft/simple_worker.hpp"
#include <iostream>

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

    // Queue work
    for (int i = 0; i < 100; ++i) {
        worker.queue(MyTask{"data-" + std::to_string(i)});
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
```

@subsection pool_example Thread Pool Example

```cpp
#include "siddiqsoft/simple_pool.hpp"

int main() {
    siddiqsoft::simple_pool<MyTask> pool{[](auto& task) {
        task();  // Execute the task
    }};

    // Queue work across multiple threads
    for (int i = 0; i < 1000; ++i) {
        pool.queue(MyTask{"data-" + std::to_string(i)});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}
```

@subsection roundrobin_example Round-Robin Pool Example

```cpp
#include "siddiqsoft/roundrobin_pool.hpp"

int main() {
    siddiqsoft::roundrobin_pool<MyTask> pool{[](auto& task) {
        task();  // Execute the task
    }};

    // Queue work with round-robin distribution
    for (int i = 0; i < 1000; ++i) {
        pool.queue(MyTask{"data-" + std::to_string(i)});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}
```

@subsection periodic_example Periodic Worker Example

```cpp
#include "siddiqsoft/periodic_worker.hpp"
#include <iostream>

int main() {
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

@section installation Installation

### Using CMake (Recommended)

```cmake
include(FetchContent)
FetchContent_Declare(asynchrony
    GIT_REPOSITORY https://github.com/SiddiqSoft/asynchrony.git
    GIT_TAG main
)
FetchContent_MakeAvailable(asynchrony)

target_link_libraries(your_target PRIVATE asynchrony::asynchrony)
```

### Using NuGet (Windows)

```bash
nuget install SiddiqSoft.asynchrony
```

### Manual Integration

Simply include the header files from `include/siddiqsoft/` in your project.

@section license License

BSD 3-Clause License - See LICENSE file for details

@section copyright Copyright

Copyright (c) 2021, Siddiq Software LLC. All rights reserved.

@section links Links

- **GitHub**: https://github.com/SiddiqSoft/asynchrony
- **NuGet**: https://www.nuget.org/packages/SiddiqSoft.asynchrony/
- **Documentation**: https://siddiqsoft.github.io/asynchrony/

@section see_also See Also

- @ref getting_started
- @ref usage_guide
- @ref examples
- @ref quick_reference
- @ref api
