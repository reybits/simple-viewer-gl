/////////////////////////////////////////////////
//
// Andrey A. Ugolnik
// andrey@ugolnik.info
//
/////////////////////////////////////////////////

#ifndef FORMAT_H
#define FORMAT_H

#include <string>
#include <iostream>
#include <stdio.h>
#include <GL/gl.h>

//#define WIDTHBYTES(bits) ((((bits) + 31) / 32) * 4)

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

typedef void (*Callback)(int);

class CFormat {
	friend class CImageLoader;

public:
	CFormat(Callback callback = 0);
	virtual ~CFormat();

	virtual bool Load(const char* filename, int subImage = 0) = 0;
	virtual void FreeMemory() = 0;

private:
	Callback m_callback;
	int m_percent;

protected:
	FILE* m_file;
	unsigned char* m_bitmap;
	GLenum m_format;
	int m_width, m_height, m_pitch;	// width, height, row pitch of image in buffer
	int m_bpp;						// bit per pixel of image in buffer
	int m_bppImage;					// bit per pixel of original image
	long m_size;					// file size on disk
	size_t m_sizeMem;				// used memory
	int m_subImage;
	int m_subCount;
	std::string m_info;				// additional info, such as EXIF

protected:
	bool openFile(const char* path);
	void progress(int percent);
	void reset();
	uint16 read_uint16(uint8* p);
	uint32 read_uint32(uint8* p);
};

#endif // FORMAT_H
