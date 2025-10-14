#include <iostream>
#include <vector>
#include "bmruntime.h"
#include "bmcv_api_ext_c.h"
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

static int read_bin(const char *input_path, unsigned char *input_data, int width, int height, int channel) {
    FILE *fp_src = fopen(input_path, "rb");
    if (fp_src == NULL) {
        printf("Unable to open input file %s: %s\n", input_path, strerror(errno));
        return -1;
    }
    if(fread(input_data, sizeof(unsigned char), width * height * channel, fp_src) != 0) {
        printf("read image success\n");
    }
    fclose(fp_src);
    return 0;
}

static int write_bin(const char *output_path, unsigned char *output_data, int width, int height, int channel) {
    FILE *fp_dst = fopen(output_path, "wb");
    if (fp_dst == NULL) {
        printf("Unable to open output file %s\n", output_path);
        return -1;
    }
    if(fwrite(output_data, sizeof(unsigned char), width * height * channel, fp_dst) != 0) {
        printf("write image success\n");
    }
    fclose(fp_dst);
    return 0;
}

int main(int argc, char* args[]) {
    bm_handle_t bm_handle;
    bm_status_t ret = bm_dev_request(&bm_handle, 0);
    if (ret != BM_SUCCESS) {
        printf("create bm_handle failed\n");
        return -1;
    }
    int width = 1920;
    int height = 1080;
    int channel = 3;
    const char* input_path = "rgbplanar_1920_1080.bin";
    const char* output_path = "bmodel_output_gaussian_blur.bin";

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage: %d\n", argc);
        printf("%s width height channel input_path output_path\n", args[0]);
        return 0;
    }

    if (argc > 1) width = atoi(args[1]);
    if (argc > 2) height =  atoi(args[2]);
    if (argc > 3) channel =  atoi(args[3]);
    if (argc > 4) input_path = args[4];
    if (argc > 5) output_path =  args[5];

    unsigned char* input_data = (unsigned char*)malloc(width * height * channel);
    unsigned char* output_data = (unsigned char*)malloc(width * height * channel);
    bm_tensor_t in_tensors[1], out_tensors[1];
    const char **network_names = NULL;
    struct timeval t1, t2;
    void *p_bmrt = bmrt_create(bm_handle);
    if (p_bmrt == NULL) {
        std::cerr << "Failed to create bmrt" << std::endl;
        return -1;
    }
    gettimeofday(&t1, NULL);
    bool val = bmrt_load_bmodel(p_bmrt, "gaussian_blur_f16_1080p_3channel.bmodel");
    if (!val) {
        std::cerr << "Failed to load bmodel" << std::endl;
        return -1;
    }
    gettimeofday(&t2, NULL);
    int using_time = ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec);
    printf("bmodel gaussian_blur bmrt_load_bmodel using time: %d(us)\n", using_time);
    bmrt_get_network_names(p_bmrt, &network_names);
    if (network_names == NULL) {
        std::cerr << "Failed to get network names" << std::endl;
        return -1;
    }
    gettimeofday(&t1, NULL);
    const bm_net_info_t* net_info = bmrt_get_network_info(p_bmrt, *network_names);
    if (net_info == NULL) {
        std::cerr << "Failed to get network info" << std::endl;
        return -1;
    }
    gettimeofday(&t2, NULL);
    using_time = ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec);
    printf("bmodel gaussian_blur bmrt_get_network_info using time: %d(us)\n", using_time);
    float* input_float = (float*)malloc(width * height * channel * sizeof(float));
    float* output_float = (float*)malloc(width * height * channel * sizeof(float));
    read_bin(input_path, input_data, width, height, channel);
    for (int i = 0; i < width * height * channel; ++i) {
        input_float[i] = input_data[i] / 255.0f;
    }
    in_tensors[0].st_mode = BM_STORE_1N;
    in_tensors[0].st_mode = BM_STORE_1N;
    bm_malloc_device_byte(bm_handle, &in_tensors[0].device_mem, width * height * channel * sizeof(float));
    bm_malloc_device_byte(bm_handle, &out_tensors[0].device_mem, width * height * channel * sizeof(float));
    int stage_idx = 0;
    in_tensors[0].shape = net_info->stages[stage_idx].input_shapes[0];
    in_tensors[0].dtype = BM_FLOAT32;
    bm_memcpy_s2d_partial(bm_handle, in_tensors[0].device_mem, (void*)input_float, width * height * channel * sizeof(float));

    gettimeofday(&t1, NULL);
    val = bmrt_launch_tensor(p_bmrt, *network_names, in_tensors, net_info->input_num, out_tensors, net_info->output_num);
    if (!val) {
        std::cerr << "Failed to launch tensor" << std::endl;
        return -1;
    }
    gettimeofday(&t2, NULL);
    using_time = ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec);
    printf("bmodel gaussian_blur bmrt_launch_tensor using time: %d(us)\n", using_time);
    bm_memcpy_d2s_partial(bm_handle, (void *)output_float, out_tensors[0].device_mem, width * height * channel * sizeof(float));
    for (int i = 0; i < width * height * channel; ++i) {
        output_data[i] = (unsigned char)(output_float[i] * 255.0f + 0.5f);
    }
    write_bin(output_path, output_data, width, height, channel);
    free(input_data);
    free(output_data);
    free(input_float);
    free(output_float);
    bm_free_device(bm_handle, in_tensors[0].device_mem);
    bm_free_device(bm_handle, out_tensors[0].device_mem);
    bmrt_destroy(p_bmrt);
    free(network_names);
    bm_dev_free(bm_handle);
    return 0;
}