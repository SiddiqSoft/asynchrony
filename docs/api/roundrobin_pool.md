# Class Template `siddiqsoft::roundrobin_pool`

Header: `#include "siddiqsoft/roundrobin_pool.hpp"`

```cpp
template<typename T, size_t ThreadCount = 0>
class roundrobin_pool;
```

A multi-threaded worker pool using per-thread queues and round-robin task dispatching.

---

## Template Parameters

- `T`: The item data type.
- `ThreadCount`: Number of background worker threads (`0` = `std::thread::hardware_concurrency()`).

---

## Member Functions

### Constructor

```cpp
template<typename Callback>
explicit roundrobin_pool(Callback&& callback, size_t threadCount = ThreadCount);
```

Initializes worker threads and per-thread task queues.

---

### `queue`

```cpp
void queue(T&& item);
```

Pushes an item into the next thread's queue according to internal round-robin sequence counter.

---

### `size`

```cpp
[[nodiscard]] size_t size() const noexcept;
```

Returns total pending items across all per-thread queues.

---

### `addCounter`

```cpp
[[nodiscard]] uint64_t addCounter() const noexcept;
```

Returns total items queued since creation.

---

### `removeCounter`

```cpp
[[nodiscard]] uint64_t removeCounter() const noexcept;
```

Returns total items processed across all threads.

---

### `to_json`

```cpp
[[nodiscard]] nlohmann::json to_json() const;
```

Generates JSON telemetry snapshot.
