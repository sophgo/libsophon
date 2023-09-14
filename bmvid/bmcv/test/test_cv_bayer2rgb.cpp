#include <iostream>
#include "bmcv_api_ext.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "test_misc.h"
#include <assert.h>
#include <vector>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

using namespace std;
#define KERNEL_SIZE 3 * 3 * 3 * 4 * 64
#define CONVD_MATRIX 12 * 9

const unsigned char convd_kernel[CONVD_MATRIX] = {1, 0, 1, 0, 0, 0, 1, 0, 1,
                                                0, 0, 2, 0, 0, 0, 0, 0, 2,
                                                0, 0, 0, 0, 0, 0, 2, 0, 2,
                                                0, 0, 0, 0, 0, 0, 0, 0, 4, // r R
                                                4, 0, 0, 0, 0, 0, 0, 0, 0, // b B
                                                2, 0, 2, 0, 0, 0, 0, 0, 0,
                                                2, 0, 0, 0, 0, 0, 2, 0, 0,
                                                1, 0, 1, 0, 0, 0, 1, 0, 1,
                                                0, 1, 0, 1, 0, 1, 0, 1, 0,
                                                0, 0, 0, 0, 0, 4, 0, 0, 0, // g1 G1
                                                0, 0, 0, 0, 0, 0, 0, 4, 0, // g2 G2
                                                0, 1, 0, 1, 0, 1, 0, 1, 0};

vector<vector<vector<float>>> kernel_r = {{{0.25, 0, 0.25}, {0, 0, 0}, {0.25, 0, 0.25}},
                                            {{0, 0, 0.5}, {0, 0, 0}, {0, 0, 0.5}},
                                            {{0, 0, 0}, {0, 0, 0}, {0.5, 0, 0.5}},
                                            {{0, 0, 0}, {0, 0, 0}, {0, 0, 1}}};
vector<vector<vector<float>>> kernel_b = {{{1, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                                            {{0.5, 0, 0.5}, {0, 0, 0}, {0, 0, 0}},
                                            {{0.5, 0, 0}, {0, 0, 0}, {0.5, 0, 0}},
                                            {{0.25, 0, 0.25}, {0, 0, 0}, {0.25, 0, 0.25}}};
vector<vector<vector<float>>> kernel_g = {{{0, 0.25, 0}, {0.25, 0, 0.25}, {0, 0.25, 0}},
                                            {{0, 0, 0}, {0, 0, 1}, {0, 0, 0}},
                                            {{0, 0, 0}, {0, 0, 0}, {0, 1, 0}},
                                            {{0, 0.25, 0}, {0.25, 0, 0.25}, {0, 0.25, 0}}};

int resultCompare(unsigned char* output_data, vector<vector<unsigned char>> dstImgCpu_r, vector<vector<unsigned char>> dstImgCpu_g, vector<vector<unsigned char>> dstImgCpu_b) {
    int height = dstImgCpu_r.size();
    int width  = dstImgCpu_r[0].size();

    for (int i = 0;i < height;i++)
        for (int j = 0;j < width;j++) {
            if (abs(dstImgCpu_r[i][j] - output_data[i * width + j]) > 1) {
                printf(" dstImgCpu_r , i = %d, j = %d\n", i, j);
                printf(" dstImgCpu_r = %u, output_tpu = %u\n", dstImgCpu_r[i][j], output_data[i * width + j]);
                return -1;
            }

            if (abs(dstImgCpu_g[i][j] - output_data[height * width + i * width + j]) > 1) {
                printf(" dstImgCpu_g , i = %d, j = %d\n", i, j);
                printf(" dstImgCpu_g = %u, output_tpu = %u\n", dstImgCpu_g[i][j], output_data[height * width + i * width + j]);
             return -1;
            }

            if (abs(dstImgCpu_b[i][j] - output_data[height * width * 2 + i * width + j]) > 1) {
                printf(" dstImgCpu_b , i = %d, j = %d\n", i, j);
                printf(" dstImgCpu_b = %u, output_tpu = %u\n", dstImgCpu_b[i][j], output_data[height * width * 2 + i * width + j]);
                return -1;
            }
        }

    return 0;
}

vector<vector<float>> gen_matrix_final(vector<vector<float>> vector_r, vector<vector<float>> vector_g1, vector<vector<float>> vector_g2, vector<vector<float>> vector_b)
{
    vector<vector<float>> vector_temp1(vector_r.size(), vector<float>(vector_r[0].size() * 2, 0));
    vector<vector<float>> vector_temp2(vector_r.size(), vector<float>(vector_r[0].size() * 2, 0));
    vector<vector<float>> vector_temp3(vector_r.size() * 2, vector<float>(vector_r[0].size() * 2, 0));

    for (size_t i = 0; i < vector_r.size(); ++i) {
        for (size_t j = 0; j < vector_r[i].size(); ++j) {
            vector_temp1[i][2 * j] = vector_r[i][j];
            vector_temp1[i][2 * j + 1] = vector_g1[i][j];
        }
    }

    for (size_t i = 0; i < vector_g2.size(); ++i) {
        for (size_t j = 0; j < vector_g2[i].size(); ++j) {
            vector_temp2[i][2 * j] = vector_g2[i][j];
            vector_temp2[i][2 * j + 1] = vector_b[i][j];
        }
    }

    for (size_t i = 0; i < vector_temp1.size(); ++i) {
        for (size_t j = 0; j < vector_temp1[i].size(); ++j) {
            vector_temp3[2 * i][j] = vector_temp1[i][j];
            vector_temp3[2 * i + 1][j] = vector_temp2[i][j];
        }
    }
    return vector_temp3;
}

vector<vector<unsigned char>> ReflectionPad2d(vector<vector<unsigned char>> srcImg, int height) {
    vector<unsigned char> second_row = srcImg[1];
    vector<unsigned char> last_second_row = srcImg[height - 2];

    // padding up & bottom
    srcImg.insert(srcImg.begin(), second_row);
    srcImg.push_back(last_second_row);

    // padding left & right
    for (auto& row : srcImg) {
        row.insert(row.begin(), row[1]);
    }

    for (auto& row : srcImg) {
        row.insert(row.end(), row[row.size() - 2]);
    }

    return srcImg;
}

vector<vector<unsigned char>> convolution_rb(const vector<vector<unsigned char>>& input, vector<vector<vector<float>>> kernel, int stride, int offset) {
    // output size
    int inputRows = input.size();
    int inputCols = input[0].size();

    // input size
    int outputRows = (inputRows - 3) / stride + 1;
    int outputCols = (inputCols - 3) / stride + 1;
    vector<vector<float>> input_float(input.size(), vector<float>(input[0].size()));
    vector<vector<float>> output_float(input.size() - 2, vector<float>(input[0].size() - 2));
    vector<vector<float>> output_rb(outputRows, vector<float>(outputCols, 0));
    vector<vector<float>> output_rg1(outputRows, vector<float>(outputCols, 0));
    vector<vector<float>> output_rg2(outputRows, vector<float>(outputCols, 0));
    vector<vector<float>> output_rr(outputRows, vector<float>(outputCols, 0));

    // uchar2float
    for (size_t i = 0; i < input.size(); ++i) {
        for (size_t j = 0; j < input[i].size(); ++j) {
            input_float[i][j] = static_cast<float>(input[i][j]);
        }
    }

    // r based on B
    for (int i = 0; i < outputRows; i++) {
        for (int j = 0; j < outputCols; j++) {
            for (int k = 0; k < 3; k++) {
                for (int l = 0; l < 3; l++) {
                    output_rb[i][j]  += (input_float[i * stride + k + offset][j * stride + l + offset] * kernel[0][k][l]);
                    output_rg1[i][j] += (input_float[i * stride + k + offset][j * stride + l + offset] * kernel[1][k][l]);
                    output_rg2[i][j] += (input_float[i * stride + k + offset][j * stride + l + offset] * kernel[2][k][l]);
                    output_rr[i][j]  += (input_float[i * stride + k + offset][j * stride + l + offset] * kernel[3][k][l]);
                }
            }
        }
    }

    // debayer
    output_float = gen_matrix_final(output_rb, output_rg1, output_rg2, output_rr);

    // float2uchar
    vector<vector<unsigned char>> uchar_output(output_float.size(), vector<unsigned char>(output_float[0].size()));
    for (size_t i = 0; i < output_float.size(); ++i) {
        for (size_t j = 0; j < output_float[i].size(); ++j) {
            uchar_output[i][j] = static_cast<unsigned char>(output_float[i][j]);
        }
    }

    return uchar_output;
}

// conv2d
vector<vector<unsigned char>> convolution_g(const vector<vector<unsigned char>>& input, vector<vector<vector<float>>> kernel, int stride, int offset) {
    // intput size
    int inputRows = input.size();
    int inputCols = input[0].size();

    // output size
    int outputRows = (inputRows - 3) / stride + 1;
    int outputCols = (inputCols - 3) / stride + 1;

    vector<vector<float>> input_float(input.size(), vector<float>(input[0].size()));
    vector<vector<float>> output_float(input.size() - 2, vector<float>(input[0].size() - 2));
    // uchar2float
    for (size_t i = 0; i < input.size(); ++i) {
        for (size_t j = 0; j < input[i].size(); ++j) {
            input_float[i][j] = static_cast<float>(input[i][j]);
        }
    }

    vector<vector<float>> output_rb(outputRows, vector<float>(outputCols, 0));
    vector<vector<float>> output_rg1(outputRows, vector<float>(outputCols, 0));
    vector<vector<float>> output_rg2(outputRows, vector<float>(outputCols, 0));
    vector<vector<float>> output_rr(outputRows, vector<float>(outputCols, 0));  // special

    // r based on B
    for (int i = 0; i < outputRows; i++) {
        for (int j = 0; j < outputCols; j++) {
            for (int k = 0; k < 3; k++) {
                for (int l = 0; l < 3; l++) {
                    output_rb[i][j]  += (input_float[i * stride + k + offset][j * stride + l + offset] * kernel[0][k][l]);
                    output_rg1[i][j] += (input_float[i * stride + k + offset][j * stride + l + offset] * kernel[1][k][l]);
                    output_rg2[i][j] += (input_float[i * stride + k + offset][j * stride + l + offset] * kernel[2][k][l]);
                    output_rr[i][j]  += (input_float[i * stride + k + 1][j * stride + l + 1] * kernel[3][k][l]);
                }
            }
        }
    }
    // debayer
    output_float = gen_matrix_final(output_rb, output_rg1, output_rg2, output_rr);

    // float2uchar
    std::vector<std::vector<unsigned char>> uchar_output(output_float.size(), std::vector<unsigned char>(output_float[0].size()));
    for (size_t i = 0; i < output_float.size(); ++i) {
        for (size_t j = 0; j < output_float[i].size(); ++j) {
            uchar_output[i][j] = static_cast<unsigned char>(output_float[i][j]);
        }
    }

    return uchar_output;
}

static int bayer2rgb_tpu(
        unsigned char* input,
        unsigned char* output,
        unsigned char* convd_kernel,
        int height,
        int width) {
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }

    bm_image input_img;
    bm_image output_img;
    bm_image_create(handle, height, width, FORMAT_BAYER, DATA_TYPE_EXT_1N_BYTE, &input_img);
    bm_image_create(handle, height, width, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &output_img);
    bm_image_alloc_dev_mem(output_img, BMCV_HEAP_ANY);

    unsigned char* input_data[3] = {input, input + height * width, input + 2 * height * width};
    bm_image_copy_host_to_device(input_img, (void **)input_data);

    bmcv_image_bayer2rgb(handle, convd_kernel, input_img, output_img);

    bm_image_copy_device_to_host(output_img, (void **)(&output));
    bm_image_destroy(input_img);
    bm_image_destroy(output_img);
    bm_dev_free(handle);
    return 0;
}

int test_bayer2rgb_random(int random, int height, int width) {

    if (random) {
        struct timespec tp;
        clock_gettime_(0, &tp);
        // bool fixed = false;
        int seed = tp.tv_nsec;
        srand(seed);
        cout << "seed = " << seed << endl;
        width = 8 + rand() % 1744;
        height = 8 + rand() % 1744;

        width = width % 2 == 0 ? width : width + 1;
        height = height % 2 == 0 ? height : height + 1;
    }

    cout << "height = " << height << ", width = " << width << endl;
    unsigned char* output_tpu = new unsigned char [width * height * 3];
    vector<unsigned char> input_data(height * width);
    vector<vector<unsigned char>> srcImg(height, vector<unsigned char>(width, 0));
    vector<vector<unsigned char>> srcImg_padding(height + 2, vector<unsigned char>(width + 2, 0));
    vector<vector<unsigned char>> dstImgCpu_r;
    vector<vector<unsigned char>> dstImgCpu_g;
    vector<vector<unsigned char>> dstImgCpu_b;
	unsigned char kernel_data[KERNEL_SIZE];
	memset(kernel_data, 0, KERNEL_SIZE);

	// fp32 to int8
	for (int i = 0;i < 12;i++) {
        for (int j = 0;j < 9;j++) {
            kernel_data[i * 9 * 64 + 64 * j] = convd_kernel[i * 9 + j];
        }
    }

    // constructing input data
    for (int i = 0;i < height;i++)
        for (int j = 0;j < width;j++) {
            input_data[i * width + j] = rand() % 255;  // rand() % 255
            srcImg[i][j] = input_data[i * width + j];
        }

    // cal the reference
    srcImg_padding = ReflectionPad2d(srcImg, height);
    dstImgCpu_r = convolution_rb(srcImg_padding, kernel_r, 2, 0);
    dstImgCpu_g = convolution_g(srcImg_padding, kernel_g, 2, 0);
    dstImgCpu_b = convolution_rb(srcImg_padding, kernel_b, 2, 1);

    bayer2rgb_tpu(
        input_data.data(),
        output_tpu,
        kernel_data,
        height,
        width);

    int ret = resultCompare(output_tpu, dstImgCpu_r, dstImgCpu_g, dstImgCpu_b);

    if (ret == 0)
        std::cout << " bayer2rgb successful! " << std::endl;
    else
        std::cout << " bayer2rgb failed! " << std::endl;

    return ret;
}

int main(int argc, char* args[]) {
    int random = 0;
    int loop = 1;
    int height = 580;
    int width = 84;
    if (argc > 1) random = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) height = atoi(args[3]);
    if (argc > 4) width = atoi(args[4]);
    int ret = 0;
    for (int i = 0; i < loop; i++) {
        ret = test_bayer2rgb_random(random, height, width);
        if (ret) {
            cout << "test bayer2rgb failed" << endl;
            return ret;
        }
    }
    cout << "Compare TPU result with CPU successfully!" << endl;
    return 0;
}
