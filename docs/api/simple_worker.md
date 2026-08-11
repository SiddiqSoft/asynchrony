# Class Template `siddiqsoft::simple_worker`

Header: `#include "siddiqsoft/simple_worker.hpp"`

```cpp
template <typename T, int ThreadPriority = 0>
    requires((ThreadPriority >= -10) && (ThreadPriority <= 10)) && std::move_constructible<T>
struct simple_worker;
```

A single-threaded asynchronous worker that queues tasks of type `T` and processes them sequentially in a dedicated background `std::jthread`.

---

## Template Parameters

| Parameter | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `T` | `typename` | *Required* | Work item type. Must satisfy `std::move_constructible`. |
| `ThreadPriority` | `int` | `0` | Thread priority level (`-10` to `+10`). On Windows, passed to `SetThreadPriority()`. |

---

## Static Constants

| Constant | Type | Value | Description |
| :--- | :--- | :--- | :--- |
| `DEFAULT_WAIT_FOR_NEXT_ITEM_MS` | `std::chrono::milliseconds` | `1500` | Default interval worker thread waits on internal queue before re-checking stop token. |
| `DEFAULT_SHUTDOWN_DRAIN_MS` | `std::chrono::milliseconds` | `1000` | Default timeout duration for queue draining during shutdown. |

---

## Public Data Members

| Member | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `accepting_items` | `std::atomic<bool>` | `true` | Indicates whether new items can be queued. Set to `false` when `shutdown()` begins. |
| `shutdown_initiated` | `std::atomic<bool>` | `false` | Indicates whether worker shutdown sequence has been started. |
| `shutdown_invoked` | `std::once_flag` | — | `std::once_flag` ensuring `shutdown()` logic executes exactly once. |

---

## Member Functions

### Constructor

```cpp
explicit simple_worker(std::function<void(T&&)> callback);
```

Constructs a `simple_worker` and immediately launches the background processing thread.

- **Parameters**: `callback` – Function or callable invoked for each dequeued item.
- **Copy/Move**: Copy and move constructors are deleted.

#### Example

```cpp
#include "siddiqsoft/simple_worker.hpp"
#include <iostream>
#include <string>

// Create a simple worker that processes strings
siddiqsoft::simple_worker<std::string> worker([](std::string&& item) {
    std::cout << "Processing item: " << item << std::endl;
});
```

---

### Destructor

```cpp
~simple_worker();
```

Gracefully shuts down the worker thread. Automatically invokes [`shutdown()`](#shutdown) to drain remaining tasks and join the background thread before object destruction completes.

#### Example

```cpp
{
    siddiqsoft::simple_worker<std::string> worker([](std::string&& item) {
        std::cout << item << std::endl;
    });

    worker.queue("Task A");
    worker.queue("Task B");
} // Destructor runs here: drains pending items and joins background thread cleanly.
```

---

### `queue`

```cpp
void queue(T&& item) noexcept(false);
```

Pushes an item into the worker's internal queue. Ownership of `item` is transferred to the worker via move semantics.

- **Parameters**: `item` – Rvalue reference to work item.
- **Exceptions**: Throws `std::runtime_error` if the worker is currently shutting down (`accepting_items` is `false`).
- **Thread Safety**: Safe to call concurrently from multiple producer threads.

#### Example

```cpp
siddiqsoft::simple_worker<std::string> worker([](std::string&& item) {
    std::cout << "Handled: " << item << std::endl;
});

// Queue using temporary rvalue
worker.queue("Instant message");

// Queue using std::move
std::string msg = "Deferred payload";
worker.queue(std::move(msg));
```

---

### `shutdown`

```cpp
bool shutdown(std::chrono::milliseconds timeout = DEFAULT_SHUTDOWN_DRAIN_MS);
```

Initiates a graceful shutdown of the worker thread.

- Stops accepting new items (`accepting_items` set to `false`).
- Waits up to `timeout` duration for the internal queue to drain.
- Signals the background thread to stop and joins it.
- Uses `std::call_once` internally so subsequent calls return the cached shutdown status.

- **Parameters**: `timeout` – Maximum duration to wait for the queue to drain (default `1000ms`).
- **Returns**: `bool` – `true` if all items were drained before timeout; `false` if timeout occurred.

#### Example

```cpp
siddiqsoft::simple_worker<int> worker([](int&& val) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
});

for (int i = 0; i < 10; ++i) {
    worker.queue(i + 1);
}

// Gracefully shutdown with a custom 2-second timeout
bool drained = worker.shutdown(std::chrono::seconds(2));
if (drained) {
    std::cout << "All items processed successfully before shutdown." << std::endl;
}
```

---

### `to_json`

```cpp
auto to_json() const -> nlohmann::json;
```

Generates a JSON snapshot containing worker diagnostics and telemetry. Available when `NLOHMANN_JSON_VERSION_MAJOR` is defined.

- **Returns**: `nlohmann::json` object containing:
    - `_typver`: Type version string (`"siddiqsoft.asynchrony.simple_worker/2.3.3"`)
    - `itemsSize`: Current number of items in queue
    - `queueCounter`: Total items queued counter
    - `itemsQueued`: Total items added
    - `itemsPopped`: Total items processed
    - `itemsOutstanding`: Number of items currently queued but not processed
    - `threadPriority`: Priority integer setting
    - `outstandingCallback`: Active executing callback count
    - `waitInterval`: Wait interval in milliseconds

Also provides a free function overload in `siddiqsoft` namespace:
```cpp
template <typename T, int Pri = 0>
static void to_json(nlohmann::json& dest, const siddiqsoft::simple_worker<T, Pri>& src);
```

#### Example

```cpp
#include <nlohmann/json.hpp>

siddiqsoft::simple_worker<int> worker([](int&&) {});
worker.queue(42);

// Direct method call
nlohmann::json stats = worker.to_json();
std::cout << stats.dump(2) << std::endl;

// Implicit conversion via nlohmann::json ADL
nlohmann::json j = worker;
std::cout << j["itemsQueued"] << std::endl;
```

