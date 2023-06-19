#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage(int argc, char *argv[])
{
    printf("usage: %s in_m2ts out_ts\n", argv[0]);
    return;
}

int main(int argc, char * argv[])
{
    FILE *fin, *fout;
    int i;
    unsigned long size;
    unsigned char src[192];

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

    while(size)
    {
        int read_size = size > 192 ? 192 : size;

        if (fread(src, read_size, 1, fin) == 0)
            break;
        size -= read_size;

        fwrite(src+4, 188, 1, fout);
    }

    if (fin) fclose(fin);
    if (fout) fclose(fout);

    return 0;
}
