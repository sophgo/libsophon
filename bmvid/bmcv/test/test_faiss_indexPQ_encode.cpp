#include "test_misc.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cassert>
#include <sys/time.h>
#include <cstring>
#include "bmcv_api_ext.h"

#define TEST_WITH_FAISS 0
#if TEST_WITH_FAISS
#include <faiss/IndexPQ.h>
#include <faiss/MetricType.h>
#endif

int main() {
    int vec_dims = 256;
    int encode_vec_num = 100;
    int database_num = 1000;
    int slice_m = 32;
    int ksub = 256;
    int dsub = vec_dims / slice_m;
    int input_dtype = 5; // 5: float
    int IP_metric = 0;

    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    unsigned int seed = tp.tv_nsec;
    srand(seed);
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        std::cerr << "Request device failed" << std::endl;
        return BM_ERR_FAILURE;
    }

    std::vector<float> database_fp32(database_num * vec_dims);
    std::vector<float> centroids_input_sys_fp32(slice_m * ksub * dsub);
    std::vector<float> nxcodes_input_sys(encode_vec_num * vec_dims);
    std::vector<unsigned char> output_codes_sys(encode_vec_num * slice_m);
    std::vector<unsigned char> output_codes_sys_cpu(encode_vec_num * slice_m);

    for (int i = 0; i < database_num; ++i) {
        for (int j = 0; j < vec_dims; ++j) {
            database_fp32[i * vec_dims + j] = static_cast<float>(rand()) / RAND_MAX * 20.0f - 10.0f;
        }
    }

    for (int i = 0; i < slice_m; ++i) {
        for (int j = 0; j < ksub; ++j) {
            for (int n = 0; n < dsub; ++n) {
                centroids_input_sys_fp32[i * ksub * dsub + j * dsub + n] = static_cast<float>(rand()) / RAND_MAX * 20.0f - 10.0f;
            }
        }
    }

    for (int i = 0; i < encode_vec_num; ++i) {
        for (int j = 0; j < vec_dims; ++j) {
            nxcodes_input_sys[i * vec_dims + j] = static_cast<float>(rand()) / RAND_MAX * 20.0f - 10.0f;
        }
    }

    #if TEST_WITH_FAISS
    //faiss::IndexPQ index;
    if(IP_metric) {
        faiss::IndexPQ index(vec_dims, slice_m, 8, faiss::METRIC_INNER_PRODUCT);
        std::cout << "Start training" << std::endl;
        index.pq.verbose = 1;
        index.train(database_num, database_fp32.data());
        std::cout << "Start computing codes" << std::endl;
        index.pq.compute_codes(nxcodes_input_sys.data(), output_codes_sys_cpu.data(), encode_vec_num);
        std::memcpy(centroids_input_sys_fp32.data(), index.pq.centroids.data(), slice_m * ksub * dsub * sizeof(float));
    } else {
        faiss::IndexPQ index(vec_dims, slice_m, 8, faiss::METRIC_L2);
        std::cout << "Start training" << std::endl;
        index.pq.verbose = 1;
        index.train(database_num, database_fp32.data());
        std::cout << "Start computing codes" << std::endl;
        index.pq.compute_codes(nxcodes_input_sys.data(), output_codes_sys_cpu.data(), encode_vec_num);
        std::memcpy(centroids_input_sys_fp32.data(), index.pq.centroids.data(), slice_m * ksub * dsub * sizeof(float));
    }

    std::cout << "Output codes (CPU):" << std::endl;
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < slice_m; ++j) {
            std::cout << static_cast<unsigned>(output_codes_sys_cpu[i * slice_m + j]) << ", ";
            if (((j + 1) % 10) == 0) {
                std::cout << std::endl;
            }
        }
    }
    std::cout << std::endl;
    #endif

    int centroids_size = slice_m * ksub * dsub * dtype_size((data_type_t)input_dtype);
    int nxcodes_size = encode_vec_num * vec_dims * dtype_size((data_type_t)input_dtype);
    int buffer_table_size = encode_vec_num * slice_m * ksub * dtype_size((data_type_t)input_dtype);
    int output_codes_size = encode_vec_num * slice_m;
    bm_device_mem_t centroids_input_dev, nxcodes_input_dev, buffer_table_dev, codes_output_dev;

    bm_malloc_device_byte(handle, &centroids_input_dev, centroids_size);
    bm_malloc_device_byte(handle, &nxcodes_input_dev, nxcodes_size);
    bm_malloc_device_byte(handle, &buffer_table_dev, buffer_table_size);
    bm_malloc_device_byte(handle, &codes_output_dev, output_codes_size);

    bm_memcpy_s2d(handle, centroids_input_dev, centroids_input_sys_fp32.data());
    bm_memcpy_s2d(handle, nxcodes_input_dev, nxcodes_input_sys.data());

    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    ret = bmcv_faiss_indexPQ_encode(handle,
                                    nxcodes_input_dev,
                                    centroids_input_dev,
                                    buffer_table_dev,
                                    codes_output_dev,
                                    encode_vec_num,
                                    vec_dims,
                                    slice_m,
                                    ksub,
                                    IP_metric);
    gettimeofday(&t2, NULL);
    printf("TPU time (us): %ld(us)\n", (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec);
    printf("TPU time (ms): %ld(um)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) / 1000);

    if (ret != BM_SUCCESS) {
        bm_free_device(handle, centroids_input_dev);
        bm_free_device(handle, nxcodes_input_dev);
        bm_free_device(handle, buffer_table_dev);
        bm_free_device(handle, codes_output_dev);
        return BM_ERR_FAILURE;
    }

    bm_memcpy_d2s(handle, output_codes_sys.data(), codes_output_dev);

    printf("Output codes (TPU):\n");
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < slice_m; ++j) {
            printf("%d, ", static_cast<unsigned>(output_codes_sys[i * slice_m + j]));
            if (((j + 1) % 10) == 0) {
                printf("\n");
            }
        }
    }
    printf("\n");

    bm_free_device(handle, centroids_input_dev);
    bm_free_device(handle, nxcodes_input_dev);
    bm_free_device(handle, buffer_table_dev);
    bm_free_device(handle, codes_output_dev);

    return 0;
}