bmcv_image_put_text
===================

The functions of writing (Chinese and English) on an image and specifying the color, size and width of words are supported.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        typedef struct {
            int x;
            int y;
        } bmcv_point_t;

        typedef struct {
            unsigned char r;
            unsigned char g;
            unsigned char b;
        } bmcv_color_t;

        bm_status_t bmcv_image_put_text(
                    bm_handle_t handle,
                    bm_image image,
                    const char* text,
                    bmcv_point_t org,
                    bmcv_color_t color,
                    float fontScale,
                    int thickness);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_image image

  Input / output parameter. The bm_image of image to be processed. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* const char* text

  Input parameter. The text content to be written. When there is Chinese in the text content, please set the parameter thickness to 0.

* bmcv_point_t org

  Input parameter. The coordinate position of the lower left corner of the first character. The upper left corner of the image is the origin, extending to the right in the x-direction and downward in the y-direction.

* bmcv_color_t color

  Input parameter. The color of the drawn line, which is the value of RGB three channels respectively.

* float fontScale

  Input parameter. Font scale.

* int thickness

  Input parameter. The width of the drawn line, which is recommended to be set to an even number for YUV format images. Please set this parameter to 0 when opening the Chinese character library.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

The interface currently supports the following images_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_GRAY            |
+-----+------------------------+
| 2   | FORMAT_YUV420P         |
+-----+------------------------+
| 3   | FORMAT_YUV422P         |
+-----+------------------------+
| 4   | FORMAT_YUV444P         |
+-----+------------------------+
| 5   | FORMAT_NV12            |
+-----+------------------------+
| 6   | FORMAT_NV21            |
+-----+------------------------+
| 7   | FORMAT_NV16            |
+-----+------------------------+
| 8   | FORMAT_NV61            |
+-----+------------------------+

The thickness parameter is configured to 0, which means that after opening the Chinese character library, the image_fata extension supports the formats supported by the bmcv_image_watermark_superpose API base image.

The following data_type is currently supported:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+

If the text content remains unchanged, it is recommended to use the text drawing method of combining bmcv_gen_text_watermark and bmcv_image_watermark_superpose to generate a watermark image for the text. Repeat the watermark image for OSD overlay to improve processing efficiency. For an example, please refer to the documentation of the bmcv_image_watermark_superpose interface.

**Code example:**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <stdio.h>
        #include <stdlib.h>

        static void readBin(const char* path, unsigned char* input_data, int size)
        {
            FILE *fp_src = fopen(path, "rb");

            if (fread((void *)input_data, 1, size, fp_src) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_src);
        }

        static void writeBin(const char * path, unsigned char* input_data, int size)
        {
            FILE *fp_dst = fopen(path, "wb");
            if (fwrite((void *)input_data, 1, size, fp_dst) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_dst);
        }

        int main()
        {
        int channel = 1;
        int width = 1920;
        int height = 1080;
        int dev_id = 0;
        int thickness = 4;
        float fontScale = 4;
        char text[20] = "hello world";
        bmcv_point_t org = {100, 100};
        bmcv_color_t color = {255, 0, 0};
        bm_handle_t handle;
        bm_image img;
        const char* input_path = "path/to/input";
        const char* output_path = "path/to/output";
        unsigned char* data_ptr = new unsigned char[channel * width * height];

        readBin(input_path, data_ptr, channel * width * height);
        bm_dev_request(&handle, dev_id);
        bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &img);
        bm_image_alloc_dev_mem(img);
        bm_image_copy_host_to_device(img, (void**)&data_ptr);
        bmcv_image_put_text(handle, img, text, org, color, fontScale, thickness);
        bm_image_copy_device_to_host(img, (void**)&data_ptr);
        writeBin(output_path, data_ptr, channel * width * height);

        bm_image_destroy(img);
        bm_dev_free(handle);
        free(data_ptr);
        return 0;
      }