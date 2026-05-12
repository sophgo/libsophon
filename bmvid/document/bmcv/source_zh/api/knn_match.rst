bmcv_knn_match
==================

在KNN算法的基础上添加多次匹配功能, 并统计匹配的好特征点个数, 距离为汉明距离。


**处理器型号支持：**

该接口支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_knn_match(
          bm_handle_t handle,
          bm_device_mem_t ref_addr,
          bm_device_mem_t test_addr,
          bm_device_mem_t distance_addr,
          bm_device_mem_t good_match_addr,
          bm_device_mem_t match_index_addr,
          int n_ref,
          int n_ref_feat,
          int n_test_feat,
          int n_descriptor,
          float ratio_thresh);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_device_mem_t ref_addr

  输入参数。存放底库数据。 形状为[n_ref x n_ref_feat, n_descriptor]。

* bm_device_mem_t test_addr

  输入参数。存放测试数据。形状为[n_test_feat, n_descriptor]。

* bm_device_mem_t distance_addr

  输出参数。存放KNN距离结果。形状为[n_ref x k, n_test_feat]。

* bm_device_mem_t good_match_addr

  输出参数。存放KNN好的匹配特征点结果。形状为[2, n_ref]。前n_ref个点为原始排列匹配好的特征点结果, 后n_ref个点为降序排列后的匹配好的特征点结果。

* bm_device_mem_t match_index_addr

  输出参数。存放KNN好的匹配特征点结果降序排列后在底库的索引。形状为[n_ref]。

* int n_ref

  输入参数。存放底库数据个数。

* int n_ref_feat

  输入参数。存放每个底库数据的特征点个数。

* int n_test_feat

  输入参数。存放测试数据的特征点个数。

* int n_descriptor

  输入参数。存放每个特征点描述子个数。

* float ratio_thresh

  输入参数。最近邻比率。判断是否是匹配的好的特征点。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**数据类型支持：**

输入数据目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+

输出数据目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+

**注意事项：**

  由于KNN中使用汉明距离, 输入参数须为无符号整数。

**代码示例：**

    .. code-block:: c
        #include <iostream>
        #include <random>
        #include <unordered_map>
        #include "bmcv_api_ext.h"
        // #include <pthread.h>
        #include <queue>
        #include <sys/time.h>

        #define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

        using namespace std;

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
            std::vector<float> class_proportions(n_class);
            float sum = 0.0f;
            for (int c = 0; c < n_class; c++) {
                class_proportions[c] = 1.0f / (c + 1);  // Example decreasing proportion
                sum += class_proportions[c];
            }
            // Normalize proportions
            for (int c = 0; c < n_class; c++) {
                class_proportions[c] /= sum;
            }

            // Generate test samples according to proportions
            std::discrete_distribution<> test_dist(class_proportions.begin(), class_proportions.end());

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

        int main() {
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

            printf("KNN_match params: n_ref = %d, n_ref_feat = %d, n_test_feat = %d, n_descriptor = %d, n_class = %d, ratio_thresh = %f\n", n_ref, n_ref_feat, n_test_feat, n_descriptor, n_class, ratio_thresh);

            int *ref_data = new int[n_ref * n_ref_feat * n_descriptor];
            int *ref_label = new int[n_ref * n_ref_feat];
            int *test_data = new int[n_test_feat * n_descriptor];
            int *test_label = new int[n_test_feat];
            int k = 2;

            generate_data(ref_data, test_data, n_ref, n_ref_feat, n_test_feat, n_descriptor, n_class, ref_label, test_label);

            float *distance_tpu = new float[n_ref * n_test_feat * k];
            int *good_match_tpu = new int[2 * n_ref];
            int *match_index = new int[n_ref];
            struct timeval t1, t2;
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

            gettimeofday(&t1, NULL);
            ret = bmcv_knn_match(handle, ref_data_dev_mem, test_data_dev_mem, distance_tpu_dev_mem, good_match_dev_mem, index_sorted_dev_mem, n_ref, n_ref_feat, n_test_feat, n_descriptor, ratio_thresh);
            if (ret != BM_SUCCESS) {
                printf("KNN_match failed!\n");
                return ret;
            }
            gettimeofday(&t2, NULL);

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


            printf("KNN TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
            printf("best_match_time = %d, index = %d\n", good_match_tpu[n_ref], match_index[0]);

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
            delete[] ref_data;
            delete[] ref_label;
            delete[] test_data;
            delete[] test_label;
            delete[] distance_tpu;
            delete[] good_match_tpu;
            delete[] match_index;
            bm_dev_free(handle);
            return ret;
        }