# Round-Robin Pool

The `roundrobin_pool` is a high-throughput multi-threaded worker pool that uses per-thread queues and distributes incoming work items across threads via round-robin indexing.

## Features

- **Per-Thread Queues**: Reduces lock contention significantly compared to a single shared queue pool.
- **Round-Robin Scheduling**: Distributes incoming tasks evenly across worker thread queues.
- **High Throughput**: Ideal for concurrent task submission scenarios and I/O heavy workloads.
- **Configurable Thread Count**: Defaults to `std::thread::hardware_concurrency()`.
- **RAII Lifespan**: Clean destruction draining all thread queues.

---

## Basic Usage Example

```cpp
#include "siddiqsoft/roundrobin_pool.hpp"
#include <iostream>
#include <chrono>

struct PacketTask {
    uint32_t packetId;
    std::string data;
};

int main() {
    // Create pool with per-thread queues
    siddiqsoft::roundrobin_pool<PacketTask> pool{
        [](auto&& packet) {
            // Process packet in background thread
            std::cout << "Processed packet #" << packet.packetId << std::endl;
        }
    };

    // Rapidly queue 10,000 tasks
    for (uint32_t i = 0; i < 10000; ++i) {
        pool.queue(PacketTask{i, "payload_data"});
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}
```

---

## Comparison: `simple_pool` vs `roundrobin_pool`

| Feature | `simple_pool` | `roundrobin_pool` |
| :--- | :--- | :--- |
| **Queue Architecture** | Single shared queue | Dedicated per-thread queue |
| **Lock Contention** | Higher under extreme queue push rates | Extremely low |
| **Load Balancing** | Automatic dynamic pulling | Round-robin distribution |
| **Best Suited For** | Uniform duration tasks | High frequency push / variable duration tasks |
