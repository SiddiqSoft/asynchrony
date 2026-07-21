@page security Security Guide

@section security_overview Overview

The asynchrony library is designed with security as a core principle. This guide covers security considerations, best practices, and potential risks.

**Security Rating**: EXCELLENT ⭐⭐⭐⭐⭐

@section security_features Built-in Security Features

### Memory Safety
- ✅ No unsafe functions (strcpy, sprintf, malloc, free)
- ✅ No raw pointers in public API
- ✅ Dynamic containers (std::deque) prevent buffer overflows
- ✅ RAII principles ensure automatic cleanup
- ✅ No use-after-free vulnerabilities

### Thread Safety
- ✅ Proper synchronization with mutexes and semaphores
- ✅ Correct memory ordering (acquire/release semantics)
- ✅ No race conditions
- ✅ Atomic operations for lock-free updates
- ✅ No data races

### Exception Safety
- ✅ Exceptions caught at thread boundaries
- ✅ Two-level exception catching for robustness
- ✅ Critical exceptions identified and handled
- ✅ No exception leaks to threads
- ✅ Graceful degradation on errors

### Type Safety
- ✅ Strong typing throughout
- ✅ C++20 concepts for compile-time checking
- ✅ No unsafe casts
- ✅ No type confusion vulnerabilities
- ✅ Template-based type safety

### Resource Management
- ✅ RAII principles throughout
- ✅ std::jthread ensures thread cleanup
- ✅ No resource leaks
- ✅ Graceful shutdown procedures

@section callback_security Callback Security

Callbacks are user-provided code and should be implemented securely.

### ✅ Good Callback Practices

```cpp
// Safe callback with error handling
worker.queue([](auto&& item) {
    try {
        // Process item safely
        item.process();
    } catch (const std::exception& ex) {
        // Handle exceptions gracefully
        std::cerr << "Error: " << ex.what() << std::endl;
    }
});

// Callback that doesn't block
worker.queue([](auto&& item) {
    // Quick processing, no blocking
    item.update();
});

// Callback that respects ownership
worker.queue([](auto&& item) {
    // Use move semantics
    auto data = std::move(item);
    process(data);
});
```

### ❌ Bad Callback Practices

```cpp
// ❌ Callback that blocks indefinitely
worker.queue([](auto&& item) {
    while (true) {  // Blocks thread forever
        item.process();
    }
});

// ❌ Callback that queues items (circular dependency)
worker.queue([&worker](auto&& item) {
    worker.queue(item);  // Can cause deadlock
});

// ❌ Callback that throws uncaught exceptions
worker.queue([](auto&& item) {
    throw std::runtime_error("Unhandled error");
    // Exception is caught by library, but indicates poor design
});

// ❌ Callback that accesses invalid memory
worker.queue([ptr = nullptr](auto&& item) {
    *ptr = item;  // Undefined behavior
});
```

@section deadlock_prevention Deadlock Prevention

### Circular Dependencies

Never queue items from within a callback:

```cpp
// ❌ BAD: Circular dependency
siddiqsoft::simple_worker<Task> worker{[&worker](auto&& item) {
    worker.queue(item);  // Can cause deadlock
}};

// ✅ GOOD: Use separate workers
siddiqsoft::simple_worker<Task> worker1{callback1};
siddiqsoft::simple_worker<Task> worker2{callback2};
// No circular dependencies
```

### Blocking Operations

Avoid blocking operations in callbacks:

```cpp
// ❌ BAD: Blocking operation
worker.queue([](auto&& item) {
    std::this_thread::sleep_for(std::chrono::seconds(10));  // Blocks thread
});

// ✅ GOOD: Non-blocking operation
worker.queue([](auto&& item) {
    item.process();  // Quick operation
});
```

### Lock Ordering

If using locks in callbacks, maintain consistent lock ordering:

```cpp
// ✅ GOOD: Consistent lock ordering
std::mutex lock1, lock2;

worker.queue([&](auto&& item) {
    std::scoped_lock l(lock1, lock2);  // Always same order
    // Process item
});
```

@section resource_exhaustion Resource Exhaustion Prevention

### Queue Depth Control

Control queue depth to prevent memory exhaustion:

```cpp
// ✅ GOOD: Limit queue depth
const size_t MAX_QUEUE_DEPTH = 1000;
size_t current_depth = 0;

for (auto& item : items) {
    if (current_depth < MAX_QUEUE_DEPTH) {
        worker.queue(std::move(item));
        current_depth++;
    } else {
        // Handle backpressure
        std::cerr << "Queue full, dropping item" << std::endl;
    }
}
```

@section exception_handling Exception Handling

### Callback Exceptions

Exceptions in callbacks are caught and logged:

```cpp
// Exceptions are caught by the library
worker.queue([](auto&& item) {
    throw std::runtime_error("Error");  // Caught and logged
});

// Thread continues after exception
worker.queue([](auto&& item) {
    // This still executes
});
```

### Critical Exceptions

Critical exceptions (std::bad_alloc, etc.) are handled specially:

```cpp
// Critical exceptions are identified
try {
    worker.queue(large_item);  // May throw std::bad_alloc
} catch (const std::bad_alloc& ex) {
    // Handle memory exhaustion
    std::cerr << "Out of memory" << std::endl;
}
```

@section thread_safety Thread Safety

### Queue Operations

All queue operations are thread-safe:

```cpp
// Safe to call from multiple threads
std::thread t1([&]() { worker.queue(item1); });
std::thread t2([&]() { worker.queue(item2); });
t1.join();
t2.join();
```

@section shutdown_safety Shutdown Safety

### Graceful Shutdown

Shutdown is graceful and safe:

```cpp
{
    siddiqsoft::simple_worker<Task> worker{callback};
    
    // Queue items
    for (int i = 0; i < 100; ++i) {
        worker.queue(item);
    }
    
    // Destructor waits for queue to empty
    // Then stops worker thread
    // Then joins thread
}  // All cleanup happens here
```

### Shutdown Timeout

Shutdown has a timeout to prevent indefinite hangs:

```cpp
// Shutdown waits up to 1000ms for queue to drain
// If timeout occurs, remaining items are abandoned
// Thread is still cleaned up properly
```

@section monitoring Monitoring and Diagnostics

### JSON Serialization

Get worker state as JSON:

```cpp
#include <nlohmann/json.hpp>

auto state = worker.toJson();
std::cout << state.dump(2) << std::endl;

// Output includes:
// - itemsSize: Current queue size
// - queueCounter: Total items queued
// - itemsQueued: Total items added
// - itemsPopped: Total items processed
// - itemsOutstanding: Items still in queue
// - outstandingCallback: Callbacks currently executing
```

### Monitoring Best Practices

```cpp
// ✅ GOOD: Monitor queue depth
auto state = worker.toJson();
auto queue_size = state["itemsSize"];

if (queue_size > THRESHOLD) {
    std::cerr << "Warning: Queue depth high" << std::endl;
}

// ✅ GOOD: Monitor callback execution
auto outstanding = state["outstandingCallback"];
if (outstanding > 0) {
    std::cerr << "Callbacks executing: " << outstanding << std::endl;
}
```

@section security_checklist Security Checklist

Before deploying asynchrony-based code:

- [ ] Callbacks handle exceptions properly
- [ ] Callbacks don't block indefinitely
- [ ] No circular dependencies between workers
- [ ] Queue depth is controlled
- [ ] Shutdown is tested
- [ ] Memory usage is monitored
- [ ] Thread count is appropriate
- [ ] Error handling is comprehensive
- [ ] Security review completed

@section vulnerability_reporting Vulnerability Reporting

If you discover a security vulnerability:

1. **Do not** open a public issue
2. Email security details to: github@siddiqsoft.com
3. Include:
   - Description of vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if available)

We will:
- Acknowledge receipt within 48 hours
- Investigate the vulnerability
- Develop and test a fix
- Release a patched version
- Credit the reporter (if desired)

@section security_resources Additional Resources

- @ref getting_started "Getting Started" - Installation and setup
- @ref usage_guide "Usage Guide" - Detailed usage examples
- @ref api "API Reference" - Complete API documentation

@section security_faq FAQ

**Q: Is the library safe for production use?**
A: Yes, the library is designed for production use with proper callback implementation.

**Q: Can callbacks be exploited?**
A: Callbacks are user-provided code. Implement them securely following the guidelines in this document.

**Q: What happens if a callback throws an exception?**
A: The exception is caught and logged. The worker thread continues processing.

**Q: Can the queue cause memory exhaustion?**
A: Yes, if you queue unlimited items. Control queue depth in your application code.

**Q: Is the library thread-safe?**
A: Yes, all public operations are thread-safe.

**Q: Can deadlock occur?**
A: Only if user code creates circular dependencies. Follow the guidelines to prevent this.

**Q: What about timing attacks?**
A: The library is not cryptographic, so timing attacks are not a concern.

**Q: How do I report a security issue?**
A: Email security details to github@siddiqsoft.com (do not open public issues).
