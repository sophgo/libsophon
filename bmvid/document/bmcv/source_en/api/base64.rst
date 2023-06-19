bmcv_base64_enc(dec)
====================

A common encoding method in base64 network transmission, which uses 64 common characters to encode 6-bit binary numbers.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_base64_enc(bm_handle_t handle,
                bm_device_mem_t src,
                bm_device_mem_t dst,
                unsigned long len[2])

        bm_status_t bmcv_base64_dec(bm_handle_t handle,
                bm_device_mem_t src,
                bm_device_mem_t dst,
                unsigned long len[2])


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t src

  Input parameter. The address of the input character string. The type is bm_device_mem_t. It is required to call bm_mem_from_system() to convert the data address to the corresponding structure of bm_device_mem_t.

* bm_device_mem_t dst

  Input parameter. The address of the output character string. The type is bm_device_mem_t. It is required to call bm_mem_from_system() to convert the data address to the corresponding structure of bm_device_mem_t.

* unsigned long len[2]

  Input parameter. The length of base64 encoding or decoding, in bytes. Len [0] represents the input length, which needs to be given by the caller. Len [1] is the output length, which is calculated by API.


**Return value:**

* BM_SUCCESS: success

* Other: failed



**Code example:**

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


**Note:**

1. The API can encode and decode up to 128MB of data at a time, that is, the parameter len cannot exceed 128MB.

2. The supported incoming address type is system or device at the same time.

3. encoded_len[1] will give the output length, especially when decoding, calculate the number of bits to be removed according to the end of the input.
