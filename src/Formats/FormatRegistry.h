/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Common/Buffer.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class cFile;
class cFormat;
struct sCallbacks;
struct sConfig;

struct sFormatEntry
{
    const char* name;

    // Simple magic-based detection (nullptr if custom probe needed)
    const uint8_t* magic;
    uint32_t magicSize;
    uint32_t magicOffset;

    // Custom probe for complex detection (called if magic is nullptr)
    using ProbeFunc = bool (*)(cFile& file, Buffer& buffer, const uint8_t* data, uint32_t dataSize, uint64_t fileSize);
    ProbeFunc probe;

    // Creates the format reader
    using FactoryFunc = std::unique_ptr<cFormat> (*)(sCallbacks*);
    FactoryFunc factory;

    // Minimum buffer size needed for detection
    uint32_t minProbeSize;
};

namespace FormatRegistry
{
    const std::vector<sFormatEntry>& getRegistry();
    const sFormatEntry* detect(cFile& file, Buffer& buffer);

} // namespace FormatRegistry
