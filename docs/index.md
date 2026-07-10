# Asynchrony: Add Asynchrony to Your C++ Applications

[![Build Status](https://dev.azure.com/siddiqsoft/siddiqsoft/_apis/build/status/SiddiqSoft.asynchrony?branchName=main)](https://dev.azure.com/siddiqsoft/siddiqsoft/_build/latest?definitionId=17&branchName=main)
![](https://img.shields.io/nuget/v/SiddiqSoft.asynchrony)
![](https://img.shields.io/github/v/tag/SiddiqSoft/asynchrony)
![](https://img.shields.io/azure-devops/tests/siddiqsoft/siddiqsoft/17)

## Overview

The **asynchrony** library provides a comprehensive set of modern C++20 utilities for building asynchronous and multi-threaded applications. It leverages standard library features like `std::jthread`, `std::semaphore`, `std::deque`, and `std::concepts` to provide clean, efficient abstractions for common asynchronous patterns.

## Key Features

- **Single-threaded Worker**: Process items asynchronously in a dedicated thread
- **Thread Pool**: Distribute work across multiple threads with a shared queue
- **Round-Robin Pool**: Minimize contention with per-thread queues
- **Periodic Worker**: Execute functions at regular intervals
- **Resource Pool**: Manage a pool of reusable resources
- **Modern C++20**: Uses only standard library features (no external dependencies for core functionality)
- **Type-Safe**: Leverages C++ concepts for compile-time type checking
- **Exception Safe**: Handles exceptions gracefully without thread termination

## Requirements

- **C++20 Support**: Requires `std::jthread` and `std::stop_token`
- **Compiler Support**:
  - GCC 10+
  - MSVC 16.11+ (Visual Studio 2019 or later)
  - Clang 10+ (with `-fexperimental-library` flag)
- **Platform Support**: Windows, Linux, macOS

## Classes and Methods

### simple_worker

A single-threaded asynchronous processor that queues work items and processes them sequentially.

```cpp
template<typename T>
class simple_worker {
    void queue(T&& item);           // Queue an item for processing
    size_t size() const;            // Get queue size
    uint64_t addCounter() const;    // Get total items added
    uint64_t removeCounter() const; // Get total items processed
};
```

### simple_pool

A multi-threaded pool with a shared queue that distributes work across multiple threads.

```cpp
template<typename T>
class simple_pool {
    void queue(T&& item);           // Queue an item for processing
    size_t size() const;            // Get queue size
    uint64_t addCounter() const;    // Get total items added
    uint64_t removeCounter() const; // Get total items processed
};
```

### roundrobin_pool

A multi-threaded pool with per-thread queues that minimizes contention through round-robin distribution.

```cpp
template<typename T>
class roundrobin_pool {
    void queue(T&& item);           // Queue an item (round-robin distribution)
    size_t size() const;            // Get total queue size
    uint64_t addCounter() const;    // Get total items added
    uint64_t removeCounter() const; // Get total items processed
};
```

### periodic_worker

Executes a function at regular intervals in a dedicated thread.

```cpp
template<typename Rep = std::milli, typename Period = std::ratio<1>>
class periodic_worker {
    periodic_worker(std::function<void()> fn, std::chrono::duration<Rep, Period> interval);
};
```

### resource_pool

Manages a pool of reusable resources for checkout/checkin operations.

```cpp
template<typename T>
class resource_pool {
    size_t size() const;            // Get current pool size
    T checkout();                   // Checkout a resource (throws if empty)
    void checkin(T&& rsrc);         // Return a resource to the pool
    void clear();                   // Clear all resources from the pool
};
```

## Quick Start Examples

### Simple Worker Example

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

### Thread Pool Example

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

### Periodic Worker Example

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

## Design Principles

- **Move Semantics**: All components use move semantics for efficient resource transfer
- **RAII**: Proper resource management through constructors and destructors
- **Exception Safety**: Exceptions in callbacks are caught and logged, not propagated
- **Thread Safety**: Internal synchronization using mutexes and semaphores
- **Zero-Copy**: Minimal data copying through perfect forwarding

## Documentation

For comprehensive documentation, see:

- **[API Reference](./doxygen/html/index.html)** - Complete Doxygen-generated API documentation
- **[Getting Started](./doxygen/html/md_docs_pages_getting_started.html)** - Installation and setup guide
- **[Usage Guide](./doxygen/html/md_docs_pages_usage_guide.html)** - Detailed usage examples and best practices
- **[Examples](./doxygen/html/md_docs_pages_examples.html)** - Real-world code examples
- **[Quick Reference](./doxygen/html/md_docs_pages_quick_reference.html)** - Quick lookup guide for common tasks

## Usage

> Requires C++20 support!
>
> Specifically we require `jthread` and `stop_token` support. This library works with GCC 10+, MSVC 16.11+, or Clang 10+.

The library uses concepts to ensure the type `T` meets move construct requirements.

## Installation

Use the NuGet package [SiddiqSoft.asynchrony](https://www.nuget.org/packages/SiddiqSoft.asynchrony/) or integrate via CMake/CPM.

## Implementation Notes

In order to use `std::jthread` on Clang 10 and later, we enable the compiler flag `"CMAKE_CXX_FLAGS": "-fexperimental-library"` in the CMakeLists.txt. This option will show up in your client library under Clang compilers.

## License

BSD 3-Clause License - See LICENSE file for details

## Copyright

Copyright (c) 2021, Siddiq Software LLC. All rights reserved.

---

**Author**: [Siddiq Software LLC](https://gravatar.com/siddiqsoft)
