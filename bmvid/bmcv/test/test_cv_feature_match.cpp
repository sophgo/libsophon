#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <vector>
#include <math.h>
#include <string.h>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"

#ifdef __linux__
  #include <unistd.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

#define FEATURE_MATCH_SUCCESS (0)
#define FEATURE_MATCH_FAILED (-1)
#define STANDARD_FEATURE_SIZE (512)
#define ERR_THRESHOLD (1e-2)
#define PRINT_SWITCH (0)

using namespace std;

#ifdef __linux
class perf_cal {
   public:
    void begin() {
        struct timeval tmp_start;
        gettimeofday(&tmp_start, NULL);
        start.push(tmp_start);
    }
    unsigned long long perf_ms() {
        gettimeofday(&tmp_end, NULL);
        struct timeval tmp_start;
        tmp_start = start.top();
        unsigned long long used =
            (tmp_end.tv_sec - tmp_start.tv_sec) * 1000000 + tmp_end.tv_usec -
            tmp_start.tv_usec;
        std::cout << "Used time: " << (float)used / 1000 << " ms" << std::endl;
        start.pop();
        return used;
    }
    unsigned long long perf_us() {
        gettimeofday(&tmp_end, NULL);
        struct timeval tmp_start;
        tmp_start = start.top();
        unsigned long long used =
            (tmp_end.tv_sec - tmp_start.tv_sec) * 1000000 + tmp_end.tv_usec -
            tmp_start.tv_usec;
        std::cout << "Used time: %.3f us" << used << " us" << std::endl;
        start.pop();
        return used;
    }

   private:
    std::stack<struct timeval> start;
    struct timeval             tmp_end;
};
#else
class perf_cal {
   public:
    void begin() {
        struct timespec tp_start;
        clock_gettime_win(0, &tp_start);
        start.push(tp_start);
    }
    unsigned long long perf_ms() {
        clock_gettime_win(0, &tp_end);
        struct timespec tp_start;
        tp_start = start.top();
        unsigned long long used =
            (tp_end.tv_sec - tp_start.tv_sec) * 1000000 + (tp_end.tv_nsec - tp_start.tv_nsec)/1000;
        std::cout << "Used time: " << (float)used / 1000 << " ms" << std::endl;
        start.pop();
        return used;
    }
    unsigned long long perf_us() {
        clock_gettime_win(0, &tp_end);
        struct timespec tp_start;
        tp_start = start.top();
        unsigned long long used =
            (tp_end.tv_sec - tp_start.tv_sec) * 1000000 + (tp_end.tv_nsec - tp_start.tv_nsec)/1000;
        std::cout << "Used time: %.3f us" << used << " us" << std::endl;
        start.pop();
        return used;
    }

   private:
    std::stack<struct timespec> start;
    struct timespec            tp_end;
};
#endif

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

typedef enum {
    CLASSIC_TEST = 0,
    MIN_TEST,
    MAX_TEST,
    RAND_TEST,
    MAX_RAND_MODE
} rand_mode_e;
typedef struct {
    int trials;
} feature_match_thread_arg_t;
typedef float bm_feature_match_data_type_t;
#define DB_DATA_PATH "test/test_api_bmcv/db_data.txt"
#define DB_DATA_TRANSPOSED_PATH "test/test_api_bmcv/db_data_transposed.txt"
#define INPUT_DATA_PATH "test/test_api_bmcv/input_data.txt"

static u64 allocate_dev_mem(bm_handle_t      handle,
                            bm_device_mem_t *dst_dev_mem,
                            int32_t          size) {
    bm_malloc_device_byte(handle, dst_dev_mem, size);
    return bm_mem_get_device_addr(*dst_dev_mem);
}

int read_data_from_file(string path, vector<vector<float>> &content_vec) {
    ifstream      file(path);
    char *        c_content;
    int           size;
    string        content;
    vector<float> vec_f;
    if (file.is_open()) {
        file.seekg(0, file.end);
        size = file.tellg();
        file.seekg(0, file.beg);
        c_content = new char[size];
        // file.read(content, size);
        while (file.getline(
            c_content,
            STANDARD_FEATURE_SIZE * sizeof(bm_feature_match_data_type_t) * 2)) {
            content = c_content;
            istringstream stream(content);
            copy(istream_iterator<float>(stream),
                 istream_iterator<float>(),
                 back_inserter(vec_f));
            content_vec.push_back(vec_f);
            vec_f.clear();
        }
        file.close();
    }

    return BM_SUCCESS;
}

void feature_write(std::vector<float> feature_vec, std::string fn) {
    std::ofstream fout(fn, ios::out | ios::app);
    if (fout.is_open()) {
        for (unsigned int n = 0; n < feature_vec.size(); n++) {
            {
                fout << feature_vec[n] << " ";
            }
        }
        fout.close();
    }
}

void feature_write_transposed(vector<vector<float>> feature_vec,
                              string                fn,
                              unsigned int          times = 1) {
    std::ofstream fout(fn, ios::out | ios::app);
    if (fout.is_open()) {
        for (unsigned int i = 0; i < feature_vec[0].size(); i++) {
            for (unsigned int k = 0; k < times; k++) {
                for (unsigned int j = 0; j < feature_vec.size(); j++) {
                    {
                        fout << feature_vec[j][i] << " ";
                    }
                }
            }
            fout << endl;
        }
        fout.close();
    }
}

inline float calc_sqrt(const std::vector<float> &feature) {
    float a = 0;
    for (unsigned int i = 0; i < feature.size(); ++i) {
        a += feature[i] * feature[i];
    }
    return 1 / sqrt(a);
}

inline int calc_sqrt_transposed(const vector<vector<float>> feature,
                                vector<float> &db_feature_vec_con) {
    float a    = 0;
    float temp = 0.0f;
    for (unsigned int i = 0; i < feature[0].size(); ++i) {
        a = 0;
        for (unsigned int j = 0; j < feature.size(); ++j) {
            a += feature[j][i] * feature[j][i];
        }
        temp = 1 / sqrt(a);
        db_feature_vec_con.push_back(temp);
    }

    return BM_SUCCESS;
}

int transpose(vector<vector<float>> &b) {
    if (b.size() == 0)
        return BM_ERR_FAILURE;

    vector<vector<float>> trans_vec(b[0].size(), vector<float>());

    for (size_t i = 0; i < b.size(); i++) {
        for (size_t j = 0; j < b[i].size(); j++) {
            if (trans_vec[j].size() != b.size())
                trans_vec[j].resize(b.size());
            trans_vec[j][i] = b[i][j];
        }
    }

    b = trans_vec;

    return BM_SUCCESS;
}

vector<vector<float>> feature_match_ref(
    vector<vector<float>> input_data,
    vector<vector<float>> db_data_transposed) {
    float                 a = 0;
    vector<vector<float>> db_data(db_data_transposed);

    transpose(db_data);
    int                   batch_size   = input_data.size();
    int                   db_size      = db_data.size();
    int                   feature_size = input_data[0].size();
    vector<vector<float>> match_res(batch_size);
    for (int batch_cnt = 0; batch_cnt < batch_size; ++batch_cnt) {
        for (int db_cnt = 0; db_cnt < db_size; ++db_cnt) {
            a = 0.0f;
            for (int feature_cnt = 0; feature_cnt < feature_size;
                 ++feature_cnt) {
                a += input_data[batch_cnt][feature_cnt] *
                     db_data[db_cnt][feature_cnt];
            }
            float b =
                calc_sqrt(input_data[batch_cnt]) * calc_sqrt(db_data[db_cnt]);
            match_res[batch_cnt].push_back(a * b);
        }
    }

    return match_res;
}

vector<vector<short>> feature_match_ref(vector<vector<signed char>> input_data,
                                        vector<vector<signed char>> db_data,
                                        int rshiftbits) {
    int                   a            = 0;
    int                   batch_size   = input_data.size();
    int                   db_size      = db_data[0].size();
    int                   feature_size = input_data[0].size();
    vector<vector<short>> match_res(batch_size);
    for (int batch_cnt = 0; batch_cnt < batch_size; ++batch_cnt) {
        for (int db_cnt = 0; db_cnt < db_size; ++db_cnt) {
            a = 0;
            for (int feature_cnt = 0; feature_cnt < feature_size;
                 ++feature_cnt) {
                a += db_data[feature_cnt][db_cnt] *
                     input_data[batch_cnt][feature_cnt];
            }
            a = a > 0x7fff ? 0x7fff : a;
            a = a < -0x8000 ? -0x8000 : a;
            if (0 != rshiftbits) {
                a = ((a >> (rshiftbits - 1)) + 1) >> 1;
            }
            match_res[batch_cnt].push_back((short)a);
        }
    }

    return match_res;
}

bool compare_pred(float opd1, float opd2) {

    if (fabs(opd1 - opd2) < ERR_THRESHOLD) {
        return true;
    }
    cout << "opd1 " << opd1 << " opd2 " << opd2 << endl;

    return false;
}

template <typename val_type_t>
bool res_compare(val_type_t *               tpu_res_similarity,
                 int *                      tpu_res_index,
                 vector<vector<val_type_t>> ref_res,
                 int                        sort_cnt = 1) {
    vector<val_type_t>         ref_res_flatten;
    vector<val_type_t>         tpu_res_vec;
    vector<vector<val_type_t>> tmp_ref_res = ref_res;

    for (size_t batch_cnt = 0; batch_cnt < ref_res.size(); batch_cnt++) {
        for (int sort_indx = 0; sort_indx < sort_cnt; sort_indx++) {
            int        ref_index      = distance(tmp_ref_res[batch_cnt].begin(),
                                     max_element(tmp_ref_res[batch_cnt].begin(),
                                                 tmp_ref_res[batch_cnt].end()));
            val_type_t ref_similarity = tmp_ref_res[batch_cnt][ref_index];
            // find the ref index in origin vector
            auto tmp_elem = std::find(ref_res[batch_cnt].begin(),
                                      ref_res[batch_cnt].end(),
                                      ref_similarity);
            int  ref_index_origin =
                std::distance(ref_res[batch_cnt].begin(), tmp_elem);
            // if (fabs((tpu_res_similarity[batch_cnt] - ref_similarity) >
            //         ERR_THRESHOLD) ||
            //    (tpu_res_index[batch_cnt] != ref_index))
            if (fabs((float)
                         tpu_res_similarity[batch_cnt * sort_cnt + sort_indx] -
                     (float)ref_similarity) > ERR_THRESHOLD) {
                cout << "tpu_res[" << batch_cnt << "]"
                     << "[" << sort_indx << "]"
                     << "[" << tpu_res_index[batch_cnt * sort_cnt + sort_indx]
                     << "] "
                     << tpu_res_similarity[batch_cnt * ref_res.size() +
                                           sort_indx]
                     << " ref_res[" << batch_cnt << "]"
                     << "[" << sort_indx << "]"
                     << "[" << ref_index_origin << "] "
                     << tmp_ref_res[batch_cnt][ref_index] << endl;
                return false;
            }
            tmp_ref_res[batch_cnt].erase(tmp_ref_res[batch_cnt].begin() +
                                         ref_index);
        }
    }

    return true;
}

static int gen_test_size(int &batch_size,
                         int &db_size,
                         int &feature_size,
                         int &sort_cnt,
                         int &rshiftbits,
                         int  gen_mode) {
    switch (gen_mode) {
        case (CLASSIC_TEST): {
            batch_size   = 4;
            db_size      = 50000;
            sort_cnt     = 1;
            feature_size = 512;
            rshiftbits   = 0;
            break;
        }
        case (MIN_TEST): {
            batch_size   = 1;
            db_size      = 1000;
            sort_cnt     = 1;
            feature_size = 1;
            rshiftbits   = 0;
            break;
        }
        case (MAX_TEST): {
            batch_size   = 8;
            db_size      = 500000;
            sort_cnt     = 30;
            feature_size = 4096;
            rshiftbits   = 2;
            break;
        }
        case (RAND_TEST): {
            batch_size   = rand() % 8 + 1;
            db_size      = (rand() % 499 + 1) * 1000;
            sort_cnt     = rand() % 30 + 1;
            feature_size = rand() % 4096 + 1;
            rshiftbits   = rand() % 3;
            break;
        }
        default: {
            cout << "gen mode error" << endl;
            exit(-1);
        }
    }

    return 0;
}

int32_t cv_feature_match_test_rand(bm_handle_t handle) {
    int    feature_size      = 512;
    int    batch_size        = 4;
    int    db_size           = 1000;
    int    db_size_aligned   = ((db_size + EU_NUM - 1) / EU_NUM) * EU_NUM;
    float *input_data        = new float[batch_size * feature_size];
    float *db_data           = new float[db_size * feature_size];
    float *db_feature        = new float[db_size];
    float *output_similarity = new float[batch_size * db_size_aligned];
    int *  output_index      = new int[db_size];

    vector<vector<float>> db_content_vec(feature_size);
    for (int i = 0; i < feature_size; i++) {
        for (int j = 0; j < db_size; j++) {
            #ifdef __linux__
            float temp_val = random() % 120;
            #else
            float temp_val = rand() % 120;
            #endif
            temp_val       = temp_val - 60;
            db_content_vec[i].push_back(temp_val);
        }
    }
    // read_data_from_file(DB_DATA_PATH, db_content_vec);
    vector<vector<float>> input_content_vec(batch_size);
    for (int i = 0; i < batch_size; i++) {
        for (int j = 0; j < feature_size; j++) {
            #ifdef __linux__
            float temp_val = random() % 120;
            #else
            float temp_val = rand() % 120;
            #endif
            temp_val       = temp_val - 60;
            input_content_vec[i].push_back(temp_val);
        }
    }
    vector<float> db_content_vec_con, input_content_vec_con, db_feature_vec_con;
    // feature_write_transposed(
    //    db_content_vec, DB_DATA_TRANSPOSED_PATH, 1);
    for (unsigned int i = 0; i < db_content_vec.size(); i++) {
        db_content_vec_con.insert(db_content_vec_con.end(),
                                  db_content_vec[i].begin(),
                                  db_content_vec[i].end());
    }
    calc_sqrt_transposed(db_content_vec, db_feature_vec_con);
    for (unsigned int i = 0; i < input_content_vec.size(); i++) {
        input_content_vec_con.insert(input_content_vec_con.end(),
                                     input_content_vec[i].begin(),
                                     input_content_vec[i].end());
    }
    memcpy(input_data,
           input_content_vec_con.data(),
           batch_size * feature_size * sizeof(float));
    memcpy(db_data,
           db_content_vec_con.data(),
           db_size * feature_size * sizeof(float));
    memcpy(db_feature, db_feature_vec_con.data(), db_size * sizeof(float));

    struct timeval t1, t2;
    gettimeofday_(&t1);
    bmcv_feature_match_normalized(handle,
                                bm_mem_from_system(input_data),
                                bm_mem_from_system(db_data),
                                bm_mem_from_system(db_feature),
                                bm_mem_from_system(output_similarity),
                                bm_mem_from_system(output_index),
                                batch_size,
                                feature_size,
                                db_size);
    gettimeofday_(&t2);
        cout << "feature match using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;


    vector<vector<float>> ref_res(batch_size);
    ref_res = feature_match_ref(input_content_vec, db_content_vec);
#if PRINT_SWITCH
    for (int i = 0; i < batch_size; i++) {
        for (int j = 0; j < db_size; j++) {
            cout << "output_similarity[" << i << "][" << j
                 << "]: " << output_similarity[i * db_size + j] << " ";
        }
        cout << endl;
    }
#endif
    if (false == res_compare<float>(output_similarity, output_index, ref_res)) {
        cout << "FEATURE MATCHING COMPARE ERROR" << endl;
        delete[] input_data;
        delete[] db_data;
        delete[] db_feature;
        delete[] output_similarity;
        delete[] output_index;
        exit(-1);
    }
    delete[] input_data;
    delete[] db_data;
    delete[] db_feature;
    delete[] output_similarity;
    delete[] output_index;
    std::cout << "FEATURE MATCHING FP32 COMPARE PASSED" << std::endl;

    return FEATURE_MATCH_SUCCESS;
}

int32_t cv_feature_match_fix8b_test_rand(bm_handle_t handle) {
    int feature_size  = 512;
    int batch_size    = 4;
    int db_size       = 10000;
    int sort_cnt      = 1;
    int rshiftbits    = 0;
    int rand_loop_num = 2;
    for (int rand_loop_idx = 0; rand_loop_idx < rand_loop_num; rand_loop_idx++) {
#if !defined(USING_CMODEL) && !defined(SOC_MODE)  // for PCIE device mode
        for (int rand_mode = 0; rand_mode < MAX_RAND_MODE; rand_mode++) {
#else       // for CModel or SOC mode
        for (int rand_mode = 0; rand_mode < MAX_TEST; rand_mode++) {
#endif
            gen_test_size(batch_size,
                          db_size,
                          feature_size,
                          sort_cnt,
                          rshiftbits,
                          rand_mode);
            std::shared_ptr<Blob<signed char>> input_data =
                std::make_shared<Blob<signed char>>(batch_size * feature_size);
            std::shared_ptr<Blob<signed char>> db_data =
                std::make_shared<Blob<signed char>>(db_size * feature_size);
            std::shared_ptr<Blob<signed char>> db_feature =
                std::make_shared<Blob<signed char>>(db_size);
            std::shared_ptr<Blob<short>> output_similarity =
                std::make_shared<Blob<short>>(batch_size * sort_cnt);
            std::shared_ptr<Blob<int>> output_index =
                std::make_shared<Blob<int>>(batch_size * sort_cnt);

            std::cout << "[FIX8B] rand_mode: " << rand_mode << std::endl;
            std::cout << "db size: " << db_size << std::endl;
            std::cout << "batch size: " << batch_size << std::endl;
            std::cout << "feature size: " << feature_size << std::endl;
            std::cout << "sort_cnt: " << sort_cnt << std::endl;
            std::cout << "rshiftbits: " << rshiftbits << std::endl;
            vector<vector<signed char>> db_content_vec(feature_size);
            for (int i = 0; i < feature_size; i++) {
                for (int j = 0; j < db_size; j++) {
                    #ifdef __linux__
                    signed char temp_val = random() % 20 - 10;
                    // signed char temp_val = random() % 7;
                    #else
                    signed char temp_val = rand() % 20 - 10;
                    #endif
                    db_content_vec[i].push_back(temp_val);
                }
            }
            // read_data_from_file(DB_DATA_PATH, db_content_vec);
            vector<vector<signed char>> input_content_vec(batch_size);
            for (int i = 0; i < batch_size; i++) {
                for (int j = 0; j < feature_size; j++) {
                    #ifdef __linux__
                    signed char temp_val = random() % 20 - 10;
                    // signed char temp_val = random() % 7;
                    #else
                    signed char temp_val = rand() % 20 - 10;
                    #endif
                    input_content_vec[i].push_back(temp_val);
                }
            }
            vector<vector<signed char>> db_content_vec_trans(db_size);
            for (int i = 0; i < db_size; i++) {
                for (int j = 0; j < feature_size; j++) {
                    db_content_vec_trans[i].push_back(db_content_vec[j][i]);
                }
            }

            vector<signed char> db_content_vec_con, input_content_vec_con;
            // feature_write_transposed(
            //    db_content_vec, DB_DATA_TRANSPOSED_PATH, 1);
            // for (unsigned int i = 0; i < db_content_vec_trans.size(); i++) {
            //    db_content_vec_con.insert(db_content_vec_con.end(),
            //                              db_content_vec_trans[i].begin(),
            //                              db_content_vec_trans[i].end());
            //}
            for (unsigned int i = 0; i < db_content_vec.size(); i++) {
                db_content_vec_con.insert(db_content_vec_con.end(),
                                          db_content_vec[i].begin(),
                                          db_content_vec[i].end());
            }
            for (unsigned int i = 0; i < input_content_vec.size(); i++) {
                input_content_vec_con.insert(input_content_vec_con.end(),
                                             input_content_vec[i].begin(),
                                             input_content_vec[i].end());
            }
            memcpy(input_data.get()->data,
                   input_content_vec_con.data(),
                   batch_size * feature_size * sizeof(signed char));
            memcpy(db_data.get()->data,
                   db_content_vec_con.data(),
                   db_size * feature_size * sizeof(signed char));
            bmcv_feature_match(
                handle,
                bm_mem_from_system(input_data.get()->data),
                bm_mem_from_system(db_data.get()->data),
                bm_mem_from_system(output_similarity.get()->data),
                bm_mem_from_system(output_index.get()->data),
                batch_size,
                feature_size,
                db_size,
                sort_cnt,
                rshiftbits);
            vector<vector<short>> ref_res(batch_size);
            ref_res = feature_match_ref(
                input_content_vec, db_content_vec, rshiftbits);
#if PRINT_SWITCH
            for (int i = 0; i < batch_size; i++) {
                for (int j = 0; j < db_size; j++) {
                    cout << "output_similarity[" << i << "][" << j
                         << "]: " << output_similarity[i * db_size + j] << " ";
                }
                cout << endl;
            }
#endif
            if (false == res_compare<short>(output_similarity.get()->data,
                                            output_index.get()->data,
                                            ref_res,
                                            sort_cnt)) {
                cout << "FEATURE MATCHING FIX8B COMPARE ERROR" << endl;

                exit(-1);
            }
        }
    }
    std::cout << "FEATURE MATCHING FIX8B COMPARE PASSED" << std::endl;

    return FEATURE_MATCH_SUCCESS;
}
#ifdef __linux__
void *test_feature_match_thread(void *arg) {
#else
DWORD WINAPI test_feature_match_thread(LPVOID arg) {
#endif
    feature_match_thread_arg_t *feature_match_thread_arg =
        (feature_match_thread_arg_t *)arg;
    int test_loop_times = feature_match_thread_arg->trials;
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST FEATURE_MATCH] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    int         dev_id = 0;
    bm_handle_t handle;
    bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
    if (dev_ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", dev_ret);
        exit(-1);
    }
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST FEATURE_MATCH] LOOP " << loop_idx << "------"
                  << std::endl;
        unsigned int seed = (unsigned)time(NULL);
        std::cout << "seed: " << seed << std::endl;
        bm_handle_t handle;
        int         dev_id = 0;
        bm_status_t ret    = bm_dev_request(&handle, dev_id);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            exit(-1);
        }
        // cv_feature_match_test_file(handle);
        std::cout << "--------------fp32 test------------------" << std::endl;
        cv_feature_match_test_rand(handle);
        std::cout << "--------------fix8b test------------------" << std::endl;
        cv_feature_match_fix8b_test_rand(handle);
        cout << "FEATURE MATCHING PASSED" << endl;
        bm_dev_free(handle);
    }
    std::cout << "------[TEST FEATURE MATCH] ALL TEST PASSED!" << std::endl;

    return FEATURE_MATCH_SUCCESS;
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
                     "order:test_cv_feature_match loop_num multi_thread_num"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST FEATURE_MATCH] loop times should be 1~1500"
                  << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST FEATURE_MATCH] thread nums should be 1~4 "
                  << std::endl;
        exit(-1);
    }
    #ifdef __linux__
    pthread_t *                 pid = new pthread_t[test_threads_num];
    feature_match_thread_arg_t *feature_match_thread_arg =
        new feature_match_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        feature_match_thread_arg[i].trials = test_loop_times;  // dev_id 0
        if (pthread_create(&pid[i],
                           NULL,
                           test_feature_match_thread,
                           &feature_match_thread_arg[i])) {
            delete[] pid;
            delete[] feature_match_thread_arg;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            delete[] feature_match_thread_arg;
            perror("Thread join failed");
            exit(-1);
        }
    }
    #else
    #define THREAD_NUM 64
    DWORD              dwThreadIdArray[THREAD_NUM];
    HANDLE             hThreadArray[THREAD_NUM];
    feature_match_thread_arg_t *feature_match_thread_arg =
        new feature_match_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        feature_match_thread_arg[i].trials = test_loop_times;
        hThreadArray[i] = CreateThread(NULL,                         // default security attributes
                                       0,                            // use default stack size
                                       test_feature_match_thread,    // thread function name
                                       &feature_match_thread_arg[i], // argument to thread function
                                       0,                            // use default creation flags
                                       &dwThreadIdArray[i]);         // returns the thread identifier
        if (hThreadArray[i] == NULL) {
            delete[] feature_match_thread_arg;
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
        delete[] feature_match_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int i = 0; i < test_threads_num; i++)
        CloseHandle(hThreadArray[i]);
    #endif

    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    #ifdef __linux__
    delete[] pid;
    #endif
    delete[] feature_match_thread_arg;

    return 0;
}
