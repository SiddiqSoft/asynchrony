
@page usage_guide Usage Guide

@section overview Overview

This guide covers the main components of the asynchrony library and how to use them effectively.

@section simple_worker Simple Worker

The `simple_worker` is the simplest component - it provides a single background thread that processes
items from a queue sequentially.

@subsection sw_basic Basic Usage

```cpp
#include "siddiqsoft/simple_worker.hpp"

struct WorkItem {
    int id;
    std::string data;
};

int main() {
    // Create a worker with a callback
    siddiqsoft::simple_worker<WorkItem> worker{
        [](auto&& item) {
            std::cout << "Processing item " << item.id << ": " << item.data << std::endl;
        }
    };

    // Queue items
    worker.queue(WorkItem{1, "first"});
    worker.queue(WorkItem{2, "second"});
    worker.queue(WorkItem{3, "third"});

    // Wait for completion
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
```

@subsection sw_priority Thread Priority

You can set thread priority using the template parameter:

```cpp
// Normal priority (0)
siddiqsoft::simple_worker<Task> worker1{callback};

// High priority (5)
siddiqsoft::simple_worker<Task, 5> worker2{callback};

// Low priority (-5)
siddiqsoft::simple_worker<Task, -5> worker3{callback};
```

Priority range: -10 to +10

@subsection sw_exception Exception Handling

Exceptions in the callback are caught and logged to stderr:

```cpp
siddiqsoft::simple_worker<Task> worker{
    [](auto&& item) {
        try {
            // Your processing logic
            if (item.invalid) throw std::runtime_error("Invalid item");
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
};
```

@subsection sw_lifetime Lifetime Management

The worker will wait for the queue to empty before shutting down:

```cpp
{
    siddiqsoft::simple_worker<Task> worker{callback};
    worker.queue(task1);
    worker.queue(task2);
}  // Destructor waits for queue to empty before returning
```

@section simple_pool Simple Pool

The `simple_pool` distributes work across multiple threads using a shared queue.
All threads wait on the same queue, so there's minimal contention.

@subsection sp_basic Basic Usage

```cpp
#include "siddiqsoft/simple_pool.hpp"

int main() {
    // Create a pool with default thread count (hardware_concurrency)
    siddiqsoft::simple_pool<WorkItem> pool{
        [](auto&& item) {
            std::cout << "Thread " << std::this_thread::get_id() 
                     << " processing item " << item.id << std::endl;
        }
    };

    // Queue items
    for (int i = 0; i < 100; ++i) {
        pool.queue(WorkItem{i, "data-" + std::to_string(i)});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}
```

@subsection sp_custom_threads Custom Thread Count

```cpp
// Create a pool with exactly 4 threads
siddiqsoft::simple_pool<WorkItem, 4> pool{callback};

// Create a pool with default thread count
siddiqsoft::simple_pool<WorkItem, 0> pool{callback};  // 0 = use hardware_concurrency()
```

@subsection sp_performance Performance Considerations

- Use `simple_pool` when tasks are CPU-bound or have similar execution times
- Use `roundrobin_pool` when tasks have variable execution times
- Increase thread count for I/O-bound tasks
- Decrease thread count for CPU-bound tasks (typically = hardware_concurrency)

@section roundrobin_pool Round-Robin Pool

The `roundrobin_pool` uses per-thread queues and distributes work in a round-robin fashion.
This reduces contention compared to `simple_pool`.

@subsection rrp_basic Basic Usage

```cpp
#include "siddiqsoft/roundrobin_pool.hpp"

int main() {
    siddiqsoft::roundrobin_pool<WorkItem> pool{
        [](auto&& item) {
            std::cout << "Processing item " << item.id << std::endl;
        }
    };

    for (int i = 0; i < 1000; ++i) {
        pool.queue(WorkItem{i, "data"});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}
```

@subsection rrp_custom_threads Custom Thread Count

```cpp
// Create a round-robin pool with exactly 8 threads
siddiqsoft::roundrobin_pool<WorkItem, 8> pool{callback};
```

@subsection rrp_vs_simple Round-Robin vs Simple Pool

| Aspect | Simple Pool | Round-Robin Pool |
|--------|-------------|------------------|
| Queue Type | Single shared queue | Per-thread queues |
| Contention | Higher | Lower |
| Load Balancing | Automatic | Round-robin |
| Best For | Uniform task times | Variable task times |
| Memory | Lower | Higher |

@section periodic_worker Periodic Worker

Execute a function at regular intervals.

@subsection pw_basic Basic Usage

```cpp
#include "siddiqsoft/periodic_worker.hpp"

int main() {
    siddiqsoft::periodic_worker<> timer{
        []() {
            std::cout << "Tick! " << std::chrono::system_clock::now() << std::endl;
        },
        std::chrono::milliseconds(500)  // Execute every 500ms
    };

    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}
```

@subsection pw_priority Priority

```cpp
// High priority periodic worker
siddiqsoft::periodic_worker<5> timer{callback, interval};
```

@subsection pw_named Named Periodic Worker

```cpp
// Named periodic worker (useful for debugging)
siddiqsoft::periodic_worker<> timer{
    callback,
    std::chrono::milliseconds(1000),
    "my-timer"
};
```

@section resource_pool Resource Pool

Manage a pool of reusable resources (e.g., database connections).

@subsection rp_basic Basic Usage

```cpp
#include "siddiqsoft/resource_pool.hpp"

class DatabaseConnection {
public:
    void execute(const std::string& query) { /* ... */ }
};

int main() {
    siddiqsoft::resource_pool<DatabaseConnection> pool;

    // Checkout a resource
    auto conn = pool.checkout();
    conn.execute("SELECT * FROM users");

    // Checkin returns the resource to the pool
    pool.checkin(std::move(conn));

    return 0;
}
```

@subsection rp_methods Resource Pool Methods

```cpp
siddiqsoft::resource_pool<T> pool;

// Get current pool size
auto size = pool.size();

// Checkout a resource (throws if empty)
auto resource = pool.checkout();

// Return a resource to the pool
pool.checkin(std::move(resource));

// Clear all resources
pool.clear();
```

@section best_practices Best Practices

@subsection bp_lifetime Lifetime Management

- Keep the worker/pool alive as long as you're queuing work
- Destruction waits for the queue to empty before shutting down threads
- Use RAII to ensure proper cleanup

@subsection bp_callbacks Callback Design

- Keep callbacks lightweight and fast
- Avoid blocking operations in callbacks
- Handle exceptions within the callback
- Use move semantics for efficiency

@subsection bp_threading Thread Safety

- The queue() method is thread-safe
- Callbacks are executed in worker threads
- Protect shared state accessed from callbacks with mutexes

@subsection bp_performance Performance Tips

- Use `roundrobin_pool` for variable-duration tasks
- Use `simple_pool` for uniform-duration tasks
- Batch small tasks to reduce overhead
- Monitor queue depth for bottlenecks

@section advanced Advanced Topics

@subsection adv_custom_types Custom Data Types

Your data type must be move-constructible:

```cpp
struct MyTask {
    std::string data;
    std::vector<int> values;

    // Must be move-constructible
    MyTask(MyTask&&) = default;
    MyTask& operator=(MyTask&&) = default;

    // Copy is optional
    MyTask(const MyTask&) = delete;
    MyTask& operator=(const MyTask&) = delete;
};
```

@subsection adv_json_serialization JSON Serialization

If nlohmann/json is available, you can serialize worker state:

```cpp
#include <nlohmann/json.hpp>
#include "siddiqsoft/simple_worker.hpp"

siddiqsoft::simple_worker<Task> worker{callback};
auto json = worker.toJson();
std::cout << json.dump(2) << std::endl;
```

The JSON output includes:
- `_typver`: Library version identifier
- `itemsSize`: Current queue size
- `queueCounter`: Total items queued
- `itemsQueued`: Total items added
- `itemsPopped`: Total items processed
- `itemsOutstanding`: Items still in queue
- `threadPriority`: Thread priority level
- `outstandingCallback`: Callbacks currently executing
- `waitInterval`: Wait timeout in milliseconds

