#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage(int argc, char *argv[])
{
    printf("this tool is used to create simualtion data format for BM1880 palladium simulation\n");
    printf("usage: %s in_file out_file\n", argv[0]);
    return;
}

int main(int argc, char * argv[])
{
    FILE *fin, *fout_low, *fout_high;
    char outname_low[256], outname_high[256];
    int i;
    unsigned long size;
    unsigned long total;
    unsigned char *src;
    unsigned short *dst;

    if (argc < 3)
    {
        usage(argc, argv);
        return -1;
    }

    if ((fin = fopen(argv[1], "rb")) == NULL)
    {
        printf("open %s file failed!\n", argv[1]);
        return -1;
    }

    size = strchr(argv[2], '.') - argv[2];
    strncpy(outname_low, argv[2], size);
    strncpy(outname_high, argv[2], size);
    outname_low[size] =
    outname_high[size] = '\0';
    strcat(outname_low, "_low.in");
    strcat(outname_high, "_high.in");

    if ((fout_low = fopen(outname_low, "wt")) == NULL)
    {
        printf("open %s file failed!\n", outname_low);
        return -1;
    }
    if ((fout_high = fopen(outname_high, "wt")) == NULL)
    {
        printf("open %s file failed!\n", outname_high);
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
    fread(src, 1, size, fin);
    dst = (unsigned short *)src;

    total = 0;
    while(total < size)
    {
        if (total & 0x3)
            fprintf(fout_high, "%04x\n", *dst++);
        else
            fprintf(fout_low, "%04x\n", *dst++);

        total += sizeof(short);
    }

    if (fin) fclose(fin);
    if (fout_low) fclose(fout_low);
    if (fout_high) fclose(fout_high);

    return 0;
}
