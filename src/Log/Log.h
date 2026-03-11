/**********************************************\
*
*  Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include <cstdarg>
#include <fmt/core.h>

class cLog
{
public:
    enum class Severity
    {
        Info,
        Warning,
        Error,
        Debug,
    };

    // -------------------------------------------------------------------------

    template <typename... Args>
    static void Write(Severity type, fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        Write(type, msg);
    }

    template <typename... Args>
    static void Info(fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        Write(Severity::Info, msg);
    }

    template <typename... Args>
    static void Warning(fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        Write(Severity::Warning, msg);
    }

    template <typename... Args>
    static void Error(fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        Write(Severity::Error, msg);
    }

    template <typename... Args>
    static void Debug(fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        Write(Severity::Debug, msg);
    }

    static void WriteV(Severity type, const char* format, va_list args);

public:
    static void setDebugEnabled(bool enabled);

private:
    static void Write(Severity type, const std::string& msg);
};
