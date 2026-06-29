/**
 * @page getting_started Getting Started
 *
 * @section installation Installation
 *
 * ### Using CMake (Recommended)
 *
 * Add the library to your CMakeLists.txt:
 *
 * ```cmake
 * include(FetchContent)
 * FetchContent_Declare(asynchrony
 *     GIT_REPOSITORY https://github.com/SiddiqSoft/asynchrony.git
 *     GIT_TAG main
 * )
 * FetchContent_MakeAvailable(asynchrony)
 *
 * target_link_libraries(your_target PRIVATE asynchrony::asynchrony)
 * ```
 *
 * ### Using NuGet (Windows)
 *
 * ```bash
 * nuget install SiddiqSoft.asynchrony
 * ```
 *
 * @section compiler_setup Compiler Setup
 *
 * #### Visual Studio 2022
 *
 * - Set C++ Language Standard to `/std:c++latest`
 * - No additional flags required
 *
 * #### GCC 14+
 *
 * ```bash
 * g++ -std=c++23 -pthread your_file.cpp
 * ```
 *
 * #### Clang 18+
 *
 * ```bash
 * clang++ -std=c++23 -fexperimental-library -pthread your_file.cpp
 * ```
 *
 * @section first_program Your First Program
 *
 * Create a simple program that uses the asynchrony library:
 *
 * ```cpp
 * #include <iostream>
 * #include <chrono>
 * #include "siddiqsoft/simple_worker.hpp"
 *
 * struct PrintTask {
 *     std::string message;
 *     
 *     void operator()() {
 *         std::cout << "Processing: " << message << std::endl;
 *     }
 * };
 *
 * int main() {
 *     // Create a worker with a callback
 *     siddiqsoft::simple_worker<PrintTask> worker{
 *         [](auto&& task) {
 *             task();  // Execute the task
 *         }
 *     };
 *
 *     // Queue some work
 *     for (int i = 0; i < 5; ++i) {
 *         worker.queue(PrintTask{"Task " + std::to_string(i)});
 *     }
 *
 *     // Wait for processing
 *     std::this_thread::sleep_for(std::chrono::seconds(1));
 *
 *     std::cout << "Done!" << std::endl;
 *     return 0;
 * }
 * ```
 *
 * @section common_patterns Common Patterns
 *
 * ### Pattern 1: Fire and Forget
 *
 * Queue work and let it process asynchronously:
 *
 * ```cpp
 * siddiqsoft::simple_worker<Task> worker{callback};
 * worker.queue(std::move(task));
 * // Continue without waiting
 * ```
 *
 * ### Pattern 2: Batch Processing
 *
 * Process multiple items in parallel:
 *
 * ```cpp
 * siddiqsoft::simple_pool<Task> pool{callback};
 * for (auto& item : items) {
 *     pool.queue(std::move(item));
 * }
 * ```
 *
 * ### Pattern 3: Periodic Tasks
 *
 * Execute a function at regular intervals:
 *
 * ```cpp
 * siddiqsoft::periodic_worker<> timer{
 *     []() { std::cout << "Tick!" << std::endl; },
 *     std::chrono::milliseconds(1000)
 * };
 * ```
 *
 * @section troubleshooting Troubleshooting
 *
 * ### Compilation Errors
 *
 * **Error**: `'jthread' is not a member of 'std'`
 * - **Solution**: Ensure you're using C++23 or later. Update your compiler flags.
 *
 * **Error**: `undefined reference to pthread_*`
 * - **Solution**: Link against pthread library: `-pthread` flag or `target_link_libraries(... pthread)`
 *
 * ### Runtime Issues
 *
 * **Issue**: Tasks not executing
 * - **Solution**: Ensure the worker/pool object is not destroyed before tasks complete
 * - **Solution**: Check that the callback function is valid and doesn't throw exceptions
 *
 * **Issue**: High CPU usage
 * - **Solution**: Increase the wait timeout in the worker configuration
 * - **Solution**: Reduce the number of threads in the pool
 *
 * @section next_steps Next Steps
 *
 * - Read the @ref usage_guide for detailed usage examples
 * - Check the @ref api_reference for complete API documentation
 * - Explore the @ref examples for more complex scenarios
 */
