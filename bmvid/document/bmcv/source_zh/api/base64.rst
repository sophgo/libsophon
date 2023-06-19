bmcv_base64_enc(dec)
====================

base64 网络传输中常用的编码方式，利用64个常用字符来对6位二进制数编码。


**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_base64_enc(bm_handle_t handle,
                bm_device_mem_t src,
                bm_device_mem_t dst,
                unsigned long len[2])

        bm_status_t bmcv_base64_dec(bm_handle_t handle,
                bm_device_mem_t src,
                bm_device_mem_t dst,
                unsigned long len[2])


**参数说明:**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_device_mem_t src

  输入参数。输入字符串所在地址，类型为bm_device_mem_t。需要调用 bm_mem_from_system()将数据地址转化成转化为 bm_device_mem_t 所对应的结构。

* bm_device_mem_t dst

  输入参数。输出字符串所在地址，类型为bm_device_mem_t。需要调用 bm_mem_from_system()将数据地址转化成转化为 bm_device_mem_t 所对应的结构。

* unsigned long len[2]

  输入参数。进行base64编码或解码的长度，单位是字节。其中len[0]代表输入长度，需要调用者给出。而len[1]为输出长度，由api计算后给出。


**返回值：**

* BM_SUCCESS: 成功

* 其他: 失败



**代码示例：**

    .. code-block:: c

        int original_len[2];
        int encoded_len[2];
        int original_len[0] = (rand() % 134217728) + 1;
        int encoded_len[0] = (original_len + 2) / 3 * 4;
        char *src = (char *)malloc((original_len + 3) * sizeof(char));
        char *dst = (char *)malloc((encoded_len + 3) * sizeof(char));
        for (j = 0; j < original_len; j++)
            a[j] = (char)((rand() % 100) + 1);

        bm_handle_t handle;
        ret = bm_dev_request(&handle, 0);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            exit(-1);
        }
        bmcv_base64_enc(
            handle,
            bm_mem_from_system(src),
            bm_mem_from_system(dst),
            original_len);

        bmcv_base64_dec(
            handle,
            bm_mem_from_system(dst),
            bm_mem_from_system(src),
            original_len);

        bm_dev_free(handle);
        free(src);
        free(dst);


**注意事项：**

1、该 api 一次最多可对 128MB 的数据进行编解码，即参数 len 不可超过128MB。

2、同时支持传入地址类型为system或device。

3、encoded_len[1]在会给出输出长度，尤其是解码时根据输入的末尾计算需要去掉的位数
