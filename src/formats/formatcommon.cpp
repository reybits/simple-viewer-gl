/////////////////////////////////////////////////
//
// Andrey A. Ugolnik
// andrey@ugolnik.info
//
/////////////////////////////////////////////////

#include "formatcommon.h"
#include <iostream>

CFormatCommon::CFormatCommon() : CFormat(), m_image(0) {
}

CFormatCommon::~CFormatCommon() {
	FreeMemory();
}

bool CFormatCommon::Load(const char* filename, int sub_image) {
	if(openFile(filename) == false) {
		return false;
	}
	fclose(m_file);

	// try to load image from disk
	Imlib_Load_Error error_return;
	m_image	= imlib_load_image_with_error_return(filename, &error_return);
	if(m_image == 0) {
		std::cout << ": error loading file '" << filename << "' (" << error_return << ")" << std::endl;
		return false;
	}

	imlib_context_set_image(m_image);

	m_width		= imlib_image_get_width();
	m_height	= imlib_image_get_height();
	m_bpp		= 32;	// Imlib2 always has 32-bit buffer, but sometimes alpha not used
	m_bppImage	= (imlib_image_has_alpha() == 1 ? 32 : 24);
	m_bitmap	= (unsigned char*)imlib_image_get_data_for_reading_only();

	return true;
}

void CFormatCommon::FreeMemory() {
	if(m_image != 0) {
		imlib_free_image();
		m_image	= 0;
	}
}
