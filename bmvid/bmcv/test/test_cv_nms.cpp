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
#include <math.h>
#include "bmcv_api.h"
#include "test_misc.h"

#ifdef __linux__
  #include <pthread.h>
  #include <unistd.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

#define GEN_PROPOSAL_SUCCESS (0)
#define GEN_PROPOSAL_FAILED (-1)
#define MAX_THRES (0.99)
#define MIN_THRES (0.01)
#define MAX_SOFT_SUPPORT_PROPOSAL_NUM (1024)
using namespace std;
int seed = -1;

typedef float bm_nms_data_type_t;
typedef struct {
    int trials;
    int box_num;
} nms_thread_arg_t;

typedef enum {
    CLASSIC_TEST = 0,
    MIN_TEST,
    MAX_TEST,
    RAND_TEST,
    NODECHIP_MAX_TEST,
    NODECHIP_RAND_TEST,
    MAX_RAND_MODE
} rand_mode_e;

struct norm_data {
    unsigned int manti : 23;
    unsigned int exp : 8;
    unsigned int sign : 1;
};

typedef union {
    int       ival;
    float     fval;
    norm_data nval;
} IF_VAL_C;

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

static vector<face_rect_t> nequal_ref_vec;
static vector<face_rect_t> nequal_res_vec;
#ifdef __linux__
// static pthread_mutex_t     vec_lock = PTHREAD_MUTEX_INITIALIZER;
#else
HANDLE ghMutex_ = CreateMutex(NULL,   // default security attributes
                             FALSE,  // initially not owned
                             NULL);  // unnamed mutex
#endif

#define LOG_TO_FILE (0)

#if LOG_TO_FILE
static int32_t     file_dump_init_flag = 0;
static FILE *      log_fp;
static const char *dump_log_path = "./test_nms_log";

#define DUMP_LOG_FILE(fmrt, ...)                                               \
    do {                                                                       \
        if (!file_dump_init_flag) {                                            \
            remove(dump_log_path);                                             \
            file_dump_init_flag = 1;                                           \
        }                                                                      \
        log_fp = fopen(dump_log_path, "a+");                                   \
        if (NULL == log_fp) {                                                  \
            printf("open dump log error\r\n");                                 \
            assert(0);                                                         \
        }                                                                      \
        fprintf(log_fp, fmrt, ##__VA_ARGS__);                                  \
        fclose(log_fp);                                                        \
    } while (0)
#endif

static IF_VAL_C microp_nr(IF_VAL_C x) {
    float c_0p5  = 0.5;
    float c_1p5  = 1.5;
    float half_x = c_0p5 * x.fval;
    x.ival       = 0x5f3759df - (x.ival >> 1);
    for (int i = 0; i < 3; i++)
        x.fval = x.fval * (c_1p5 - (half_x * x.fval * x.fval));
    return x;
}

static int microop_normb(float x) {
    IF_VAL_C work_b;
    int      exp_inc;
    work_b.fval = x;
    exp_inc     = work_b.nval.manti > 0x2aaaab && !work_b.nval.sign;
    return work_b.nval.exp + exp_inc;
}

static float div_fnc(float x, float y) {
    IF_VAL_C srca, srcb;
    srca.fval      = x;
    srcb.fval      = y;
    srca.nval.sign = srca.nval.sign ^ srcb.nval.sign;
    srcb.ival &= 0x7fffffff;
    srcb      = microp_nr(srcb);
    srcb.fval = srcb.fval * srcb.fval;
    return srca.fval * srcb.fval;
}

static int gen_test_size(bm_nms_data_type_t &threshold,
                         int &               size,
                         int                 gen_mode) {
    switch (gen_mode) {
        case (CLASSIC_TEST): {
            threshold = 0.77;
            size      = rand() % 170 + 2;
            break;
        }
        case (MIN_TEST): {
            threshold = MIN_THRES;
            size      = MIN_PROPOSAL_NUM;
            break;
        }
        case (MAX_TEST): {
            threshold = MAX_THRES;
            size      = MAX_SOFT_SUPPORT_PROPOSAL_NUM;
            break;
        }
        case (RAND_TEST): {
            threshold = ((float)(rand() % 100)) / 100;
            size      = rand() % MAX_SOFT_SUPPORT_PROPOSAL_NUM;
            threshold = (threshold < MIN_THRES) ? (MIN_THRES) : (threshold);
            size      = (size < MIN_PROPOSAL_NUM) ? (MIN_PROPOSAL_NUM) : (size);
            break;
        }
        case (NODECHIP_MAX_TEST): {
            threshold = MAX_THRES;
            size      = MAX_SOFT_SUPPORT_PROPOSAL_NUM;
            break;
        }
        case (NODECHIP_RAND_TEST): {
            threshold = ((float)(rand() % 100)) / 100;
            size      = rand() % MAX_SOFT_SUPPORT_PROPOSAL_NUM;
            threshold = (threshold < MIN_THRES) ? (MIN_THRES) : (threshold);
            size      = (size < MIN_PROPOSAL_NUM) ? (MIN_PROPOSAL_NUM) : (size);
            break;
        }
        default: {
            cout << "gen mode error" << endl;
            exit(-1);
        }
    }

    return 0;
}

static int soft_nms_gen_test_size(bm_nms_data_type_t &score_threshold,
                                  bm_nms_data_type_t &threshold,
                                  bm_nms_data_type_t &sigma,
                                  int &               size,
                                  int &               weighting_method,
                                  int                 gen_mode) {
    switch (gen_mode) {
        case (CLASSIC_TEST): {
            score_threshold  = 0.4;
            sigma            = 0.4;
            weighting_method = GAUSSIAN_WEIGHTING;
            gen_test_size(threshold, size, gen_mode);
            break;
        }
        case (MIN_TEST): {
            score_threshold  = MIN_THRES;
            sigma            = 0.4;
            weighting_method = GAUSSIAN_WEIGHTING;
            gen_test_size(threshold, size, gen_mode);
            break;
        }
        case (MAX_TEST): {
            score_threshold  = MAX_THRES;
            sigma            = 0.4;
            weighting_method = GAUSSIAN_WEIGHTING;
            gen_test_size(threshold, size, MAX_TEST);
            break;
        }
        case (NODECHIP_MAX_TEST): {
            score_threshold  = MAX_THRES;
            sigma            = 0.4;
            weighting_method = GAUSSIAN_WEIGHTING;
            gen_test_size(threshold, size, NODECHIP_MAX_TEST);
            break;
        }
        case (RAND_TEST): {
            score_threshold = ((float)(rand() % 100)) / 100;
            score_threshold =
                (score_threshold < MIN_THRES) ? (MIN_THRES) : (score_threshold);
            sigma            = ((float)(rand() % 10)) / 10 + 0.01;
            weighting_method = rand() % 2;
            gen_test_size(threshold, size, RAND_TEST);
            break;
        }
        case (NODECHIP_RAND_TEST): {
            score_threshold = ((float)(rand() % 100)) / 100;
            score_threshold =
                (score_threshold < MIN_THRES) ? (MIN_THRES) : (score_threshold);
            sigma            = ((float)(rand() % 10)) / 10 + 0.01;
            weighting_method = rand() % 2;
            gen_test_size(threshold, size, NODECHIP_RAND_TEST);
            break;
        }
        default: {
            cout << "gen mode error" << endl;
            exit(-1);
        }
    }

    return 0;
}

static u64 allocate_dev_mem(bm_handle_t      handle,
                            bm_device_mem_t *dst_dev_mem,
                            int32_t          size) {
    u64 addr = 0;

    bm_malloc_device_byte(handle, dst_dev_mem, size);
    addr = bm_mem_get_device_addr(*dst_dev_mem);

    return addr;
}

inline bool pair_compare_bbox(const std::pair<face_rect_t, int> &a,
                              const std::pair<face_rect_t, int> &b) {
    return a.first.score > b.first.score;
}

inline bool compareBBox(const face_rect_t &a, const face_rect_t &b) {
    return a.score > b.score;
}

void nms_ref(const std::vector<face_rect_t> &proposals,
             const float                     nms_threshold,
             std::vector<face_rect_t> &      nmsProposals) {
    if (proposals.empty()) {
        nmsProposals.clear();
        return;
    }
    std::vector<face_rect_t> bboxes = proposals;
    // std::sort(bboxes.begin(), bboxes.end(), compareBBox);
    std::stable_sort(bboxes.begin(), bboxes.end(), compareBBox);
    int              select_idx = 0;
    int              num_bbox   = bboxes.size();
    std::vector<int> mask_merged(num_bbox, 0);
    bool             all_merged = false;
    while (!all_merged) {
        while (select_idx < num_bbox && 1 == mask_merged[select_idx])
            ++select_idx;
        if (select_idx == num_bbox) {
            all_merged = true;
            continue;
        }
        nmsProposals.push_back(bboxes[select_idx]);
        mask_merged[select_idx] = 1;
        face_rect_t select_bbox = bboxes[select_idx];
        float       area1       = (select_bbox.x2 - select_bbox.x1 + 1) *
                      (select_bbox.y2 - select_bbox.y1 + 1);
        ++select_idx;
        for (int i = select_idx; i < num_bbox; ++i) {
            if (mask_merged[i] == 1) {
                continue;
            }
            face_rect_t &bbox_i = bboxes[i];
            #ifdef __linux__
            float        x      = std::max(select_bbox.x1, bbox_i.x1);
            float        y      = std::max(select_bbox.y1, bbox_i.y1);
            float        w      = std::min(select_bbox.x2, bbox_i.x2) - x + 1;
            float        h      = std::min(select_bbox.y2, bbox_i.y2) - y + 1;
            #else
            float x = (std::max)(select_bbox.x1, bbox_i.x1);
            float y = (std::max)(select_bbox.y1, bbox_i.y1);
            float w = (std::min)(select_bbox.x2, bbox_i.x2) - x + 1;
            float h = (std::min)(select_bbox.y2, bbox_i.y2) - y + 1;
            #endif
            if (w <= 0 || h <= 0)
                continue;
            float area2 =
                (bbox_i.x2 - bbox_i.x1 + 1) * (bbox_i.y2 - bbox_i.y1 + 1);
            float area_intersect = w * h;
            // Union method
            if (div_fnc(area_intersect, (area1 + area2 - area_intersect)) >
                nms_threshold) {
                mask_merged[i] = 1;
            }
        }
    }
}

bool face_rect_equal_cond(const face_rect_t &a, const face_rect_t &b) {
    const float threshold = 0.45;

    if ((fabs(a.x1 - b.x1) <= threshold) && (fabs(a.x2 - b.x2) <= threshold) &&
        (fabs(a.y1 - b.y1) <= threshold) && (fabs(a.y2 - b.y2) <= threshold) &&
        (fabs(a.score - b.score) <= threshold)) {
        return true;
    }

    return false;
}

bool compare_face_rect(const face_rect_t &ref, const face_rect_t &res) {
    if (true == face_rect_equal_cond(ref, res)) {
        return true;
    }
    if ((0 == nequal_ref_vec.size()) && (0 == nequal_res_vec.size())) {
        nequal_ref_vec.push_back(ref);
        nequal_res_vec.push_back(res);

        return true;
    }
    int32_t ref_i          = 0;
    bool    find_something = false;
    for (auto iter : nequal_ref_vec) {
        if (fabs(iter.score - ref.score) > 1e-6) {
            printf(
                "ref.x1:%f, res.x1:%f, ref.x2:%f, res.x2:%f,ref.y1:%f, "
                "res.y1:%f,ref.y2:%f,res.y2:%f,ref.score:%f,res.score:%"
                "f\r\n",
                ref.x1,
                res.x1,
                ref.x2,
                res.x2,
                ref.y1,
                res.y1,
                ref.y2,
                res.y2,
                ref.score,
                res.score);

            return false;
        }
        if (true == face_rect_equal_cond(res, iter)) {
            nequal_ref_vec.erase(nequal_ref_vec.begin() + ref_i);
            find_something = true;

            break;
        }
        ref_i++;
    }
    if (false == find_something) {
        nequal_res_vec.push_back(res);
    }
    int32_t res_i  = 0;
    find_something = false;
    for (auto iter : nequal_res_vec) {
        if (fabs(iter.score - res.score) > 1e-6) {
            printf(
                "ref.x1:%f, res.x1:%f, ref.x2:%f, res.x2:%f,ref.y1:%f, "
                "res.y1:%f,ref.y2:%f,res.y2:%f,ref.score:%f,res.score:%"
                "f\r\n",
                ref.x1,
                res.x1,
                ref.x2,
                res.x2,
                ref.y1,
                res.y1,
                ref.y2,
                res.y2,
                ref.score,
                res.score);

            return false;
        }
        if (true == face_rect_equal_cond(ref, iter)) {
            nequal_res_vec.erase(nequal_res_vec.begin() + res_i);
            find_something = true;

            break;
        }
        res_i++;
    }
    if (false == find_something) {
        nequal_ref_vec.push_back(ref);
    }

    return true;
}

bool result_compare(vector<face_rect_t> &ref, nms_proposal_t *p_result) {
    if (((int32_t)ref.size()) != (int32_t)p_result->size) {
        printf("ERROR BOX NUMBER: exp=%d but got=%d\n", (int32_t)ref.size(), (int32_t)p_result->size);
        return false;
    }
    vector<face_rect_t> result;
    for (int32_t i = 0; i < p_result->size; i++) {
        result.push_back(p_result->face_rect[i]);
    }

    if (false == equal(ref.begin(), ref.end(), result.begin(), compare_face_rect)) {
        return false;
    } else {
        if (0 != (nequal_ref_vec.size()) || 0 != (nequal_res_vec.size())) {
            return false;
        }
    }

    return true;
}

template <typename data_type>
static bool generate_random_buf(std::vector<data_type> &random_buffer,
                                int                     random_min,
                                int                     random_max,
                                int                     scale) {
    for (int i = 0; i < scale; i++) {
        data_type data_val = (data_type)(
            random_min + (((float)((random_max - random_min) * i)) / scale));
        random_buffer.push_back(data_val);
    }
    std::random_shuffle(random_buffer.begin(), random_buffer.end());

    return false;
}

int32_t cv_nms_test_rand(bm_handle_t handle, int box_num) {
    struct timespec tp;
    clock_gettime_(0, &tp);
    unsigned int seed1 = tp.tv_nsec;
    if (seed != -1) {
        srand(seed);
        cout << "seed: " << seed << endl;
    } else {
        srand(seed1);
        cout << "ramdon seed: " << seed1 << endl;
    }
    bm_nms_data_type_t nms_threshold = 0.22;
    int                proposal_size = 0;
    int                rand_loop_num = 1;

    for (int rand_loop_idx = 0; rand_loop_idx < rand_loop_num;rand_loop_idx++) {
        for (int rand_mode = 0; rand_mode < MAX_RAND_MODE; rand_mode++) {
        // for (int rand_mode = 2; rand_mode < 3; rand_mode++) {
            gen_test_size(nms_threshold, proposal_size, rand_mode);
            proposal_size = box_num <= 0 ? proposal_size : box_num;
            proposal_size = 1000;
            std::cout << "[HARD NMS] rand_mode : " << rand_mode << std::endl;
            face_rect_t *   proposal_rand   = new face_rect_t[MAX_PROPOSAL_NUM];
            nms_proposal_t *output_proposal = new nms_proposal_t;

            std::vector<face_rect_t>        proposals_ref;
            vector<face_rect_t>             nms_proposal;
            std::vector<bm_nms_data_type_t> score_random_buf;
            generate_random_buf<bm_nms_data_type_t>(score_random_buf, 0, 1, 100000);
            for (int32_t i = 0; i < proposal_size; i++) {
                proposal_rand[i].x1 = ((bm_nms_data_type_t)(rand() % 100)) / 10;
                proposal_rand[i].x2 = proposal_rand[i].x1 + ((bm_nms_data_type_t)(rand() % 100)) / 10;
                proposal_rand[i].y1 = ((bm_nms_data_type_t)(rand() % 100)) / 10;
                proposal_rand[i].y2 = proposal_rand[i].y1 + ((bm_nms_data_type_t)(rand() % 100)) / 10;

                proposal_rand[i].score = score_random_buf[i];
                proposals_ref.push_back(proposal_rand[i]);
            }

            nms_ref(proposals_ref, nms_threshold, nms_proposal);

            struct timeval t1, t2;
            gettimeofday_(&t1);
            bmcv_nms(handle,
                    bm_mem_from_system(proposal_rand),
                    proposal_size,
                    nms_threshold,
                    bm_mem_from_system(output_proposal));
            gettimeofday_(&t2);
            cout << "hard using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

            if (false == result_compare(nms_proposal, output_proposal)) {
                delete[] proposal_rand;
                delete[] output_proposal;
                printf("nms test error !\r\n");
                exit(-1);
            }
            delete[] proposal_rand;
            delete output_proposal;
        }
    }

    return GEN_PROPOSAL_SUCCESS;
}

int soft_nms_ref(const std::vector<face_rect_t> &   proposals,
                 const float                        nms_threshold,
                 const float                        nms_score_threshold,
                 const float                        sigma,
                 std::function<float(float, float)> weighting_func,
                 std::vector<face_rect_t> &         nmsProposals,
                 std::vector<float>                 density_vec,
                 int                                nms_type = SOFT_NMS) {
    if (proposals.empty()) {
        nmsProposals.clear();
        return -1;
    }
    int   num_bbox               = proposals.size();
    float adaptive_nms_threshold = nms_threshold;
    // std::sort(bboxes.begin(), bboxes.end(), compareBBox);
    std::vector<std::pair<face_rect_t, float>> pair_vec;
    for (int i = 0; i < num_bbox; i++) {
        if (nms_type == ADAPTIVE_NMS) {
            pair_vec.push_back(std::make_pair(proposals[i], density_vec[i]));
        } else if (nms_type == SOFT_NMS) {
            pair_vec.push_back(std::make_pair(proposals[i], nms_threshold));
        }
    }
    for (int select_idx = 0; select_idx < num_bbox; select_idx++) {
        // std::stable_sort(bboxes.begin(), bboxes.end(), compareBBox);
        std::stable_sort(pair_vec.begin(), pair_vec.end(), pair_compare_bbox);
        face_rect_t select_bbox = pair_vec[select_idx].first;
        float       area1       = (select_bbox.x2 - select_bbox.x1 + 1) *
                      (select_bbox.y2 - select_bbox.y1 + 1);
        #ifdef __linux__
        adaptive_nms_threshold =
            std::max(nms_threshold, pair_vec[select_idx].second);
        #else
        adaptive_nms_threshold =
            (std::max)(nms_threshold, pair_vec[select_idx].second);
        #endif
        for (int i = select_idx + 1; i < num_bbox; ++i) {
            face_rect_t &bbox_i = pair_vec[i].first;
            #ifdef __linux__
            float        x      = std::max(select_bbox.x1, bbox_i.x1);
            float        y      = std::max(select_bbox.y1, bbox_i.y1);
            float        w      = std::min(select_bbox.x2, bbox_i.x2) - x + 1;
            float        h      = std::min(select_bbox.y2, bbox_i.y2) - y + 1;
            #else
            float x = (std::max)(select_bbox.x1, bbox_i.x1);
            float y = (std::max)(select_bbox.y1, bbox_i.y1);
            float w = (std::min)(select_bbox.x2, bbox_i.x2) - x + 1;
            float h = (std::min)(select_bbox.y2, bbox_i.y2) - y + 1;
            #endif
            if (w <= 0 || h <= 0)
                continue;
            float area2 =
                (bbox_i.x2 - bbox_i.x1 + 1) * (bbox_i.y2 - bbox_i.y1 + 1);
            float area_intersect = w * h;
            // Union method
            float overlap =
                div_fnc(area_intersect, (area1 + area2 - area_intersect));
            if (overlap >= adaptive_nms_threshold) {
                pair_vec[i].first.score *= weighting_func(overlap, sigma);
            }
        }
    }
    for (auto iter = pair_vec.begin(); iter != pair_vec.end(); iter++) {
        if (iter->first.score >= nms_score_threshold) {
            nmsProposals.push_back(iter->first);
        }
    }

    return 0;
}


int soft_nms_ref(const std::vector<face_rect_t> &   proposals,
                 const float                        nms_threshold,
                 const float                        nms_score_threshold,
                 const float                        sigma,
                 std::function<float(float, float)> weighting_func,
                 std::vector<face_rect_t> &         nmsProposals,
                 int                                nms_type = SOFT_NMS) {
    if (proposals.empty()) {
        nmsProposals.clear();
        return -1;
    }
    int   num_bbox               = proposals.size();
    float adaptive_nms_threshold = nms_threshold;
    std::vector<std::pair<face_rect_t, float>> pair_vec;

    for (int i = 0; i < num_bbox; i++) {
        if (nms_type == ADAPTIVE_NMS) {
            pair_vec.push_back(std::make_pair(proposals[i], proposals[i].score/*density_vec[i]*/));
        } else if (nms_type == SOFT_NMS) {
            pair_vec.push_back(std::make_pair(proposals[i], proposals[i].score/*nms_threshold*/));
        }
    }

    for (int select_idx = 0; select_idx < num_bbox; select_idx++) {
        std::stable_sort(pair_vec.begin(), pair_vec.end(), pair_compare_bbox);
        face_rect_t select_bbox = pair_vec[select_idx].first;
        float       area1       = (select_bbox.x2 - select_bbox.x1 + 1) *
                      (select_bbox.y2 - select_bbox.y1 + 1);

        for (int i = select_idx + 1; i < num_bbox; ++i) {
            face_rect_t &bbox_i = pair_vec[i].first;
            #ifdef __linux__
            float        x      = std::max(select_bbox.x1, bbox_i.x1);
            float        y      = std::max(select_bbox.y1, bbox_i.y1);
            float        w      = std::min(select_bbox.x2, bbox_i.x2) - x + 1;
            float        h      = std::min(select_bbox.y2, bbox_i.y2) - y + 1;
            #else
            float x = (std::max)(select_bbox.x1, bbox_i.x1);
            float y = (std::max)(select_bbox.y1, bbox_i.y1);
            float w = (std::min)(select_bbox.x2, bbox_i.x2) - x + 1;
            float h = (std::min)(select_bbox.y2, bbox_i.y2) - y + 1;
            #endif
            if (w <= 0 || h <= 0)
                continue;
            float area2 =
                (bbox_i.x2 - bbox_i.x1 + 1) * (bbox_i.y2 - bbox_i.y1 + 1);
            float area_intersect = w * h;
            // Union method
            float overlap =
                div_fnc(area_intersect, (area1 + area2 - area_intersect));

            UNUSED(adaptive_nms_threshold);
            // if (overlap >= adaptive_nms_threshold) {
                pair_vec[i].first.score *= weighting_func(overlap, sigma);
            // }
        }
    }
    for (auto iter = pair_vec.begin(); iter != pair_vec.end(); iter++) {
        if (iter->first.score >= nms_score_threshold) {
            iter->first.score = iter->second;
            nmsProposals.push_back(iter->first);
        }
    }
    //for to compare later conveniently, shanglin.guo
    std::stable_sort(nmsProposals.begin(), nmsProposals.end(), compareBBox);

    return 0;
}

int ssd_nms_ref(const std::vector<face_rect_t> proposals,
                const float                    nms_threshold,
                const float                    eta,
                std::vector<face_rect_t> &     nmsProposals) {
    std::vector<face_rect_t> bboxes = proposals;
    // std::sort(bboxes.begin(), bboxes.end(), compareBBox);
    std::stable_sort(bboxes.begin(), bboxes.end(), compareBBox);
    float adaptive_threshold = nms_threshold;
    nmsProposals.clear();
    while (bboxes.size() != 0) {
        face_rect_t bbox_i = bboxes.front();
        bool        keep   = true;
        float area2 = (bbox_i.x2 - bbox_i.x1 + 1) * (bbox_i.y2 - bbox_i.y1 + 1);
        for (unsigned int k = 0; k < nmsProposals.size(); ++k) {
            if (keep) {
                face_rect_t kept_bbox = nmsProposals[k];
                #ifdef __linux__
                float       x         = std::max(kept_bbox.x1, bbox_i.x1);
                float       y         = std::max(kept_bbox.y1, bbox_i.y1);
                float       w = std::min(kept_bbox.x2, bbox_i.x2) - x + 1;
                float       h = std::min(kept_bbox.y2, bbox_i.y2) - y + 1;
                #else
                float x = (std::max)(kept_bbox.x1, bbox_i.x1);
                float y = (std::max)(kept_bbox.y1, bbox_i.y1);
                float w = (std::min)(kept_bbox.x2, bbox_i.x2) - x + 1;
                float h = (std::min)(kept_bbox.y2, bbox_i.y2) - y + 1;
                #endif
                if (w <= 0 || h <= 0)
                    continue;
                float area_intersect = w * h;
                float area1          = (kept_bbox.x2 - kept_bbox.x1 + 1) *
                              (kept_bbox.y2 - kept_bbox.y1 + 1);
                // Union method
                float overlap =
                    div_fnc(area_intersect, (area1 + area2 - area_intersect));
                keep = overlap <= adaptive_threshold;
            } else {
                break;
            }
        }
        if (keep) {
            nmsProposals.push_back(bboxes.front());
        }
        bboxes.erase(bboxes.begin());
        if (keep && eta < 1 && adaptive_threshold > 0.5) {
            adaptive_threshold *= eta;
        }
    }

    return 0;
}

float linear_weighting(float overlap, float sigma) {
    UNUSED(sigma);

    return (1 - overlap);
}

float gaussian_weighting(float overlap, float sigma) {
    return exp(-1 * ((overlap * overlap) / sigma));
}

int32_t cv_soft_nms_test_rand(bm_handle_t handle) {
    struct timespec tp;
    clock_gettime_(0, &tp);

    unsigned int seed1 = tp.tv_nsec;
    // seed1              = 36551579;
    if (seed != -1) {
        srand(seed);
        cout << "seed: " << seed << endl;
    } else {
        srand(seed1);
        cout << "ramdon seed: " << seed1 << endl;
    }
    bm_nms_data_type_t                 nms_threshold       = 0.22;
    bm_nms_data_type_t                 nms_score_threshold = 0.22;
    bm_nms_data_type_t                 sigma               = 0.4;
    int                                proposal_size       = 0;
    int                                rand_loop_num       = 10;
    int                                weighting_method    = GAUSSIAN_WEIGHTING;
    std::function<float(float, float)> weighting_func;
    int                                nms_type            = SOFT_NMS;
    const int soft_nms_total_types                         = MAX_NMS_TYPE - HARD_NMS - 2;

    unsigned int chipid;
    if (BM_SUCCESS != bm_get_chipid(handle, &chipid)) {
        cout << "bmcv_nms_send_api get chipid error\r\n" << endl;
        return BM_ERR_FAILURE;
    }

    for (int rand_loop_idx = 0;rand_loop_idx < (rand_loop_num * soft_nms_total_types);rand_loop_idx++) {
        for (int rand_mode = 0; rand_mode < MAX_RAND_MODE; rand_mode++) {
        // for (int rand_mode = 0; rand_mode < 3; rand_mode++) {
            soft_nms_gen_test_size(nms_score_threshold,
                                   nms_threshold,
                                   sigma,
                                   proposal_size,
                                   weighting_method,
                                   rand_mode);
            nms_type = rand_loop_idx % soft_nms_total_types + HARD_NMS + 1;
            if (nms_type == ADAPTIVE_NMS) {
                std::cout << "[ADAPTIVE NMS] rand_mode : " << rand_mode
                          << std::endl;
            } else if (nms_type == SOFT_NMS) {
                std::cout << "[SOFT NMS] rand_mode : " << rand_mode
                          << std::endl;
            } else if (nms_type == SSD_NMS) {
                std::cout << "[SSD NMS] rand_mode : " << rand_mode << std::endl;
            } else {
                std::cout << "nms type error" << std::endl;
                exit(-1);
            }

            std::shared_ptr<Blob<face_rect_t>> proposal_rand =
                std::make_shared<Blob<face_rect_t>>(MAX_PROPOSAL_NUM);
            std::shared_ptr<nms_proposal_t> output_proposal =
                std::make_shared<nms_proposal_t>();

            std::vector<face_rect_t>        proposals_ref;
            std::vector<face_rect_t>        nms_proposal;
            std::vector<bm_nms_data_type_t> score_random_buf;
            std::vector<bm_nms_data_type_t> density_vec;
            std::shared_ptr<Blob<float>>    densities =
                std::make_shared<Blob<float>>(proposal_size);
            generate_random_buf<bm_nms_data_type_t>(
                score_random_buf, 0, 1, 10000);
            face_rect_t *proposal_rand_ptr = proposal_rand.get()->data;
            float        eta               = ((float)(rand() % 10)) / 10;

            for (int32_t i = 0; i < proposal_size; i++) {
                proposal_rand_ptr[i].x1 = ((bm_nms_data_type_t)(rand() % 100)) / 10;
                proposal_rand_ptr[i].x2 = proposal_rand_ptr[i].x1 + ((bm_nms_data_type_t)(rand() % 100)) / 10;
                proposal_rand_ptr[i].y1 = ((bm_nms_data_type_t)(rand() % 100)) / 10;
                proposal_rand_ptr[i].y2 = proposal_rand_ptr[i].y1 + ((bm_nms_data_type_t)(rand() % 100)) / 10;
                // // proposal_rand_ptr[i].score =
                // //    ((bm_nms_data_type_t)(rand() % 100)) / 100;
                // proposal_rand_ptr[i].score = score_random_buf[i];
                proposals_ref.push_back(proposal_rand_ptr[i]);
                densities.get()->data[i] = ((float)(rand() % 100)) / 100;
            }

            assert(proposal_size <= MAX_SOFT_SUPPORT_PROPOSAL_NUM);
            if (weighting_method == LINEAR_WEIGHTING) {
                weighting_func = linear_weighting;
            } else if (weighting_method == GAUSSIAN_WEIGHTING) {
                weighting_func = gaussian_weighting;
            } else {
                std::cout << "weighting_method error: " << weighting_method
                          << std::endl;
            }
            if (nms_type == ADAPTIVE_NMS) {
                for (int i = 0; i < proposal_size; i++) {
                    density_vec.push_back(densities.get()->data[i]);
                }
            } else {
                for (int i = 0; i < proposal_size; i++) {
                    density_vec.push_back(0);
                }
            }
            if (nms_type == SSD_NMS) {
                ssd_nms_ref(proposals_ref, nms_threshold, eta, nms_proposal);
            } else {
                if (chipid == BM1684X){
                    soft_nms_ref(proposals_ref,
                                nms_threshold,
                                nms_score_threshold,
                                sigma,
                                weighting_func,
                                nms_proposal,
                                nms_type);
                }else{
                    soft_nms_ref(proposals_ref,
                                nms_threshold,
                                nms_score_threshold,
                                sigma,
                                weighting_func,
                                nms_proposal,
                                density_vec,
                                nms_type);
                }
            }

            bmcv_nms_ext(handle,
                         bm_mem_from_system(proposal_rand.get()->data),
                         proposal_size,
                         nms_threshold,
                         bm_mem_from_system(output_proposal.get()),
                         1,
                         nms_score_threshold,
                         nms_type,
                         sigma,
                         weighting_method,
                         densities.get()->data,
                         eta);
            if (false == result_compare(nms_proposal, output_proposal.get())) {
                printf("nms test error !\r\n");
                exit(-1);
            }
        }
    }

    return GEN_PROPOSAL_SUCCESS;
}

#ifdef __linux__
void *test_nms_thread(void *arg) {
#else
DWORD WINAPI test_nms_thread(LPVOID arg) {
#endif
    nms_thread_arg_t *nms_thread_arg  = (nms_thread_arg_t *)arg;
    int test_loop_times = nms_thread_arg->trials;
    int box_num = nms_thread_arg->box_num;
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST NMS] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST NMS] LOOP " << loop_idx << "------"
                  << std::endl;
        bm_handle_t handle;
        int         dev_id = 0;
        bm_status_t ret    = bm_dev_request(&handle, dev_id);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            exit(-1);
        }
        cv_nms_test_rand(handle, box_num);
        cv_soft_nms_test_rand(handle);
        bm_dev_free(handle);
    }
    std::cout << "------[TEST NMS] ALL TEST PASSED!" << std::endl;

    return NULL;
}

int32_t main(int32_t argc, char **argv) {
    std::cout << "bmcv version:" << bm_get_bmcv_version() << std::endl;
    int test_loop_times  = 0;
    int test_threads_num = 1;
    int box_num = 0;
    if (argc == 1) {
        test_loop_times  = 1;
        test_threads_num = 1;
    } else if (argc == 2) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = 1;
    } else if (argc == 3) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = atoi(argv[2]);
    } else if (argc == 4) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = atoi(argv[2]);
        box_num          = atoi(argv[3]);
    } else if (argc == 5) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = atoi(argv[2]);
        box_num          = atoi(argv[3]);
        seed             = atoi(argv[4]);
    } else {
        std::cout << "command input error, please follow this "
                     "order:test_cv_nms loop_num multi_thread_num box_num seed"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST NMS] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST NMS] thread nums should be 1~4 " << std::endl;
        exit(-1);
    }
    #ifdef __linux__
    pthread_t *       pid            = new pthread_t[test_threads_num];
    nms_thread_arg_t *nms_thread_arg = new nms_thread_arg_t[test_threads_num];
    test_threads_num = 1;
    for (int i = 0; i < test_threads_num; i++) {
        nms_thread_arg[i].trials = test_loop_times;  // dev_id 0
        nms_thread_arg[i].box_num = box_num;
        if (pthread_create(
                &pid[i], NULL, test_nms_thread, &nms_thread_arg[i])) {
            delete[] pid;
            delete[] nms_thread_arg;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            delete[] nms_thread_arg;
            perror("Thread join failed");
            exit(-1);
        }
    }
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] pid;
    delete[] nms_thread_arg;
    #else
    #define THREAD_NUM 64
    DWORD dwThreadIdArray[THREAD_NUM];
    HANDLE hThreadArray[THREAD_NUM];
    nms_thread_arg_t *nms_thread_arg =
        new nms_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        nms_thread_arg[i].trials = test_loop_times;
        nms_thread_arg[i].box_num = box_num;
        hThreadArray[i] = CreateThread(
            NULL,                 // default security attributes
            0,                    // use default stack size
            test_nms_thread,      // thread function name
            &nms_thread_arg[i],   // argument to thread function
            0,                    // use default creation flags
            &dwThreadIdArray[i]); // returns the thread identifier
        if (hThreadArray[i] == NULL) {
            delete[] nms_thread_arg;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
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
        delete[] nms_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int i = 0; i < test_threads_num; i++)
        CloseHandle(hThreadArray[i]);

    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] nms_thread_arg;
    #endif

    return GEN_PROPOSAL_SUCCESS;
}