/**********************************************\
*
*  Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

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

private:
    static void Write(Severity type, const std::string& msg);
};
