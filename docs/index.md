asynchrony : Asynchrony support library
---------------------------------------

<img align="right" src="https://gravatar.com/avatar/b22603b65d11dcab44885c65e44f7dc9">

[![CodeQL](https://github.com/SiddiqSoft/asynchrony/actions/workflows/github-code-scanning/codeql/badge.svg)](https://github.com/SiddiqSoft/asynchrony/actions/workflows/github-code-scanning/codeql)
[![Build Status](https://dev.azure.com/siddiqsoft/siddiqsoft/_apis/build/status/SiddiqSoft.asynchrony?branchName=main)](https://dev.azure.com/siddiqsoft/siddiqsoft/_build/latest?definitionId=17&branchName=main)
![](https://img.shields.io/nuget/v/SiddiqSoft.asynchrony)
![](https://img.shields.io/github/v/tag/SiddiqSoft/asynchrony)
![](https://img.shields.io/azure-devops/tests/siddiqsoft/siddiqsoft/17)
<!-- end badges -->

## Documentation

- **[API Reference](./doxygen/html/index.html)** - Complete Doxygen-generated API documentation
- **[Getting Started](./doxygen/html/md_docs_pages_getting_started.html)** - Installation and setup guide
- **[Usage Guide](./doxygen/html/md_docs_pages_usage_guide.html)** - Detailed usage examples
- **[Examples](./doxygen/html/md_docs_pages_examples.html)** - Real-world code examples
- **[Quick Reference](./doxygen/html/md_docs_pages_quick_reference.html)** - Quick lookup guide

## Getting started

- This library uses standard C++20 code with support for Windows, Linux, and macOS.
  - We use [`<concepts>`](https://en.cppreference.com/w/cpp/concepts), [`<jthread>`](https://en.cppreference.com/w/cpp/thread/jthread), [`<semaphore>`](https://en.cppreference.com/w/cpp/thread/counting_semaphore), and [`<format>`](https://en.cppreference.com/w/cpp/header/format).
  - The most compatible compilers are GCC 10+, MSVC 16.11+, and Clang 10+.
- On Windows, you can use the Nuget package or CMakeLists.

## Quick Start

### Single-threaded Worker

```cpp
#include "siddiqsoft/simple_worker.hpp"

struct MyWork {
   std::string urlDestination{};
   std::string data{};
   void operator()(){
      // Process work
   }
};

int main() {
   siddiqsoft::simple_worker<MyWork> worker{[](auto& item){
      item();
   }};
   
   for(int i=0; i < 100; i++) {
      worker.queue({std::format("https://localhost:443/test?iter={}",i),
                    "hello-world"});
   }

   std::this_thread::sleep_for(std::chrono::seconds(1));
   return 0;
}
```

### Multi-threaded Pool

```cpp
#include "siddiqsoft/simple_pool.hpp"

int main() {
   siddiqsoft::simple_pool<MyWork> pool{[](auto& item){
      item();
   }};
   
   for(int i=0; i < 100; i++) {
      pool.queue({std::format("https://localhost:443/test?iter={}",i),
                  "hello-world"});
   }

   std::this_thread::sleep_for(std::chrono::seconds(1));
   return 0;
}
```

### Round-Robin Pool

```cpp
#include "siddiqsoft/roundrobin_pool.hpp"

int main() {
   siddiqsoft::roundrobin_pool<MyWork> pool{[](auto& item){
      item();
   }};
   
   for(int i=0; i < 100; i++) {
      pool.queue({std::format("https://localhost:443/test?iter={}",i),
                  "hello-world"});
   }

   std::this_thread::sleep_for(std::chrono::seconds(1));
   return 0;
}
```

### Periodic Worker

```cpp
#include "siddiqsoft/periodic_worker.hpp"

int main() {
   siddiqsoft::periodic_worker<> timer{
      []() { std::cout << "Tick!" << std::endl; },
      std::chrono::milliseconds(1000)
   };

   std::this_thread::sleep_for(std::chrono::seconds(5));
   return 0;
}
```

## API

Utility                   | Description
-------------------------:|:------------
[`siddiqsoft::simple_worker`](./doxygen/html/structsiddiqsoft_1_1simple__worker.html) | Provides a single thread with an internal deque.<br/>Use this to make any "long" task asynchronous.<br/>Use instead of `std::async`.<br/>Just register your callback/lambda and you're done. No need to worry about waiting for the result (no futures or waiting on them).<br/>Your declared callback will be invoked!
[`siddiqsoft::simple_pool`](./doxygen/html/structsiddiqsoft_1_1simple__pool.html) | Implements an array of threads backed with a *single* deque. Each thread waits for and processes the next available item from the single deque.
[`siddiqsoft::roundrobin_pool`](./doxygen/html/structsiddiqsoft_1_1roundrobin__pool.html) | Implements a vector of basic_workers (each worker has its independent queue therefore minimizing contention time).<br/>The queue method implements a running counter based round-robin feeder.
[`siddiqsoft::periodic_worker`](./doxygen/html/structsiddiqsoft_1_1periodic__worker.html) | Provides a facility where you can have your function/lambda invoked at a given periodic rate (in microseconds).
[`siddiqsoft::resource_pool`](./doxygen/html/structsiddiqsoft_1_1resource__pool.html) | Provides a basic resource pool useful for keeping a pool of connection objects for the various threadpools to checkout/checkin.

## Implementation note

In order to use `std::jthread` on Clang 10 and later, we enable the compiler flag `"CMAKE_CXX_FLAGS": "-fexperimental-library"` in the CMakeLists.txt. This option will show up in your client library under Clang compilers.

---

**Author**: [Siddiq Software LLC](https://gravatar.com/siddiqsoft)

<p align="right">
&copy; 2021 Siddiq Software LLC. All rights reserved.
</p>
