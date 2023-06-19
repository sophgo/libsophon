#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage(int argc, char *argv[])
{
    printf("this tool is used to combind bm1880 palladium data into one binary file\n");
    printf("usage: %s in_low_file in_high_file out_file\n", argv[0]);
    return;
}

int main(int argc, char * argv[])
{
    FILE *fin_low, *fin_high, *fout;
    int i;
    unsigned char dst[4096];
    unsigned int data;
    unsigned char *p;
    int chunk;
    int total = 0;

    if (argc < 4)
    {
        usage(argc, argv);
        return -1;
    }

    if ((fin_low = fopen(argv[1], "rt")) == NULL)
    {
        printf("open %s file failed!\n", argv[1]);
        return -1;
    }
    if ((fin_high = fopen(argv[2], "rt")) == NULL)
    {
        printf("open %s file failed!\n", argv[2]);
        return -1;
    }
    if ((fout = fopen(argv[3], "wb")) == NULL)
    {
        printf("open %s file failed!\n", argv[3]);
        return -1;
    }

    chunk = 0;
    p = (unsigned char *)&data;
    fscanf(fin_high, "@%*[^\n]\n");
    fscanf(fin_low, "@%*[^\n]\n");
    while(!feof(fin_low) || !feof(fin_high))
    {
        if (chunk & 0x3){
            if (EOF == fscanf(fin_high, "%04x\n", &data))
                break;
        }
        else{
            if (EOF == fscanf(fin_low, "%04x\n", &data))
                break;
        }

        dst[chunk++] = p[0];
        dst[chunk++] = p[1];

        if (chunk == 4096){
            fwrite(dst, sizeof(char), 4096, fout);
            chunk = 0;
        }
        total += 2;
    }
    if (chunk)
        fwrite(dst, sizeof(char), chunk, fout);

    printf("total %d byte processed!\n", total);

    if (fin_low) fclose(fin_low);
    if (fin_high) fclose(fin_high);
    if (fout) fclose(fout);

    return 0;
}
