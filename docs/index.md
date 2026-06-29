asynchrony : Asynchrony support library
---------------------------------------

<!-- badges -->
[![CodeQL](https://github.com/SiddiqSoft/asynchrony/actions/workflows/github-code-scanning/codeql/badge.svg)](https://github.com/SiddiqSoft/asynchrony/actions/workflows/github-code-scanning/codeql)
[![Build Status](https://dev.azure.com/siddiqsoft/siddiqsoft/_apis/build/status/SiddiqSoft.asynchrony?branchName=main)](https://dev.azure.com/siddiqsoft/siddiqsoft/_build/latest?definitionId=17&branchName=main)
![](https://img.shields.io/nuget/v/SiddiqSoft.asynchrony)
![](https://img.shields.io/github/v/tag/SiddiqSoft/asynchrony)
![](https://img.shields.io/azure-devops/tests/siddiqsoft/siddiqsoft/17)
<!--![](https://img.shields.io/azure-devops/coverage/siddiqsoft/siddiqsoft/17)-->
<!-- end badges -->

## Documentation

- **[API Reference](./doxygen/html/index.html)** - Complete Doxygen-generated API documentation
- **[Getting Started](./doxygen/html/md_docs_pages_getting_started.html)** - Installation and setup guide
- **[Usage Guide](./doxygen/html/md_docs_pages_usage_guide.html)** - Detailed usage examples
- **[Examples](./doxygen/html/md_docs_pages_examples.html)** - Real-world code examples

## Getting started

- This library uses standard C++23 code with support for Windows, Linux, and macOS.
  - We use [`<concepts>`](https://en.cppreference.com/w/cpp/concepts), [`<jthread>`](https://en.cppreference.com/w/cpp/thread/jthread), [`<semaphore>`](https://en.cppreference.com/w/cpp/thread/counting_semaphore) and [`<barrier>`](https://en.cppreference.com/w/cpp/thread/barrier) and for tests we use [`<format>`](https://en.cppreference.com/w/cpp/header/format).
  - Given the [status of C++23](https://github.com/microsoft/STL/wiki/Changelog), the most conformant compilers are GCC 14+, MSVC 17+, and Clang 18+.
- On Windows with VisualStudio, use the Nuget package! 
- Make sure you use `c++latest` or `-std=c++23` as the language standard.

> **NOTE**
> We are tracking the latest C++23 standard and the API is subject to change.

## Quick Start

### Single-threaded Worker

```cpp
#include "siddiqsoft/simple_worker.hpp"

struct MyWork {
   std::string urlDestination{};
   std::string data{};
   void operator()(){
      magic_post_to(urlDestination, data);
   }
};

void main() {
   siddiqsoft::simple_worker<MyWork> worker{[](auto& item){
      item();
   }};
   
   for(int i=0; i < 100; i++) {
      worker.queue({std::format("https://localhost:443/test?iter={}",i),
                    "hello-world"});
   }

   std::this_thread::sleep_for(1s);
}
```

### Multi-threaded Pool

```cpp
#include "siddiqsoft/simple_pool.hpp"

void main() {
   siddiqsoft::simple_pool<MyWork> pool{[](auto& item){
      item();
   }};
   
   for(int i=0; i < 100; i++) {
      pool.queue({std::format("https://localhost:443/test?iter={}",i),
                  "hello-world"});
   }

   std::this_thread::sleep_for(1s);
}
```

### Round-Robin Pool

```cpp
#include "siddiqsoft/roundrobin_pool.hpp"

void main() {
   siddiqsoft::roundrobin_pool<MyWork> pool{[](auto& item){
      item();
   }};
   
   for(int i=0; i < 100; i++) {
      pool.queue({std::format("https://localhost:443/test?iter={}",i),
                  "hello-world"});
   }

   std::this_thread::sleep_for(1s);
}
```

## API

Utility                   | Description
-------------------------:|:------------
[`siddiqsoft::simple_worker`](./doxygen/html/structsiddiqsoft_1_1simple__worker.html) | Provides for a single thread with an internal deque.<br/>You'd use this to immediately make any "long" task asynchronous.<br/>Use instead of `std::async`.<br/>Just register your callback/lambda and you're done. No need to worry about waiting for the result (no worry about futures or waiting on them).<br/>Your declared callback will be invoked!
[`siddiqsoft::simple_pool`](./doxygen/html/structsiddiqsoft_1_1simple__pool.html) | Implements an array of threads backed with a *single* deque. Each thread waits for and processes the next available item from the single deque.
[`siddiqsoft::roundrobin_pool`](./doxygen/html/structsiddiqsoft_1_1roundrobin__pool.html) | Implements an vector of basic_workers (each worker has its independent queue therefore minimizing contention time).<br/>The queue method implements a running counter based round-robin feeder.
[`siddiqsoft::periodic_worker`](./doxygen/html/structsiddiqsoft_1_1periodic__worker.html) | Provides a facility where you can have your function/lambda invoked at a given periodic rate (in microseconds).
[`siddiqsoft::resource_pool`](./doxygen/html/structsiddiqsoft_1_1resource__pool.html) | Provides a basic resource pool useful for keeping a pool of connection objects for the various threadpools to checkout/checkin.

## Implementation note

In order to use `std::jthread` on Clang 18 and Clang 19, we enable the compiler flag `"CMAKE_CXX_FLAGS": "-fexperimental-library"` in the CMakeLists.txt. This option will show up in your client library under Clang compilers.

<p align="right">
&copy; 2021 Siddiq Software LLC. All rights reserved.
</p>
