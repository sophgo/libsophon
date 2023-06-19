bmcv_hm_distance
==================

Calculates the Hamming distance of each element in two vectors.


**Interface form:**

    .. code-block:: c

         bmcv_hamming_distance(
            bm_handle_t handle,
            bm_device_mem_t input1,
            bm_device_mem_t input2,
            bm_device_mem_t output,
            int bits_len,
            int input1_num,
            int input2_num);

**Description of parameters:**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* bm_image input1

  Input parameters. Device address information for vector 1 data.

* bm_image input2

  Input parameters. Device address information for vector 2 data.

* bm_image output

  Output parameters. Device address information for output vector data.

* int bits_len

  Input parameters. The length of each element in the vector.

* int input1_num

  Input parameters. The num of vector 1 data.

* int input2_num

  Input parameters. The num of vector 2 data.


**Return value description:**

* BM_SUCCESS: success

* Other: failed

**Note:**

This interface only supports BM1684X.


**Code example:**


    .. code-block:: c

        int bits_len = 8;
        int input1_num = 2;
        int input2_num = 2562;

        int* input1_data = new int[input1_num * bits_len];
        int* input2_data = new int[input2_num * bits_len];
        int* output_ref  = new int[input1_num * input2_num];
        int* output_tpu  = new int[input1_num * input2_num];

        memset(input1_data, 0, input1_num * bits_len * sizeof(int));
        memset(input2_data, 0, input2_num * bits_len * sizeof(int));
        memset(output_ref,  0,  input1_num * input2_num * sizeof(int));
        memset(output_tpu,  0,  input1_num * input2_num * sizeof(int));

        // fill data
        for(int i = 0; i < input1_num * bits_len; i++) input1_data[i] = rand() % 10;
        for(int i = 0; i < input2_num * bits_len; i++) input2_data[i] = rand() % 20 + 1;

        bm_device_mem_t input1_dev_mem;
        bm_device_mem_t input2_dev_mem;
        bm_device_mem_t output_dev_mem;

        if(BM_SUCCESS != bm_malloc_device_byte(handle, &input1_dev_mem, input1_num * bits_len * sizeof(int))){
            std::cout << "malloc input fail" << std::endl;
            exit(-1);
        }

        if(BM_SUCCESS != bm_malloc_device_byte(handle, &input2_dev_mem, input2_num * bits_len * sizeof(int))){
            std::cout << "malloc input fail" << std::endl;
            exit(-1);
        }

        if(BM_SUCCESS != bm_malloc_device_byte(handle, &output_dev_mem, input1_num * input2_num * sizeof(int))){
            std::cout << "malloc input fail" << std::endl;
            exit(-1);
        }

        if(BM_SUCCESS != bm_memcpy_s2d(handle, input1_dev_mem, input1_data)){
            std::cout << "copy input1 to device fail" << std::endl;
            exit(-1);
        }

        if(BM_SUCCESS != bm_memcpy_s2d(handle, input2_dev_mem, input2_data)){
            std::cout << "copy input2 to device fail" << std::endl;
            exit(-1);
        }

        struct timeval t1, t2;
        gettimeofday(&t1, NULL);
        bm_status_t status = bmcv_hamming_distance(handle,
                                                   input1_dev_mem,
                                                   input2_dev_mem,
                                                   output_dev_mem,
                                                   bits_len,
                                                   input1_num,
                                                   input2_num);
        gettimeofday(&t2, NULL);
        cout << "--using time = " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "(us)--" << endl;

        if(status != BM_SUCCESS){
            printf("run bmcv_hamming_distance failed status = %d \n", status);
            bm_free_device(handle, input1_dev_mem);
            bm_free_device(handle, input2_dev_mem);
            bm_free_device(handle, output_dev_mem);
            bm_dev_free(handle);
            exit(-1);
        }

        if(BM_SUCCESS != bm_memcpy_d2s(handle, output_tpu, output_dev_mem)){
                std::cout << "bm_memcpy_d2s fail" << std::endl;
                exit(-1);
        }

        delete [] input1_data;
        delete [] input2_data;
        delete [] output_ref;
        delete [] output_tpu;
        bm_free_device(handle, input1_dev_mem);
        bm_free_device(handle, input2_dev_mem);
        bm_free_device(handle, output_dev_mem);