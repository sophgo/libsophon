#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    unsigned char *src, *dst;

    if (argc < 6)
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

    while(size)
    {
        int read_size = size > src_size ? src_size : size;
        unsigned char *ps = src, *pd = dst;

        if (fread(src, read_size, 1, fin) == 0)
            break;
        size -= read_size;

        for (i = 0; i < h; i++)
        {
            memcpy(pd, ps, w);
            pd += w;
            ps += stride_w;
        }

        ps = src + stride_w * stride_h;
        for (i = 0; i < h /2; i++)
        {
            memcpy(pd, ps, w/2);
            pd += w/2;
            ps += stride_w/2;
        }

        ps = src + stride_w * stride_h * 5 /4;
        for (i = 0; i < h /2; i++)
        {
            memcpy(pd, ps, w/2);
            pd += w/2;
            ps += stride_w/2;
        }

        fwrite(dst, dst_size, 1, fout);
    }

    if (fin) fclose(fin);
    if (fout) fclose(fout);

    return 0;
}