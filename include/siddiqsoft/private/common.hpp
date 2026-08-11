#pragma once
#ifndef ASYNCHRONY_COMMON_HPP
#define ASYNCHRONY_COMMON_HPP

#include <exception>


namespace siddiqsoft
{
    /**
     * @brief Helper function to determine if an exception is critical and should be rethrown
     *
     * This utility function examines an exception_ptr and determines whether the exception
     * represents a critical error that indicates the system is in an unstable state.
     * Critical exceptions should typically be rethrown or cause immediate shutdown,
     * while non-critical exceptions can often be logged and handled gracefully.
     *
     * @param ep The exception pointer to check
     * @return true if the exception is critical and should be rethrown, false otherwise
     *
     * @details Critical exceptions include:
     * - std::bad_alloc: Memory allocation failure - indicates system resource exhaustion
     * - std::bad_exception: Unexpected exception type - indicates exception handling failure
     * - std::bad_cast: Invalid dynamic_cast - indicates type system corruption
     * - std::bad_typeid: Invalid typeid operation - indicates type system corruption
     * - Unknown exceptions (catch-all): Treated as critical for safety
     *
     * Non-critical exceptions:
     * - std::exception and derived classes (except those listed above)
     * - Regular application exceptions that can be handled gracefully
     *
     * @example
     * @code
     * try {
     *     // Some operation that might throw
     *     riskyOperation();
     * }
     * catch (...) {
     *     auto ep = std::current_exception();
     *     if (isCriticalException(ep)) {
     *         // System is unstable, shutdown
     *         std::rethrow_exception(ep);
     *     } else {
     *         // Log and continue
     *         std::println(std::cerr, "Non-critical exception occurred");
     *     }
     * }
     * @endcode
     *
     * @note This function rethrows the exception internally to examine its type,
     *       so it should only be called when you have an active exception context
     *       or when you're prepared to handle the rethrow.
     */
    static bool isCriticalException(const std::exception_ptr& ep)
    {
        if (!ep) return false;

        try {
            std::rethrow_exception(ep);
        }
        catch (const std::bad_alloc&) {
            // Memory allocation failure - critical
            return true;
        }
        catch (const std::bad_exception&) {
            // Bad exception - critical
            return true;
        }
        catch (const std::bad_cast&) {
            // Bad cast - critical
            return true;
        }
        catch (const std::bad_typeid&) {
            // Bad typeid - critical
            return true;
        }
        catch (const std::exception&) {
            // Regular exception - not critical
            return false;
        }
        catch (...) {
            // Unknown exception - treat as critical
            return true;
        }
    }
} // namespace siddiqsoft

#endif // !ASYNCHRONY_COMMON_HPP
