# Class Template `siddiqsoft::periodic_worker`

Header: `#include "siddiqsoft/periodic_worker.hpp"`

```cpp
template <int ThreadPriority = 0>
    requires((ThreadPriority >= -10) && (ThreadPriority <= 10))
struct periodic_worker;
```

Executes a user callback function periodically at regular time intervals in a dedicated background `std::jthread`.

---

## Template Parameters

| Parameter | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `ThreadPriority` | `int` | `0` | Thread priority level (`-10` to `+10`). On Windows, passed to `SetThreadPriority()`. |

---

## Static Constants

| Constant | Type | Value | Description |
| :--- | :--- | :--- | :--- |
| `DEFAULT_WAIT_FOR_NEXT_ITEM_MS` | `std::chrono::milliseconds` | `1500` | Default wait interval for background worker loop. |

---

## Member Functions

### Constructor

```cpp
periodic_worker(std::function<void()> callback,
                std::chrono::microseconds interval,
                std::string name = "anonymous-periodic-worker");
```

Constructs a `periodic_worker` and immediately launches the background timer loop.

- **Parameters**:
    - `callback`: Function with signature `void()` executed on every period.
    - `interval`: Time interval between callback invocations (`std::chrono::microseconds` or compatible `std::chrono::duration`).
    - `name`: Diagnostic name string for identifying the worker thread (default: `"anonymous-periodic-worker"`).
- **Copy/Move**: Copy and move constructors/operators are deleted.

#### Usage Example

```cpp
#include "siddiqsoft/periodic_worker.hpp"
#include <iostream>
#include <chrono>

// Execute heartbeat every 500 milliseconds
siddiqsoft::periodic_worker<> heartbeat(
    []() { std::cout << "Heartbeat tick!" << std::endl; },
    std::chrono::milliseconds(500),
    "HeartbeatWorker"
);

// High-priority periodic task (+3)
siddiqsoft::periodic_worker<3> highPriorityWorker(
    []() { /* High priority check */ },
    std::chrono::seconds(1)
);
```

---

### Destructor

```cpp
~periodic_worker();
```

Gracefully shuts down the periodic worker thread. Sets `invokePeriod` to `0` to instantly trigger semaphore wakeup, requests thread cancellation, and joins the background thread.

#### Usage Example

```cpp
{
    siddiqsoft::periodic_worker<> worker(
        []() { std::cout << "Tick" << std::endl; },
        std::chrono::seconds(1)
    );

    std::this_thread::sleep_for(std::chrono::seconds(3));
} // Destructor runs: instantly signals thread wakeup and joins cleanly.
```

---

### `forceCleanupTerminate`

```cpp
void forceCleanupTerminate(const std::source_location& sl = std::source_location::current());
```

Forcefully terminates the worker thread using platform native thread APIs (`pthread_cancel` on POSIX / `TerminateThread` on Windows).

> [!WARNING]
> Last-resort API. Call only during emergency application shutdown when the callback cannot be guaranteed to terminate cleanly or respect `stop_token`. May lead to resource leaks if invoked during normal application flow.

- **Parameters**: `sl` – Source location automatically captured for diagnostic error logging.

#### Usage Example

```cpp
siddiqsoft::periodic_worker<> worker(
    []() { /* long hanging operation */ },
    std::chrono::seconds(1)
);

// Force immediate OS thread cancellation during emergency application exit
worker.forceCleanupTerminate();
```

---

### `to_json`

```cpp
nlohmann::json to_json() const;
```

Generates a JSON snapshot containing worker diagnostics and telemetry. Available when `NLOHMANN_JSON_VERSION_MAJOR` is defined.

- **Returns**: `nlohmann::json` object containing:
    - `_typver`: Type version string (`"siddiqsoft.asynchrony.periodic_worker/2.3.3"`)
    - `threadName`: Thread diagnostic name string
    - `outstandingCallbacks`: Number of callbacks currently executing
    - `invokeCounter`: Total number of times the callback has been executed
    - `threadPriority`: Integer thread priority level
    - `waitInterval`: Current interval in microseconds

Also provides a free function overload in `siddiqsoft` namespace:
```cpp
template <int Pri = 0>
static void to_json(nlohmann::json& dest, const siddiqsoft::periodic_worker<Pri>& src);
```

#### Usage Example

```cpp
#include <nlohmann/json.hpp>

siddiqsoft::periodic_worker<> worker(
    []() {},
    std::chrono::milliseconds(100),
    "StatusMonitor"
);

// Direct method call
nlohmann::json info = worker.to_json();
std::cout << info.dump(2) << std::endl;

// Convert via nlohmann::json ADL
nlohmann::json j = worker;
std::cout << "Total invocations: " << j["invokeCounter"] << std::endl;
```

