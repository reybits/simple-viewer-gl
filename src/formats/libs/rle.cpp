/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "rle.h"

#include <algorithm>
#include <cstdio>

cRLE::cRLE()
    : m_desired_size(0)
{
}

cRLE::~cRLE()
{
}

unsigned cRLE::encode(const unsigned char* in, unsigned in_size, unsigned char* const rle, unsigned rle_size)
{
    //printf("--------------------------\n");
    //printf("Encode...\n");

    unsigned rle_pos = 0;
    for (unsigned i = 0; i < in_size; i++)
    {
        unsigned char symbol = in[i];
        unsigned count = 1;

        const unsigned length = std::min<unsigned>(i + 0x7f, in_size);
        for (unsigned a = i + 1; a < length; a++)
        {
            if (in[a] != symbol)
            {
                break;
            }
            count++;
            i++;
        }

        if (count == 1)
        {
            symbol = in[i + 1];
            const unsigned length = std::min<unsigned>(i + 0x7f + 1, in_size);
            for (unsigned a = i + 2; a < length; a++, count++)
            {
                if (in[a] == symbol)
                {
                    break;
                }
                symbol = in[a];
            }

            m_desired_size = rle_pos + count + 1;
            if (m_desired_size > rle_size)
            {
                //printf("no room for copy: required %u, allowed: %u\n", m_desired_size, rle_size);
                return 0;
            }

            //printf("raw %u symbols...\n", count);

            rle[rle_pos++] = 0x80 + count;
            for (unsigned a = i, copy = 0; copy < count; a++, copy++)
            {
                rle[rle_pos++] = in[a];
            }
            i += count - 1;
        }
        else
        {
            m_desired_size = rle_pos + 2;
            if (m_desired_size > rle_size)
            {
                //printf("no room for clone: required %u, allowed: %u\n", m_desired_size, rle_size);
                return 0;
            }

            //printf("rle %c %u times...\n", (char)symbol, count);
            rle[rle_pos++] = count;
            rle[rle_pos++] = symbol;
        }
    }

    //printf("\n");

    m_desired_size = rle_pos;

    return rle_pos;
}

static const unsigned half = (1 << sizeof(unsigned) * 8 / 2) - 1;

unsigned cRLE::encodeBy4(const unsigned* in, unsigned in_size, unsigned* const rle, unsigned rle_size)
{
    //printf("--------------------------\n");
    //printf("Encode...\n");

    unsigned rle_pos = 0;

    for (unsigned i = 0; i < in_size; i++)
    {
        unsigned symbol = in[i];
        unsigned count = 1;

        const unsigned length = std::min<unsigned>(i + half - 1, in_size);
        for (unsigned a = i + 1; a < length; a++)
        {
            if (in[a] != symbol)
            {
                break;
            }
            count++;
            i++;
        }

        if (count == 1)
        {
            symbol = in[i + 1];
            const unsigned length = std::min<unsigned>(i + half, in_size);
            for (unsigned a = i + 2; a < length; a++, count++)
            {
                if (in[a] == symbol)
                {
                    break;
                }
                symbol = in[a];
            }

            m_desired_size = rle_pos + count + 1;
            if (m_desired_size > rle_size)
            {
                //printf("no room for copy: required %u, allowed: %u\n", m_desired_size, rle_size);
                return 0;
            }

            //printf("raw %u quads...\n", count);

            rle[rle_pos++] = half + count;
            for (unsigned a = i, copy = 0; copy < count; a++, copy++)
            {
                rle[rle_pos++] = in[a];
            }
            i += count - 1;
        }
        else
        {
            m_desired_size = rle_pos + 2;
            if (m_desired_size > rle_size)
            {
                //printf("no room for clone: required %u, allowed: %u\n", m_desired_size, rle_size);
                return 0;
            }

            //printf("rle 0x%x %u times...\n", symbol, count);
            rle[rle_pos++] = count;
            rle[rle_pos++] = symbol;
        }
    }

    //printf("\n");

    m_desired_size = rle_pos;

    return rle_pos;
}

unsigned cRLE::decode(const unsigned char* rle, unsigned rle_size, unsigned char* const out, unsigned out_size)
{
    //printf("--------------------------\n");
    //printf("decode...\n");

    unsigned out_pos = 0;
    for (unsigned i = 0; i < rle_size;)
    {
        unsigned char symbol = rle[i];
        if (symbol <= 0x7f)
        {
            const unsigned count = (unsigned)symbol;
            m_desired_size = out_pos + count;
            if (m_desired_size > out_size)
            {
                //printf("no room: required %u, allowed: %u\n", m_desired_size, out_size);
                return 0;
            }

            const unsigned char clone = rle[++i];
            //printf("cloning %c %u times...\n", clone, count);
            for (unsigned a = 0; a < count; a++)
            {
                out[out_pos++] = clone;
            }
            i++;
        }
        else
        {
            const unsigned count = (unsigned)symbol - 0x80;
            m_desired_size = out_pos + count;
            if (m_desired_size > out_size)
            {
                //printf("no room: required %u, allowed: %u\n", m_desired_size, out_size);
                return 0;
            }

            i++;
            //printf("raw %u symbols (%u)...\n", count, symbol);
            for (unsigned a = 0; a < count; a++)
            {
                out[out_pos++] = rle[i];
                i++;
            }
        }
    }

    //printf("\n");

    m_desired_size = out_pos;

    return out_pos;
}

unsigned cRLE::decodeBy4(const unsigned* rle, unsigned rle_size, unsigned* const out, unsigned out_size)
{
    //printf("--------------------------\n");
    //printf("decode...\n");

    unsigned out_pos = 0;
    for (unsigned i = 0; i < rle_size;)
    {
        unsigned symbol = rle[i];
        if (symbol <= half - 1)
        {
            const unsigned count = symbol;
            m_desired_size = out_pos + count;
            if (m_desired_size > out_size)
            {
                //printf("no room: required %u, allowed: %u\n", m_desired_size, out_size);
                return 0;
            }

            const unsigned clone = rle[++i];
            //printf("cloning %c %u times...\n", clone, count);
            for (unsigned a = 0; a < count; a++)
            {
                out[out_pos++] = clone;
            }
            i++;
        }
        else
        {
            const unsigned count = symbol - half;
            m_desired_size = out_pos + count;
            if (m_desired_size > out_size)
            {
                //printf("no room: required %u, allowed: %u\n", m_desired_size, out_size);
                return 0;
            }

            i++;
            //printf("raw %u symbols (%u)...\n", count, symbol);
            for (unsigned a = 0; a < count; a++)
            {
                out[out_pos++] = rle[i];
                i++;
            }
        }
    }

    //printf("\n");

    m_desired_size = out_pos;

    return out_pos;
}
