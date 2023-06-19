#include <iostream>
#include <vector>
#include "bmcv_api_ext.h"
#include "stdio.h"
#include "stdlib.h"
#include <string.h>


static bm_handle_t handle;
char opencvFile_path[200] = "./";
#define UNUSED_VARIABLE(x)  ((x) = (x))

#ifndef USING_CMODEL
void read_image(unsigned char **input_ptr, int *src_len, const char * src_name)
{
    char input_name[200] = {0};
    int len = strlen(opencvFile_path);
    if(opencvFile_path[len-1] != '/')
      opencvFile_path[len] = '/';
    snprintf(input_name, 200,"%s%s", opencvFile_path,src_name);

    FILE *fp_src = fopen(input_name, "rb+");
    fseek(fp_src, 0, SEEK_END);
    *src_len = ftell(fp_src);
    *input_ptr   = (unsigned char *)malloc(*src_len);
    fseek(fp_src, 0, SEEK_SET);
    size_t cnt = fread((void *)*input_ptr, 1, *src_len, fp_src);
    fclose(fp_src);
    UNUSED_VARIABLE(cnt);

}

static void test_vpp_loop(int test_loop_times) {
    int            IMAGE_H     = 1080;
    int            IMAGE_W     = 1920;
    unsigned char *input_ptr = NULL;
    int src_len;

    read_image(&input_ptr, &src_len,"girl_1920x1080_nv12.bin");

    int            plane0_size = IMAGE_H * IMAGE_W;
    bm_image       src[4], dst[4];
    bm_image       dst_tmp[4];
    bm_device_mem_t mem[4];

    bmcv_rect_t rect[] = {{0, 0, IMAGE_W, IMAGE_H},
                          {0, 0, IMAGE_W, IMAGE_H},
                          {0, 0, IMAGE_W, IMAGE_H},
                          {0, 0, IMAGE_W, IMAGE_H}};
    for (int i = 0; i < 4; i++) {
        bm_image_create(
            handle, IMAGE_H, IMAGE_W, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, src + i);
        bm_image_alloc_dev_mem_heap_mask(src[i], 6);

        void *buf[4] = {(void *)input_ptr, (void *)(input_ptr + plane0_size), 0, 0};
        bm_image_copy_host_to_device(src[i], buf);

        bm_image_create(handle,
                        IMAGE_H,
                        IMAGE_W,
                        FORMAT_BGR_PACKED,
                        DATA_TYPE_EXT_1N_BYTE,
                        dst + i);
        bm_image_alloc_dev_mem(dst[i]);
        bm_image_create(handle,
                        IMAGE_H,
                        IMAGE_W,
                        FORMAT_BGR_PACKED,
                        DATA_TYPE_EXT_1N_BYTE,
                        dst_tmp + i);
    }

    for(int idx=0; idx<test_loop_times; idx++) {
        if(idx%100==0)
            printf("idx=%d\n", idx);
        bm_image_get_device_mem(dst[0], mem);
        bm_image_attach(dst_tmp[0], mem);
        bm_image_get_device_mem(dst[1], mem);
        bm_image_attach(dst_tmp[1], mem);
        bm_image_get_device_mem(dst[2], mem);
        bm_image_attach(dst_tmp[2], mem);
        bm_image_get_device_mem(dst[3], mem);
        bm_image_attach(dst_tmp[3], mem);
        bmcv_image_vpp_convert(handle, 4, src[0], dst_tmp, rect);
        bmcv_image_storage_convert(handle, 4, src, dst_tmp);
    }

    for (int i = 0; i < 4; i++) {
        bm_image_destroy(src[i]);
        bm_image_destroy(dst[i]);
        bm_image_destroy(dst_tmp[i]);
    }
    free(input_ptr);

}

#endif

int main(int argc, char *argv[]) {
    int test_loop_times = 0;

    if (NULL != getenv("BMCV_TEST_FILE_PATH")) {
        strcpy(opencvFile_path, getenv("BMCV_TEST_FILE_PATH"));
    }

    if (argc == 1) {
        test_loop_times = 1;
    } else {
        test_loop_times = atoi(argv[1]);
    }
    std::cout << "[TEST CV VPP] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < 1; loop_idx++) {
        std::cout << "------[TEST CV VPP] LOOP " << loop_idx << "------"
                  << std::endl;
        int         dev_id = 0;
        bm_status_t ret    = bm_dev_request(&handle, dev_id);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            exit(-1);
        }
#ifndef USING_CMODEL
        test_vpp_loop(test_loop_times);
#endif

        bm_dev_free(handle);
    }
    std::cout << "------[TEST CV VPP] ALL TEST PASSED!" << std::endl;

    return 0;
}
