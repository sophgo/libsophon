bmcv_calc_cbcr_addr
===================

视频解码（Vdec）输出的压缩格式的地址时，可以通过 Y 压缩数据的物理地址，Y 通道数据的stride，以及原图的高，计算得出 CbCr 压缩数据 的物理地址。
此接口主要用于匹配内部解码器的压缩格式。使用方法请看示例。

**接口形式:**

  .. code-block:: c

     unsigned long long bmcv_calc_cbcr_addr(
       unsigned long long y_addr,
       unsigned int y_stride,
       unsigned int frame_height);

**输入参数说明：**

* unsigned long long y_addr

  输入参数。Y 压缩数据的物理地址。

* unsigned int y_stride

  输入参数。Y 压缩数据的stride。

* unsigned int frame_height

  输入参数。Y 压缩数据的物理地址。


**返回值说明：**

返回值即为CbCr 压缩数据 的物理地址。

    **示例代码**

    .. code-block:: c


                    bm_image src;
                    unsigned long long cbcr_addr;
                    bm_image_create(bm_handle,
                                    pFrame->height,
                                    pFrame->width,
                                    FORMAT_COMPRESSED,
                                    DATA_TYPE_EXT_1N_BYTE,
                                    &src,
                                    NULL);
                    bm_device_mem_t input_addr[4];
                    int size = pFrame->height * pFrame->stride[4];
                    input_addr[0] = bm_mem_from_device((unsigned long long)pFrame->buf[6], size);
                    size = (pFrame->height / 2) * pFrame->stride[5];
                    input_addr[1] = bm_mem_from_device((unsigned long long)pFrame->buf[4], size);
                    size = pFrame->stride[6];
                    input_addr[2] = bm_mem_from_device((unsigned long long)pFrame->buf[7], size);
                    size =  pFrame->stride[7];
                    cbcr_addr = bmcv_calc_cbcr_addr((unsigned long long)pFrame->buf[4], pFrame->stride[5], pFrame->height);
                    input_addr[3] = bm_mem_from_device(cbcr_addr, 0);
                    bm_image_attach(src, input_addr);


