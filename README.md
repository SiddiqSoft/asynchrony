![](https://gravatar.com/avatar/b22603b65d11dcab44885c65e44f7dc9)

asynchrony : Add asynchrony to your apps
-------------------------------------------
<!-- badges -->
[![Build Status](https://dev.azure.com/siddiqsoft/siddiqsoft/_apis/build/status/SiddiqSoft.asynchrony?branchName=main)](https://dev.azure.com/siddiqsoft/siddiqsoft/_build/latest?definitionId=17&branchName=main)
![](https://img.shields.io/nuget/v/SiddiqSoft.asynchrony)
![](https://img.shields.io/github/v/tag/SiddiqSoft/asynchrony)
![](https://img.shields.io/azure-devops/tests/siddiqsoft/siddiqsoft/17)
<!-- end badges -->

## Documentation

For comprehensive documentation, visit our **[GitHub Pages](https://siddiqsoft.github.io/asynchrony/)**:

- **[API Reference](https://siddiqsoft.github.io/asynchrony/doxygen/html/index.html)** - Complete Doxygen-generated API documentation
- **[Getting Started](https://siddiqsoft.github.io/asynchrony/doxygen/html/md_docs_pages_getting_started.html)** - Installation and setup guide
- **[Usage Guide](https://siddiqsoft.github.io/asynchrony/doxygen/html/md_docs_pages_usage_guide.html)** - Detailed usage examples and best practices
- **[Examples](https://siddiqsoft.github.io/asynchrony/doxygen/html/md_docs_pages_examples.html)** - Real-world code examples
- **[Quick Reference](https://siddiqsoft.github.io/asynchrony/doxygen/html/md_docs_pages_quick_reference.html)** - Quick lookup guide for common tasks

# Motivation
- We needed to add asynchrony to our code.
- The code here is a set of helpers that utilize the underlying deque, semaphore, mutex features found in std.
- Be instructive while providing functional code.
- Use only C++20 standard code: jthread, deque, semaphore, and concepts
- Depends on RunOnEnd for encapsulating cleanup code on destructor.

# Usage

> Requires C++20 support!
>
> Specifically we require `jthread` and `stop_token` support. This library works with GCC 10+, MSVC 16.11+, or Clang 10+.

The library uses concepts to ensure the type `T` meets move construct requirements.

## Single threaded worker

```cpp
#include "siddiqsoft/simple_worker.hpp"
#include <chrono>
#include <iostream>
#include <format>

// Define your data
struct MyWork
{
   std::string urlDestination{};
   std::string data{};
   void operator()(){
      // magic_post_to(urlDestination, data);
      std::cout << "Processing: " << urlDestination << std::endl;
   }
};

int main()
{
   // Declare worker with our data type and the driver function.
   siddiqsoft::simple_worker<MyWork> worker{[](auto& item){
                                              // call the item's operator()
                                              // to invoke actual work.
                                              item();
                                           }};
   // Fire 100 items
   for( int i=0; i < 100; i++ )
   {
      // Queues into the single worker
      worker.queue({std::format("https://localhost:443/test?iter={}",i),
                    "hello-world"});
   }

   // As the user, you must control the lifetime of the worker
   // Trying to delete the worker will cause it to stop
   // and abandon any items in the internal deque.
   std::this_thread::sleep_for(std::chrono::seconds(1));
   return 0;
}
```

## Multi-threaded worker pool

```cpp
#include "siddiqsoft/simple_pool.hpp"
#include <chrono>
#include <iostream>
#include <format>

int main()
{
   // Declare worker with our data type and the driver function.
   siddiqsoft::simple_pool<MyWork> worker{[](auto& item){
                                           // call the item's operator()
                                           // to invoke actual work.
                                           item();
                                        }};
   // Fire 100 items
   for( int i=0; i < 100; i++ )
   {
      // Queues into the single queue but multiple worker threads
      // (defaults to CPU thread count)
      worker.queue({std::format("https://localhost:443/test?iter={}",i),
                    "hello-world"});
   }

   // As the user, you must control the lifetime of the worker
   // Trying to delete the worker will cause it to stop
   // and abandon any items in the internal deque.
   std::this_thread::sleep_for(std::chrono::seconds(1));
   return 0;
}
```

## Multi-threaded roundrobin pool

```cpp
#include "siddiqsoft/roundrobin_pool.hpp"
#include <chrono>
#include <iostream>
#include <format>

int main()
{
   // Declare worker with our data type and the driver function.
   siddiqsoft::roundrobin_pool<MyWork> worker{[](auto& item){
                                               // call the item's operator()
                                               // to invoke actual work.
                                               item();
                                             }};
   // Fire 100 items
   for( int i=0; i < 100; i++ )
   {
      // Queues into the thread pools individual queue by round-robin
      // across the threads with simple counter.
      // (defaults to CPU thread count)
      worker.queue({std::format("https://localhost:443/test?iter={}",i),
                    "hello-world"});
   }

   // As the user, you must control the lifetime of the worker
   // Trying to delete the worker will cause it to stop
   // and abandon any items in the internal deque.
   std::this_thread::sleep_for(std::chrono::seconds(1));
   return 0;
}
```

## Periodic Worker

Execute a function at regular intervals:

```cpp
#include "siddiqsoft/periodic_worker.hpp"
#include <chrono>
#include <iostream>

int main()
{
   // Create a periodic worker that executes every 500ms
   siddiqsoft::periodic_worker<> timer{
      []() {
         std::cout << "Tick!" << std::endl;
      },
      std::chrono::milliseconds(500)
   };

   // Keep the program running
   std::this_thread::sleep_for(std::chrono::seconds(5));
   return 0;
}
```

## Resource Pool

Provides a basic resource pool useful for keeping a pool of connection objects for the various threadpools to checkout/checkin.

```cpp
namespace siddiqsoft {
    template<typename T>
    class resource_pool {
        public:
        auto size();                           // Get current pool size
        [[nodiscard]] T checkout();            // Checkout a resource (throws if empty)
        void checkin(T&& rsrc);                // Return a resource to the pool
        void clear();                          // Clear all resources from the pool
    };
}
```

## Implementation note
In order to use `std::jthread` on Clang 10 and later, we enable the compiler flag `"CMAKE_CXX_FLAGS": "-fexperimental-library"` in the CMakeLists.txt. This option will show up in your client library under Clang compilers.

---

**Author**: [Siddiq Software LLC](https://gravatar.com/siddiqsoft)

<p align="right">
&copy; 2021 Siddiq Software LLC. All rights reserved.
</p>
