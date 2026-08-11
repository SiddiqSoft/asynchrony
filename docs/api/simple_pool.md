# Class Template `siddiqsoft::simple_pool`

Header: `#include "siddiqsoft/simple_pool.hpp"`

```cpp
template <typename T, uint16_t ThreadCount = 0>
    requires std::is_move_constructible_v<T>
struct simple_pool;
```

A multi-threaded worker pool that processes tasks from a single shared queue across `N` background worker threads using a counting semaphore.

---

## Template Parameters

| Parameter | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `T` | `typename` | *Required* | Work item type. Must satisfy `std::is_move_constructible_v<T>`. |
| `ThreadCount` | `uint16_t` | `0` | Number of worker threads. If `0`, resolves to `std::thread::hardware_concurrency()`. |

---

## Static Constants

| Constant | Type | Value | Description |
| :--- | :--- | :--- | :--- |
| `DEFAULT_WAIT_FOR_NEXT_ITEM_MS` | `std::chrono::milliseconds` | `1500` | Default wait interval for threads waiting on work semaphore. |

---

## Public Data Members

| Member | Type | Availability | Description |
| :--- | :--- | :--- | :--- |
| `queueCounter` | `std::atomic_uint64_t` | `_DEBUG` builds | Counter tracking total items queued since pool creation. |

---

## Member Functions

### Constructor

```cpp
explicit simple_pool(std::function<void(T&&)> callback);
```

Constructs the thread pool with `ThreadCount` background worker threads (or hardware concurrency if `ThreadCount == 0`) and immediately launches them.

- **Parameters**: `callback` – Callable function invoked by worker threads when dequeuing work items.
- **Copy/Move**: Copy and move constructors/operators are deleted.

#### Example

```cpp
#include "siddiqsoft/simple_pool.hpp"
#include <iostream>
#include <string>

// Create a pool using hardware concurrency (default)
siddiqsoft::simple_pool<std::string> pool([](std::string&& item) {
    std::cout << "Thread [" << std::this_thread::get_id() << "] processed: " << item << std::endl;
});

// Create a pool with explicitly 4 threads via template parameter
siddiqsoft::simple_pool<std::string, 4> fixedPool([](std::string&& item) {
    std::cout << "Fixed pool processed: " << item << std::endl;
});
```

---

### Destructor

```cpp
~simple_pool();
```

Gracefully shuts down all worker threads. Sends stop requests to threads, signals waiting semaphores, and joins each worker thread before destruction completes.

#### Example

```cpp
{
    siddiqsoft::simple_pool<int> pool([](int&& val) {
        // Process work item
    });

    pool.queue(1);
    pool.queue(2);
} // Destructor signals threads to stop and joins them.
```

---

### `queue`

```cpp
void queue(T&& item);
```

Adds a work item to the shared queue via perfect forwarding and signals an idle worker thread via counting semaphore.

- **Parameters**: `item` – Rvalue reference to work item.
- **Thread Safety**: Safe to call concurrently from multiple producer threads.

#### Example

```cpp
siddiqsoft::simple_pool<std::string> pool([](std::string&& task) {
    std::cout << "Executing " << task << std::endl;
});

// Queue temporary object
pool.queue("Task #1");

// Queue moved object
std::string task2 = "Task #2";
pool.queue(std::move(task2));
```

---

### `to_json`

```cpp
auto to_json() const -> nlohmann::json;
```

Generates a JSON object containing pool state and performance statistics. Available when `NLOHMANN_JSON_VERSION_MAJOR` is defined.

- **Returns**: `nlohmann::json` object containing:
    - `_typver`: Type version identifier (`"siddiqsoft.asynchrony.simple_pool/2.3.3"`)
    - `workersSize`: Number of active worker threads in the pool
    - `dequeSize`: Current number of items in the shared queue
    - `queueCounter`: Total items queued counter
    - `waitInterval`: Wait interval in milliseconds (`1500`)

Also provides a free function overload in `siddiqsoft` namespace:
```cpp
template <typename T, uint16_t N = 0>
static auto to_json(nlohmann::json& dest, const siddiqsoft::simple_pool<T, N>& src) -> void const;
```

#### Example

```cpp
#include <nlohmann/json.hpp>

siddiqsoft::simple_pool<int, 2> pool([](int&&) {});
pool.queue(100);

// Direct JSON dump
nlohmann::json info = pool.to_json();
std::println(info.dump(2));

// Implicit JSON conversion
nlohmann::json j = pool;
std::cout << "Workers: " << j["workersSize"] << std::endl;
```

