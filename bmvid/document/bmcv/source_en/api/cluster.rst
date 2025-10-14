bmcv_cluster
====================

Spectral clustering. Internally, this is implemented by sequentially calling: bmcv_cos_similarity, bmcv_matrix_prune, bmcv_lap_matrix, bmcv_qr, bmcv_knn.


**Processor model support**

This interface supports BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_cluster(
                    bm_handle_t handle,
                    bm_device_mem_t input,
                    bm_device_mem_t output,
                    int row,
                    int col,
                    float p,
                    int min_num_spks,
                    int max_num_spks,
                    int user_num_spks,
                    int weight_mode_KNN = 0,
                    int num_iter_KNN);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t input

  Input parameter. Holds the input matrix, sized row * col * sizeof(float32).

* bm_device_mem_t output

  Output parameter. Holds the KNN result labels, sized row * sizeof(int).

* int row

  Input parameter. The number of rows in the input matrix.

* int col

  Input parameter. The number of columns in the input matrix.

* float p

  Input parameter. Used in the pruning step as a proportional parameter to control the reduction of connections in the similarity matrix.

* int min_num_spks

  Input parameter. The minimum number of clusters.

* int max_num_spks

  Input parameter. The maximum number of clusters.

* int user_num_spks

  Input parameter. Specifies the number of eigenvectors to use, which can directly control the output number of clusters. If unspecified, it is computed based on the data.

* int weight_mode_KNN

  Input parameter. In the SciPy library, this parameter specifies the centroid initialization method for the K-means algorithm. A value of 0 represents CONST_WEIGHT, 1 represents POISSON_CPP, and 2 represents MT19937_CPP. The default is CONST_WEIGHT.

* int num_iter_KNN

  Input parameter. The number of iterations for the KNN algorithm.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note**

1. Currently, this interface only supports matrix data type of float.


**Code example:**

    .. code-block:: c

        #include "bmcv_api.h"
        #include "test_misc.h"
        #include <assert.h>
        #include <stdint.h>
        #include <stdio.h>
        #include <string.h>

        int main()
        {
            int row = 128;
            int col = 128;
            float p = 0.01;
            int min_num_spks = 2;
            int max_num_spks = 8;
            int num_iter_KNN = 2;
            int weight_mode_KNN = 0;
            int user_num_spks = -1;
            float* input_data = (float *)malloc(row * col * sizeof(float));
            int* output_data = (int *)malloc(row * max_num_spks * sizeof(int));
            bm_handle_t handle;
            bm_device_mem_t input, output;

            bm_dev_request(&handle, 0);

            for (int i = 0; i < row * col; ++i) {
                input_data[i] = (float)rand() / RAND_MAX;
            }

            bm_malloc_device_byte(handle, &input, sizeof(float) * row * col);
            bm_malloc_device_byte(handle, &output, sizeof(float) * row * max_num_spks);
            bm_memcpy_s2d(handle, input, input_data);
            bmcv_cluster(handle, input, output, row, col, p, min_num_spks, max_num_spks,
                        user_num_spks, weight_mode_KNN, num_iter_KNN);
            bm_memcpy_d2s(handle, output_data, output);

            bm_free_device(handle, input);
            bm_free_device(handle, output);
            free(input_data);
            free(output_data);
            bm_dev_free(handle);
            return 0;
        }