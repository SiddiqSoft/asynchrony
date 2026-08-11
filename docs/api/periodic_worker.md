# Class Template `siddiqsoft::periodic_worker`

Header: `#include "siddiqsoft/periodic_worker.hpp"`

```cpp
template<int ThreadPriority = 0>
class periodic_worker;
```

Executes a user callback function periodically at fixed intervals.

---

## Member Functions

### Constructor

```cpp
template<typename Callback, typename Rep, typename Period>
periodic_worker(Callback&& callback,
                std::chrono::duration<Rep, Period> interval,
                std::string name = "");
```

- `callback`: Zero-argument callable `void()`.
- `interval`: `std::chrono::duration` specifying execution frequency.
- `name`: Optional diagnostic string name.

---

### Destructor

```cpp
~periodic_worker();
```

Stops the internal timer loop and joins the worker thread.
