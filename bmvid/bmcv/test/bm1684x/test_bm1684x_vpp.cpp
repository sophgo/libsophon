#include <iostream>
#include <vector>
#include "bmcv_api_ext.h"
#include "stdio.h"
#include "stdlib.h"
#include <sstream>
#include <string.h>

static bm_handle_t handle;
void bm1684x_vpp_write_bin(bm_image dst, const char *output_name);
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
static void test_vpp_yuv2rgb() {
    std::ostringstream os;
    int            IMAGE_H     = 1080;
    int            IMAGE_W     = 1920;
    int            plane0_size = IMAGE_H * IMAGE_W;
    bm_image       src, dst[256];
    unsigned char *input_ptr = NULL;
    int src_len;

    read_image(&input_ptr, &src_len,"girl_1920x1080_nv12.bin");

    bm_image_create(
        handle, IMAGE_H, IMAGE_W, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &src);
    bm_image_alloc_dev_mem(src);

    void *buf[4] = {(void *)input_ptr, (void *)(input_ptr + plane0_size), 0, 0};
    bm_image_copy_host_to_device(src, buf);

 std::vector<bmcv_rect_t> rects;
 int                      x = IMAGE_W / 2;
 int                      y = IMAGE_H / 2;
 for (int i = 0; i < 256; i++) {
         rects.push_back({i*2, i*2, x, y});
     }

 bmcv_rect_t *rect = &rects[0];
    for (int i = 0; i < 256; i++) {
        bm_image_create(handle,
                        IMAGE_H / 2,
                        IMAGE_W / 2,
                        FORMAT_BGR_PACKED,
                        DATA_TYPE_EXT_1N_BYTE,
                        dst + i);
        bm_image_alloc_dev_mem(dst[i]);
    }


    bmcv_image_vpp_convert(handle, 256, src, dst, rect);
 //   bmcv_image_vpp_convert(handle, 4, src, dst + 4);

    for (int i = 0; i < 256; i++) {
        std::ostringstream os;
        os << "yuv2rgb";
        os << "_BT601_";
        os << i;
        os << ".bgr24";
        bm1684x_vpp_write_bin(dst[i], os.str().c_str());
    }

    bmcv_image_vpp_csc_matrix_convert(handle, 1, src, dst, CSC_YCbCr2RGB_BT709);

    bm1684x_vpp_write_bin(dst[0], "yuv2rgb_BT709.bgr24");

    for (int i = 0; i < 8; i++) {
        bm_image_destroy(dst[i]);
    }

    bm_image_destroy(src);
    free(input_ptr);
}

static void test_vpp_yuv_crop_to_odd_num_height() {
    int            IMAGE_H     = 1080;
    int            IMAGE_W     = 1920;
    int            plane0_size = IMAGE_H * IMAGE_W;
    bm_image       src, dst, dst_bgr;
    unsigned char *input_ptr = NULL;
    int src_len;

    read_image(&input_ptr, &src_len,"girl_1920x1080_nv12.bin");

    bm_image_create(
        handle, IMAGE_H, IMAGE_W, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &src);
    bm_image_alloc_dev_mem(src);

    void *buf[4] = {(void *)input_ptr, (void *)(input_ptr + plane0_size), 0, 0};
    bm_image_copy_host_to_device(src, buf);

    bmcv_rect_t rect = {IMAGE_W / 2, 0, IMAGE_W / 2, IMAGE_H / 2};
    bmcv_rect_t rect2 = {0, IMAGE_H/2, IMAGE_W / 2, IMAGE_H / 2};
    bm_image_create(handle,
                    IMAGE_H / 4 - 1,
                    IMAGE_W / 2 - 1,
                    FORMAT_YUV420P,
                    DATA_TYPE_EXT_1N_BYTE,
                    &dst);
    bm_image_alloc_dev_mem(dst);
    bm_image_create(handle,
                    IMAGE_H / 2 - 1,
                    IMAGE_W / 4 - 1 ,
                    FORMAT_BGR_PACKED,
                    DATA_TYPE_EXT_1N_BYTE,
                    &dst_bgr);
    bm_image_alloc_dev_mem(dst_bgr);
    bmcv_image_vpp_convert(handle, 1, src, &dst, &rect);
    bmcv_image_vpp_convert(handle, 1, src, &dst_bgr, &rect2);

    bm1684x_vpp_write_bin(dst, "odd_yuv1.bgr24");
    bm1684x_vpp_write_bin(dst_bgr, "odd_yuv2.bgr24");

    bm_image_destroy(dst);
    bm_image_destroy(src);
    bm_image_destroy(dst_bgr);
    free(input_ptr);
}

static void test_vpp_compressed2rgb() {
    int IMAGE_H = 1080;
    int IMAGE_W = 1920;

    bm_image src, dst[32];
    bm_image_create(handle,
                    IMAGE_H,
                    IMAGE_W,
                    FORMAT_COMPRESSED,
                    DATA_TYPE_EXT_1N_BYTE,
                    &src);
    bm_device_mem_t mem[4];
    memset(mem, 0, sizeof(bm_device_mem_t) * 4);

    unsigned char * buf[4] = {NULL};
    int plane_size[4] = {0};

    read_image(&buf[0], &plane_size[0],"offset_base_y.bin");
    read_image(&buf[1], &plane_size[1],"offset_comp_y.bin");
    read_image(&buf[2], &plane_size[2],"offset_base_c.bin");
    read_image(&buf[3], &plane_size[3],"offset_comp_c.bin");

    for (int i = 0; i < 4; i++) {
        bm_malloc_device_byte(handle, mem + i, plane_size[i]);
        bm_memcpy_s2d(handle, mem[i], (void *)buf[i]);
    }
    bm_image_attach(src, mem);

    std::vector<bmcv_rect_t> rects;
    int                      x = IMAGE_W / 8;
    int                      y = IMAGE_H / 4;
    for (int x_ = 0; x_ < 8; x_++) {
        for (int y_ = 0; y_ < 4; y_++) {
            rects.push_back({x_ * x, y_ * y, x, y});
        }
    }
    int crop_num = rects.size();

    for (int i = 0; i < 32; i++) {
        bm_image_create(handle,
                        IMAGE_H / 2,
                        IMAGE_W / 2,
                        FORMAT_BGR_PACKED,
                        DATA_TYPE_EXT_1N_BYTE,
                        dst + i);
        bm_image_alloc_dev_mem(dst[i]);
    }
    bmcv_rect_t *rect = &rects[0];


    bmcv_image_vpp_convert(handle, crop_num, src, dst, rect, BMCV_INTER_LINEAR);
    for (int i = 0; i < 32; i++) {
        std::ostringstream os;
        os << "fbd";
        os << "_BT601_";
        os << i;
        os << ".bgr24";
        bm1684x_vpp_write_bin(dst[i], os.str().c_str());
    }

    bmcv_image_vpp_csc_matrix_convert(handle, 1, src, dst, CSC_YCbCr2RGB_BT709);
    bm1684x_vpp_write_bin(dst[0], "fbd_BT709.bgr24");

    for (int i = 0; i < 32; i++)
    {
        bm_image_destroy(dst[i]);
    }
    bm_image_destroy(src);
    for (int i = 0; i < 4; i++) {
        bm_free_device(handle, mem[i]);
    }
    free(buf[0]);
    free(buf[1]);
    free(buf[2]);
    free(buf[3]);

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
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST CV VPP] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    std::cout << "[TEST CV VPP] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST CV VPP] LOOP " << loop_idx << "------"
                  << std::endl;
        int         dev_id = 0;
        bm_status_t ret    = bm_dev_request(&handle, dev_id);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            exit(-1);
        }
#ifndef USING_CMODEL
        test_vpp_yuv2rgb();
        test_vpp_compressed2rgb();
        test_vpp_yuv_crop_to_odd_num_height();
#endif

        bm_dev_free(handle);
    }
    std::cout << "------[TEST CV VPP] ALL TEST PASSED!" << std::endl;

    return 0;
}
