bmcv_sort
=========

该接口可以实现浮点数据的排序（升序/降序），并且支持排序后可以得到原数据所对应的 index。

**接口形式：**

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



**输入参数说明：**

* bm_handle_t handle

  输入参数。输入的 bm_handle 句柄。

* bm_device_mem_t  src_index_addr

  输入参数。每个输入数据所对应 index 的地址。如果使能 index_enable 并且不使用 auto_index 时，则该参数有效。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* bm_device_mem_t  src_data_addr

  输入参数。待排序的输入数据所对应的地址。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* int  data_cnt

  输入参数。待排序的输入数据的数量。

* bm_device_mem_t  dst_index_addr

  输出参数。排序后输出数据所对应 index 的地址， 如果使能 index_enable 并且不使用 auto_index 时，则该参数有效。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* bm_device_mem_t  dst_data_addr

  输出参数。排序后的输出数据所对应的地址。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* int  sort_cnt

  输入参数。需要排序的数量，也就是输出结果的个数，包括排好序的数据和对应 index 。比如降序排列，如果只需要输出前 3 大的数据，则该参数设置为 3 即可。

* int  order

  输入参数。升序还是降序，0 表示升序， 1 表示降序。

* bool  index_enable

  输入参数。是否使能 index。如果使能即可输出排序后数据所对应的 index ，否则 src_index_addr 和 dst_index_addr 这两个参数无效。

* bool  auto_index

  输入参数。是否使能自动生成 index 功能。使用该功能的前提是 index_enable 参数为 true，如果该参数也为 true 则表示按照输入数据的存储顺序从 0 开始计数作为 index，参数 src_index_addr 便无效，输出结果中排好序数据所对应的 index 即存放于 dst_index_addr 地址中。



**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项：**

1、要求 sort_cnt <= data_cnt。

2、若需要使用 auto index 功能，前提是参数 index_enable 为 true。

3、该 api 至多可支持 1MB 数据的全排序。


**示例代码**

    
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


