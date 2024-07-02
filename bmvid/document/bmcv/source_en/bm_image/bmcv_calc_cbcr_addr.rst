bmcv_calc_cbcr_addr
===================

Video decoding (Vdec) outputs compressed format data, and the physical address of the Y compressed data,
the stride of the Y channel data, as well as the original image's height can be used to calculate the physical address of the CbCr compressed data. 
This interface is primarily used to match the internal decoder's compressed format.
For usage, please refer to the example provided.

**Interface form:**

  .. code-block:: c

     unsigned long long bmcv_calc_cbcr_addr(
       unsigned long long y_addr,
       unsigned int y_stride,
       unsigned int frame_height);

**Parameter description:**

* unsigned long long y_addr

  Input parameter. The physical address of the Y compressed data.

* unsigned int y_stride

  Input parameter. The stride of the Y compressed data.

* unsigned int frame_height

  Input parameter. The height of the Y compressed data.


**Return value description:**

The return value is the physical address of the CbCr compressed data.

**Code example:**

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


