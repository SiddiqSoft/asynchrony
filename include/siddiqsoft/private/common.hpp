#pragma once
#ifndef ASYNCHRONY_COMMON_HPP
#define ASYNCHRONY_COMMON_HPP

#include <exception>


namespace siddiqsoft
{
    /// @brief Helper function to determine if an exception is critical and should be rethrown
    /// @details Critical exceptions include memory allocation failures and other fatal errors
    /// that indicate the system is in an unstable state
    /// @param ep The exception pointer to check
    /// @return true if the exception is critical and should be rethrown, false otherwise
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