#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <cmath>
#include "bmcv_api.h"
#include "test_misc.h"
#ifdef __linux__
  #include <pthread.h>
  #include <unistd.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

using namespace std;

#define CEHCK(x) { \
     if (x) {} \
     else {fprintf(stderr, "Check failed: %s, file %s, line: %d\n", #x, __FILE__, __LINE__); return -1; } \
}

#define EPS_ 1.0e-4

template<typename T>
int compare_result(const T* src1, const T* src2, int length)
{
    CEHCK(src1);
    CEHCK(src2);

    int count{0};
    for (int i = 0; i< length; i++){
        if (fabs(double(src1[i] - src2[i])) > EPS_){
            if (typeid(float).name() == typeid(T).name() || typeid(double).name() == typeid(T).name())
                fprintf(stderr, "Error:index:%d, val1: %f, val2:%f\n", i, double(src1[i]), double(src2[i]));
            else
                fprintf(stderr, "Error:index:%d, val1:%d, val2:%d\n", i, int(src1[i]), int(src2[i]));
            count++;
        }

        if (count >=100)
            return -1;
    }

    if (count == 0)
        fprintf(stdout, "they are equal\n");

    return 0;
}

int Laplacian_cpu(const unsigned char* src_, unsigned char* dst_, int width_, int height_, int ksize_)
{
	const int kernel_size{ 3 };
	std::vector<float> kernel;
	if (ksize_ == 1) kernel = { 0.f, 1.f, 0.f, 1.f, -4.f, 1.f, 0.f, 1.f, 0.f };
	else kernel = { 2.f, 0.f, 2.f, 0.f, -8.f, 0.f, 2.f, 0.f, 2.f };
	int new_width = width_ + kernel_size - 1, new_height = height_ + kernel_size - 1;
	std::unique_ptr<unsigned char[]> data(new unsigned char[new_width * new_height]);
	unsigned char* p = data.get();

	for (int y = 0; y < new_height; ++y) {
		if (y != 0 && y != new_height - 1) {
			for (int x = 0; x < new_width; ++x) {
				if (x == 0) {
					p[y * new_width + x] = src_[(y - 1) * width_ + 1];
				} else if (x == new_width - 1) {
					p[y * new_width + x] = src_[(y - 1) * width_ + (width_ - 1 - 1)];
				} else {
					p[y * new_width + x] = src_[(y - 1) * width_ + (x - 1)];
				}
			}
		}

		if (y == new_height - 1) {
			for (int x = 0; x < new_width; ++x) {
				p[y * new_width + x] = p[(y - 2) * new_width + x];
			}

			for (int x = 0; x < new_width; ++x) { // y = 0
				p[x] = p[2 * new_width + x];
			}
		}
	}

	for (int y = 1; y < new_height - 1; ++y) {
		for (int x = 1; x < new_width - 1; ++x) {
			float value{ 0.f };
			int count{ 0 };
			for (int m = -1; m <= 1; ++m) {
				for (int n = -1; n <= 1; ++n) {
					value += p[(y + m) * new_width + (x + n)] * kernel[count++];
				}
			}

			if (value < 0.) dst_[(y - 1) * width_ + (x - 1)] = 0;
			else if (value > 255.) dst_[(y - 1) * width_ + (x - 1)] = 255;
			else dst_[(y - 1) * width_ + (x - 1)] = static_cast<unsigned char>(value);
		}
	}

	return 0;
}

int main(int argc, char* argv[]) {
    int loop = 10;
    int ih = 1080;
    int iw = 1920;
    unsigned int ksize = 3;
    int random = 1;
    bm_image_format_ext fmt = FORMAT_GRAY;

    fmt = argc > 1 ? (bm_image_format_ext)atoi(argv[1]) : fmt;
    ih = argc > 2 ? atoi(argv[2]) : ih;
    iw = argc > 3 ? atoi(argv[3]) : iw;
    loop = argc > 4 ? atoi(argv[4]) : loop;
    ksize = argc >5 ? atoi(argv[5]) : ksize;
    random = argc > 6? atoi(argv[6]) : random;

    bm_status_t ret = BM_SUCCESS;
    bm_handle_t handle;
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS)
        throw("bm_dev_request failed");

    struct timeval t1, t2;

    for (int i = 0; i < loop; i++) {
        if (random) {
            struct timespec tp;
            clock_gettime_(0, &tp);

            unsigned int seed = tp.tv_nsec;
            srand(seed);
            cout << "seed = " << seed << endl;
            iw = 100 + rand() % 1900;
            ih = 100 + rand() % 2048;
        }

        cout << "the "<< i << " loop " <<"---------------parameter-------------" << endl;
        cout << "format: " << fmt << endl;
        cout << "input size: " << iw << " * " << ih << endl;
        cout << "ksize: " << ksize << endl;


        bm_image_data_format_ext data_type = DATA_TYPE_EXT_1N_BYTE;
        bm_image input;
        bm_image output;

        bm_image_create(handle, ih, iw, fmt, data_type, &input);
        bm_image_alloc_dev_mem(input);

        bm_image_create(handle,ih, iw, fmt, data_type, &output);
        bm_image_alloc_dev_mem(output);

        std::shared_ptr<unsigned char*> ch0_ptr = std::make_shared<unsigned char*>(new unsigned char[ih * iw]);
        std::shared_ptr<unsigned char*> tpu_res_ptr = std::make_shared<unsigned char *>(new unsigned char[ih * iw]);
        std::shared_ptr<unsigned char*> cpu_res_ptr = std::make_shared<unsigned char *>(new unsigned char[ih*iw]);
        for (int j = 0; j < ih * iw; j++) {
            (*ch0_ptr.get())[j] = rand() % 256;
        }

        unsigned char *host_ptr[] = {*ch0_ptr.get()};
        bm_image_copy_host_to_device(input, (void **)host_ptr);

        //cpu calculate
        Laplacian_cpu(*ch0_ptr.get(), *cpu_res_ptr.get(), iw, ih, ksize);

        gettimeofday_(&t1);
        ret = bmcv_image_laplacian(handle, input, output, ksize);
        gettimeofday_(&t2);
        cout << "image_laplacian using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec)  << "us" << endl;
        cout <<endl;

        if (ret) {
            cout << "test laplacian failed" << endl;
            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            return ret;
        } else {
            host_ptr[0] = *tpu_res_ptr.get();
            bm_image_copy_device_to_host(output, (void **)host_ptr);
            if (compare_result(*tpu_res_ptr.get(), *cpu_res_ptr.get(), iw*ih) ==0 ){
                std::cout <<"the " << i <<" loop: "<<"equal" <<endl;
            } else {
                std::cout <<"the " << i <<" loop: "<<"not equal" <<endl;
            }
        }

        bm_image_destroy(input);
        bm_image_destroy(output);
    }

    bm_dev_free(handle);

    return 0;
}
