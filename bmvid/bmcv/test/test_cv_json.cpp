#include <iostream>
#include <vector>
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "stdio.h"
#include "stdlib.h"
#include <string.h>

#ifdef __linux__
#include <dlfcn.h>
#else
#include <windows.h>
#include <winsock.h>
#endif

static bm_handle_t handle;
#include <memory>
#define UNUSED_VARIABLE(x)  ((x) = (x))
char opencvFile_path[200] = "./";

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

static void print_binary(const char *file) {
    FILE *   fp           = fopen(file, "rb");
    uint32_t data_offset  = 0;
    uint32_t width        = 0;
    uint32_t height       = 0;
    uint32_t image_format = 0;
    uint32_t data_type    = 0;
    uint32_t plane_num    = 0;

    uint32_t pitch_stride[4] = {0};
    uint64_t size[4]         = {0};

    size_t cnt = fread(&data_offset, sizeof(uint32_t), 1, fp);
    cnt = fread(&width, sizeof(uint32_t), 1, fp);
    cnt = fread(&height, sizeof(uint32_t), 1, fp);
    cnt = fread(&image_format, sizeof(uint32_t), 1, fp);
    cnt = fread(&data_type, sizeof(uint32_t), 1, fp);
    cnt = fread(&plane_num, sizeof(uint32_t), 1, fp);

    cnt = fread(size, sizeof(size), 1, fp);
    cnt = fread(pitch_stride, sizeof(pitch_stride), 1, fp);

    uint32_t channel_stride[4] = {0};
    uint32_t batch_stride[4]   = {0};
    uint32_t meta_data_size[4] = {0};

    uint32_t N[4] = {0};
    uint32_t C[4] = {0};
    uint32_t H[4] = {0};
    uint32_t W[4] = {0};

    cnt = fread(channel_stride, sizeof(channel_stride), 1, fp);
    cnt = fread(batch_stride, sizeof(batch_stride), 1, fp);
    cnt = fread(meta_data_size, sizeof(meta_data_size), 1, fp);

    cnt = fread(N, sizeof(N), 1, fp);
    cnt = fread(C, sizeof(C), 1, fp);
    cnt = fread(H, sizeof(H), 1, fp);
    cnt = fread(W, sizeof(W), 1, fp);

    fseek(fp, data_offset, SEEK_SET);
    std::vector<std::unique_ptr<unsigned char[]>> host_ptr;
    host_ptr.resize(plane_num);
    void* void_ptr[4] = {0};
    for (uint32_t i = 0; i < plane_num; i++) {
        host_ptr[i] =
            std::unique_ptr<unsigned char[]>(new unsigned char[size[i]]);
        void_ptr[i] = host_ptr[i].get();
        cnt = fread(host_ptr[i].get(), 1, size[i], fp);
    }
    UNUSED_VARIABLE(cnt);
    fclose(fp);
    std::cout << "image width " << width << " image height " << height
              << " image format " << image_format << " data type " << data_type
              << " plane num " << plane_num << std::endl;
    for (uint32_t i = 0; i < plane_num; i++) {
        std::cout << "plane" << i << " size " << size[i] << " C " << C[i]
                  << " H " << H[i] << " W " << W[i] << " stride "
                  << pitch_stride[i] << std::endl;
    }
    bm_image recover;
    bm_image_create(handle,
                    height,
                    width,
                    (bm_image_format_ext)image_format,
                    (bm_image_data_format_ext)data_type,
                    &recover,
                    (int *)pitch_stride);
    bm_image_copy_host_to_device(recover, (void **)&void_ptr);
    bm_image_write_to_bmp(recover, "recover.bmp");
    bm_image_destroy(recover);
}

static void test_cv_debug_binary() {
    int            IMAGE_H     = 1080;
    int            IMAGE_W     = 1920;
    unsigned char *input_ptr = NULL;
    int src_len;
    int            plane0_size = IMAGE_H * IMAGE_W;
    bm_image       src;
    bm_image_create(
        handle, IMAGE_H, IMAGE_W, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &src);
    bm_image_alloc_dev_mem_heap_mask(src, 6);

    read_image(&input_ptr, &src_len,"girl_1920x1080_nv12.bin");
    void *buf[4] = {(void *)input_ptr, (void *)(input_ptr + plane0_size), 0, 0};
    UNUSED(buf);
    bm_image_copy_host_to_device(src, buf);
    bmcv_debug_savedata(src, "src.bin");

    print_binary("src.bin");
    bm_image_destroy(src);
    free(input_ptr);

}
int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    if (NULL != getenv("BMCV_TEST_FILE_PATH")) {
        strcpy(opencvFile_path, getenv("BMCV_TEST_FILE_PATH"));
    }

    int dev_id = 0;
    bm_dev_request(&handle, dev_id);
    test_cv_debug_binary();

    bm_dev_free(handle);

    return 0;
}
