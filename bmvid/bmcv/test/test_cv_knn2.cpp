#include <pthread.h>
#include <queue>
#include <iostream>
#include "test_misc.h"
#include <random>
#include <unordered_map>
#include "bmcv_api_ext.h"
#include <sys/time.h>
#include <cassert>

pthread_mutex_t lock;
typedef struct {
    int loop_num;
    int n_ref;
    int n_test;
    int n_feat;
    int n_class;
    int k;
    bm_handle_t handle;
} cv_knn2_thread_arg_t;

#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

using namespace std;

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

static void findNearest(
    const float*    sqdist,
    const int       n_ref,
    const int       k,
    float*          distance,
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
        distance[i] = sqrtf(top.first);
        indices[i] = top.second;
        max_heap.pop();
    }
}

static void get_prediction(
    int             n_tests,
    int*            indices,
    int             k,
    const float*    ref_labels,
    int             ref_labels_size,
    float*          predictions)
{
    if (!indices || !ref_labels || !predictions) {
        std::cerr << "Error: Null pointer detected!" << std::endl;
        return;
    }

    for (int i = 0; i < n_tests; i++) {
        std::unordered_map<int, int> label_counts;

        for (int j = 0; j < k; j++) {
            int neighbor_index = indices[i * k + j];

            if (neighbor_index < 0 || neighbor_index >= ref_labels_size) {
                std::cerr << "Warning: Invalid neighbor index " << neighbor_index << std::endl;
                continue;
            }

            float label = ref_labels[neighbor_index];
            label_counts[label]++;
        }

        int max_count = 0;
        float predicted_label = -1.0;

        for (const auto& pair : label_counts) {
            if (pair.second > max_count ||
                (pair.second == max_count && pair.first < predicted_label)) {
                max_count = pair.second;
                predicted_label = pair.first;
            }
        }
        predictions[i] = predicted_label;
    }
}

void _knn2_classifier(
    const float*        ref_data,       // Referenced data points(n_ref x n_features)
    const float*        ref_labels,     // Corresponding labels(n_ref)
    const float*        Test_input,     // test data points(n_tests x n_features)
    float*              distance,       // Output distances(n_tests x k)
    int*                indices,        // Output indices of nearest neighbors(n_tests x k)
    float*              predictions,    // Prediction index of test data(n_test)
    const int           n_tests,        // Number of test data
    const int           n_ref,          // Number of referenced data
    const int           n_feats,        // Number of features
    const int           k               // Number of neighbors
    ) {
    // Validate k is reasonable
    if (k < 0 || k > n_ref) {
        printf("[ERROR] k must be between 1 and number of ref data\n");
        return;
    }

    // Temporary storage for all distances
    float *all_distance = new float[n_ref];

    float *id_dist = new float[k];
    int *id_index = new int[k];

    // Compute distances from Test_input to all data points
    for (int i = 0; i < n_tests; i++) {
        standard_euclidean(all_distance, ref_data, &Test_input[i * n_feats], n_ref, n_feats);
        findNearest(all_distance, n_ref, k, id_dist, id_index);
        for (int j = 0; j < k; j++) {
            distance[i * k + j] = id_dist[j];
            indices[i * k + j] = id_index[j];
        }
    }

    get_prediction(n_tests, indices, k, ref_labels, n_ref, predictions);

    delete[] all_distance;
    delete[] id_dist;
    delete[] id_index;
}

void generate_data(
    float*              ref_data,
    float*              test_data,
    int                 n_ref,
    int                 n_test,
    int                 n_feat,
    int                 n_class,
    float*              ref_label,
    float*              test_label) {
    // Set seed for random generator
    std::random_device rd;
    std::mt19937 gen(rd());

    // Generate n_class normal distributions
    std::vector<std::normal_distribution<>> distributions;
    for (int c = 0; c < n_class; c++) {
        // You can customize mean and stddev for each class here
        // For example, means spaced evenly between -2.0 and 2.0
        double mean = -2.0 + 4.0 * c / (n_class - 1);
        double stddev = 0.3 + 0.1 * c;  // Varying stddev slightly
        distributions.emplace_back(mean, stddev);
    }

    // Generate ref_data
    for (int i = 0; i < n_ref; i++) {
        // Assign class labels evenly
        int class_id = i % n_class;
        ref_label[i] = static_cast<float>(class_id);

        for (int j = 0; j < n_feat; j++) {
            // Generate data from the corresponding distribution
            ref_data[i * n_feat + j] = distributions[class_id](gen);
        }
    }

    // Generate test_data
    // Generate test_data with different proportions
    // Define the desired proportions for each class in test data
    std::vector<float > class_proportions(n_class);
    // Example: Class 0 gets 50%, Class 1 gets 30%, Class 2 gets 20% (for n_class=3)
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

    for (int i = 0; i < n_test; i++) {
        // Assign class labels according to specified proportions
        int class_id = test_dist(gen);
        test_label[i] = static_cast<float>(class_id);

        for (int j = 0; j < n_feat; j++) {
            // Generate data from the corresponding distribution
            test_data[i * n_feat + j] = distributions[class_id](gen);
        }
    }
}

int tpu_knn2(
    bm_handle_t     handle,
    float*          ref_data,           // n_ref x n_feat
    float*          ref_label,          // n_ref
    float*          test_data,          // n_test x n_feat
    float*          distance_tpu,       // n_test x k
    int*            indices_tpu,        // n_test x k
    float*          prediction_tpu,     // n_test
    int             n_test,
    int             n_ref,
    int             n_feat,
    int             k) {
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1, t2;
    bm_device_mem_t ref_data_dev_mem;       // [n_ref, n_feat]
    bm_device_mem_t test_data_dev_mem;      // [n_test, n_feat]
    bm_device_mem_t distance_tpu_dev_mem;   // [n_test, k]
    bm_device_mem_t indices_tpu_dev_mem;    // [n_test, k]

    ret = bm_malloc_device_byte(handle, &ref_data_dev_mem, n_ref * n_feat * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte ref_data_dev_mem failed.\n");
        goto exit0;
    }
    ret = bm_memcpy_s2d(handle, ref_data_dev_mem, ref_data);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d ref_data failed!\n");
        goto exit1;
    }
    ret = bm_malloc_device_byte(handle, &test_data_dev_mem, n_test * n_feat * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte test_data_dev_mem failed.\n");
        goto exit1;
    }
    ret = bm_memcpy_s2d(handle, test_data_dev_mem, test_data);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d test_data failed!\n");
        goto exit2;
    }
    ret = bm_malloc_device_byte(handle, &distance_tpu_dev_mem, n_test * k * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte distance failed!\n");
        goto exit2;
    }
    ret = bm_malloc_device_byte(handle, &indices_tpu_dev_mem, n_test * k * sizeof(int));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte indices failed!\n");
        goto exit3;
    }

    pthread_mutex_lock(&lock);
    ret = bmcv_knn2(handle, ref_data_dev_mem, test_data_dev_mem, distance_tpu_dev_mem, indices_tpu_dev_mem, n_test, n_ref, n_feat, k);
    gettimeofday(&t1, NULL);
    ret = bmcv_knn2(handle, ref_data_dev_mem, test_data_dev_mem, distance_tpu_dev_mem, indices_tpu_dev_mem, n_test, n_ref, n_feat, k);
    if (ret != BM_SUCCESS) {
        printf("KNN2 failed!\n");
        return ret;
    }
    pthread_mutex_unlock(&lock);
    gettimeofday(&t2, NULL);
    printf("KNN TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    ret = bm_memcpy_d2s(handle, distance_tpu, distance_tpu_dev_mem);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s distance failed\n");
        goto exit4;
    }
    ret = bm_memcpy_d2s(handle, indices_tpu, indices_tpu_dev_mem);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s indices failed\n");
        goto exit4;
    }

    get_prediction(n_test, indices_tpu, k, ref_label, n_ref, prediction_tpu);

exit4:
    bm_free_device(handle, indices_tpu_dev_mem);
exit3:
    bm_free_device(handle, distance_tpu_dev_mem);
exit2:
    bm_free_device(handle, test_data_dev_mem);
exit1:
    bm_free_device(handle, ref_data_dev_mem);
exit0:
    return ret;
}

void print_result(
    int n_class,
    int n_test,
    float* test_label,
    float* prediction_cpu) {
    // Count different classes num
    std::vector<int> class_num(n_class, 0);
    for (int i = 0; i < n_test; i++) {
        int class_id = static_cast<int>(prediction_cpu[i]);
        class_num[class_id] += 1;
    }
    for (int i = 0; i < n_class; i++) {
        std::cout << "Class " << i << ": " << class_num[i] << std::endl;
    }

    // Print results for each test sample
    // printf("Results for n_class = %d:\n", n_class);
    // for (int i = 0; i < n_test; i++) {
    //     printf("Test sample %d: Predicted class = %.0f, Actual class = %.0f\n",
    //         i, prediction_cpu[i], test_label[i]);
    // }

    // Calculate and print accuracy
    int correct = 0;
    for (int i = 0; i < n_test; i++) {
        if (prediction_cpu[i] == test_label[i]) {
            correct++;
        }
    }
    float accuracy = (float)correct / n_test * 100;
    printf("Accuracy: %.2f%%\n", accuracy);
}

int cmp_res(float* distance_cpu, float* distance_tpu, int n_test, int k) {
    int count = 0;
    for (int i = 0; i < n_test; i++) {
        for (int j = 0; j < k; j++) {
            if (fabs(distance_cpu[i * k + j] - distance_tpu[i * k + j])>0.01) {
                printf("cpu_dist[%d][%d] = %f, tpu_dist[%d][%d] = %f\n", i, j, distance_cpu[i*k+j], i, j, distance_tpu[i*k+j]);
                count++;
            }
        }
        if (count != 0) {
            if ((float)count / k > 0.1) {
                printf("Compare knn_cpu with knn_tpu failed! %dth n_test, count = %d\n", i, count);
                return -1;
            }
        }
    }
    return 0;
}
int test_knn2(int n_ref_, int n_test_, int n_feat_, int n_class_, int k_, bm_handle_t handle) {
    int ret = 0;
    struct timeval t1 = {0, 0}, t2 = {0, 0};

    int n_ref = n_ref_;
    int n_test = n_test_;
    int n_feat = n_feat_;
    int n_class = n_class_;
    int k = k_;
    printf("KNN params: n_ref = %d, n_test = %d, n_feat = %d, n_class = %d, k = %d\n", n_ref, n_test, n_feat, n_class, k);
    float *ref_data = new float[n_ref * n_feat];
    float *ref_label = new float[n_ref];
    float *test_data = new float[n_test * n_feat];
    float *test_label = new float[n_test];

    generate_data(ref_data, test_data, n_ref, n_test, n_feat, n_class, ref_label, test_label);

    float *distance_cpu = new float[n_test * k];
    int *indices_cpu = new int[n_test * k];
    float *prediction_cpu = new float[n_test];
    gettimeofday(&t1, NULL);
    _knn2_classifier(ref_data, ref_label, test_data, distance_cpu, indices_cpu, prediction_cpu, n_test, n_ref, n_feat, k);
    gettimeofday(&t2, NULL);
    printf("KNN CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    print_result(n_class, n_test, test_label, prediction_cpu);


    float *distance_tpu = new float[n_test * k];
    int *indices_tpu = new int[n_test * k];
    float *prediction_tpu = new float[n_test];

    ret = tpu_knn2(handle, ref_data, ref_label, test_data, distance_tpu, indices_tpu, prediction_tpu, n_test, n_ref, n_feat, k);

    print_result(n_class, n_test, test_label, prediction_tpu);
    ret = cmp_res(distance_cpu, distance_tpu, n_test, k);


    delete[] ref_data;
    delete[] ref_label;
    delete[] test_data;
    delete[] test_label;
    delete[] distance_cpu;
    delete[] indices_cpu;
    delete[] prediction_cpu;
    delete[] distance_tpu;
    delete[] indices_tpu;
    delete[] prediction_tpu;
    return ret;
}

void* test_knn2_thread(void* args) {
    cv_knn2_thread_arg_t* cv_knn2_thread_arg = (cv_knn2_thread_arg_t*)args;
    int loop_num = cv_knn2_thread_arg->loop_num;
    int n_ref = cv_knn2_thread_arg->n_ref;
    int n_test = cv_knn2_thread_arg->n_test;
    int n_feat = cv_knn2_thread_arg->n_feat;
    int n_class = cv_knn2_thread_arg->n_class;
    int k = cv_knn2_thread_arg->k;
    bm_handle_t handle = cv_knn2_thread_arg->handle;
    for (int i = 0; i < loop_num; i++) {
        if (0 != test_knn2(n_ref, n_test, n_feat, n_class, k, handle)){
            printf("------TEST KNN FAILED------\n");
            exit(-1);
        }
        printf("------TEST KNN PASSED!------\n");
    }
    return NULL;
}

int main(int argc, char* args[]) {
    struct timespec tp;
    clock_gettime_(0, &tp);
    srand(tp.tv_nsec);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> ref_dist(1000, 5000);
    std::uniform_int_distribution<int> test_dist(1000, 5000);
    std::uniform_int_distribution<int> feat_dist(5, 55);
    std::uniform_int_distribution<int> class_dist(2, 12);
    std::uniform_int_distribution<int> k_dist(1, 20);

    int loop = 1;
    int thread_num = 1;
    int n_ref = ref_dist(gen);
    int n_test = test_dist(gen);
    int n_feat = feat_dist(gen);
    int n_class = class_dist(gen);
    int k = k_dist(gen);
    int ret = 0;
    bm_handle_t handle;

    ret = bm_dev_request(&handle, 0);
    if (ret) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return ret;
    }

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage:\n");
        printf("%s thread_num loop n_ref n_test n_feat n_class k) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 2\n", args[0]);
        printf("%s 2 2 2000 1000 18 6 9 \n", args[0]);
        return 0;
    }

    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) n_ref = atoi(args[3]);
    if (argc > 4) n_test = atoi(args[4]);
    if (argc > 5) n_feat = atoi(args[5]);
    if (argc > 6) n_class = atoi(args[6]);
    if (argc > 7) k = atoi(args[7]);

    pthread_t pid[thread_num];
    cv_knn2_thread_arg_t cv_knn2_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        cv_knn2_thread_arg[i].loop_num = loop;
        cv_knn2_thread_arg[i].n_ref = n_ref;
        cv_knn2_thread_arg[i].n_test = n_test;
        cv_knn2_thread_arg[i].n_feat = n_feat;
        cv_knn2_thread_arg[i].n_class = n_class;
        cv_knn2_thread_arg[i].k = k;
        cv_knn2_thread_arg[i].handle = handle;
        if (pthread_create(pid + i, NULL, test_knn2_thread, cv_knn2_thread_arg + i) != 0) {
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
