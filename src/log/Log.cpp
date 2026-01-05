/**********************************************\
*
*  Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "Log.h"

#include <cstdio>

void cLog::Write(Severity severity, const std::string& msg)
{
    static constexpr const char* SeverityNames[] = {
        "",
        "(WW) ",
        "(EE) ",
        "(DD) ",
    };

    auto idx = static_cast<size_t>(severity);
    ::printf("%s%s\n", SeverityNames[idx], msg.c_str());
}
