# Integration & Setup Guide

This guide describes how to integrate the **asynchrony** header-only C++23 library into your build systems and projects.

---

## 1. CMake Integration

### Method A: FetchContent (Recommended)

Add the following to your `CMakeLists.txt`:

```cmake
include(FetchContent)

FetchContent_Declare(
    asynchrony
    GIT_REPOSITORY https://github.com/SiddiqSoft/asynchrony.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(asynchrony)

# Link to your target
target_link_libraries(your_target_name PRIVATE asynchrony::asynchrony)
```

### Method B: CPM.cmake

If your project uses [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake):

```cmake
CPMAddPackage(
    NAME asynchrony
    GITHUB_REPOSITORY SiddiqSoft/asynchrony
    GIT_TAG main
)

target_link_libraries(your_target_name PRIVATE asynchrony::asynchrony)
```

### Method C: Git Submodule

If you keep dependencies in a `third_party` or `vendor` directory:

```bash
git submodule add https://github.com/SiddiqSoft/asynchrony.git third_party/asynchrony
```

Then in your `CMakeLists.txt`:

```cmake
add_subdirectory(third_party/asynchrony)
target_link_libraries(your_target_name PRIVATE asynchrony::asynchrony)
```

---

## 2. Windows NuGet Package

For Visual Studio C++ projects, **asynchrony** is available on NuGet:

=== "Package Manager Console"

    ```cmd
    Install-Package SiddiqSoft.asynchrony
    ```

=== "dotnet CLI"

    ```bash
    dotnet add package SiddiqSoft.asynchrony
    ```

The NuGet package automatically imports header paths and target properties into your Visual Studio `.vcxproj` build.

---

## 3. Manual Integration (Header-Only)

Since **asynchrony** is a header-only library:

1. Copy the `include/siddiqsoft` directory into your project's include path.
2. Add `#include "siddiqsoft/simple_worker.hpp"` (or desired component headers) in your C++ code.

---

## 4. Compiler Flags & C++23 Requirements

The library requires C++23 standard support (`std::jthread`, `std::stop_token`, `std::semaphore`).

| Compiler | Command Line Flags / Settings |
| :--- | :--- |
| **MSVC 16.11+** | `/std:c++23` or `/std:c++latest` |
| **GCC 10+** | `-std=c++23 -pthread` |
| **Clang 10+** | `-std=c++23 -fexperimental-library -pthread` |

!!! note "Clang Note"
    When compiling with Clang 10 through Clang 15, `-fexperimental-library` is required to enable standard library `<concepts>` and `std::jthread` features.
