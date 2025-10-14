bmcv_knn2
==================

KNN, 距离为欧式距离。


**处理器型号支持：**

该接口支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_knn2(
            bm_handle_t handle,
            bm_device_mem_t ref_data_addr,
            bm_device_mem_t test_data_addr,
            bm_device_mem_t distance_addr,
            bm_device_mem_t indices_addr,
            int n_test,
            int n_ref,
            int n_feat,
            int k);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_device_mem_t ref_data_addr

  输入参数。存放底库数据。 形状为[n_ref, n_feat]。

* bm_device_mem_t test_data_addr

  输入参数。存放测试数据。形状为[n_test, n_feat]。

* bm_device_mem_t distance_addr

  输出参数。存放KNN距离结果。形状为[n_test, k]。

* bm_device_mem_t indices_addr

  输出参数。存放KNN索引结果。 形状为[n_test, k]。

* int n_test

  输入参数。存放测试数据行数(第一个维度)。

* int n_ref

  输入参数。存放底库数据行数(第一个维度)。

* int n_feat

  输入参数。存放输入矩阵的公有列数, 或最后维度 。

* int k

  输入参数。存放KNN的聚类簇个数。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**数据类型支持：**

输入数据目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+

输出数据目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+


**代码示例：**

    .. code-block:: c

        #include <iostream>
        #include "test_misc.h"
        #include <random>
        #include <unordered_map>
        #include "bmcv_api_ext.h"
        #include <sys/time.h>


        #define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

        using namespace std;

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

            int correct = 0;
            for (int i = 0; i < n_test; i++) {
                if (prediction_cpu[i] == test_label[i]) {
                    correct++;
                }
            }
            float accuracy = (float)correct / n_test * 100;
            printf("Accuracy: %.2f%%\n", accuracy);
        }

        int main() {
            struct timespec tp;
            clock_gettime_(0, &tp);
            srand(tp.tv_nsec);

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> ref_dist(3000, 5000);
            std::uniform_int_distribution<int> test_dist(1000, 3000);
            std::uniform_int_distribution<int> feat_dist(5, 55);
            std::uniform_int_distribution<int> class_dist(2, 12);
            std::uniform_int_distribution<int> k_dist(1, 20);

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
            printf("KNN params: n_ref = %d, n_test = %d, n_feat = %d, n_class = %d, k = %d\n", n_ref, n_test, n_feat, n_class, k);
            float *ref_data = new float[n_ref * n_feat];
            float *ref_label = new float[n_ref];
            float *test_data = new float[n_test * n_feat];
            float *test_label = new float[n_test];

            generate_data(ref_data, test_data, n_ref, n_test, n_feat, n_class, ref_label, test_label);

            float *distance_tpu = new float[n_test * k];
            int *indices_tpu = new int[n_test * k];
            float *prediction_tpu = new float[n_test];

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

            gettimeofday(&t1, NULL);
            ret = bmcv_knn2(handle, ref_data_dev_mem, test_data_dev_mem, distance_tpu_dev_mem, indices_tpu_dev_mem, n_test, n_ref, n_feat, k);
            if (ret != BM_SUCCESS) {
                printf("KNN2 failed!\n");
                return ret;
            }
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

            printf("KNN TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));

            print_result(n_class, n_test, test_label, prediction_tpu);
        exit4:
            bm_free_device(handle, indices_tpu_dev_mem);
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
            delete[] indices_tpu;
            delete[] prediction_tpu;
            bm_dev_free(handle);
            return ret;
        }
