# Simple Pool

The `simple_pool` is a multi-threaded pool that distributes work across multiple worker threads using a single shared queue.

## Features

- **Multi-Threaded Execution**: Leverages `std::thread::hardware_concurrency()` by default.
- **Shared Queue**: Automatic load balancing across threads as any idle worker pulls the next available item.
- **Move-Only Types**: Efficient resource transfer with move semantics and perfect forwarding.
- **Configurable Thread Count**: Ability to specify explicit worker thread pool sizes.
- **RAII Destructor**: Clean shutdown waiting for active worker threads and queue depletion.

---

## Basic Example

```cpp
#include "siddiqsoft/simple_pool.hpp"
#include <iostream>
#include <chrono>

struct WorkItem {
    int id;
    std::string data;
};

int main() {
    // Create pool with default hardware thread count
    siddiqsoft::simple_pool<WorkItem> pool{
        [](auto&& item) {
            std::cout << "Thread [" << std::this_thread::get_id()
                      << "] processing item #" << item.id << std::endl;
        }
    };

    // Queue 100 items
    for (int i = 0; i < 100; ++i) {
        pool.queue(WorkItem{i, "payload"});
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
```

---

## Specifying Custom Thread Count

By default, passing `0` or omitting the thread count parameter uses `std::thread::hardware_concurrency()`. You can pass a fixed number of threads via the second template parameter or constructor:

```cpp
// Pool with fixed 4 worker threads
siddiqsoft::simple_pool<WorkItem, 4> pool{callback};

// Explicit hardware concurrency pool
siddiqsoft::simple_pool<WorkItem, 0> pool{callback};
```

---

## Workload Recommendations

`simple_pool` is ideal when:

- Tasks have relatively uniform execution times.
- Automatic dynamic load balancing is desired across all threads.
- Queue push frequency is low to moderate so shared queue mutex contention remains minimal.
