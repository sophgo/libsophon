/**
 * Copyright (C) 2018 Solan Shang
 * Copyright (C) 2014 Carlos Rafael Giani
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */
/**
 * This is a example of how to encode JPEGs with the bmjpuapi library.
 * It reads the given raw YUV file and configures the JPU to encode MJPEG data.
 * Then, the encoded JPEG data is written to the output file.
 *
 * Note that using the JPEG encoder is optional, and it is perfectly OK to use
 * the lower-level video encoder API for JPEGs as well. (In fact, this is what
 * the JPEG encoder does internally.) The JPEG encoder is considerably easier
 * to use and requires less boilerplate code, however.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#include <time.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <stdarg.h>
#include "jpeg_common.h"
#include "bmjpurun.h"

static int parse_args(int argc, char **argv,  void* pConfig, CodecType flag);
void *FnDecodeTest(void *param);
static void usage(char *progname);
int jpu_slt_en = 0;
int jpu_stress_test = 0;
int jpu_rand_sleep = 0;
int use_npu_ion_en = 0;

int main(int argc, char *argv[])
{
    int i;
    HANDLE thread_id[MAX_NUM_INSTANCE];
    DecConfigParam  DecConfig;
    DecConfigParam *pDecConfig = &DecConfig;
    int num_threads = 0;

	memset(&DecConfig, 0, sizeof(DecConfigParam));
    num_threads = atoi(argv[3]);

	memcpy(pDecConfig->bitStreamFileName, argv[1], strlen(argv[1]));
	memcpy(pDecConfig->yuvFileName, argv[4], strlen(argv[4]));
	pDecConfig->loopNums = atoi(argv[2]);
    pDecConfig->device_index = atoi(argv[5]);

    for(i=0; i<num_threads; i++)
    {
        pDecConfig->instanceIndex = i;
        if ((thread_id[i] = CreateThread(NULL, 0, (void*)FnDecodeTest, (void*)pDecConfig, 0, NULL)) == NULL ) 
            printf("create thread error\n");
        Sleep(1000);
    }

    for(i=0; i<num_threads; i++)
    {
        if (WaitForSingleObject(thread_id[i], INFINITE) != WAIT_OBJECT_0) {
        	printf("release thread error\n");
        }
    }
    return 0;
}

static void usage(char *progname)
{
    static char options[] =
       "\t-f multi_lst file name\n"
       "\t-t codec type   0: decoder  1:encoder\n"
       "\t-i input file name\n"
       "\t[-o] output file name\n"
       "\t[-r] ref file for comparing\n"
       "\t[-n] loopNums, default loopNums=1\n"
       "\t-s (only used in encoder) source format: 0 - 4:2:0, 1 - 4:2:2, 2 - 2:2:4, 3 - 4:4:4, 4 - 4:0:0\n"
       "\t-f (only used in encoder) frame format: 0-planar, 1-NV12,NV16(CbCr interleave) 2-NV21,NV61(CbCr alternative),3-YUYV, 4-UYVY, 5-YVYU, 6-VYUY, 7-YUV packed (444 only)\n"
       "\t-w (only used in encoder) actual width\n"
       "\t-h (only used in encoder) actual height\n"

       "For decoder example:\n"
       "\tbmjpegmulti -t 0 -i input.jpg\n"
       "\tbmjpegmulti -t 0 -i input.jpg -n 100 \n"
       "\tbmjpegmulti -t 0 -i input.jpg -o output_yuv420.yuv\n"
       "\tbmjpegmulti -t 0 -i input.jpg -o output_yuv420.yuv -r ref.yuv\n"

       "For encoder example:\n"
       "\tbmjpegmulti -t 1 -i input.yuv -s 0 -f 0  -w 1920 -h 1080 \n"
       "\tbmjpegmulti -t 1 -i input.yuv -s 0 -f 0  -w 1920 -h 1080 -o output.jpg\n"
       "\tbmjpegmulti -t 1 -i input.yuv -s 0 -f 0  -w 1920 -h 1080 -o output.jpg -r ref.jpg\n"
       "\tbmjpegmulti -t 1 -i input.yuv -s 0 -f 0  -w 1920 -h 1080 -n 100\n"
       ;
    fprintf(stderr, "usage:\t%s [option]\n\noption:\n%s\n", progname, options);
}

static int parse_args(int argc, char **argv,  void* pConfig, CodecType codecType)
{
    int opt;
    DecConfigParam* decConfig = NULL;
    EncConfigParam* encConfig = NULL;
    extern char *optarg;
    if(codecType == DEC)
    {
        decConfig = (DecConfigParam*)pConfig;
        while ((opt = getopt(argc, argv, "t:i:o:r:n:d:")) != -1)
        {
            switch (opt)
            {
            case 't':
                break;
            case 'i':
                memcpy(decConfig->bitStreamFileName, optarg, strlen(optarg));
                break;
            case 'o':
                memcpy(decConfig->yuvFileName, optarg, strlen(optarg));
                break;
            case 'r':
                memcpy(decConfig->refFileName, optarg, strlen(optarg));
                break;
            case 'n':
                decConfig->loopNums = atoi(optarg);
                break;
            case 'd':
                decConfig->device_index = atoi(optarg);
                break;
            default:
                usage(argv[0]);
                return 0;
            }
        }
        if (decConfig->loopNums <= 0)
            decConfig->loopNums = 1;
    }
    else{
        encConfig = (EncConfigParam*)pConfig;
        while ((opt = getopt(argc, argv, "t:i:s:f:w:h:o:r:n:")) != -1)
        {
            switch (opt)
            {
            case 't':
                break;
            case 'i':
                memcpy(encConfig->yuvFileName, optarg, strlen(optarg));
                break;
            case 'o':
                memcpy(encConfig->bitStreamFileName, optarg, strlen(optarg));
                break;
            case 'r':
                memcpy(encConfig->refFileName, optarg, strlen(optarg));
                break;
            case 'n':
                encConfig->loopNums = atoi(optarg);
                break;
            case 'w':
                encConfig->picWidth = atoi(optarg);
                break;
            case 'h':
                encConfig->picHeight = atoi(optarg);
                break;
            case 's':
                encConfig->srcFormat = atoi(optarg);
                break;
            case 'f':
                encConfig->frameFormat = atoi(optarg);
                break;
            default:
                usage(argv[0]);
                return 0;
            }
        }
        if (encConfig->loopNums <= 0)
            encConfig->loopNums = 1;
    }
    return RETVAL_OK;
}
