# Features Overview

The **asynchrony** library provides specialized concurrency structures suited for different workload patterns and scaling requirements.

## Component Matrix

| Component | Class Template | Queue Architecture | Worker Threads | Primary Use Case |
| :--- | :--- | :--- | :--- | :--- |
| [**Simple Worker**](simple_worker.md) | `siddiqsoft::simple_worker<T, Priority>` | Single shared deque | 1 thread | Sequential async background processing |
| [**Simple Pool**](simple_pool.md) | `siddiqsoft::simple_pool<T, Threads>` | Single shared deque | N threads | Workload with uniform task durations |
| [**Round-Robin Pool**](roundrobin_pool.md) | `siddiqsoft::roundrobin_pool<T, Threads>` | Per-thread deques | N threads | High-throughput, variable duration tasks |
| [**Periodic Worker**](periodic_worker.md) | `siddiqsoft::periodic_worker<Priority>` | N/A (Timer loop) | 1 thread | Recurring timers and background maintenance |

---

## Key Design & Architecture Aspects

### Move Semantics & Efficiency
All queue operations accept rvalue references (`T&&`). This ensures that non-copyable types (such as `std::unique_ptr` or move-only task wrappers) can be queued effortlessly without unnecessary copies.

### Lock Contention & Thread Pools
- **`simple_pool`**: Features a single thread-safe queue. All worker threads block on the shared condition variable/semaphore. Works best when individual task execution times dominate queue operation costs.
- **`roundrobin_pool`**: Assigns an isolated queue to each worker thread. Pushing work distributes items across worker threads using a round-robin index counter. This drastically reduces mutex contention under heavy load.

### Thread Priority Customization
Worker threads can optionally set system thread priority levels ranging from `-10` (lowest) to `+10` (highest priority), defaulting to `0` (normal priority).

### Diagnostic JSON Output
If `nlohmann/json` is available in your build, calling `.toJson()` on any worker or pool returns a structured JSON object detailing queue depth, cumulative pushed/popped item counts, and thread status.

---

## Detailed Topic Guides

- [Simple Worker Guide](simple_worker.md)
- [Simple Pool Guide](simple_pool.md)
- [Round-Robin Pool Guide](roundrobin_pool.md)
- [Periodic Worker Guide](periodic_worker.md)
- [Quick Reference Cheat Sheet](quick_reference.md)
- [Security & Best Practices](security.md)
