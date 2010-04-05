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

#define WIDTHBYTES(bits) ((((bits) + 31) / 32) * 4)

class CFormat {
	friend class CImageLoader;

public:
	CFormat();
	virtual ~CFormat();

	virtual bool Load(const char* filename, int sub_image = 0) = 0;
	virtual void FreeMemory() = 0;

protected:
	FILE* m_file;
	unsigned char* m_bitmap;
	int m_width, m_height, m_pitch;	// width, height, row pitch of image in buffer
	int m_bpp;						// bit per pixel of image in buffer
	int m_bppImage;					// bit per pixel of original image
	long m_size;					// file size on disk
	std::string m_info;				// additional info, such as EXIF

protected:
	bool openFile(const char* path);
	void convertRGB2BGR();

private:
};

#endif // FORMAT_H
