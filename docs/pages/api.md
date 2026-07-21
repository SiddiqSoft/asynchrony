# API Reference

## Table of Contents

- [simple_worker](#simple_worker)
- [simple_pool](#simple_pool)
- [roundrobin_pool](#roundrobin_pool)
- [periodic_worker](#periodic_worker)

---

## simple_worker

A single-threaded asynchronous processor that queues work items and processes them sequentially in a dedicated thread.

### Template Parameters

- `T` - The type of items to process (must be move-constructible)

### Constructor

```cpp
template<typename Callback>
simple_worker(Callback&& callback);
```

Creates a simple worker with the given callback function. The callback is invoked for each queued item.

**Parameters:**
- `callback` - A callable that accepts a reference to an item of type `T`

**Example:**
```cpp
siddiqsoft::simple_worker<MyTask> worker{[](auto& task) {
    task();  // Process the task
}};
```

### Methods

#### queue

```cpp
void queue(T&& item);
```

Queues an item for processing.

**Parameters:**
- `item` - The item to queue (moved into the queue)

**Example:**
```cpp
worker.queue(MyTask{"data"});
```

#### size

```cpp
size_t size() const;
```

Returns the current number of items in the queue.

**Returns:** Number of pending items

#### addCounter

```cpp
uint64_t addCounter() const;
```

Returns the total number of items added to the queue since creation.

**Returns:** Total items added

#### removeCounter

```cpp
uint64_t removeCounter() const;
```

Returns the total number of items processed since creation.

**Returns:** Total items processed

---

## simple_pool

A multi-threaded pool with a shared queue that distributes work across multiple threads.

### Template Parameters

- `T` - The type of items to process (must be move-constructible)

### Constructor

```cpp
template<typename Callback>
simple_pool(Callback&& callback, size_t threadCount = std::thread::hardware_concurrency());
```

Creates a thread pool with the given callback function and number of worker threads.

**Parameters:**
- `callback` - A callable that accepts a reference to an item of type `T`
- `threadCount` - Number of worker threads (defaults to CPU core count)

**Example:**
```cpp
siddiqsoft::simple_pool<MyTask> pool{[](auto& task) {
    task();  // Process the task
}, 4};  // 4 worker threads
```

### Methods

#### queue

```cpp
void queue(T&& item);
```

Queues an item for processing by any available worker thread.

**Parameters:**
- `item` - The item to queue (moved into the queue)

**Example:**
```cpp
pool.queue(MyTask{"data"});
```

#### size

```cpp
size_t size() const;
```

Returns the current number of items in the shared queue.

**Returns:** Number of pending items

#### addCounter

```cpp
uint64_t addCounter() const;
```

Returns the total number of items added to the pool since creation.

**Returns:** Total items added

#### removeCounter

```cpp
uint64_t removeCounter() const;
```

Returns the total number of items processed since creation.

**Returns:** Total items processed

---

## roundrobin_pool

A multi-threaded pool with per-thread queues that minimizes contention through round-robin distribution of work items.

### Template Parameters

- `T` - The type of items to process (must be move-constructible)

### Constructor

```cpp
template<typename Callback>
roundrobin_pool(Callback&& callback, size_t threadCount = std::thread::hardware_concurrency());
```

Creates a round-robin pool with the given callback function and number of worker threads.

**Parameters:**
- `callback` - A callable that accepts a reference to an item of type `T`
- `threadCount` - Number of worker threads (defaults to CPU core count)

**Example:**
```cpp
siddiqsoft::roundrobin_pool<MyTask> pool{[](auto& task) {
    task();  // Process the task
}, 4};  // 4 worker threads
```

### Methods

#### queue

```cpp
void queue(T&& item);
```

Queues an item for processing using round-robin distribution across worker threads.

**Parameters:**
- `item` - The item to queue (moved into the appropriate thread's queue)

**Example:**
```cpp
pool.queue(MyTask{"data"});
```

#### size

```cpp
size_t size() const;
```

Returns the total number of items across all per-thread queues.

**Returns:** Total number of pending items

#### addCounter

```cpp
uint64_t addCounter() const;
```

Returns the total number of items added to the pool since creation.

**Returns:** Total items added

#### removeCounter

```cpp
uint64_t removeCounter() const;
```

Returns the total number of items processed since creation.

**Returns:** Total items processed

---

## periodic_worker

Executes a function at regular intervals in a dedicated thread.

### Template Parameters

- `Rep` - The representation type for the interval duration (default: `std::milli`)
- `Period` - The period type for the interval duration (default: `std::ratio<1>`)

### Constructor

```cpp
template<typename Callback>
periodic_worker(Callback&& callback, std::chrono::duration<Rep, Period> interval);
```

Creates a periodic worker that executes the callback at the specified interval.

**Parameters:**
- `callback` - A callable that takes no arguments and returns void
- `interval` - The time interval between executions

**Example:**
```cpp
siddiqsoft::periodic_worker<> timer{
    []() {
        std::cout << "Tick!" << std::endl;
    },
    std::chrono::milliseconds(500)
};
```

### Behavior

- The worker starts executing immediately upon construction
- The callback is executed at approximately the specified interval
- The worker stops when the object is destroyed
- Exceptions in the callback are caught and logged

---

## Common Patterns

### Exception Handling

All worker classes catch exceptions in callbacks and log them without propagating:

```cpp
siddiqsoft::simple_worker<MyTask> worker{[](auto& task) {
    try {
        task();
    } catch (const std::exception& e) {
        // Exception is caught and logged
        // Worker continues processing
    }
}};
```

### Lifetime Management

Worker objects use RAII principles. The worker stops when the object is destroyed:

```cpp
{
    siddiqsoft::simple_worker<MyTask> worker{callback};
    worker.queue(item1);
    worker.queue(item2);
    // Worker stops here when scope ends
}
```

---

## Thread Safety

All classes are thread-safe for their public interfaces:

- `queue()` can be called from multiple threads
- `size()`, `addCounter()`, and `removeCounter()` are atomic reads
- Internal synchronization uses `std::mutex` and `std::semaphore`

---

## Performance Considerations

### simple_worker vs simple_pool

- Use `simple_worker` for sequential processing with minimal overhead
- Use `simple_pool` for CPU-bound work that benefits from parallelization

### roundrobin_pool

- Provides better scalability than `simple_pool` for high-contention scenarios
- Each thread has its own queue, reducing lock contention
- Ideal for I/O-bound operations with many threads

### periodic_worker

- Suitable for monitoring, cleanup, and periodic maintenance tasks
- Interval is approximate; actual execution may vary based on system load
- Exceptions in callbacks are logged but don't stop the worker
