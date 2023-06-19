bmcv_sort
=========

This interface can sort floating-point data (ascending/descending), and support the index corresponding to the original data after sorting.

**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_sort(bm_handle_t     handle,
                              bm_device_mem_t src_index_addr,
                              bm_device_mem_t src_data_addr,
                              int             data_cnt,
                              bm_device_mem_t dst_index_addr,
                              bm_device_mem_t dst_data_addr,
                              int             sort_cnt,
                              int             order,
                              bool            index_enable,
                              bool            auto_index);



**Input parameter description:**

* bm_handle_t handle

  Input parameter. The handle of input bm_handle.

* bm_device_mem_t  src_index_addr

  Input parameter. The address of the index corresponding to each input data. If index_enable rather than auto_index is used, this parameter is valid. bm_device_mem_t is the built-in data type representing the address. The function bm_mem_from_system (addr) can be used to convert the pointer or address used by ordinary users to this type. Users can refer to the example code.

* bm_device_mem_t  src_data_addr

  Input parameter. The address corresponding to the input data to be sorted. bm_device_mem_t is the built-in data type representing the address. The function bm_mem_from_system (addr) can be used to convert the pointer or address used by ordinary users to this type. Users can refer to the example code.

* int  data_cnt

  Input parameter. The number of input data to be sorted.

* bm_device_mem_t  dst_index_addr

  Output parameter. The address of the index corresponding to the output data after sorting. If index_enable rather than auto_index is used, this parameter is valid. bm_device_mem_t is the built-in data type representing the address. The function bm_mem_from_system (addr) can be used to convert the pointer or address used by ordinary users to this type. Users can refer to the example code.

* bm_device_mem_t  dst_data_addr

  Output parameter. The address corresponding to the sorted output data. bm_device_mem_t is the built-in data type representing the address. The function bm_mem_from_system (addr) can be used to convert the pointer or address used by ordinary users to this type. Users can refer to the example code.

* int  sort_cnt

  Input parameter. The quantity to be sorted, that is, the number of output results, including the ordered data and the corresponding index. For example, in descending order, if you only need to output the top 3 data, set this parameter to 3.

* int  order

  Input parameter. Ascending or descending, 0 means ascending and 1 means descending.

* bool  index_enable

  Input parameter. Whether index is enabled. If enabled, the index corresponding to the sorted data can be output; otherwise, src_index_addr and dst_index_addr will be invalid.

* bool  auto_index

  Input parameter. Whether to enable the automatic generation of index function. The premise of using this function is index_enable parameter is true. If the parameter is also true, it means counting from 0 according to the storage order of the input data as index and src_index_addr is invalid, and the index corresponding to the ordered data in the output result is stored in dst_index_addr.



**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. It is required that sort_cnt <= data_cnt.

2. If the auto index function is required, the precondition is the parameter index_enable is true.

3. The API can support full sorting of 1MB data at most.


**Sample code**


    .. code-block:: c

         int data_cnt = 100;
         int sort_cnt = 50;
         float src_data_p[100];
         int src_index_p[100];
         float dst_data_p[50];
         int dst_index_p[50];
         for (int i = 0; i < 100; i++) {
             src_data_p[i] = rand() % 1000;
             src_index_p[i] = 100 - i;
         }
         int order = 0;
         bmcv_sort(handle,
                   bm_mem_from_system(src_index_p),
                   bm_mem_from_system(src_data_p),
                   data_cnt,
                   bm_mem_from_system(dst_index_p),
                   bm_mem_from_system(dst_data_p),
                   sort_cnt,
                   order,
                   true,
                   false);


