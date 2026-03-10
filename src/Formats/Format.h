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
#include "Common/PixelFormat.h"

class cFile;
struct sCallbacks;
struct sChunkData;
struct sConfig;
struct sImageInfo;
struct sPreviewData;

class cFormat
{
public:
    virtual ~cFormat();

    void setConfig(const sConfig* config);

    virtual bool isSupported(cFile& file, Buffer& buffer) const = 0;

    bool Load(const char* filename, sChunkData& chunk, sImageInfo& info);
    bool LoadSubImage(uint32_t subImage, sChunkData& chunk, sImageInfo& info);

    void updateProgress(float percent);
    void signalImageInfo();
    void signalBitmapAllocated();
    void signalPreviewReady(sPreviewData&& preview);

    // Centralized bitmap setup: signals image info, allocates bitmap, signals viewer.
    // Caller must set chunk.width, chunk.height, info.bppImage before calling.
    void setupBitmap(sChunkData& chunk, sImageInfo& info, uint32_t bpp, ePixelFormat format, const char* formatName);

    virtual void stop()
    {
        m_stop = true;
    }

    virtual void dump(const sChunkData& chunk, const sImageInfo& info) const;

    double getDecodeMs() const { return m_decodeMs; }
    double getIccMs() const { return m_iccMs; }

    cFormat(sCallbacks* callbacks);

protected:
    bool openFile(cFile& file, const char* filename, sImageInfo& info) const;
    bool readBuffer(cFile& file, Buffer& buffer, uint32_t minSize) const;
    bool applyIccProfile(sChunkData& chunk, const void* iccProfile, uint32_t iccProfileSize);
    bool applyIccProfile(sChunkData& chunk, const float* chr, const float* wp, const uint16_t* gmr, const uint16_t* gmg, const uint16_t* gmb);

    virtual bool LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info) = 0;
    virtual bool LoadSubImageImpl(uint32_t /*subImage*/, sChunkData& /*chunk*/, sImageInfo& /*info*/)
    {
        return false;
    }

private:
    sCallbacks* m_callbacks;
    sChunkData* m_chunk = nullptr;
    sImageInfo* m_info = nullptr;
    double m_decodeMs = 0.0;
    double m_iccMs = 0.0;

protected:
    const sConfig* m_config = nullptr;
    bool m_stop = false;
};
