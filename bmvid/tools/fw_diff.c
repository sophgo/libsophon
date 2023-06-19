#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "picasso_bitcode.h"


void usage(int argc, char *argv[])
{
    printf("usage: %s in_file out_file width_stride height_stride  width, height\n", argv[0]);
    printf("note: support non-interleave YUV image only\n");
    return;
}

int main(int argc, char * argv[])
{
    FILE *fin, *fout;
    int i;
    unsigned long size;
    int stride_w, stride_h, w, h;
    int luma_size, chroma_size, src_size, dst_size;
    unsigned char *src;
    unsigned short *p;

    if ((fin = fopen(argv[1], "rb")) == NULL)
    {
        printf("open %s file failed!\n", argv[1]);
        return -1;
    }


    fseek(fin, 0, SEEK_SET);
    size = ftell(fin);
    fseek(fin, 0, SEEK_END);
    size = ftell(fin) - size;
    fseek(fin, 0, SEEK_SET);

    src = malloc(size);
    if (src == NULL)
    {
        printf("malloc failed\n");
        return -1;
    }

    fread(src, sizeof(short), size/sizeof(short), fin);
    p = (unsigned short*)src;

    for (i = 0; i < size/sizeof(short); i++)
        if (p[i] != wave512_bit_code[i]){
            printf("mismatch: [%d] diff [%04x - %04x]\n", i, p[i], wave512_bit_code[i]);
            break;
        }

    if (i == size/sizeof(short))
        printf("two file matches\n");


    if (fin) fclose(fin);
    if (src) free(src);

    return 0;
}
