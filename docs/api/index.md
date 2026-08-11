# API Reference Overview

The `siddiqsoft::asynchrony` library is organized into header-only class templates in the `siddiqsoft` namespace.

## API Navigation

<div class="card-grid">
  <div class="card">
    <h3>siddiqsoft::simple_worker</h3>
    <p>Single-threaded asynchronous worker class template.</p>
    <a href="simple_worker.md">View Class Reference &rarr;</a>
  </div>
  <div class="card">
    <h3>siddiqsoft::simple_pool</h3>
    <p>Multi-threaded pool with a shared task queue.</p>
    <a href="simple_pool.md">View Class Reference &rarr;</a>
  </div>
  <div class="card">
    <h3>siddiqsoft::roundrobin_pool</h3>
    <p>Multi-threaded pool with per-thread task queues.</p>
    <a href="roundrobin_pool.md">View Class Reference &rarr;</a>
  </div>
  <div class="card">
    <h3>siddiqsoft::periodic_worker</h3>
    <p>Periodic interval timer worker class template.</p>
    <a href="periodic_worker.md">View Class Reference &rarr;</a>
  </div>
</div>

---

## Common Concepts & Conventions

- **Move Semantics**: All queue methods accept rvalue references `T&& item`.
- **Thread Safety**: Method invocations on `queue()`, `shutdown()`, `forceCleanupTerminate()`, and `to_json()` are thread-safe.
- **Exceptions**: Callback exceptions are caught locally without terminating background worker threads.
- **Diagnostics**: `.to_json()` methods return `nlohmann::json` objects containing snapshot telemetry.
