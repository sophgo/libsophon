#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coda960_bitcode.h"
void usage(int argc, char *argv[])
{
    printf("this tool is used to create coda960 firmware file for bm1880 palladium simulation\n");
    printf("usage: %s out_file\n", argv[0]);
    return;
}

int main(int argc, char * argv[])
{
    FILE *fout_low, *fout_high;
    char outname_low[256];
    char outname_high[256];
    int i;
    unsigned long size;
    unsigned long total;
    unsigned char *src;
    unsigned short *dst;

    if (argc < 2)
    {
        usage(argc, argv);
        return -1;
    }

    size = strchr(argv[1], '.') - argv[1];
    strncpy(outname_low, argv[1], size);
    strncpy(outname_high, argv[1], size);
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

    size = sizeof(coda960_bit_code) / sizeof(coda960_bit_code[0]);

    total = 0;
    while(total < size)
    {
        if (total & 0x1)
            fprintf(fout_high, "%04x\n", coda960_bit_code[total++]);
        else
            fprintf(fout_low, "%04x\n", coda960_bit_code[total++]);
    }

    if (fout_low) fclose(fout_low);
    if (fout_high) fclose(fout_high);

    return 0;
}
