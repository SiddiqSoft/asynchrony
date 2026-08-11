# Class Template `siddiqsoft::simple_pool`

Header: `#include "siddiqsoft/simple_pool.hpp"`

```cpp
template<typename T, size_t ThreadCount = 0>
class simple_pool;
```

A multi-threaded worker pool sharing a single input queue across worker threads.

---

## Template Parameters

- `T`: The item data type (must be move-constructible).
- `ThreadCount`: Number of background worker threads. Default `0` resolves to `std::thread::hardware_concurrency()`.

---

## Member Functions

### Constructor

```cpp
template<typename Callback>
explicit simple_pool(Callback&& callback, size_t threadCount = ThreadCount);
```

Constructs the thread pool and launches worker threads.

---

### `queue`

```cpp
void queue(T&& item);
```

Queues an item into the shared queue. The next available idle worker thread will pop and process it.

---

### `size`

```cpp
[[nodiscard]] size_t size() const noexcept;
```

Returns total pending items currently waiting in the shared queue.

---

### `addCounter`

```cpp
[[nodiscard]] uint64_t addCounter() const noexcept;
```

Returns total number of items queued since initialization.

---

### `removeCounter`

```cpp
[[nodiscard]] uint64_t removeCounter() const noexcept;
```

Returns total number of items processed across all threads.

---

### `toJson`

```cpp
[[nodiscard]] nlohmann::json toJson() const;
```

Exports JSON diagnostic state.
