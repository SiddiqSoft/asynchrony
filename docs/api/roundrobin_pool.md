# Class Template `siddiqsoft::roundrobin_pool`

Header: `#include "siddiqsoft/roundrobin_pool.hpp"`

```cpp
template <typename T, uint16_t ThreadCount = 0>
    requires std::is_move_constructible_v<T>
struct roundrobin_pool;
```

A multi-threaded thread pool that distributes tasks across individual per-thread queues (`siddiqsoft::simple_worker<T>`) using round-robin indexing.

---

## Template Parameters

| Parameter | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `T` | `typename` | *Required* | Work item type. Must satisfy `std::is_move_constructible_v<T>`. |
| `ThreadCount` | `uint16_t` | `0` | Number of worker threads in the pool. If `0`, resolves to `std::thread::hardware_concurrency()`. |

---

## Public Data Members

| Member | Type | Availability | Description |
| :--- | :--- | :--- | :--- |
| `queueCounter` | `std::atomic_uint64_t` | `_DEBUG` builds | Counter tracking total items queued across all workers. |

---

## Member Functions

### Constructor

```cpp
explicit roundrobin_pool(std::function<void(T&&)> callback);
```

Constructs a `roundrobin_pool` with `ThreadCount` worker threads (or hardware concurrency if `ThreadCount == 0`), stored inside a `std::deque` to prevent element relocation.

- **Parameters**: `callback` – Callable function invoked by individual worker threads.
- **Copy/Move**: Copy and move constructors/operators are deleted.

#### Example

```cpp
#include "siddiqsoft/roundrobin_pool.hpp"
#include <iostream>
#include <string>

// Create a pool using default hardware thread count
siddiqsoft::roundrobin_pool<std::string> pool([](std::string&& item) {
    std::cout << "Worker processed: " << item << std::endl;
});

// Create a pool with explicitly 8 worker threads
siddiqsoft::roundrobin_pool<std::string, 8> customPool([](std::string&& item) {
    std::cout << "Custom worker processed: " << item << std::endl;
});
```

---

### Destructor

```cpp
~roundrobin_pool() = default;
```

Destroys the `roundrobin_pool` instance and triggers destruction of all worker threads in the internal `std::deque`. Each worker drains its queue and joins cleanly.

#### Example

```cpp
{
    siddiqsoft::roundrobin_pool<int> pool([](int&& val) {
        // Process item
    });

    pool.queue(10);
    pool.queue(20);
} // Destructor runs automatically when pool goes out of scope.
```

---

### `queue`

```cpp
void queue(T&& item);
```

Assigns `item` to the next worker thread using round-robin index dispatch (`queueCounter % workersSize`).

- **Parameters**: `item` – Rvalue reference to work item.
- **Thread Safety**: Safe to call concurrently from multiple producer threads. Atomic `fetch_add` guarantees unique round-robin indexes.

#### Example

```cpp
siddiqsoft::roundrobin_pool<int> pool([](int&& val) {
    std::cout << "Value: " << val << std::endl;
});

// Push items into round-robin pool
for (int i = 0; i < 50; ++i) {
    pool.queue(i * 10);
}
```

---

### `to_json`

```cpp
nlohmann::json to_json() const;
```

Generates a JSON snapshot containing pool diagnostics and telemetry. Available when `NLOHMANN_JSON_VERSION_MAJOR` is defined.

- **Returns**: `nlohmann::json` object containing:
    - `_typver`: Type version identifier (`"siddiqsoft.asynchrony.roundrobin_pool/2.3.3"`)
    - `workersSize`: Number of worker threads in the pool
    - `queueCounter`: Total items queued counter

Also provides a free function overload in `siddiqsoft` namespace:
```cpp
template <typename T, uint16_t N = 0>
static void to_json(nlohmann::json& dest, const siddiqsoft::roundrobin_pool<T, N>& src);
```

#### Example

```cpp
#include <nlohmann/json.hpp>

siddiqsoft::roundrobin_pool<std::string, 4> pool([](std::string&&) {});
pool.queue("test");

// Retrieve diagnostic JSON snapshot
nlohmann::json info = pool.to_json();
std::cout << info.dump(2) << std::endl;

// Convert via nlohmann::json ADL
nlohmann::json j = pool;
std::cout << "Active Workers: " << j["workersSize"] << std::endl;
```

