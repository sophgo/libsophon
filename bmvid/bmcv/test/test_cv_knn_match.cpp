#include <iostream>
#include <random>
#include <unordered_map>
#include "bmcv_api_ext.h"
#include <pthread.h>
#include <queue>
#include <sys/time.h>

#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

using namespace std;
pthread_mutex_t lock;
typedef struct {
    int loop_num;
    int n_ref;
    int n_ref_feat;
    int n_test_feat;
    int n_descriptor;
    int n_class;
    float ratio_thresh;
    bm_handle_t handle;
} cv_knn_match_thread_arg_t;

static inline void standard_euclidean(
    float*              std_euclidean_res,
    const float*        ref_data,
    const float*        Test_Input,
    const int           n_ref,
    const int           n_feats) {
    for (int i = 0; i < n_ref; i++) {
        // Compute distances from query to all data points
        float dist_sq = 0.0;
        for (int j = 0; j < n_feats; j++) {
            float diff = ref_data[i * n_feats + j] - Test_Input[j];
            dist_sq += diff * diff;
        }
        std_euclidean_res[i] = dist_sq;
    }
}

int hammiingDistance(int x, int y){
    int cnt = 0;
    int z = x ^ y;
    while (z != 0){
        cnt += z & 1;
        z = z >> 1;
    }
    return cnt;
}

static void hm_distance(const int *input1,
                        const int *input2,
                        int *output,
                        int bits_len,
                        int input2_num){
    for(int j = 0; j < input2_num; j++){
        int dist = 0;
        for(int d = 0; d < bits_len; d++){
            dist += hammiingDistance(input1[d], input2[j * bits_len + d]);
        }
        output[j] = dist;
    }
}

static void findNearest(
    const int*    sqdist,
    const int       n_ref,
    const int       k,
    int*          distance,
    int*            indices) {
    // Find k smallest distances using a simple approach
    // for better performance, consider using a priority queue
    std::priority_queue<std::pair<float, int>> max_heap;
    for (int i = 0; i < n_ref; i++) {
        if (max_heap.size() < static_cast<size_t>(k)) {
            max_heap.emplace(sqdist[i], i);
        }
        else if(sqdist[i] < max_heap.top().first) {
            max_heap.pop();
            max_heap.emplace(sqdist[i], i);
        }
    }
    // heap is max_heap, output should be inversed
    int count = max_heap.size();
    for (int i = count - 1; i >= 0; i--) {
        const auto& top = max_heap.top();
        distance[i] = top.first;
        indices[i] = top.second;
        max_heap.pop();
    }
}

void _knn2_classifier(
    const int*          ref_data,       // Referenced data points(n_ref_feat x n_descriptor)
    const int*          Test_input,     // test data points(n_test_feat x n_descriptor)
    int*                distance,       // Output distances(n_test_feat x k)
    int*                indices,        // Output indices of nearest neighbors(n_test_feat x k)
    const int           n_tests,        // Number of test data
    const int           n_ref,          // Number of referenced data
    const int           n_feats,        // Number of features
    const int           k               // Number of neighbors
    ) {
    // Validate k is reasonable
    if (k < 0 || k > n_feats) {
        printf("[ERROR] k must be between 1 and number of ref data\n");
        return;
    }

    // Temporary storage for all distances
    int *all_distance = new int[n_ref];

    int *id_dist = new int[k];
    int *id_index = new int[k];

    // Compute distances from Test_input to all data points
    for (int i = 0; i < n_tests; i++) {
        // standard_euclidean(all_distance, ref_data, Test_input + i * n_feats, n_ref, n_feats);
        hm_distance(Test_input + i * n_feats, ref_data, all_distance, n_feats, n_ref);
        findNearest(all_distance, n_ref, k, id_dist, id_index);
        for (int j = 0; j < k; j++) {
            distance[i * k + j] = id_dist[j];
            indices[i * k + j] = id_index[j];
        }
    }

    delete[] all_distance;
    delete[] id_dist;
    delete[] id_index;
}

void generate_data(
    int*                ref_data,
    int*                test_data,
    int                 n_ref,
    int                 n_ref_feat,
    int                 n_test_feat,
    int                 n_descriptor,
    int                 n_class,
    int*                ref_label,
    int*                test_label) {
    // Set seed for random generator
    std::random_device rd;
    std::mt19937 gen(rd());

    // Generate n_class normal distributions
    std::vector<std::normal_distribution<>> distributions;
    for (int c = 0; c < n_class; c++) {
        // You can customize mean and stddev for each class here
        // For example, means spaced evenly between -2.0 and 2.0
        double mean = 5.0 + 10.0 * c / (n_class - 1);  // Higher means to avoid negatives
        double stddev = 1.0 + 0.5 * c;  // Smaller stddev relative to mean
        distributions.emplace_back(mean, stddev);
    }

    // Generate ref_data
    for (int n = 0; n < n_ref; n++) {
        for (int i = 0; i < n_ref_feat; i++) {
            // Assign class labels evenly
            int class_id = i % n_class;
            ref_label[n * n_ref_feat + i] = class_id;
            for (int j = 0; j < n_descriptor; j++) {
                // Generate non-negative data
                int value;
                do {
                    value = static_cast<int>(std::round(distributions[class_id](gen)));
                } while (value < 0);  // Reject negative values
                ref_data[n * n_ref_feat * n_descriptor + i * n_descriptor + j] = value;
            }
        }
    }


    // Generate test_data
    // Generate test_data with different proportions
    // Define the desired proportions for each class in test data
    std::vector<float, std::allocator<float>> class_proportions(n_class);    // Example: Class 0 gets 50%, Class 1 gets 30%, Class 2 gets 20% (for n_class=3)
    // You can customize these proportions as needed
    float sum = 0.0f;
    for (int c = 0; c < n_class; c++) {
        class_proportions[c] = 1.0f / (c + 1);  // Example decreasing proportion
        sum += class_proportions[c];
    }
    // Normalize proportions
    for (int c = 0; c < n_class; c++) {
        class_proportions[c] /= sum;
    }

    std::vector<double> weights_double(class_proportions.begin(), class_proportions.end());
    // Generate test samples according to proportions
    std::discrete_distribution<> test_dist(weights_double.begin(), weights_double.end());

    for (int i = 0; i < n_test_feat; i++) {
        // Assign class labels according to specified proportions
        int class_id = test_dist(gen);
        test_label[i] = class_id;

        for (int j = 0; j < n_descriptor; j++) {
            // Generate non-negative data
            int value;
            do {
                value = static_cast<int>(std::round(distributions[class_id](gen)));
            } while (value < 0);  // Reject negative values
            test_data[i * n_descriptor + j] = value;
        }
    }
}

int tpu_knn_match(
    bm_handle_t     handle,
    int*            ref_data,           // n_ref x n_ref_feat x n_descriptor
    int*            test_data,          // n_test_feat x n_descriptor
    float*          distance_tpu,
    int*            good_match_tpu,
    int*            match_index,
    int             n_ref,
    int             n_ref_feat,
    int             n_test_feat,
    int             n_descriptor,
    float           ratio_thresh) {
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1, t2;
    int k = 2;
    bm_device_mem_t ref_data_dev_mem;       // [n_ref x n_ref_feat, n_descriptor]
    bm_device_mem_t test_data_dev_mem;      // [n_test_feat, n_descriptor]
    bm_device_mem_t distance_tpu_dev_mem;   // [n_ref x n_test_feat, k]
    bm_device_mem_t good_match_dev_mem;     // [2 * n_ref]
    bm_device_mem_t index_sorted_dev_mem;   // [n_ref]
    ret = bm_malloc_device_byte(handle, &ref_data_dev_mem, n_ref * n_ref_feat * n_descriptor * sizeof(int));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte ref_data_dev_mem failed.\n");
        goto exit0;
    }
    ret = bm_memcpy_s2d(handle, ref_data_dev_mem, ref_data);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d ref_data failed!\n");
        goto exit1;
    }
    ret = bm_malloc_device_byte(handle, &test_data_dev_mem, n_test_feat * n_descriptor * sizeof(int));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte test_data_dev_mem failed.\n");
        goto exit1;
    }
    ret = bm_memcpy_s2d(handle, test_data_dev_mem, test_data);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d test_data failed!\n");
        goto exit2;
    }
    ret = bm_malloc_device_byte(handle, &distance_tpu_dev_mem, n_ref * n_test_feat * k * sizeof(int));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte distance failed!\n");
        goto exit2;
    }
    ret = bm_malloc_device_byte(handle, &good_match_dev_mem, (2 * n_ref) * sizeof(int));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte indices failed!\n");
        goto exit3;
    }

    ret = bm_malloc_device_byte(handle, &index_sorted_dev_mem, n_ref * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte indices failed!\n");
        goto exit4;
    }
    pthread_mutex_lock(&lock);
    gettimeofday(&t1, NULL);
    ret = bmcv_knn_match(handle, ref_data_dev_mem, test_data_dev_mem, distance_tpu_dev_mem, good_match_dev_mem, index_sorted_dev_mem, n_ref, n_ref_feat, n_test_feat, n_descriptor, ratio_thresh);
    if (ret != BM_SUCCESS) {
        printf("KNN_match failed!\n");
        return ret;
    }
    gettimeofday(&t2, NULL);
    pthread_mutex_unlock(&lock);
    ret = bm_memcpy_d2s(handle, distance_tpu, distance_tpu_dev_mem);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s distance failed\n");
        goto exit5;
    }
    ret = bm_memcpy_d2s(handle, good_match_tpu, good_match_dev_mem);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s good_match failed\n");
        goto exit5;
    }
    ret = bm_memcpy_d2s(handle, match_index, index_sorted_dev_mem);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s match_index failed\n");
        goto exit5;
    }

exit5:
    bm_free_device(handle, index_sorted_dev_mem);
exit4:
    bm_free_device(handle, good_match_dev_mem);
exit3:
    bm_free_device(handle, distance_tpu_dev_mem);
exit2:
    bm_free_device(handle, test_data_dev_mem);
exit1:
    bm_free_device(handle, ref_data_dev_mem);
exit0:
    printf("KNN TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    return ret;
}

int cmp_res(int* good_match_cpu, int* good_match_tpu, int n_ref) {
    int count = 0;
    for (int i = 0; i < n_ref; i++) {
        if (fabs(good_match_cpu[i] - good_match_tpu[i]) > 0.001) {
            count++;
            printf("n_ref: %d, %d, %d\n", i, good_match_cpu[i], good_match_tpu[i]);
        }
    }
    if (count != 0) {
        if ((float)count / n_ref > 0.001) {
            printf("Compare knn_cpu with knn_tpu failed! count = %d\n", count);
            return -1;
        }
    }
    return 0;
}
int test_knn_match(int n_ref_, int n_ref_feat_, int n_test_feat_, int n_descriptor_, int n_class_, float ratio_thresh_, bm_handle_t handle) {
    int ret = 0;
    struct timeval t1 = {0, 0}, t2 = {0, 0};

    int n_ref = n_ref_;
    int n_ref_feat = n_ref_feat_;
    int n_test_feat = n_test_feat_;
    int n_descriptor = n_descriptor_;
    int n_class = n_class_;
    float ratio_thresh = ratio_thresh_;
    int k = 2;

    printf("KNN_match params: n_ref = %d, n_ref_feat = %d, n_test_feat = %d, n_descriptor = %d, n_class = %d, k = %d, ratio_thresh = %f\n", n_ref, n_ref_feat, n_test_feat, n_descriptor, n_class, k, ratio_thresh);

    int *ref_data = new int[n_ref * n_ref_feat * n_descriptor];
    int *ref_label = new int[n_ref * n_ref_feat];
    int *test_data = new int[n_test_feat * n_descriptor];
    int *test_label = new int[n_test_feat];

    generate_data(ref_data, test_data, n_ref, n_ref_feat, n_test_feat, n_descriptor, n_class, ref_label, test_label);

    float *distance_tpu = new float[n_ref * n_test_feat * k];
    int *good_match_tpu = new int[2 * n_ref];
    int *match_index = new int[n_ref];
    ret = tpu_knn_match(handle, ref_data, test_data, distance_tpu, good_match_tpu, match_index, n_ref, n_ref_feat, n_test_feat, n_descriptor, ratio_thresh);
    printf("best_match_time = %d, index = %d\n", good_match_tpu[n_ref], match_index[0]);

    int *distance_cpu = new int[n_test_feat * k];
    int *indices_cpu = new int[n_test_feat * k];
    int *good_match_cpu = new int[n_ref];

    gettimeofday(&t1, NULL);
    for (int i = 0; i < n_ref; i++) {
        good_match_cpu[i] = 0;
        _knn2_classifier(ref_data + i * n_ref_feat * n_descriptor, test_data, distance_cpu, indices_cpu, n_test_feat, n_ref_feat, n_descriptor, k);
        for (int j = 0; j < n_test_feat; j++) {
            if (distance_cpu[j * k]< ratio_thresh * distance_cpu[j * k + 1]) {
                good_match_cpu[i]++;
            }
        }
    }

    gettimeofday(&t2, NULL);
    printf("KNN CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));

    ret = cmp_res(good_match_cpu, good_match_tpu, n_ref);


    delete[] ref_data;
    delete[] ref_label;
    delete[] test_data;
    delete[] test_label;
    delete[] distance_cpu;
    delete[] distance_tpu;
    delete[] good_match_cpu;
    delete[] good_match_tpu;
    delete[] match_index;
    delete[] indices_cpu;
    return ret;
}

void *test_knn_match_thread(void* args) {
    cv_knn_match_thread_arg_t* cv_knn_match_thread_arg = (cv_knn_match_thread_arg_t*)args;
    int loop_num = cv_knn_match_thread_arg->loop_num;
    int n_ref = cv_knn_match_thread_arg->n_ref;
    int n_ref_feat = cv_knn_match_thread_arg->n_ref_feat;
    int n_test_feat = cv_knn_match_thread_arg->n_test_feat;
    int n_descriptor = cv_knn_match_thread_arg->n_descriptor;
    int n_class = cv_knn_match_thread_arg->n_class;
    float ratio_thresh = cv_knn_match_thread_arg->ratio_thresh;
    bm_handle_t handle = cv_knn_match_thread_arg->handle;
    for (int i = 0; i < loop_num; i++) {
        if (0 != test_knn_match(n_ref, n_ref_feat, n_test_feat, n_descriptor, n_class, ratio_thresh, handle)){
            printf("------TEST KNN match FAILED------\n");
            exit(-1);
        }
        printf("------TEST KNN match PASSED!------\n");
    }
    return NULL;
}

int main(int argc, char* args[]) {
    struct timespec tp;
    clock_gettime(0, &tp);
    srand(tp.tv_nsec);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> ref_dist(200, 1000);
    std::uniform_int_distribution<int> ref_feat_dist(200, 500);
    std::uniform_int_distribution<int> test_feat_dist(200, 500);
    std::uniform_int_distribution<int> descriptor_dist(10, 55);
    std::uniform_int_distribution<int> class_dist(2, 12);
    int thread_num = 1;
    int loop = 1;
    int n_ref = ref_dist(gen);
    int n_ref_feat = ref_feat_dist(gen);
    int n_test_feat = test_feat_dist(gen);
    int n_descriptor = descriptor_dist(gen);
    int n_class = class_dist(gen);
    float ratio_thresh = 0.7;
    int ret = 0;
    bm_handle_t handle;

    ret = bm_dev_request(&handle, 0);
    if (ret) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return ret;
    }

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage:\n");
        printf("%s thread_num loop n_ref n_ref_feat n_test_feat n_descriptor n_class ratio_thresh) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 2\n", args[0]);
        printf("%s 2 1 2000 1000 1000 18 6 0.7\n", args[0]);
        return 0;
    }
    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) n_ref = atoi(args[3]);
    if (argc > 4) n_ref_feat = atoi(args[4]);
    if (argc > 5) n_test_feat = atoi(args[5]);
    if (argc > 6) n_descriptor = atoi(args[6]);
    if (argc > 7) n_class = atoi(args[7]);
    if (argc > 8) ratio_thresh = atof(args[8]);

    pthread_t pid[thread_num];
    cv_knn_match_thread_arg_t cv_knn_match_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        cv_knn_match_thread_arg[i].loop_num = loop;
        cv_knn_match_thread_arg[i].n_ref = n_ref;
        cv_knn_match_thread_arg[i].n_ref_feat = n_ref_feat;
        cv_knn_match_thread_arg[i].n_test_feat = n_test_feat;
        cv_knn_match_thread_arg[i].n_descriptor = n_descriptor;
        cv_knn_match_thread_arg[i].n_class = n_class;
        cv_knn_match_thread_arg[i].ratio_thresh = ratio_thresh;
        cv_knn_match_thread_arg[i].handle = handle;
        if (pthread_create(pid + i, NULL, test_knn_match_thread, cv_knn_match_thread_arg + i) != 0) {
            printf("create thread failed\n");
            return -1;
        }
    }

    for (int i = 0; i < thread_num; i++) {
        int ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            printf("Thread join failed\n");
            exit(-1);
        }
    }

    bm_dev_free(handle);
    return ret;
}