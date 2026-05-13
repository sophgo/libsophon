#include <iostream>
#include <fstream>
#include <mutex>
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "md5.h"
#include <assert.h>
#ifdef __linux__
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

using namespace std;

extern void morph_ref_84x(unsigned char* input_data,
                            unsigned char* output_data,
                            int format, int img_w,
                            int img_h, int kw, int kh,
                            int op, int shape);

static void fill(
        unsigned char* input,
        int width,
        int channel,
        int height) {
    int count = 0;
    for (int i = 0; i < height * channel; i++) {
        for (int j = 0; j < width; j++) {
            input[i * width + j] = count;
            count++;
            if( count == 101 )
                count = 0;
        }
    }
}


static int morph_tpu(
        bm_handle_t handle,
        unsigned char* input,
        unsigned char* output,
        int format,   // 0:gray  1:rgb-planar  2:rgb-packed
        int height,
        int width,
        int op,
        int shape,
        int kh,
        int kw) {
    bm_status_t ret;

    ret = bmcv_open_cpu_process(handle);
    if (ret != BM_SUCCESS) {
        printf("BMCV enable CPU failed. ret = %d\n", ret);
        bm_dev_free(handle);
        return -1;
    }
    bm_device_mem_t kmem = bmcv_get_structuring_element(handle, (bmcv_morph_shape_t)shape, kw, kh);
    bm_image_format_ext fmt = format == 0 ? FORMAT_GRAY :
                              (format == 1 ? FORMAT_RGB_PLANAR : FORMAT_RGB_PACKED);
    bm_image img_i;
    bm_image img_o;
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_i);
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_o);
    bm_image_alloc_dev_mem(img_i);
    bm_image_alloc_dev_mem(img_o);
    unsigned char* in_ptr[3] = {input, input + height * width, input + 2 * height * width};
    bm_image_copy_host_to_device(img_i, (void **)(in_ptr));
    struct timeval t1, t2;
    gettimeofday_(&t1);
    if (op == 0)
        bmcv_image_erode(handle, img_i, img_o, kw, kh, kmem);
    else
        bmcv_image_dilate(handle, img_i, img_o, kw, kh, kmem);
    gettimeofday_(&t2);

    printf("morph TPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    unsigned char* out_ptr[3] = {output, output + height * width, output + 2 * height * width};
    bm_image_copy_device_to_host(img_o, (void **)out_ptr);
    bm_image_destroy(img_i);
    bm_image_destroy(img_o);
    bm_free_device(handle, kmem);
    ret = bmcv_close_cpu_process(handle);
    if (ret != BM_SUCCESS) {
        printf("BMCV disable CPU failed. ret = %d\n", ret);
        bm_dev_free(handle);
        return -1;
    }
    bm_dev_free(handle);
    return 0;
}

static int cmp(
        unsigned char* got,
        unsigned char* exp,
        int len) {
    for (int i = 0; i < len; i++) {
        if (got[i] != exp[i]) {
            printf("cmp error: idx=%d  exp=%d  got=%d\n", i, exp[i], got[i]);
            return -1;
        }
    }
    return 0;
}

static string  unsignedCharToHex(unsigned char ch[16]){
    const char hex_chars[] = "0123456789abcdef";
    string result = "";
    for(int i = 0; i < 16; i++){
        unsigned int highHalfByte = (ch[i] >> 4) & 0x0f;
        unsigned int lowHalfByte = (ch[i] & 0x0f);
        result += hex_chars[highHalfByte];
        result += hex_chars[lowHalfByte];
    }
    return result;
}

static int cmpv2(unsigned char* got, unsigned char* exp ,int width, int height, int channel){
    unsigned char* md5_tpuOut = new unsigned char[16];
    md5_get(got, (sizeof(unsigned char) * channel * height * width), md5_tpuOut);
    if(0 != strcmp((unsignedCharToHex(md5_tpuOut).c_str()), (const char*)exp)){
        printf("cmp error!\n");
        return -1;
    }
    return 0;
}

static int fill_img (unsigned char *input, int w, int h, int channel, int format)
{
    if (format == FORMAT_RGB_PACKED ||
        format == FORMAT_BGR_PACKED) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < 3 * w; x += 3) {
                input[y * 3 * w + x] = rand() % 256;
                input[y * 3 * w + x + 1] = rand() % 256;
                input[y * 3 * w + x + 2] = rand() % 256;
            }
        }
    } else {
        for (int c = 0; c < channel; c++) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    input[c * h * w + y * w + x] = rand() % 256; // rand() % 256
                }
            }
        }
    }

    return 0;
}

static int read_file(const char* path, unsigned char* input_data, int size)
{
    ifstream file_read((string(path)), ios :: in | ios :: binary);
    if(!file_read){
        printf("Error opening file\n");
        return -1;
    }

    file_read.read((char*)input_data, size);
    file_read.close();

    return 0;
}

static int write_file(const char* path, const unsigned char* output_data, int size)
{
    ofstream file_write(string(path), ios::out | ios::binary);
    if (!file_write) {
        printf("Error opening file for writing\n");
        return -1;
    }

    file_write.write(reinterpret_cast<const char*>(output_data), size);
    file_write.close();

    return 0;
}

static int array_cmp_u8(unsigned char *p_exp,
                        unsigned char *p_got,
                        int dsize,
                        int img_w,
                        int img_h,
                        const char *info_label,
                        unsigned char delta) {
    int idx = 0;

    for (int y = 0; y < img_h; y++) {
        for (int x = 0; x < img_w * dsize; x++) {
            idx = y * (img_w * dsize) + x;
            if ((int)fabs(p_exp[idx] - (int)p_got[ idx]) > delta) {
                printf("%s abs error at index %d exp %d got %d\n",
                    info_label,
                    idx,
                    p_exp[idx],
                    p_got[idx]);
                return -1;
            }
        }
    }

    return 0;
}

static int morph_tpu_84x(bm_handle_t handle,
                     unsigned char *input,
                     unsigned char *output,
                     int format,
                     int height,
                     int width,
                     int op,
                     int shape,
                     int kh, int kw)
{
    bm_status_t ret = BM_SUCCESS;

    bm_device_mem_t kmem = bmcv_get_structuring_element(handle, (bmcv_morph_shape_t)shape, kw, kh);

    bm_image img_i, img_o;

    ret = bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &img_i);
    if (BM_SUCCESS != ret) {
        printf("bm_image: img_i create failed\n");
        bm_free_device(handle, kmem);
        return -1;
    }

    ret = bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &img_o);
    if (BM_SUCCESS != ret) {
        printf("bm_image: img_o create failed\n");
        bm_free_device(handle, kmem);
        return -1;
    }

    ret = bm_image_alloc_dev_mem(img_i);
    if (BM_SUCCESS != ret) {
        printf("bm_image: img_i alloc dev mem failed\n");
        bm_free_device(handle, kmem);
        return -1;
    }

    ret = bm_image_alloc_dev_mem(img_o);
    if (BM_SUCCESS != ret) {
        printf("bm_image: img_o alloc dev mem failed\n");
        bm_image_destroy(img_i);
        bm_free_device(handle, kmem);
        return -1;
    }

    unsigned char *in_ptr[3] = {input, input + height * width, input + 2 * height * width};
    ret = bm_image_copy_host_to_device(img_i, (void**)(in_ptr));
    if (BM_SUCCESS != ret) {
        printf("bm_image: img_i copy S2D failed\n");
        bm_image_destroy(img_i);
        bm_image_destroy(img_o);
        bm_free_device(handle, kmem);
        return -1;
    }

    ret = bmcv_image_dilate(handle, img_i, img_o, kw, kh, kmem);

    struct timeval t1, t2;
    gettimeofday_(&t1);
        if (op == 0)
            ret = bmcv_image_erode(handle, img_i, img_o, kw, kh, kmem);
        else
            ret = bmcv_image_dilate(handle, img_i, img_o, kw, kh, kmem);
    gettimeofday_(&t2);
    printf("[TPU %s] using time: %ld(us)\n",
            (op == 0 ? "ERODE" : "DILATE"), ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));

    if (ret != BM_SUCCESS) {
        printf(" %s failed\n", (op == 0 ? "bmcv_image_erode" : "bmcv_image_dilate"));
        bm_image_destroy(img_i);
        bm_image_destroy(img_o);
        bm_free_device(handle, kmem);
        return -1;
    }

    unsigned char *out_ptr[3] = {output, output + height * width, output + 2 * height * width};
    ret = bm_image_copy_device_to_host(img_o, (void**)(out_ptr));
    if (BM_SUCCESS != ret) {
        printf("bm_image: img_i copy S2D failed\n");
        bm_image_destroy(img_i);
        bm_image_destroy(img_o);
        bm_free_device(handle, kmem);
        return -1;
    }

    bm_image_destroy(img_i);
    bm_image_destroy(img_o);
    bm_free_device(handle, kmem);
    if (ret != BM_SUCCESS) {
        printf("BMCV disable CPU failed. ret = %d\n", ret);
        return -1;
    }
    return ret;
}

static int test_morph_random(bm_handle_t handle,
                             int format,
                             int height,
                             int width) {
    int kh = 2;
    int kw = 4;
    int shape = 2;
    int op = 1;
    struct timespec tp;
    clock_gettime_(0, &tp);

    unsigned int seed = tp.tv_nsec;
    srand(seed);
    printf("seed = %d\n", seed);
    string op_str[2] = {"erode", "dilate"};
    string shape_str[3] = {"RECT", "CROSS", "ELLIPSE"};
    string format_str[3] = {"gray", "rgb-planar", "rgb-packed"};
    printf("op: %s\n", op_str[op].c_str());
    printf("format: %s\n", format_str[op].c_str());
    printf("width: %d, height: %d\n", width, height);
    printf("kh: %d, kw: %d, shape: %s\n", kh, kw, shape_str[shape].c_str());

    int channel = format == 0 ? 1 : 3;
    printf("channel: %d\n", channel);
    unsigned char* input_data = new unsigned char [width * height * channel];
    unsigned char* output_tpu = new unsigned char [width * height * channel];
    unsigned char* output_opencv = new unsigned char [width * height * channel];
    fill(input_data, channel, width, height);
    morph_tpu(
        handle,
        input_data,
        output_tpu,
        format,
        height,
        width,
        op,
        shape,
        kh,
        kw);
    int ret = 1;
    switch (format)
    {
    case 0:
        {
            unsigned char ocv_gary_md5[] = "b9a49341e0bba7160f91920e0ae819b4";
            ret = cmpv2(output_tpu, ocv_gary_md5, width, height, channel);
            if(ret != 0)
                return -1;
            break;
        }
    case 1:
        {
            unsigned char ocv_rgb_planar_md5[] = "d2df74c0400933739f8e6c82e690eece";
            ret = cmpv2(output_tpu, ocv_rgb_planar_md5, width, height, channel);
            if(ret != 0)
                return -1;
            break;
        }
    case 2:
        {
            unsigned char ocv_rgb_packed_md5[] = "a8e285e625d86b87a5c3421789b1ef4f";
            ret = cmpv2(output_tpu, ocv_rgb_packed_md5, width, height, channel);
            if(ret != 0)
                return -1;
            break;
        }
    default:
        break;
    }
    delete [] input_data;
    delete [] output_tpu;
    delete [] output_opencv;
    return ret;
}

int test_morph_random_84X(bm_handle_t handle,
                          int img_w, int img_h,
                          int kh, int kw,
                          int op, int shape,
                          int format,
                          int use_real_img,
                          char *img_name,
                          char *dst_name)
{
    int ret = 0;
    string op_str[2] = {"erode", "dilate"};
    string shape_str[3] = {"RECT", "CROSS", "ELLIPSE"};
    string format_str[3] = {"gray", "rgb-packed", "rgb-planar"};
    printf("op %d, format %d, width %d, height %d\n", op, format, img_w, img_h);

    printf("===== TEST INFO =====\n");
    printf("op: %s\n", op_str[op].c_str());
    printf("format: %s\n", format_str[format].c_str());
    printf("width: %d, height: %d\n", img_w, img_h);
    printf("kh: %d, kw: %d, shape: %s\n", kh, kw, shape_str[shape].c_str());

    int channel = format == 0 ? 1 : 3;

    int img_format = FORMAT_GRAY;
    if (format == 1)
        img_format = FORMAT_RGB_PACKED;
    else if (format == 2)
        img_format = FORMAT_RGB_PLANAR;

    unsigned char *input_data = new unsigned char [img_w * img_h * channel];
    unsigned char *output_tpu = new unsigned char [img_w * img_h * channel];
    unsigned char *output_cpu = new unsigned char [img_w * img_h * channel];

    if (use_real_img) {
        read_file(img_name, input_data, img_w * img_h * channel);
    } else {
        fill_img(input_data, img_w, img_h, channel, img_format);
    }
/* Calculate REF */
    struct timeval t1, t2;
    gettimeofday_(&t1);
    morph_ref_84x(input_data, output_cpu,
              img_format,
              img_w,
              img_h,
              kw,
              kh,
              op,
              shape);
    gettimeofday_(&t2);
    printf("[CPU %s] using time: %ld(us)\n",
            (op == 0 ? "ERODE" : "DILATE"), ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));

    /* Calculate TPU */
    ret = morph_tpu_84x(handle,
                    input_data,
                    output_tpu,
                    img_format,
                    img_h,
                    img_w,
                    op,
                    shape,
                    kh, kw);

    if (BM_SUCCESS != ret) {
        printf("morph tpu failed\n");
        delete [] input_data;
        delete [] output_tpu;
        delete [] output_cpu;
    }

    ret = array_cmp_u8(output_cpu, output_tpu,
                       sizeof(unsigned char),
                       (img_format == FORMAT_BGR_PACKED || img_format == FORMAT_RGB_PACKED ? 3 * img_w : img_w),
                       img_h,
                       (op == 0 ? "erode" : "dilate"), 0);

    if (ret) {
        printf("[%s]: img_size %dx%d failed!\n",
                (op == 1 ? "MORPH_DILATE" : "MORPH_ERODE"), img_w, img_h);
        delete [] input_data;
        delete [] output_tpu;
        delete [] output_cpu;
        return -1;
    }

    if (use_real_img) {
        write_file(dst_name, output_tpu, img_w * img_h * channel);
    }

    printf("[%s]: img_size %dx%d successful!\n",
                (op == 1 ? "MORPH_DILATE" : "MORPH_ERODE"), img_w, img_h);

    delete [] input_data;
    delete [] output_tpu;
    delete [] output_cpu;

    return ret;
}


int main(int argc, char* args[])
{
    int ret = 0;
    int loop = 2;
    int use_real_img = 0;
    int height = 4 + rand() % (8192 - 4);
    int width = 4 + rand() % (7200 - 4);
    int format[3] = {0, 1, 2};
    int op = rand() % 2;
    int kw = 7, kh = 7;
    int shape = 0; // 0 : BM_MORPH_RECT; 1 : BM_MORPH_CROSS; 2:BM_MORPH_ELLIPSE
    char *src_img_name = NULL;
    char *dst_name = NULL;
    int format_index = 0;

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("84X usage: %d\n", argc);
        printf("%s loop use_real_img height width kw kh format(0:GRAY; 1:RGB PACKED; 2:RGB PLANAR) op (0:erode; 1: dilate) src_name dst_name(when use_real_img = 1,need to set src_name and dst_name)) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 1 0\n", args[0]);
        printf("%s 1 0 1080 1920 5 5 \n", args[0]);
        printf("%s 1 1 1080 1920 5 5 1 0 input.bin output.bin\n", args[0]);
        return 0;
    }

    if (argc > 1) loop = atoi(args[1]);
    if (argc > 2) use_real_img = atoi(args[2]);
    if (argc > 3) height = atoi(args[3]);
    if (argc > 4) width = atoi(args[4]);
    if (argc > 5) kw = atoi(args[5]);
    if (argc > 6) kh = atoi(args[6]);
    if (argc > 7) format_index = atoi(args[7]);
    if (argc > 8) op = atoi(args[8]);
    if (argc > 9) src_img_name = args[9];
    if (argc > 10) dst_name = args[10];

    bm_handle_t handle;
    if (BM_SUCCESS != bm_dev_request(&handle, 0)) {
        printf("get handle failed! \n");
        return -1;
    }

    unsigned int chipid = 0x1686;
    bm_get_chipid(handle, &chipid);
    if (chipid == 0x1684) {
        height = 1080;
        width = 1920;
        for (int i = 0; i < loop; i++) {
            ret = test_morph_random(handle, format[i], height, width);
            if (ret) {
                printf("test morph failed\n");
                bm_dev_free(handle);
                return ret;
            }
        }
        printf("Compare TPU result with OpenCV successfully!\n");
    } else if (chipid == 0x1686){
        for (int i = 0; i < loop; i++) {
            if (i != 0) {
                op = rand() % 2;
                height = 4 + rand() % (8192 - 4);
                width = 4 + rand() % (7200 - 4);
                format_index = rand() % 3;
            }

            ret = test_morph_random_84X(handle, width, height, kh, kw, op, shape, format[format_index],
                                        use_real_img, src_img_name,
                                        dst_name);
            if (ret) {
                printf("===== loop %d test morph_random failed =====\n", i);
                bm_dev_free(handle);
                return -1;
            }
            printf("===== loop %d test morph succed =====\n", i);
        }
    }

    bm_dev_free(handle);
    return 0;
}

// 84: test performance:
// 1080P  erode  rectangle-kernel: 3X3
// gray:       850um
// rgb-planar: 1950um
// rgb-packed: 1440um

