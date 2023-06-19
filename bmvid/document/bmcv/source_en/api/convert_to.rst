bmcv_image_convert_to
=====================

The interface is used to do the linear change of image pixels. The specific data relationship can be expressed by the following formula:

.. math::
    \begin{array}{c}
    y=kx+b
    \end{array}

**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_convert_to (
                bm_handle_t handle,
                int input_num,
                bmcv_convert_to_attr convert_to_attr,
                bm_image* input,
                bm_image* output
        );


**Input parameter description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* int input_num

  Input parameter. The number of input images. If input_num > 1, then multiple input images must be stored continuously (users can use bm_image_alloc_contiguous_mem to apply continuous space for multiple images).

* bmcv_convert_to_attr convert_to_attr

  Input parameter. The configuration parameter corresponding to each image.

* bm_image\* input

  Input parameter. The input bm_image. The creation of each bm_image require the calling of bmcv_image_create externally. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* bm_image\* output

  Output parameter. The output bm_image. The creation of each bm_image require the calling of bmcv_image_create externally. Image memory can use bm_image_alloc_dev_mem to create new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be automatically allocated within the API.


**Return Value Description:**

* BM_SUCCESS: success

* Other: failed


**Data Type Description:**

    .. code-block:: c

        typedef struct bmcv_convert_to_attr_s{
                float alpha_0;
                float beta_0;
                float alpha_1;
                float beta_1;
                float alpha_2;
                float beta_2;
        } bmcv_convert_to_attr;


* alpha_0 describes the coefficient of the linear transformation of the 0th channel

* beta_0 describes the offset of the linear transformation of the 0th channel

* alpha_1 describes the coefficient of the linear transformation of the 1st channel

* beta_1 describes the offset of linear transformation of the 1st channel

* alpha_2 describes the coefficient of the linear transformation of the 2nd channel

* beta_2 describes the offset of linear transformation of the 2nd channel


**Code Example:**

    .. code-block:: c

        int image_num = 4, image_channel = 3;
        int image_w = 1920, image_h = 1080;
        bm_image input_images[4], output_images[4];
        bmcv_convert_to_attr convert_to_attr;
        convert_to_attr.alpha_0 = 1;
        convert_to_attr.beta_0 = 0;
        convert_to_attr.alpha_1 = 1;
        convert_to_attr.beta_1 = 0;
        convert_to_attr.alpha_2 = 1;
        convert_to_attr.beta_2 = 0;
        int img_size = image_w * image_h * image_channel;
        std::unique_ptr<unsigned char[]> img_data(
                new unsigned char[img_size * image_num]);
        std::unique_ptr<unsigned char[]> res_data(
                new unsigned char[img_size * image_num]);
        memset(img_data.get(), 0x11, img_size * image_num);
        for (int img_idx = 0; img_idx < image_num; img_idx++) {
          bm_image_create(handle,
                image_h,
                image_w,
                FORMAT_BGR_PLANAR,
                DATA_TYPE_EXT_1N_BYTE,
                &input_images[img_idx]);
        }
        bm_image_alloc_contiguous_mem(image_num, input_images, 0);
        for (int img_idx = 0; img_idx < image_num; img_idx++) {
          unsigned char *input_img_data = img_data.get() + img_size * img_idx;
          bm_image_copy_host_to_device(input_images[img_idx],
                (void **)&input_img_data);
        }

        for (int img_idx = 0; img_idx < image_num; img_idx++) {
          bm_image_create(handle,
                image_h,
                image_w,
                FORMAT_BGR_PLANAR,
                DATA_TYPE_EXT_1N_BYTE,
                &output_images[img_idx]);
        }
        bm_image_alloc_contiguous_mem(image_num, output_images, 1);
        bmcv_image_convert_to(handle, image_num, convert_to_attr, input_images,
                output_images);
        for (int img_idx = 0; img_idx < image_num; img_idx++) {
          unsigned char *res_img_data = res_data.get() + img_size * img_idx;
          bm_image_copy_device_to_host(output_images[img_idx],
                (void **)&res_img_data);
        }
        bm_image_free_contiguous_mem(image_num, input_images);
        bm_image_free_contiguous_mem(image_num, output_images);
        for(int i = 0; i < image_num; i++) {
          bm_image_destroy(input_images[i]);
          bm_image_destroy(output_images[i]);
        }

**Supported Format:**

1. This interface supports the conversion of the following image_format:

* FORMAT_BGR_PLANAR ——> FORMAT_BGR_PLANAR

* FORMAT_RGB_PLANAR ——> FORMAT_RGB_PLANAR

* FORMAT_GRAY ——> FORMAT_GRAY

2. This interface supports the conversion of data type in the following cases:

bm1684 supports the fllowing data_type:

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_FLOAT32

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_1N_BYTE

* DATA_TYPE_EXT_1N_BYTE_SIGNED ——> DATA_TYPE_EXT_1N_BYTE_SIGNED

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_1N_BYTE_SIGNED

* DATA_TYPE_EXT_FLOAT32 ——> DATA_TYPE_EXT_FLOAT32

* DATA_TYPE_EXT_4N_BYTE ——> DATA_TYPE_EXT_FLOAT32

bm1684x supports the fllowing data_type:

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_FLOAT32

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_1N_BYTE

* DATA_TYPE_EXT_1N_BYTE_SIGNED ——> DATA_TYPE_EXT_1N_BYTE_SIGNED

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_1N_BYTE_SIGNED

* DATA_TYPE_EXT_FLOAT32 ——> DATA_TYPE_EXT_FLOAT32

**Note:**

1. Before calling bmcv_image_convert_to(), users must ensure that the input image memory has been applied.

2. The input width, height, data _type and image_format must be the same.

3. The output width, height, data_type and image_format must be the same.

4. The width and height of the input image must be equal to the width and height of the output image.

5. image_num must be greater than 0.

6. The stride of the output image must be equal to the width.

7. The stride of the input image must be greater than or equal to the width.

8. bm1684 supports the maximum size is 2048*2048 and the minimum size is 16*16. When the image format is DATA_TYPE_EXT_4N_BYTE, w*h should not be greater than 1024*1024.

   bm1684x supports the maximum size is 4096*4096 and the minimum size is 16*16.
