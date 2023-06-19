#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "bmcv_api.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#ifdef __linux__
  #include <unistd.h>
  #include <pthread.h>
#else
  #include <windows.h>
#endif

#define MAX_SIZE (2048)
#define MIN_SIZE (32)

typedef struct {
    int trials;
} image_align_thread_arg_t;

typedef enum {
    MAX_SIZE_MODE = 0,
    MIN_SIZE_MODE,
    RAND_SIZE_MODE,
    MAX_RAND_MODE
} rand_mode_e;

template <typename T>
class Blob {
   public:
    T * data      = NULL;
    int elem_size = 0;
    int byte_size = 0;

    explicit Blob(int size) {
        if (size == 0) {
            data      = nullptr;
            byte_size = 0;
            return;
        }
        data      = new T[size];
        elem_size = size;
        byte_size = size * sizeof(T);
    }
    ~Blob() {
        if (data) {
            delete[] data;
            elem_size = 0;
            byte_size = 0;
        }
    }
};

template <typename T>
static bool res_ref_comp(T ** res_1n_planner,
                         T ** ref_1n_planner,
                         int  plane_num,
                         int *plane_len) {
    for (int c_idx = 0; c_idx < plane_num; c_idx++) {
        for (int idx = 0; idx < plane_len[c_idx]; idx++) {
            if (res_1n_planner[c_idx][idx] != ref_1n_planner[c_idx][idx]) {
                std::cout << " res: " << (int)res_1n_planner[c_idx][idx]
                          << " ref: " << (int)ref_1n_planner[c_idx][idx]
                          << " plane_num: " << c_idx << " idx: " << idx
                          << std::endl;

                return false;
            }
        }
    }

    return true;
}

static int get_plane_num(int image_format, int w, int h, int *plane_num) {
    switch (image_format) {
        case FORMAT_YUV420P: {

            break;
        }
        case FORMAT_YUV422P: {

            break;
        }
        case FORMAT_YUV444P: {
            break;
        }
        case FORMAT_NV24: {
            break;
        }
        case FORMAT_NV12:
        case FORMAT_NV21: {
            plane_num[0] = w * h;
            plane_num[1] = ALIGN(w, 2) * ALIGN(h, 2) / 2;
            break;
        }
        case FORMAT_NV16:
        case FORMAT_NV61: {
            break;
        }
        case FORMAT_GRAY: {
            break;
        }
        case FORMAT_COMPRESSED: {
            break;
        }
        case FORMAT_BGR_PACKED:
        case FORMAT_RGB_PACKED: {
            break;
        }
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PLANAR: {
            break;
        }
        default: {
            std::cout << "image format not support\n" << std::endl;

            return -1;
        }
    }
    return 0;
}

template <typename T>
static int image_align_test_rand(bm_handle_t handle,
                                 int         image_format,
                                 int         data_type,
                                 int         in_h,
                                 int         in_w,
                                 int         align_option = DO_HEIGHT_ALIGN) {
    UNUSED(align_option);
    int                      plane_len[3];
    int                      aligned_plane_len[3];
    std::shared_ptr<Blob<T>> src_ptr[3];
    std::shared_ptr<Blob<T>> dst_ptr[3];
    std::shared_ptr<Blob<T>> aligned_ptr[3];
    get_plane_num(image_format, in_w, in_h, plane_len);
    bm_image input, aligned_image, output;
    bm_image_create(handle,
                    in_h,
                    in_w,
                    (bm_image_format_ext)image_format,
                    (bm_image_data_format_ext)data_type,
                    &input);
    bm_image_alloc_dev_mem(input);
    T * src_data[3];
    T * dst_data[3];
    T * aligned_data[3];
    int plane_num = bm_image_get_plane_num(input);
    for (int i = 0; i < plane_num; i++) {
        src_ptr[i]  = std::make_shared<Blob<T>>(plane_len[i]);
        dst_ptr[i]  = std::make_shared<Blob<T>>(plane_len[i]);
        src_data[i] = src_ptr[i].get()->data;
        dst_data[i] = dst_ptr[i].get()->data;
        for (int idx = 0; idx < plane_len[i]; idx++) {
            src_data[i][idx] = rand() % 255;
        }
    }

    bm_image_create(handle,
                    in_h,
                    in_w,
                    (bm_image_format_ext)image_format,
                    (bm_image_data_format_ext)data_type,
                    &output);
    bm_image_alloc_dev_mem(output);
    bm_image_copy_host_to_device(input, (void **)src_data);
    if (BM_SUCCESS != bm_shape_align(input, &aligned_image, align_option)) {
        std::cout << "bmcv_image_align error !!!" << std::endl;
        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_image_destroy(aligned_image);

        exit(-1);
    }
    get_plane_num(image_format,
                  aligned_image.width,
                  aligned_image.height,
                  aligned_plane_len);
    for (int i = 0; i < plane_num; i++) {
        aligned_ptr[i]  = std::make_shared<Blob<T>>(aligned_plane_len[i]);
        aligned_data[i] = aligned_ptr[i].get()->data;
    }
    bm_image_copy_device_to_host(aligned_image, (void **)aligned_data);
    if (BM_SUCCESS != bm_shape_dealign(aligned_image, output, align_option)) {
        std::cout << "bmcv_image_align error !!!" << std::endl;
        bm_image_destroy(input);
        bm_image_destroy(output);

        exit(-1);
    }
    bm_image_copy_device_to_host(output, (void **)dst_data);
    if (false ==
        res_ref_comp<T>(src_data, aligned_data, plane_num, plane_len)) {
        std::cout << "aligned_data compare error!!!" << std::endl;
        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_image_destroy(aligned_image);

        exit(-1);
    }
    if (false == res_ref_comp<T>(src_data, dst_data, plane_num, plane_len)) {
        std::cout << "dst_data compare error!!!" << std::endl;
        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_image_destroy(aligned_image);

        exit(-1);
    }
    std::cout << "COMPARE PASSED" << std::endl;
    bm_image_destroy(input);
    bm_image_destroy(output);
    bm_image_destroy(aligned_image);

    return 0;
}

#ifdef __linux__
void *test_image_align_thread(void *arg) {
#else
DWORD WINAPI test_image_align_thread(LPVOID arg) {
#endif
    image_align_thread_arg_t *image_align_thread_arg =
        (image_align_thread_arg_t *)arg;
    int test_loop_times = image_align_thread_arg->trials;
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST IMAGE ALIGN] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    int         dev_id = 0;
    bm_handle_t handle;
    bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
    if (dev_ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", dev_ret);
        exit(-1);
    }
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST IMAGE ALIGN] LOOP " << loop_idx << "------"
                  << std::endl;
        unsigned int seed = (unsigned)time(NULL);
        std::cout << "seed: " << seed << std::endl;
        srand(seed);
        int in_w = 400;
        int in_h = 399;
        std::cout << "---------IMAGE ALIGN CLASSICAL SIZE TEST----------"
                  << std::endl;
        std::cout << "[IMAGE ALIGN TEST] nv12: " << std::endl;
        image_align_test_rand<unsigned char>(
            handle, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, in_h, in_w);
        std::cout << "----------IMAGE ALIGN TEST OVER---------" << std::endl;
    }
    bm_dev_free(handle);
    std::cout << "------[TEST IMAGE ALIGN] ALL TEST PASSED!" << std::endl;
    return NULL;
}

int main(int argc, char **argv) {
    int test_loop_times  = 0;
    int test_threads_num = 1;
    if (argc == 1) {
        test_loop_times  = 1;
        test_threads_num = 1;
    } else if (argc == 2) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = 1;
    } else if (argc == 3) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = atoi(argv[2]);
    } else {
        std::cout << "command input error, please follow this "
                     "order:test_image_align loop_num multi_thread_num"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST IMAGE ALIGN] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST IMAGE ALIGN] thread nums should be 1~4 " << std::endl;
        exit(-1);
    }
    #ifdef __linux__
    pthread_t *               pid = new pthread_t[test_threads_num];
    image_align_thread_arg_t *image_align_thread_arg =
        new image_align_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        image_align_thread_arg[i].trials = test_loop_times;  // dev_id 0
        if (pthread_create(&pid[i],
                           NULL,
                           test_image_align_thread,
                           &image_align_thread_arg[i])) {
            delete[] pid;
            delete[] image_align_thread_arg;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            delete[] image_align_thread_arg;
            perror("Thread join failed");
            exit(-1);
        }
    }
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] pid;
    delete[] image_align_thread_arg;
    #else
    #define THREAD_NUM 64
    DWORD              dwThreadIdArray[THREAD_NUM];
    HANDLE             hThreadArray[THREAD_NUM];
    image_align_thread_arg_t *image_align_thread_arg =
        new image_align_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        image_align_thread_arg[i].trials = test_loop_times;
        hThreadArray[i] = CreateThread(
            NULL,                        // default security attributes
            0,                           // use default stack size
            test_image_align_thread,     // thread function name
            &image_align_thread_arg[i],  // argument to thread function
            0,                           // use default creation flags
            &dwThreadIdArray[i]);        // returns the thread identifier
        if (hThreadArray[i] == NULL) {
            delete[] image_align_thread_arg;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int   ret = 0;
    DWORD dwWaitResult =
        WaitForMultipleObjects(test_threads_num, hThreadArray, TRUE, INFINITE);
    // DWORD dwWaitResult = WaitForSingleObject(hThreadArray[i], INFINITE);
    switch (dwWaitResult) {
        // case WAIT_OBJECT_0:
        //    ret = 0;
        //    break;
        case WAIT_FAILED:
            ret = -1;
            break;
        case WAIT_ABANDONED:
            ret = -2;
            break;
        case WAIT_TIMEOUT:
            ret = -3;
            break;
        default:
            ret = 0;
            break;
    }
    if (ret < 0) {
        delete[] image_align_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int i = 0; i < test_threads_num; i++)
        CloseHandle(hThreadArray[i]);

    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] image_align_thread_arg;
    #endif

    return 0;
}
