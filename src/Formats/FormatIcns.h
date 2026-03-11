/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Format.h"

class cFormatIcns final : public cFormat
{
public:
    using cFormat::cFormat;

    bool isSupported(cFile& file, Buffer& buffer) const override;

    enum class Type
    {
        TOC_,
        ICON,
        ICN3,
        icm3,
        icm4,
        icm8,
        ics3,
        ics4,
        ics8,
        is32,
        s8mk,
        icl4,
        icl8,
        il32,
        l8mk,
        ich3,
        ich4,
        ich8,
        ih32,
        h8mk,
        it32,
        t8mk,
        icp4,
        icp5,
        icp6,
        ic07,
        ic08,
        ic09,
        ic10,
        ic11,
        ic12,
        ic13,
        ic14,

        icnV,
        name,
        info,

        Count
    };

    enum class Compression : uint32_t
    {
        None,
        Pack,
        PngJ,

        Count
    };

    struct Entry
    {
        Type type;

        Compression compression;
        uint32_t srcBpp;
        uint32_t dstBpp;
        uint32_t iconSize;

        uint32_t offset;
        uint32_t size;
    };

    struct Chunk
    {
        uint8_t type[4];    // Icon type, see OSType below.
        uint8_t dataLen[4]; // Length of data, in bytes (including type and length), msb first
        // Variable Icon data
    };

private:
    bool LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info) override;
    bool LoadSubImageImpl(uint32_t current, sChunkData& chunk, sImageInfo& info) override;

    bool load(uint32_t current, sChunkData& chunk, sImageInfo& info);
    void iterateContent(const uint8_t* icon, uint32_t offset, uint32_t size);
    const Entry& getDescription(const Chunk& chunk) const;

    std::vector<uint8_t> m_icon;
    std::vector<Entry> m_entries;
};
