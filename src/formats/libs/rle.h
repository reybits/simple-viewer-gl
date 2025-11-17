/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

class cRLE final
{
public:
    cRLE();
    ~cRLE();

    // returns 0 if error occured
    // or length in bytes of encoded data in rle
    unsigned encode(const unsigned char* in, unsigned in_size, unsigned char* const rle, unsigned rle_size);

    // returns 0 if error occured
    // or length in unsigned int of encoded data in rle
    unsigned encodeBy4(const unsigned* in, unsigned in_size, unsigned* const rle, unsigned rle_size);

    // returns 0 if error occured
    // or length in bytes of decoded data in out
    unsigned decode(const unsigned char* rle, unsigned rle_size, unsigned char* const out, unsigned out_size);

    // returns 0 if error occured
    // or length in unsigned int of decoded data in out
    unsigned decodeBy4(const unsigned* rle, unsigned rle_size, unsigned* const out, unsigned out_size);

    // returns desired buffer size if error occured
    unsigned getDesiredSize() const
    {
        return m_desired_size;
    }

private:
    unsigned m_desired_size;
};
