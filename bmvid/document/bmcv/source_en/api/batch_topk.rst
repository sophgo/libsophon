bmcv_batch_topk
================
Compute the largest or smallest k number in each db, and return the index.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_batch_topk(
                 bm_handle_t     handle,
                 bm_device_mem_t src_data_addr,
                 bm_device_mem_t src_index_addr,
                 bm_device_mem_t dst_data_addr,
                 bm_device_mem_t dst_index_addr,
                 bm_device_mem_t buffer_addr,
                 bool            src_index_valid,
                 int             k,
                 int             batch,
                 int *           per_batch_cnt,
                 bool            same_batch_cnt,
                 int             src_batch_stride,
                 bool            descending);


**Description of parameters:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t src_data_addr

  Input parameters. The device address information of input_data.

* bm_device_mem_t src_index_addr

  Input parameters. The device address information of input_index, when src_index_valid is true, set this parameter.

* bm_device_mem_t dst_data_addr

  Output parameters. The output_data device address information.

* bm_device_mem_t dst_index_addr

  Output parameters. The output_index device information

* bm_device_mem_t buffer_addr

  Input parameters. The buffer device address information.

* bool src_index_valid

  Input parameters. If true, use src_index, otherwise use auto-generated index.

* int k

  Input parameters. The value of k.

* int batch

  Input parameters. The number of batches.

* int * per_batch_cnt

  Input parameters. The amount of data in each batch.

* bool same_batch_cnt

  Input parameters. Determine whether each batch data is the same.

* int src_batch_stride

  Input parameters. The distance between two batches.

* bool descending

  Input parameters. Ascending or descending order.


**Return value description:**

* BM_SUCCESS: success

* Other: failed

**Format support:**

This interface currently only supports float32 type data.

**Code example:**

    .. code-block:: c


        int batch_num = 100000;
        int k = batch_num / 10;
        int descending = rand() % 2;
        int batch = rand() % 20 + 1;
        int batch_stride = batch_num;
        bool bottom_index_valid = true;

        bm_handle_t handle;
        bm_status_t ret = bm_dev_request(&handle, 0);
        if (ret != BM_SUCCESS) {
            std::cout << "Create bm handle failed. ret = " << ret << std::endl;
            exit(-1);
        }

        float* bottom_data = new float[batch * batch_stride * sizeof(float)];
        int* bottom_index = new int[batch * batch_stride];
        float* top_data = new float[batch * batch_stride * sizeof(float)];
        int* top_index = new int[batch * batch_stride];
        float* top_data_ref = new float[batch * k * sizeof(float)];
        int* top_index_ref = new int[batch * k];
        float* buffer = new float[3 * batch_stride * sizeof(float)];

        for(int i = 0; i < batch; i++){
            for(int j = 0; j < batch_num; j++){
                bottom_data[i * batch_stride + j] = rand() % 10000 * 1.0f;
                bottom_index[i * batch_stride + j] = i * batch_stride + j;
            }
        }

        bm_status_t ret = bmcv_batch_topk( handle,
                                           bm_mem_from_system((void*)bottom_data),
                                           bm_mem_from_system((void*)bottom_index),
                                           bm_mem_from_system((void*)top_data),
                                           bm_mem_from_system((void*)top_index),
                                           bm_mem_from_system((void*)buffer),
                                           bottom_index_valid,
                                           k,
                                           batch,
                                           &batch_num,
                                           true,
                                           batch_stride,
                                           descending);

        if(ret == BM_SUCCESS){
            int data_cmp = -1;
            int index_cmp = -1;
            data_cmp = array_cmp( (float*)top_data_ref,
                                  (float*)top_data,
                                  batch * k,
                                  "topk data",
                                  0);
            index_cmp = array_cmp( (float*)top_index_ref,
                                   (float*)top_index,
                                   batch * k,
                                   "topk index",
                                   0);
            if (data_cmp == 0 && index_cmp == 0) {
                printf("Compare success for topk data and index!\n");
            } else {
                printf("Compare failed for topk data and index!\n");
                exit(-1);
            }
        } else {
            printf("Compare failed for topk data and index!\n");
            exit(-1);
        }
        delete [] bottom_data;
        delete [] bottom_index;
        delete [] top_data;
        delete [] top_data_ref;
        delete [] top_index;
        delete [] top_index_ref;
        bm_dev_free(handle);
