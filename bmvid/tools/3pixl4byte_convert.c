#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage(int argc, char *argv[])
{
    printf("usage: %s in_file out_file width_stride height_stride  width, height msb\n", argv[0]);
    printf("note: support non-interleave YUV image only\n");
    return;
}

typedef void (*pFunc)(unsigned char*, unsigned char *);
void DePxlOrder3pxl4byteMSB(unsigned char* pPxl16Src, unsigned char* pPxl16Dst);

void DePxlOrder3pxl4byteLSB(unsigned char* pPxl16Src, unsigned char* pPxl16Dst);

int main(int argc, char * argv[])
{
    FILE *fin, *fout;
    int i, j;
    unsigned long size;
    int stride_w, stride_h, w, h;
    int luma_size, chroma_size, src_size, dst_size;
    unsigned char *src, *dst;
    int msb;
    pFunc pConvert;

    if (argc < 8)
    {
        usage(argc, argv);
        return -1;
    }

    if ((fin = fopen(argv[1], "rb")) == NULL)
    {
        printf("open %s file failed!\n", argv[1]);
        return -1;
    }

    if ((fout = fopen(argv[2], "wb")) == NULL)
    {
        printf("open %s file failed!\n", argv[2]);
        return -1;
    }

    fseek(fin, 0, SEEK_SET);
    size = ftell(fin);
    fseek(fin, 0, SEEK_END);
    size = ftell(fin) - size;
    fseek(fin, 0, SEEK_SET);

    w = atoi(argv[5]);
    h = atoi(argv[6]);
    stride_w = atoi(argv[3]);
    stride_h = atoi(argv[4]);
    src_size = stride_w * stride_h * 3/2;
    dst_size = w * h * 3 / 2;
    src = malloc(src_size);
    dst = malloc(dst_size);
    if (src == NULL || dst == NULL)
    {
        printf("malloc failed\n");
        return -1;
    }

    msb = atoi(argv[7]);
    if (msb)
        pConvert = DePxlOrder3pxl4byteMSB;
    else
        pConvert = DePxlOrder3pxl4byteLSB;

    while(size)
    {
        int read_size = size > src_size ? src_size : size;
        unsigned char *ps = src, *pd = dst;

        if (fread(src, read_size, 1, fin) == 0)
            break;
        size -= read_size;

        for (i = 0; i < h; i++)
        {
            for (j = 0; j < w; j+=16)
            {
                pConvert(ps+j, pd+j);
            }
            pd += w;
            ps += stride_w;
        }

        ps = src + stride_w * stride_h;
        for (i = 0; i < h /2; i++)
        {
            for (j = 0; j < w/2; j+=16)
            {
                pConvert(ps+j, pd+j);
            }
            pd += w/2;
            ps += stride_w/2;
        }

        ps = src + stride_w * stride_h * 5 /4;
        for (i = 0; i < h /2; i++)
        {
            for (j = 0; j < w/2; j+=16)
            {
                pConvert(ps+j, pd+j);
            }
            pd += w/2;
            ps += stride_w/2;
        }

        fwrite(dst, dst_size, 1, fout);
    }

    if (fin) fclose(fin);
    if (fout) fclose(fout);

    return 0;
}

void DePxlOrder3pxl4byteMSB(unsigned char* pPxl16Src, unsigned char* pPxl16Dst)
{
#if 1
    //-----------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|
    //mode    1: | p11[9:2] | p11[1:0],p10[9:4] | p10[3:0],p09[9:6] | p09[5:0],2'b0 | p08[9:2] | p08[1:0],p07[9:4] | p07[3:0],p06[9:6] | p06[5:0],2'b0 | p05[9:2] | p05[1:0],p04[9:4] | p04[3:0],p03[9:6] | p03[5:0],2'b0 | p02[9:2] | p02[1:0],p01[9:4] | p01[3:0],p00[9:6] | p00[5:0],2'b0 |
    //                       |     A    |         B         |         C         |       D       |     E    |         F         |         G         |       H       |     I    |         J         |         K         |       L       |     M    |         N         |         O         |       P       |

    //-----------|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|
    //HW output: | p00[5:0],2'b0 | p01[3:0],p00[9:6] | p02[1:0],p01[9:4] | p02[9:2] | p03[5:0],2'b0 | p04[3:0],p03[9:6] | p05[1:0],p04[9:4] | p05[9:2] | p06[5:0],2'b0 | p07[3:0],p06[9:6] | p08[1:0],p07[9:4] | p08[9:2] | p09[5:0],2'b0 | p10[3:0],p09[9:6] | p11[1:0],p10[9:4] | p11[9:2] |
    //                       |       P       |         O         |         N         |     M    |       L       |         K         |         J         |     I    |       H       |         G         |         F         |     E    |       D       |         C         |         B         |     A    |

    //-----------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|
    //mode    0: | p00[9:2] | p00[1:0],p01[9:4] | p01[3:0],p02[9:6] | p02[5:0],2'b0 | p03[9:2] | p03[1:0],p04[9:4] | p04[3:0],p05[9:6] | p05[5:0],2'b0 | p06[9:2] | p06[1:0],p07[9:4] | p07[3:0],p08[9:6] | p08[5:0],2'b0 | p09[9:2] | p09[1:0],p10[9:4] | p10[3:0],p11[9:6] | p11[5:0],2'b0 |

    // mode 1 vs HW output : endian swap
    // HW output vs mode 1 : byte swap in 4 bytes & pixel swap in 3 pixels
    //1) byte swap in 4 bytes
    //2) pixel swap in 3 pixels
    int i;
    unsigned char*   pSrc = pPxl16Src;
    unsigned char*   pDst = pPxl16Dst;
    unsigned int   temp ;
    unsigned int   temp0;
    unsigned int   temp1;
    unsigned int   temp2;
    for (i = 0; i < 4; i++) {
        pDst[i * 4 + 3] = pSrc[i * 4 + 0];
        pDst[i * 4 + 2] = pSrc[i * 4 + 1];
        pDst[i * 4 + 1] = pSrc[i * 4 + 2];
        pDst[i * 4 + 0] = pSrc[i * 4 + 3]; //| p02[9:2] | p02[1:0],p01[9:4] | p01[3:0],p00[9:6] | p00[5:0],2'b0 |
        temp  = (pDst[4*i + 0] << 24) + (pDst[4*i + 1] << 16) + (pDst[4*i + 2] << 8) + (pDst[4*i + 3] << 0);
        temp0 = (temp >> 2) & 0x3ff;
        temp1 = (temp >> 12) & 0x3ff;
        temp2 = (temp >> 22) & 0x3ff;
        temp = (temp2 << 2) + (temp1 << 12) + (temp0 << 22);
        pDst[i * 4 + 3] = (temp >> 24) & 0xff;
        pDst[i * 4 + 2] = (temp >> 16) & 0xff;
        pDst[i * 4 + 1] = (temp >> 8) & 0xff;
        pDst[i * 4 + 0] = (temp >> 0) & 0xff; //| p00[9:2] | p00[1:0],p01[9:4] | p01[3:0],p02[9:6] | p02[5:0],2'b0 |
    }
#else
    //-----------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|
    //mode    1: | p11[9:2] | p11[1:0],p10[9:4] | p10[3:0],p09[9:6] | p09[5:0],2'b0 | p08[9:2] | p08[1:0],p07[9:4] | p07[3:0],p06[9:6] | p06[5:0],2'b0 | p05[9:2] | p05[1:0],p04[9:4] | p04[3:0],p03[9:6] | p03[5:0],2'b0 | p02[9:2] | p02[1:0],p01[9:4] | p01[3:0],p00[9:6] | p00[5:0],2'b0 |
    //                       |     A    |         B         |         C         |       D       |     E    |         F         |         G         |       H       |     I    |         J         |         K         |       L       |     M    |         N         |         O         |       P       |

    //HW output: | p11[9:2] | p11[1:0],p10[9:4] | p10[3:0],p09[9:6] | p09[5:0],2'b0 | p08[9:2] | p08[1:0],p07[9:4] | p07[3:0],p06[9:6] | p06[5:0],2'b0 | p05[9:2] | p05[1:0],p04[9:4] | p04[3:0],p03[9:6] | p03[5:0],2'b0 | p02[9:2] | p02[1:0],p01[9:4] | p01[3:0],p00[9:6] | p00[5:0],2'b0 |
    //                       |     A    |         B         |         C         |       D       |     E    |         F         |         G         |       H       |     I    |         J         |         K         |       L       |     M    |         N         |         O         |       P       |

    //mode    0: | p00[9:2] | p00[1:0],p01[9:4] | p01[3:0],p02[9:6] | p02[5:0],2'b0 | p03[9:2] | p03[1:0],p04[9:4] | p04[3:0],p05[9:6] | p05[5:0],2'b0 | p06[9:2] | p06[1:0],p07[9:4] | p07[3:0],p08[9:6] | p08[5:0],2'b0 | p09[9:2] | p09[1:0],p10[9:4] | p10[3:0],p11[9:6] | p11[5:0],2'b0 |

    // mode 1 = HW output
    // HW output vs mode 0 : 4-byte word swap & pixel swap in 3 pixels
    //1) 4-byte word swap
    //2) pixel swap in 3 pixels
    int i;
    unsigned char*   pSrc = pPxl16Src;
    unsigned char*   pDst = pPxl16Dst;
    for (i = 0; i < 4; i++) {
        unsigned int   temp  = (pSrc[4*i + 0] << 24) + (pSrc[4*i + 1] << 16) + (pSrc[4*i + 2] << 8) + (pSrc[4*i + 3] << 0);
        unsigned int   temp0 = (temp >> 2) & 0x3ff;
        unsigned int   temp1 = (temp >> 12) & 0x3ff;
        unsigned int   temp2 = (temp >> 22) & 0x3ff;
        temp = (temp2 << 2) + (temp1 << 12) + (temp0 << 22);
        pDst[4*(3 - i) + 0] = (temp >> 24) & 0xff;
        pDst[4*(3 - i) + 1] = (temp >> 16) & 0xff;
        pDst[4*(3 - i) + 2] = (temp >> 8) & 0xff;
        pDst[4*(3 - i) + 3] = (temp >> 0) & 0xff;
    }
#endif
}

void DePxlOrder3pxl4byteLSB(unsigned char* pPxl16Src, unsigned char* pPxl16Dst)
{
#if 1
    //-----------|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|
    //mode    1: | 2'b0,p11[9:4] | p11[3:0],p10[9:6] | p10[5:0],p09[9:8] | p09[7:0] | 2'b0,p08[9:4] | p08[3:0],p07[9:6] | p07[5:0],p06[9:8] | p06[7:0] | 2'b0,p05[9:4] | p05[3:0],p04[9:6] | p04[5:0],p03[9:8] | p03[7:0] | 2'b0,p02[9:4] | p02[3:0],p01[9:6] | p01[5:0],p00[9:8] | p00[7:0] |
    //                       |       A       |         B         |         C         |     D    |       E       |         F         |         G         |     H    |       I       |         J         |         K         |     L    |       M       |         N         |         O         |     P    |

    //-----------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|---8bit---|-------8bit--------|-------8bit--------|-----8bit------|
    //HW output: | p00[7:0] | p01[5:0],p00[9:8] | p02[3:0],p01[9:6] | 2'b0,p02[9:4] | p03[7:0] | p04[5:0],p03[9:8] | p05[3:0],p04[9:6] | 2'b0,p05[9:4] | p06[7:0] | p07[5:0],p06[9:8] | p08[3:0],p07[9:6] | 2'b0,p08[9:4] | p09[7:0] | p10[5:0],p09[9:8] | p11[3:0],p10[9:6] | 2'b0,p11[9:4] |
    //           |     P    |         O         |         N         |       M       |     L    |         K         |         J         |       I       |     H    |         G         |         F         |       E       |     D    |         C         |         B         |       A       |

    //-----------|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|
    //mode    0: | 2'b0,p02[9:4] | p02[3:0],p01[9:6] | p01[5:0],p00[9:8] | p00[7:0] | 2'b0,p05[9:4] | p05[3:0],p04[9:6] | p04[5:0],p03[9:8] | p03[7:0] | 2'b0,p08[9:4] | p08[3:0],p07[9:6] | p07[5:0],p06[9:8] | p06[7:0] | 2'b0,p11[9:4] | p11[3:0],p10[9:6] | p10[5:0],p09[9:8] | p09[7:0] |
    //           |       M       |         N         |         O         |     P    |       I       |         J         |         K         |     L    |       E       |         F         |         G         |     H    |       A       |         B         |         C         |     D    |

    // mode 1 vs HW output : endian swap
    // HW output vs mode 1 : byte swap in 4 bytes
    //1) byte swap in 4 bytes
    int i;
    unsigned char* pSrc = pPxl16Src;
    unsigned char* pDst = pPxl16Dst;
    for (i = 0; i < 4; i++) {
        pDst[i * 4 + 3] = pSrc[i * 4 + 0];
        pDst[i * 4 + 2] = pSrc[i * 4 + 1];
        pDst[i * 4 + 1] = pSrc[i * 4 + 2];
        pDst[i * 4 + 0] = pSrc[i * 4 + 3];
    }

#else
    //-----------|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|-----8bit------|-------8bit--------|-------8bit--------|---8bit---|
    //mode    1: | 2'b0,p11[9:4] | p11[3:0],p10[9:6] | p10[5:0],p09[9:8] | p09[7:0] | 2'b0,p08[9:4] | p08[3:0],p07[9:6] | p07[5:0],p06[9:8] | p06[7:0] | 2'b0,p05[9:4] | p05[3:0],p04[9:6] | p04[5:0],p03[9:8] | p03[7:0] | 2'b0,p02[9:4] | p02[3:0],p01[9:6] | p01[5:0],p00[9:8] | p00[7:0] |
    //                       |       A       |         B         |         C         |     D    |       E       |         F         |         G         |     H    |       I       |         J         |         K         |     L    |       M       |         N         |         O         |     P    |
    //HW output: | 2'b0,p11[9:4] | p11[3:0],p10[9:6] | p10[5:0],p09[9:8] | p09[7:0] | 2'b0,p08[9:4] | p08[3:0],p07[9:6] | p07[5:0],p06[9:8] | p06[7:0] | 2'b0,p05[9:4] | p05[3:0],p04[9:6] | p04[5:0],p03[9:8] | p03[7:0] | 2'b0,p02[9:4] | p02[3:0],p01[9:6] | p01[5:0],p00[9:8] | p00[7:0] |
    //                       |       A       |         B         |         C         |     D    |       E       |         F         |         G         |     H    |       I       |         J         |         K         |     L    |       M       |         N         |         O         |     P    |
    //mode    0: | 2'b0,p02[9:4] | p02[3:0],p01[9:6] | p01[5:0],p00[9:8] | p00[7:0] | 2'b0,p05[9:4] | p05[3:0],p04[9:6] | p04[5:0],p03[9:8] | p03[7:0] | 2'b0,p08[9:4] | p08[3:0],p07[9:6] | p07[5:0],p06[9:8] | p06[7:0] | 2'b0,p11[9:4] | p11[3:0],p10[9:6] | p10[5:0],p09[9:8] | p09[7:0] |
    //                       |       M       |         N         |         O         |     P    |       I       |         J         |         K         |     L    |       E       |         F         |         G         |     H    |       A       |         B         |         C         |     D    |

    // mode 1 = HW output
    // HW output vs mode 0 : 4-byte word swap
    //1) 4-byte word swap
    int i;
    unsigned int*   pSrc = (unsigned int*) pPxl16Src;
    unsigned int*   pDst = (unsigned int*) pPxl16Dst;
    for (i = 0; i < 4; i++)
        pDst[3 - i] = pSrc[i];
#endif
}