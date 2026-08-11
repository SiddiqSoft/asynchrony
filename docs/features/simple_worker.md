# Simple Worker

The `simple_worker` is a single-threaded asynchronous task processor. It manages a dedicated background thread (`std::jthread`) that sequentially pops items off an internal queue and processes them using a user-supplied callback.

## Features

- **Single Background Thread**: Guarantees sequential execution of queued tasks in FIFO order.
- **Move-Only Types**: Fully supports move-only objects (`std::unique_ptr`, move-only structs).
- **Custom Thread Priority**: Configurable thread priority range from `-10` to `+10`.
- **RAII Lifecycle**: Destructor waits for pending tasks to drain before thread termination.
- **Exception Isolation**: Exceptions thrown within callbacks are caught internally without killing the background thread.

---

## Basic Example

```cpp
#include "siddiqsoft/simple_worker.hpp"
#include <iostream>
#include <chrono>

struct LogTask {
    std::string message;
    
    void operator()() {
        std::cout << "[LOG] " << message << std::endl;
    }
};

int main() {
    // Create worker with callback
    siddiqsoft::simple_worker<LogTask> logger{
        [](auto&& task) {
            task(); // Execute
        }
    };

    // Queue log messages
    logger.queue(LogTask{"Application starting..."});
    logger.queue(LogTask{"Performing initialization..."});
    logger.queue(LogTask{"System ready."});

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return 0;
}
```

---

## Thread Priority

You can specify a thread priority via the second template parameter:

```cpp
// Normal priority (0, default)
siddiqsoft::simple_worker<LogTask> normalWorker{callback};

// High priority (+5)
siddiqsoft::simple_worker<LogTask, 5> highPriorityWorker{callback};

// Low background priority (-5)
siddiqsoft::simple_worker<LogTask, -5> backgroundWorker{callback};
```

---

## Exception Handling

If a task callback throws an exception, `simple_worker` catches the exception and logs it to `std::cerr`. The worker thread remains alive and continues processing remaining items in the queue.

```cpp
siddiqsoft::simple_worker<LogTask> worker{
    [](auto&& task) {
        if (task.message.empty()) {
            throw std::invalid_argument("Empty message");
        }
        task();
    }
};

worker.queue(LogTask{""});      // Throws exception, caught internally
worker.queue(LogTask{"Valid"}); // Executes normally
```

---

## Lifetime Management

`simple_worker` uses RAII. When the object goes out of scope, its destructor requests cancellation and waits for the internal queue to empty before stopping the thread.

```cpp
{
    siddiqsoft::simple_worker<LogTask> worker{callback};
    worker.queue(task1);
    worker.queue(task2);
} // Destructor blocks until task1 and task2 finish processing
```

---

## Diagnostics & JSON Monitoring

When `nlohmann/json` is included:

```cpp
#include <nlohmann/json.hpp>

auto state = worker.to_json();
std::cout << state.dump(2) << std::endl;
```

Outputs metrics like `itemsSize`, `queueCounter`, `itemsQueued`, `itemsPopped`, `itemsOutstanding`, and `threadPriority`.
