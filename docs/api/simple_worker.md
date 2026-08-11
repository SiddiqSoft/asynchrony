# Class Template `siddiqsoft::simple_worker`

Header: `#include "siddiqsoft/simple_worker.hpp"`

```cpp
template<typename T, int ThreadPriority = 0>
class simple_worker;
```

A single-threaded asynchronous processor that queues work items and executes them sequentially in a dedicated background `std::jthread`.

---

## Template Parameters

- `T`: The item data type. Must satisfy move-constructible concept (`std::move_constructible<T>`).
- `ThreadPriority`: Integer priority adjustment for the worker thread (`-10` to `+10`, default `0`).

---

## Member Functions

### Constructors & Destructor

```cpp
template<typename Callback>
explicit simple_worker(Callback&& callback);
```

Constructs a worker and starts the background processing thread.

- **Parameters**: `callback` - A callable object accepting `T&` or `T&&`.

---

### `queue`

```cpp
void queue(T&& item);
```

Pushes an item into the worker's internal FIFO queue.

- **Parameters**: `item` - Rvalue reference to the object being queued.
- **Thread Safety**: Safe to call concurrently from multiple producer threads.

---

### `size`

```cpp
[[nodiscard]] size_t size() const noexcept;
```

Returns the current number of pending items in the queue.

- **Returns**: Number of pending tasks.

---

### `addCounter`

```cpp
[[nodiscard]] uint64_t addCounter() const noexcept;
```

Returns the cumulative total number of items pushed into the queue since worker creation.

---

### `removeCounter`

```cpp
[[nodiscard]] uint64_t removeCounter() const noexcept;
```

Returns the cumulative total number of items processed and popped from the queue.

---

### `toJson`

```cpp
[[nodiscard]] nlohmann::json toJson() const;
```

Generates a JSON snapshot containing worker state and performance counters.
