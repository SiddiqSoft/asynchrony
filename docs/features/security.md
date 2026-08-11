# Security & Best Practices Guide

This document outlines memory safety guarantees, thread synchronization rules, callback safety, and deadlock prevention strategies for applications using the **asynchrony** library.

---

## Built-in Library Safeguards

- **Memory Safety**: Completely header-only C++23 design avoiding raw pointers, manual memory allocation (`malloc`/`free`), or legacy C-string functions (`strcpy`/`sprintf`).
- **Thread Safety**: All public entry points (`queue()`, `size()`, `addCounter()`, `removeCounter()`, `to_json()`) are thread-safe and protect internal queue structures via `std::mutex`, `std::counting_semaphore`, or `std::atomic`.
- **RAII Lifetimes**: Background threads (`std::jthread`) request cancellation via `std::stop_token` and automatically join upon destruction.
- **Exception Isolation**: Exceptions originating within client-provided callbacks are caught internally to prevent stack unwinding from destroying background threads unexpectedly.

---

## Callback Security & Safety Guidelines

Because client callbacks execute on background threads, applications should adhere to the following best practices:

### 1. Exception Handling inside Callbacks

Always handle local domain errors within callbacks to ensure controlled behavior:

```cpp
// Good: Local exception handling
worker.queue([](auto&& item) {
    try {
        item.process();
    } catch (const std::exception& e) {
        std::println(std::cerr, "Callback error: {}", e.what());
    }
});
```

### 2. Avoiding Deadlocks & Circular Dependencies

Never call `worker.queue()` recursively or attempt to lock mutexes in an inconsistent order from inside a worker callback:

```cpp
// Bad: Potential deadlock via circular reference
worker.queue([&worker](auto&& item) {
    worker.queue(item); // Circular submission can block queue workers
});
```

### 3. Backpressure & Memory Limits

`simple_worker` and `simple_pool` deques grow dynamically. When processing high-frequency data streams, monitor queue size to prevent out-of-memory issues:

```cpp
const size_t MAX_QUEUE_DEPTH = 5000;

if (pool.size() < MAX_QUEUE_DEPTH) {
    pool.queue(std::move(item));
} else {
    // Apply backpressure / log queue warning
}
```

---

## Vulnerability Reporting

If you discover a security vulnerability, please email details directly to **github@siddiqsoft.com**. Please do not open public GitHub issues for security vulnerabilities before contacting the maintainers.
