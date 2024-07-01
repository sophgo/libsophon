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
#include "getopt.h"
#include "jpeg_common.h"
#include "bmjpurun.h"

#define DUMP_READ  1
#define DUMP_WRITE 0

static int parse_args(int argc, char **argv,  void* pConfig, CodecType flag);
int DumpConfig(char *fileName, void *data, int size, int rd);
static void usage(char *progname);
int jpu_slt_en = 0;
int jpu_stress_test = 0;
int jpu_rand_sleep = 0;
int use_npu_ion_en = 0;

int main(int argc, char *argv[])
{
    DecConfigParam    decConfig;
    MultiConfigParam multiConfig;
    EncConfigParam    encConfig;
    int cmd, ret = 0;
    char    cmdstr[265]={0};
    CodecType codecType = DEC; // 1: dec, 2: enc, 3, multi
    int enable;
    int quit = 0;
    char *multi_lst_file = NULL;

    char *slt = getenv("JPU_SLT");
    char *stress = getenv("JPU_STRESS");
    char *rand_sleep = getenv("RAND_SLEEP");
    char *npu_ion = getenv("NPU_ION");

    if (slt) jpu_slt_en = atoi(slt);
    if (stress) jpu_stress_test = atoi(stress);
    if (rand_sleep) jpu_rand_sleep = atoi(rand_sleep);
    if (npu_ion) use_npu_ion_en = atoi(npu_ion);

    if(argc > 1)
    {
        if(strcmp(argv[1], "-t") == 0)
        {
            if(argc > 2)
            {
                if(strcmp(argv[2], "0") == 0)
                {
                    printf("codec type is %s(DEC)\n",argv[2]);
                }
                else if(strcmp(argv[2], "1") == 0)
                {
                    printf("codec type is %s(ENC)\n",argv[2]);
                }else
                {
                    printf("test type: %s  error!\n\n", argv[2]);
                    usage(argv[0]);
                    return -1;
                }
            }
            else{
                usage(argv[0]);
                return -1;
            }
        }
        else if (strcmp(argv[1], "-f") == 0)
        {
            multi_lst_file = argv[2];

            if(!multi_lst_file)
            {
                printf("error, lack of multi_lst file name\n");
                return -1;
            }
        }
        else{
            printf("argv[1] must be \"-t\" \"-f\" !\n");
            usage(argv[0]);
            return -1;
        }

        if(strcmp(argv[1], "-t") == 0)
        {
            if(argc >=4)
            {
                codecType = atoi(argv[2]);
                printf("codecType : %d\n", codecType);
                switch(codecType) {
                case DEC:
                    memset(&decConfig, 0, sizeof(DecConfigParam));
                    ret = parse_args(argc, argv, &decConfig, DEC);
                    if(RETVAL_OK != ret)
                        return ret;
                    ret = DecodeTest(&decConfig);
                    if (ret)
                        fprintf(stderr,  "\nFailed to DecodeTest\n");
                    return ret;
                    break;
                case ENC:
                    memset(&encConfig, 0, sizeof(EncConfigParam));
                    ret = parse_args(argc, argv, &encConfig, ENC);
                    if(RETVAL_OK != ret)
                        return ret;
                    ret = EncodeTest(&encConfig);
                    if (ret)
                        fprintf(stderr,  "\nFailed to EncodeTest\n");
                    return ret;
                    break;
                default:
                    break;
                }
            }
            else
            {
                usage(argv[0]);
                return -1;
            }
        }
    }

    while (!quit)
    {
        if(jpu_slt_en || jpu_stress_test)
        {
            cmd = 30;
        }
        else{
            puts("\n\n\n-------------------------------------------");
            puts("10. DECODER TEST");
            puts("11. LAST DECODER TEST");
            puts("-------------------------------------------");
            puts("20. ENCODER TEST");
            puts("22. LAST ENCODER TEST");
            puts("-------------------------------------------");
            puts("30. MULTI INSTANCE TEST");
            puts("33. LAST MULTI INSTANCE TEST");
            puts("-------------------------------------------");
            puts("99. Exit");

            puts("\nEnter option: ");
            if (scanf("%s", cmdstr) <= 0){
                printf("no cmd inputs\n");
                continue;
            }

            cmd = atoi(cmdstr);
        }
        switch (cmd)
        {
        case 10:
            memset(&decConfig,0x00, sizeof(decConfig));
        case 11:
            if(cmd == 11)
            {
                if(DumpConfig("dec_cmd",&decConfig,sizeof(decConfig), DUMP_READ))
                {
                    goto RUN_LAST_DEC_CMD;
                }
            }

            printf("Enter jpeg file name: ");
            if(scanf("%s", decConfig.bitStreamFileName ) <= 0){
                printf("no file inputs\n");
                break;
            }

            printf("Enter output file enable [0](OFF) [1](ON):");
            if (scanf("%d", &enable) <= 0){
                printf("no option is choiced\n");
                break;
            }
            if(enable){
                printf("Enter output(YUV)file name :");
                if (scanf("%s", decConfig.yuvFileName) <=0){
                    printf("no yuv file inputs\n");
                    break;
                }
            }
            printf("Enter comparion file enable [0](OFF) [1](ON):");
            if (scanf("%d", &enable) <= 0){
                printf("no option inputs\n");
                break;
            }
            if(enable){
                printf("Enter compare(YUV) file name :");
                if (scanf("%s", decConfig.refFileName) <= 0){
                    printf("no compare file inputs\n");
                    break;
                };
            }
            printf("Enter loop nums :");
            if (scanf("%d", &decConfig.loopNums) <= 0){
                printf("no loop nums inputs\n");
                break;
            }
RUN_LAST_DEC_CMD:
            DumpConfig("dec_cmd", &decConfig, sizeof(decConfig), DUMP_WRITE);
            ret = DecodeTest(&decConfig);
            if (ret)
                fprintf(stderr,  "\nFailed to DecodeTest\n");
            break;
        case 20:
            memset(&encConfig,0x00, sizeof(encConfig));
        case 22:
             if(cmd == 11)
            {
                if(DumpConfig("enc_cmd",&encConfig,sizeof(encConfig), DUMP_READ))
                {
                    goto RUN_LAST_ENC_CMD;
                }
            }

            printf("Enter input(YUV) file name: ");
            if (0 >= scanf("%s", encConfig.yuvFileName)){
                printf("no input filename\n");
                break;
            }

            printf("Enter output file enable [0](OFF) [1](ON):");
            if (0 >= scanf("%d", &enable)){
                printf("no option inputs\n");
                break;
            }
            if(enable){
                printf("Enter output(jpg) file name :");
                if (0 >= scanf("%s", encConfig.bitStreamFileName)){
                    printf("no output file inputs\n");
                    break;
                }
            }

            printf("Enter comparion file enable [0](OFF) [1](ON):");
            if (0 >= scanf("%d", &enable)){
                printf("no option selected\n");
                break;
            }
            if(enable){
                printf("Enter compare(jpg) file name :");
                if (0 >= scanf("%s", encConfig.refFileName)){
                    printf("no compare file name inputs\n");
                    break;
                }
            }
            printf("Enter picture width :");
            if (0 >= scanf("%d", &encConfig.picWidth)){
                printf("no width inputs\n");
                break;
            }
            printf("Enter picture height :");
            if (0 >= scanf("%d", &encConfig.picHeight)){
                printf("no height inputs\n");
                break;
            }
            printf("Enter Frame Format [0](PLANAR) [1](NV12,NV16) [2](NV21,NV61) [3](YUYV) [4](UYVY) [5](YVYU) [6](VYUY) [7](YUV_444 PACKED): ");
            if (0 >= scanf("%d", &encConfig.frameFormat)){
                printf("no format inputs\n");
                break;
            }
            printf("Enter Source Chroma Format 0 (4:2:0) / 1 (4:2:2) / 2 (2:2:4 4:2:2 rotated) / 3 (4:4:4) / 4 (4:0:0)  : ");
            if (0 >= scanf("%d", &encConfig.srcFormat)){
                printf("no source format inputs\n");
                break;
            }
            printf("Enter loop nums :");
            if (0 >= scanf("%d", &encConfig.loopNums)){
                printf("no loop inputs\n");
                break;
            }
RUN_LAST_ENC_CMD:
            DumpConfig("enc_cmd", &encConfig, sizeof(encConfig), DUMP_WRITE);
            ret = EncodeTest(&encConfig);
            if (ret)
                fprintf(stderr,  "\nFailed to EncodeTest\n");
            break;

        case 30:
            memset(&multiConfig, 0x00, sizeof(multiConfig));
            memset(&multiConfig.encConfig[0], 0x00, sizeof(EncConfigParam)*MAX_NUM_INSTANCE);
            memset(&multiConfig.decConfig[0], 0x00, sizeof(DecConfigParam)*MAX_NUM_INSTANCE);
        case 33:
            if (cmd == 33)
            {
                if (DumpConfig("mul_cmd", &multiConfig, sizeof(multiConfig), DUMP_READ))
                    goto RUN_LAST_MULTI_CMD;
            }
           do
            {
                FILE *fp = NULL;
                FILE *fp_test = NULL;
                int i;
                int instanceIndex = 0;
                int loopNums = 1;
                int str_len = 0;
                int multi_lst_lines = 0;
                char str_line[1025] = {0,};
                char str[128] = {0,};
                char str_type[2] = {0,};
                char str_skip[4] = {0,};
                char str_device_index[3] = {0,};
                char str_ref[128] = {0,};
                int type = 0;
                int skip = 0;
                int device_index = 0;

                if (multi_lst_file)
                {
                    fp = fopen(multi_lst_file, "rt");
                }
                else
                {
                    fp = fopen("multi.lst", "rt");
                }
                if (fp == NULL) {
                    fprintf(stderr, "Failed to open list file\n");
                    return -1;
                }
                if ( NULL == fgets(str_line, 512, fp)){
                    printf("failed to read file\n");
                    return -1;
                }
                sscanf(str_line, "%d %d", &instanceIndex, &loopNums);
                multiConfig.numMulti = instanceIndex;
                printf("\nThreads = %d, loops = %d\n", multiConfig.numMulti,loopNums);
                for (i=0; i < multiConfig.numMulti; i++)
                {
                    fseek(fp, 0, SEEK_CUR);
                    if (fgets(str_line, 1025, fp) == NULL)
                    {
                        if (multi_lst_lines < multiConfig.numMulti)
                        {
                            printf("multi.lst lines err, please check it\n");
                            if (fp)
                                fclose(fp);
                            if (fp_test)
                                fclose(fp);
                            return -1;
                        }
                        else
                        {
                            if (fp)
                                fclose(fp);
                            if (fp_test)
                                fclose(fp);
                            return 0;
                        }
                    }
                    else
                    {
                        str_len = strlen(str_line);
                        if (str_len >= 1024)
                        {
                            printf("multi.lst line too long, must be less than 1024 byte, please check it\n");
                            if (fp)
                                fclose(fp);
                            if (fp_test)
                                fclose(fp);
                            return -1;
                        }
                    }
                    multi_lst_lines++;
                    printf("thread%d parameter: %s\n",i, str_line);
                    sscanf(str_line, "%s %s %s %s %s", str, str_skip, str_type, str_device_index, str_ref);
                    fp_test = fopen(str, "rt");
                    if (fp_test == NULL) {
                        fprintf(stderr, "multi.lst error, no %s file\n", str);
                        return -1;
                    }
                    else
                    {
                        fclose(fp_test);
                    }

                    if (0 != str_ref[0])
                    {
                        fp_test = fopen(str_ref, "rt");
                        if (fp_test == NULL) {
                            fprintf(stderr, "multi.lst error, no %s ref file\n", str_ref);
                            return -1;
                        }
                        else
                        {
                            fclose(fp_test);
                        }
                    }

                    skip = atoi(str_skip);
                    if (skip <= 0)
                    {
                        skip = 1;
                    }

                    type = atoi(str_type);
                    if (type != 0 && type != 1)
                    {
                        fprintf(stderr, "str_type %d err\n", type);
                        return -1;
                    }

                    device_index = atoi(str_device_index);
                    if (device_index < 0 || device_index > 63)
                    {
                        fprintf(stderr, "sophon idx: %d error, must be 0~63\n", device_index);
                        return -1;
                    }

                    multiConfig.multiMode[i] = type;
                    if (multiConfig.multiMode[i]) //decoder
                    {
                        memset(&multiConfig.decConfig[i], 0x00, sizeof(DecConfigParam));
                        strcpy(multiConfig.decConfig[i].bitStreamFileName, str);
                        multiConfig.decConfig[i].device_index = device_index;
                        if( 0 == jpu_slt_en)
                        {
                            sprintf(str, "dec_out%d.yuv",i);
                            strcpy(multiConfig.decConfig[i].yuvFileName, str);
                        }
                        if(0 != str_ref[0]){

                            strcpy(multiConfig.decConfig[i].refFileName, str_ref);
                            memset(str_ref, 0, 128);
                        }
                        multiConfig.decConfig[i].loopNums = loopNums;
                        multiConfig.decConfig[i].Skip = skip;
                    }
                    else
                    {
                        memset(&multiConfig.encConfig[i], 0x00, sizeof(EncConfigParam));
                        strcpy(multiConfig.encConfig[i].cfgFileName, str);
                        multiConfig.encConfig[i].device_index = device_index;
                        if(0 != str_ref[0])
                        {
                            strcpy(multiConfig.encConfig[i].refFileName, str_ref);
                            memset(str_ref, 0, 128);
                        }
                        multiConfig.encConfig[i].loopNums = loopNums;

                        do{
                            // Check CFG file validity
                            FILE *fpTmp;
                            fpTmp = fopen(multiConfig.encConfig[i].cfgFileName, "r");
                            if (fpTmp == 0)
                                multiConfig.encConfig[i].cfgFileName[0] = 0;
                            else
                                fclose(fpTmp);
                        }while(0);

                        sprintf(str, "enc_out%d.jpg",i);
                        strcpy(multiConfig.encConfig[i].bitStreamFileName, str);
                        multiConfig.encConfig[i].Skip = skip;

                    }
                }
                fclose(fp);

RUN_LAST_MULTI_CMD:
                DumpConfig("mul_cmd", &multiConfig, sizeof(multiConfig), DUMP_WRITE);
                ret = MultiInstanceTest(&multiConfig);
                if (ret)
                {
                    fprintf(stderr, "Failed to MultiDecodeTest\n");
                    return ret;
                }
            } while(0);

            if(jpu_slt_en || jpu_stress_test)
            {
                quit = 1;
            }
            break;
        case 99:
            ret = 1;
            goto ERROR_MAIN;
            break;
        }
    }
ERROR_MAIN:
    return ret;
}

int DumpConfig(char *fileName, void *data, int size, int rd)
{
    FILE *fp = NULL;

    if (rd)
    {
        fp = fopen(fileName, "rb");
        if (!fp)
            return 0;

        if (fread(data, 1, size, fp) < (size_t)size){
            printf("failed to read %d byte\n", size);
            return 0;
        }
    }
    else
    {
        fp = fopen(fileName, "wb");
        if (!fp)
            return 0;

        fwrite(data, 1, size, fp);
    }

    fclose(fp);

    return 1;
}

static void usage(char *progname)
{
    static char options[] =
       "\t-f multi_lst file name\n"
       "\t-t codec type [0: decoder, 1:encoder]\n"
       "\t-i input file name\n"
       "\t-d device id\n"
       "\t[-o] output file name\n"
       "\t[-r] ref file for comparing\n"
       "\t[-n] loopNums, default loopNums=1\n"
       "\t-s (only used in encoder) source format: 0 - 4:2:0, 1 - 4:2:2, 2 - 2:2:4, 3 - 4:4:4, 4 - 4:0:0\n"
       "\t-f (only used in encoder) frame format: 0-planar, 1-NV12,NV16(CbCr interleave) 2-NV21,NV61(CbCr alternative),3-YUYV, 4-UYVY, 5-YVYU, 6-VYUY, 7-YUV packed (444 only)\n"
       "\t-w (only used in encoder) actual width\n"
       "\t-h (only used in encoder) actual height\n"
       "\t-g rotate (default 0) [rotate mode[1:0]  0:No rotate  1:90  2:180  3:270] [rotator mode[2]:vertical flip] [rotator mode[3]:horizontal flip]\n"
       "\t-c crop output (default 0) [0: no crop, 1: crop to actual size]\n"

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
        while ((opt = getopt(argc, argv, "t:i:o:r:n:d:c:")) != -1)
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
            case 'c':
                decConfig->crop_yuv = atoi(optarg);
                break;
            default:
                usage(argv[0]);
                return 0;
            }
        }
        if (decConfig->loopNums <= 0)
            decConfig->loopNums = 1;
        decConfig->Skip = 1;
    }
    else{
        encConfig = (EncConfigParam*)pConfig;
        while ((opt = getopt(argc, argv, "t:i:s:f:w:h:o:r:n:g:d:")) != -1)
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
            case 'd':
                encConfig->device_index = atoi(optarg);
                break;
            case 'g':
                encConfig->rotate = atoi(optarg);
                break;
            default:
                usage(argv[0]);
                return 0;
            }
        }
        if (encConfig->loopNums <= 0)
            encConfig->loopNums = 1;
        encConfig->Skip = 1;
    }
    return RETVAL_OK;
}
