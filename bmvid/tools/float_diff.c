#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(int argc, char *argv[])
{
    FILE *fp1=NULL, *fp2=NULL;
    size_t len = 0, len2;
    int i;
    float *buf1, *buf2;
    float max_diff = 0.0f;
    int max_index;

    fp1 = fopen(argv[1], "rb");
    fp2 = fopen(argv[2], "rb");

    if (fp1 == NULL || fp2 == NULL){
        printf("open file failed\n");
        return -1;
    }

    len = ftell(fp1);
    fseek(fp1, 0, SEEK_END);
    len = ftell(fp1) - len;
    fseek(fp1, 0, SEEK_SET);

    len2 = ftell(fp2);
    fseek(fp2, 0, SEEK_END);
    len2 = ftell(fp2) - len2;
    fseek(fp2, 0, SEEK_SET);

    if (len != len2){
        printf("file size is different!\n");
        fclose(fp1);
        fclose(fp2);
        return -1;
    }

    buf1 = (float *)malloc(len);
    buf2 = (float *)malloc(len);
    if (!buf1 || !buf2){
        printf("malloc failed\n");
        fclose(fp1);
        fclose(fp2);
        return -1;
    }

    fread(buf1, sizeof(float), len/sizeof(float), fp1);
    fread(buf2, sizeof(float), len/sizeof(float), fp2);

    for (i=0; i<len/sizeof(float); i++){
        if (fabs(buf1[i] - buf2[i]) > max_diff){
            max_diff = fabs(buf1[i] - buf2[i]);
            max_index = i;
        }
    }

    printf("max_float diff %f, index %d , value %f %f\n", max_diff, max_index, buf1[max_index], buf2[max_index]);

    fclose(fp1);
    fclose(fp2);
    free(buf1);
    free(buf2);

    return 0;
}
