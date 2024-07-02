#include <stdint.h>
#include "bmjpuapi_parse_jpeg.h"
#include "bmjpuapi_priv.h"


/* Start Of Frame markers, non-differential, Huffman coding */
#define SOF0      0xc0  /* Baseline DCT */
#define SOF1      0xc1  /* Extended sequential DCT */
#define SOF2      0xc2  /* Progressive DCT */
#define SOF3      0xc3  /* Lossless */

/* Start Of Frame markers, differential, Huffman coding */
#define SOF5      0xc5
#define SOF6      0xc6
#define SOF7      0xc7

/* Start Of Frame markers, non-differential, arithmetic coding */
#define JPG       0xc8  /* Reserved */
#define SOF9      0xc9
#define SOF10     0xca
#define SOF11     0xcb

/* Start Of Frame markers, differential, arithmetic coding */
#define SOF13     0xcd
#define SOF14     0xce
#define SOF15     0xcf

/* Restart interval termination */
#define RST0      0xd0  /* Restart ... */
#define RST1      0xd1
#define RST2      0xd2
#define RST3      0xd3
#define RST4      0xd4
#define RST5      0xd5
#define RST6      0xd6
#define RST7      0xd7

#define SOI       0xd8  /* Start of image */
#define EOI       0xd9  /* End Of Image */
#define SOS       0xda  /* Start Of Scan */

#define DHT       0xc4  /* Huffman Table(s) */
#define DAC       0xcc  /* Algorithmic Coding Table */
#define DQT       0xdb  /* Quantisation Table(s) */
#define DNL       0xdc  /* Number of lines */
#define DRI       0xdd  /* Restart Interval */
#define DHP       0xde  /* Hierarchical progression */
#define EXP       0xdf

#define APP0      0xe0  /* Application marker */
#define APP1      0xe1
#define APP2      0xe2
#define APP13     0xed
#define APP14     0xee
#define APP15     0xef

#define JPG0      0xf0  /* Reserved ... */
#define JPG13     0xfd
#define COM       0xfe  /* Comment */

#define TEM       0x01

#define MAX_MJPG_PIC_WIDTH  32768 /* please refer to jpuconfig.h */
#define MAX_MJPG_PIC_HEIGHT 32768 /* please refer to jpuconfig.h */

static int find_next_marker(uint8_t ** jpeg_data_cur, uint8_t * jpeg_data_end)
{
    int word = 0x0;
    uint8_t * p;
    for(;;)
    {
        p = *jpeg_data_cur;
        if(( p+2 ) >= jpeg_data_end)
            return 0;
        word = *p++ << 8;
        word |= *p++;
        if((word > 0xFF00) &&(word < 0xFFFF))
            break;
        ++(*jpeg_data_cur);
    }
    return word;
}
int bm_jpu_parse_jpeg_header(void *jpeg_data, size_t jpeg_data_size,
                             unsigned int *width, unsigned int *height,
                             BmJpuColorFormat *color_format)
{
    uint8_t *jpeg_data_start = jpeg_data;
    uint8_t *jpeg_data_end = jpeg_data_start + jpeg_data_size;
    uint8_t *jpeg_data_cur = jpeg_data_start;
    int found_info = 0;

#define READ_UINT8(value) do \
    { \
        (value) = *jpeg_data_cur; \
        ++jpeg_data_cur; \
    } \
    while (0)
#define READ_UINT16(value) do \
    { \
        uint16_t w = *((uint16_t *)jpeg_data_cur); \
        jpeg_data_cur += 2; \
        (value) = ( ((w & 0xff) << 8) | ((w & 0xff00) >> 8) ); \
    } \
    while (0)

    while (jpeg_data_cur < jpeg_data_end)
    {
        uint8_t marker;

        /* Marker is preceded by byte 0xFF */
        if (*(jpeg_data_cur++) != 0xff)
        {
            if(find_next_marker(&jpeg_data_cur, jpeg_data_end) == 0)
            {
                break;
            }
            jpeg_data_cur++;
        }
        READ_UINT8(marker);
        if (marker == SOS)
        {
            uint16_t length;
            uint8_t num_components;

            READ_UINT16(length);
            if (jpeg_data_cur + length - 2 >= jpeg_data_end)
            {
                BM_JPU_ERROR("Marker SOS has not sufficient data");
                return 0;
            }

            READ_UINT8(num_components); // read for components number
            if (num_components > 3)
            {
                BM_JPU_ERROR("JPEGs with %d components are not supported", (int)num_components);
                return 0;
            }

            for (int i = 0; i < num_components; i++)
            {
                uint8_t b;
                ++jpeg_data_cur;
                READ_UINT8(b);
                if (((b & 0xf0) > 0x10) || ((b & 0xf) > 0x1))
                {
                    BM_JPU_ERROR("only baseline JPEGs are supported");
                    return 0;
                }
            }

            break;
        }

        switch (marker)
        {
            case SOI:
                break;
            case DRI:
                jpeg_data_cur += 4;
                break;
            case SOF2:
            {
                BM_JPU_ERROR("progressive JPEGs are not supported");
                return 0;
            }

            case SOF0:
            case SOF1:
            {
                uint16_t length;
                uint8_t precision;
                uint8_t num_components;
                uint8_t block_width[3], block_height[3];

                READ_UINT16(length);
                length -= 2;
                BM_JPU_LOG("marker: %#lx length: %u", (unsigned long long)marker, length);

                READ_UINT8(precision);
                if (precision != 8)
                {
                    BM_JPU_ERROR("JPEG with data precision %u are not supported", precision);
                    return 0;
                }

                READ_UINT16(*height);
                READ_UINT16(*width);

                if ((*width) > MAX_MJPG_PIC_WIDTH)
                {
                    BM_JPU_ERROR("width of %u pixels exceeds the maximum of %d",
                                 *width, MAX_MJPG_PIC_WIDTH);
                    return 0;
                }

                if ((*height) > MAX_MJPG_PIC_HEIGHT)
                {
                    BM_JPU_ERROR("height of %u pixels exceeds the maximum of %d",
                                 *height, MAX_MJPG_PIC_HEIGHT);
                    return 0;
                }

                READ_UINT8(num_components);

                if (num_components <= 3)
                {
                    for (int i = 0; i < num_components; ++i)
                    {
                        uint8_t b;
                        ++jpeg_data_cur;
                        READ_UINT8(b);
                        block_width[i] = (b & 0xf0) >> 4;
                        block_height[i] = (b & 0x0f);
                        ++jpeg_data_cur;
                    }
                }

                if (num_components > 3)
                {
                    BM_JPU_ERROR("JPEGs with %d components are not supported", (int)num_components);
                    return 0;
                }
                if (num_components == 3)
                {
                    int temp = (block_width[0] * block_height[0]) / (block_width[1] * block_height[1]);

                    if ((temp == 4) && (block_height[0] == 2))
                        *color_format = BM_JPU_COLOR_FORMAT_YUV420;
                    else if ((temp == 2) && (block_height[0] == 1))
                        *color_format = BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL;
                    else if ((temp == 2) && (block_height[0] == 2))
                        *color_format = BM_JPU_COLOR_FORMAT_YUV422_VERTICAL;
                    else if ((temp == 1) && (block_height[0] == 1))
                        *color_format = BM_JPU_COLOR_FORMAT_YUV444;
                    else
                        *color_format = BM_JPU_COLOR_FORMAT_YUV420;
                }
                else
                    *color_format = BM_JPU_COLOR_FORMAT_YUV400;

                BM_JPU_LOG("width: %u  height: %u  number of components: %d", *width, *height, (int)num_components);

                found_info = 1;

                break;
            }

            default:
            {
                uint16_t length;
                READ_UINT16(length);
                length -= 2;
                BM_JPU_LOG("marker: %#lx length: %u", (unsigned long long)marker, length);
                jpeg_data_cur += length;
            }
        }
    }

    return found_info;
}
