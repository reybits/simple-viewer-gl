/**********************************************\
*
*  Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "Log.h"

#include <cstdio>

namespace
{
    bool DebugEnabled = false;
}

void cLog::setDebugEnabled(bool enabled)
{
    DebugEnabled = enabled;
}

void cLog::WriteV(Severity severity, const char* format, va_list args)
{
    char buf[1024];
    ::vsnprintf(buf, sizeof(buf), format, args);
    Write(severity, std::string(buf));
}

void cLog::Write(Severity severity, const std::string& msg)
{
    if (severity == Severity::Debug && DebugEnabled == false)
    {
        return;
    }

    static constexpr const char* SeverityNames[] = {
        "",
        "(WW) ",
        "(EE) ",
        "(DD) ",
    };

    auto idx = static_cast<size_t>(severity);
    // Diagnostics (warnings/errors) go to stderr; informational output to stdout.
    FILE* out = (severity == Severity::Warning || severity == Severity::Error)
        ? stderr
        : stdout;
    ::fprintf(out, "%s%s\n", SeverityNames[idx], msg.c_str());
    // Flush each line: keep stdout/stderr ordered when merged and avoid loss on abort.
    ::fflush(out);
}
