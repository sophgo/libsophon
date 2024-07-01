bmcv_image_quantify
====================

Convert float type data into int type (the rounding mode is truncation directly after the decimal point), and change the number less than 0 to 0, and the number greater than 255 to 255.


**Processor model support**

This interface only support BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_quantify(
                bm_handle_t handle,
                bm_image input,
                bm_image output);


**Description of parameters:**

* bm_handle_t handle

  Input parameters. bm_handle handle.

* bm_image input

  Input parameter. The bm_image of the input image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. The bm_image of the output image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

The interface currently supports the following image_format and data_type:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_RGB_PLANAR      | FORMAT_RGB_PLANAR      |
+-----+------------------------+------------------------+
| 2   | FORMAT_BGR_PLANAR      | FORMAT_BGR_PLANAR      |
+-----+------------------------+------------------------+


Input data currently supports the following data_types:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+

Output data currently supports the following data_types:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note:**

1. Before calling this interface, you must ensure that the input image memory has been allocated.

2. If the program calling this interface is a multi-threaded program, thread locks need to be added before creating bm_image and after destroying bm_image.

3. This interface supports image width and height ranging from 1x1 to 8192x8192.

**Code example:**

    .. code-block:: c

        //pthread_mutex_t lock;
        static void read_bin(const char *input_path, float *input_data, int width, int height) {
            FILE *fp_src = fopen(input_path, "rb");
            if (fp_src == NULL)
            {
                printf("Unable to open output file %s\n", input_path);
                return;
            }
            if(fread(input_data, sizeof(float), width * height, fp_src) != 0)
                printf("read image success\n");
            fclose(fp_src);
        }

        static int quantify_tpu(float* input, unsigned char* output, int height, int width, bm_handle_t handle) {
            bm_image input_img;
            bm_image output_img;
            //pthread_mutex_lock(&lock);
            bm_image_create(handle, height, width, (bm_image_format_ext)FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32, &input_img, NULL);
            bm_image_create(handle, height, width, (bm_image_format_ext)FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &output_img, NULL);
            bm_image_alloc_dev_mem(input_img, 1);
            bm_image_alloc_dev_mem(output_img, 1);
            float* in_ptr[1] = {input};
            bm_image_copy_host_to_device(input_img, (void **)in_ptr);
            bmcv_image_quantify(handle, input_img, output_img);
            unsigned char* out_ptr[1] = {output};
            bm_image_copy_device_to_host(output_img, (void **)out_ptr);
            bm_image_destroy(input_img);
            bm_image_destroy(output_img);
            //pthread_mutex_unlock(&lock);
            return 0;
        }

        int main(int argc, char* args[]) {
            int width     = 1920;
            int height    = 1080;
            int dev_id    = 0;
            char *input_path = NULL;
            char *output_path = NULL;

            bm_handle_t handle;
            bm_status_t ret = bm_dev_request(&handle, 0);
            if (ret != BM_SUCCESS) {
                printf("Create bm handle failed. ret = %d\n", ret);
                return -1;
            }

            if (argc > 1) width = atoi(args[1]);
            if (argc > 2) height = atoi(args[2]);
            if (argc > 3) input_path = args[3];
            if (argc > 4) output_path = args[4];

            float* input_data = (float*)malloc(width * height * 3 * sizeof(float));
            unsigned char* output_tpu = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));

            read_bin(input_path, input_data, width, height);

            int ret = quantify_tpu(input_data, output_tpu, height, width, handle);

            free(input_data);
            free(output_tpu);
            bm_dev_free(handle);
            return ret;

