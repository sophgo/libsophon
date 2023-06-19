#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#ifdef __linux__
  #include <unistd.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

#define SORT_SUCCESS (0)
#define SORT_FAILED (-1)
#define TEST_DDR1_DDR2 (0)
#define TEST_ALL_MEM (0)
#define MAX_SORT_NUM (1024*1024)

using namespace std;
typedef float bm_sort_data_type_t;
typedef enum {
    DATA_IN_DDR0 = 0,
    DATA_IN_SRAM,
    DATA_IN_DTCM,
    DATA_IN_DDR1,
    DATA_IN_DDR2,
    DATA_IN_ANY,
    MAX_LOCATION
} sort_data_location_e;
typedef enum { ASCEND_ORDER, DESCEND_ORDER } cdma_sort_order_e;

typedef struct {
    int loop_num;
    bm_handle_t handle;
} sort_thread_arg_t;

static u64 allocate_dev_mem(bm_handle_t      handle,
                            bm_device_mem_t *dst_dev_mem,
                            int32_t          size) {
    bm_malloc_device_byte(handle, dst_dev_mem, size);
    return bm_mem_get_device_addr(*dst_dev_mem);
}

inline bool compare_ascend_order(const sort_t &a, const sort_t &b) {
    return a.val <= b.val;
}

inline bool compare_descend_order(const sort_t &a, const sort_t &b) {
    return a.val >= b.val;
}

bool sort_equal_pred(sort_t a, sort_t b) {
    return ((a.val == b.val) && (a.index == b.index)) ? true : false;
}

inline bool push_unstable_map(const vector<sort_t>::iterator cdma_iter,
                              const vector<sort_t>::iterator ref_iter,
                              vector<sort_t>                 cdma_res,
                              vector<sort_t>                 ref_res,
                              int &                          loop_cnt) {
    UNUSED(ref_res);

    map<int, bm_sort_data_type_t> cdma_unstable_map;
    map<int, bm_sort_data_type_t> ref_unstable_map;
    vector<sort_t>::iterator      cdma_iter_temp = cdma_iter;
    vector<sort_t>::iterator      ref_iter_temp  = ref_iter;
    float                         init_val       = cdma_iter_temp->val;
    while (cdma_iter_temp != cdma_res.end()) {
        float cdma_val   = cdma_iter_temp->val;
        float ref_val    = ref_iter_temp->val;
        int   cdma_index = cdma_iter_temp->index;
        int   ref_index  = ref_iter_temp->index;
        if (cdma_val != ref_val) {
            printf("cmp error in halfway!!!\r\n");

            return false;
        } else if (cdma_val != init_val) {
            break;
        }
        cdma_unstable_map.insert(
            pair<int, bm_sort_data_type_t>(cdma_index, cdma_val));
        ref_unstable_map.insert(
            pair<int, bm_sort_data_type_t>(ref_index, ref_val));
        cdma_iter_temp++;
        ref_iter_temp++;
        loop_cnt++;
    }
    if (!equal(cdma_unstable_map.begin(),
               cdma_unstable_map.end(),
               ref_unstable_map.begin())) {
        printf("cache map not equal!!!\r\n");

        return false;
    }

    return true;
}

static bool get_cdma_result(bm_handle_t                 handle,
                            cdma_sort_order_e           order,
                            sort_data_location_e        location,
                            vector<int>                 src_index,
                            vector<bm_sort_data_type_t> src_data,
                            vector<sort_t> &            cdma_res,
                            bool                        index_enable,
                            bool                        auto_index) {
    bm_sort_data_type_t *src_data_p  = src_data.data();
    int *                src_index_p = src_index.data();
    int                  data_cnt    = src_data.size();
    int                  sort_cnt    = cdma_res.size();
    int *                dst_index_p = nullptr;
    bm_sort_data_type_t *dst_data_p  = nullptr;

    dst_index_p = new int[cdma_res.size()];
    dst_data_p  = new bm_sort_data_type_t[cdma_res.size()];
    printf("test location in %d\n", location);

    if (location == DATA_IN_ANY) {
        printf("index_enable: %d, auto_index: %d\n",
                index_enable, auto_index);
        #ifdef __linux__
            struct timeval t1, t2;
            gettimeofday(&t1, NULL);
            bmcv_sort(handle,
                  bm_mem_from_system(src_index_p),
                  bm_mem_from_system(src_data_p),
                  data_cnt,
                  bm_mem_from_system(dst_index_p),
                  bm_mem_from_system(dst_data_p),
                  sort_cnt,
                  (int)order,
                  index_enable,
                  auto_index);
            gettimeofday(&t2, NULL);
            long used = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
            printf("Used time: %.3f s\n", (float)used / 1000000);
        #else
            struct timespec tp1, tp2;
            clock_gettime(0, &tp1);
            bmcv_sort(handle,
                  bm_mem_from_system(src_index_p),
                  bm_mem_from_system(src_data_p),
                  data_cnt,
                  bm_mem_from_system(dst_index_p),
                  bm_mem_from_system(dst_data_p),
                  sort_cnt,
                  (int)order,
                  index_enable,
                  auto_index);
            clock_gettime(0, &tp2);
            long used1 = (tp2.tv_sec - tp1.tv_sec) * 1000000 + (tp2.tv_nsec - tp1.tv_nsec)/1000;
            printf("Used time: %.3f s\n", (float)used1 / 1000000);
        #endif
    } else {
        bmcv_sort_test(handle,
                       bm_mem_from_system(src_index_p),
                       bm_mem_from_system(src_data_p),
                       bm_mem_from_system(dst_index_p),
                       bm_mem_from_system(dst_data_p),
                       (int)order,
                       (int)location,
                       data_cnt,
                       sort_cnt);
    }
    for (int i = 0; i < sort_cnt; i++) {
        cdma_res[i].index = dst_index_p[i];
        cdma_res[i].val   = dst_data_p[i];
    }
    delete[] dst_index_p;
    delete[] dst_data_p;

    return true;
}

static bool unstable_process(vector<sort_t> cdma_res, vector<sort_t> ref_res) {
    vector<sort_t>::iterator cdma_iter_temp = cdma_res.begin();
    vector<sort_t>::iterator ref_iter_temp  = ref_res.begin();
    bool                     push_res       = true;

    for (int i = 0; i < (int)(cdma_res.size()); i++) {
        if (cdma_iter_temp->index != ref_iter_temp->index) {
            int temp_i = i;
            push_res   = push_unstable_map(
                cdma_iter_temp, ref_iter_temp, cdma_res, ref_res, i);
            if (false == push_res) {
                printf("cmp "
                       "error[%d],res_index:%d,res_val:%f,ref_index:%d,ref_val:"
                       "%f\r\n",
                       i,
                       cdma_res[i].index,
                       cdma_res[i].val,
                       ref_res[i].index,
                       ref_res[i].val);

                return false;
            }
            int step       = i - temp_i;
            cdma_iter_temp = cdma_iter_temp + step;
            ref_iter_temp  = ref_iter_temp + step;
        } else {
            cdma_iter_temp++;
            ref_iter_temp++;
        }
    }

    return true;
}

static bool result_compare(vector<sort_t> cdma_res,
                           vector<sort_t> ref_res,
                           sort_data_location_e location,
                           bool index_enable) {
    int unstable_res = true;

    for (unsigned int i = 0; i < cdma_res.size(); i++) {
        if (cdma_res[i].val != ref_res[i].val) {
            printf("[val error] index: %d, cdma: %f, ref: %f\n",
                    i, cdma_res[i].val, ref_res[i].val);
            return false;
        }
    }
    if (!index_enable && (location == DATA_IN_ANY)) {
        return true;
    } else {
        if (equal(cdma_res.begin(),
                  cdma_res.end(),
                  ref_res.begin(),
                  sort_equal_pred)) {

            return true;
        } else {
            unstable_res = unstable_process(cdma_res, ref_res);
        }

        return unstable_res;
    }
}

int32_t cv_sort_test_rand(bm_handle_t          handle,
                          cdma_sort_order_e    order,
                          sort_data_location_e location) {
    // produce sort num
    #ifdef __linux__
    int data_num = random() % MAX_SORT_NUM;
    #else
    int data_num = rand() % MAX_SORT_NUM;
    #endif
    // avoid the scenario of data_num = 0
    data_num = (0 == data_num) ? (MAX_SORT_NUM) : (data_num);
    data_num = (location == DATA_IN_ANY) ? data_num : rand() % 256;
    int sort_num = rand() % data_num + 1;
    std::vector<bm_sort_data_type_t> src_data;
    std::vector<int>                 src_data_index;
    std::vector<sort_t>              ref_res;
    std::vector<sort_t>              cdma_res;
    std::vector<bm_sort_data_type_t> dst_data;
    std::vector<int>                 dst_data_index;
    bool index_enable = rand() % 2 ? true : false;
    bool auto_index = rand() % 2 ? true : false;
    printf("data num: %d, sort num: %d\n", data_num, sort_num);
    cdma_res.resize(sort_num);
    dst_data.resize(sort_num);
    dst_data_index.resize(sort_num);
    // produce src data and index
    for (int32_t i = 0; i < data_num; i++) {
        sort_t temp;
        if (auto_index) {
            src_data_index.push_back(i);
        } else {
            src_data_index.push_back(rand() % MAX_SORT_NUM);
        }
        temp.index = src_data_index[i];
        #ifdef __linux__
        temp.val   = ((float)(random() % MAX_SORT_NUM)) / 100;
        #else
        temp.val   = ((float)(rand() % MAX_SORT_NUM)) / 100;
        #endif
        ref_res.push_back(temp);
        src_data.push_back(temp.val);
    }
    // cdma_result
    get_cdma_result(
        handle, order, location, src_data_index, src_data, cdma_res, index_enable, auto_index);
    // ref result
    if (order == ASCEND_ORDER) {
        stable_sort(ref_res.begin(), ref_res.end(), compare_ascend_order);
    } else {
        stable_sort(ref_res.begin(), ref_res.end(), compare_descend_order);
    }
    // results compare
    if (false == result_compare(cdma_res, ref_res, location, index_enable)) {
        printf("----SORT TEST ERROR!!!----\r\n");

        exit(-1);
    }
    // release memory

    return SORT_SUCCESS;
}

#ifdef __linux__
void* test_sort(void* args) {
#else
DWORD WINAPI test_sort(LPVOID args) {
#endif
    sort_thread_arg_t* sort_thread_arg = (sort_thread_arg_t*)args;
    bm_handle_t handle = sort_thread_arg->handle;
    int loop_num = sort_thread_arg->loop_num;
    for (int i = 0; i < loop_num; i++) {
        cv_sort_test_rand(handle, DESCEND_ORDER, DATA_IN_ANY);
        cv_sort_test_rand(handle, ASCEND_ORDER, DATA_IN_ANY);
    }
    return NULL;
}

int32_t main(int32_t argc, char **argv) {
    int test_loop_times = 1;
    int test_threads_num = 1;
    int dev_id = -1;
    if (argc > 1) {
        test_loop_times = atoi(argv[1]);
    }
    if (argc > 2) {
        test_threads_num = atoi(argv[2]);
    }
    if (argc > 3) {
        dev_id = atoi(argv[3]);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST SORT] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST SOER] thread nums should be 1~4 "
                  << std::endl;
        exit(-1);
    }
    std::cout << "[TEST SORT] test starts... LOOP times = " << test_loop_times
              << " threads num = " << test_loop_times << std::endl;

    unsigned int seed = (unsigned)time(NULL);
    srand(seed);
    printf("random seed is %u\r\n", seed);
    int dev_cnt;
    bm_dev_getcount(&dev_cnt);
    if (dev_id >= dev_cnt) {
        std::cout << "[TEST SOER] dev_id should less than device count, only detect "<< dev_cnt << " devices "
                  << std::endl;
        exit(-1);
    }
    printf("device count = %d\n", dev_cnt);
    #ifdef __linux__
    bm_handle_t handle[dev_cnt];
    #else
    std::shared_ptr<bm_handle_t> handle_(new bm_handle_t[dev_cnt], std::default_delete<bm_handle_t[]>());
    bm_handle_t*                 handle = handle_.get();
    #endif
    for (int i = 0; i < dev_cnt; i++) {
        int id;
        if (dev_id != -1) {
            dev_cnt = 1;
            id = dev_id;
        } else {
            id = i;
        }
        bm_status_t req = bm_dev_request(handle + i, id);
        if (req != BM_SUCCESS) {
            printf("create bm handle for dev%d failed!\n", id);
            exit(-1);
        }
    }
    // test for multi-thread
    #ifdef __linux__
    pthread_t pid[dev_cnt * test_threads_num];
    sort_thread_arg_t sort_thread_arg[dev_cnt * test_threads_num];
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            sort_thread_arg[idx].loop_num = test_loop_times;
            sort_thread_arg[idx].handle = handle[d];
            if (pthread_create(pid + idx, NULL, test_sort, sort_thread_arg + idx)) {
                printf("create thread failed\n");
                return -1;
            }
        }
    }
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int ret = pthread_join(pid[d * test_threads_num + i], NULL);
            if (ret != 0) {
                printf("Thread join failed\n");
                exit(-1);
            }
        }
    }
    for (int d = 0; d < dev_cnt; d++) {
        bm_dev_free(handle[d]);
    }
    #else
    #define THREAD_NUM 64
    DWORD dwThreadIdArray[THREAD_NUM];
    HANDLE hThreadArray[THREAD_NUM];
    sort_thread_arg_t *sort_thread_arg =
        new sort_thread_arg_t[dev_cnt * test_threads_num];
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            sort_thread_arg[idx].loop_num = test_loop_times;
            sort_thread_arg[idx].handle = handle[d];
            hThreadArray[idx] = CreateThread(
                NULL,                    // default security attributes
                0,                       // use default stack size
                test_sort,               // thread function name
                sort_thread_arg + idx,   // argument to thread function
                0,                       // use default creation flags
                &dwThreadIdArray[idx]);  // returns the thread identifier
            if (hThreadArray[i] == NULL) {
                delete[] sort_thread_arg;
                perror("create thread failed\n");
                exit(-1);
            }
        }
    }
    int ret = 0;
    DWORD dwWaitResult =
        WaitForMultipleObjects(dev_cnt * test_threads_num, hThreadArray, TRUE, INFINITE);
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
        delete[] sort_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            CloseHandle(hThreadArray[i]);
        }
    }

    for (int d = 0; d < dev_cnt; d++) {
        bm_dev_free(handle[d]);
    }
    #endif

#if TEST_ALL_MEM
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST SORT] LOOP " << loop_idx << "------"
                  << std::endl;
        bm_handle_t handle;
        int         dev_id = 0;
        bm_status_t ret    = bm_dev_request(&handle, dev_id);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            exit(-1);
        }
        unsigned int seed = (unsigned)time(NULL) + loop_idx;
        srand(seed);
        printf("seed is %u\r\n", seed);
        // asend order
        // test sort when data in ddr0
        cv_sort_test_rand(handle, ASCEND_ORDER, DATA_IN_DDR0);
#if TEST_DDR1_DDR2
        // test sort when data in ddr1
        cv_sort_test_rand(handle, ASCEND_ORDER, DATA_IN_DDR1);
        // test sort when data in ddr2
        cv_sort_test_rand(handle, ASCEND_ORDER, DATA_IN_DDR2);
#endif
        // test sort when data in sram
        cv_sort_test_rand(handle, ASCEND_ORDER, DATA_IN_SRAM);
        // test sort when data in dtcm
        cv_sort_test_rand(handle, ASCEND_ORDER, DATA_IN_DTCM);
        // desend order
        // test sort when data in ddr0
        cv_sort_test_rand(handle, DESCEND_ORDER, DATA_IN_DDR0);
#if TEST_DDR1_DDR2
        // test sort when data in ddr1
        cv_sort_test_rand(handle, DESCEND_ORDER, DATA_IN_DDR1);
        // test sort when data in ddr2
        cv_sort_test_rand(handle, DESCEND_ORDER, DATA_IN_DDR2);
#endif
        // test sort when data in sram
        cv_sort_test_rand(handle, DESCEND_ORDER, DATA_IN_SRAM);
        // test sort when data in dtcm
        cv_sort_test_rand(handle, DESCEND_ORDER, DATA_IN_DTCM);
        bm_dev_free(handle);
    }
#endif
    std::cout << "------[TEST SORT] ALL TEST PASSED!" << std::endl;

    return SORT_SUCCESS;
}