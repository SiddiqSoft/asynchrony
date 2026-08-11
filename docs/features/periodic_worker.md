# Periodic Worker

The `periodic_worker` executes a specified function or lambda at regular time intervals in a dedicated thread.

## Features

- **Timer Execution**: Invokes callbacks periodically with `std::chrono` interval control.
- **Dedicated Thread**: Runs inside a dedicated `std::jthread` loop without blocking main threads.
- **RAII Lifespan**: Stopping the worker happens deterministically when the `periodic_worker` object goes out of scope or is destroyed.
- **Priority Override**: Supports system thread priority adjustments (`-10` to `+10`).

---

## Basic Usage Example

```cpp
#include "siddiqsoft/periodic_worker.hpp"
#include <iostream>
#include <chrono>

int main() {
    // Execute a heartbeat check every 250 milliseconds
    siddiqsoft::periodic_worker<> heartbeat{
        []() {
            std::cout << "Heartbeat ping!" << std::endl;
        },
        std::chrono::milliseconds(250)
    };

    // Run main application for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // When heartbeat leaves scope, the background timer thread stops cleanly.
    return 0;
}
```

---

## Named Periodic Worker

Passing an optional string name helps identify periodic workers in telemetry or debugging logs:

```cpp
siddiqsoft::periodic_worker<> monitor{
    []() { /* status check */ },
    std::chrono::seconds(5),
    "SystemHealthMonitor"
};
```
