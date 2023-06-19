bmcv_image_jpeg_dec
===================

The interface can decode multiple  JPEG  images.

**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_jpeg_dec(
                bm_handle_t handle,
                void *      p_jpeg_data[],
                size_t *    in_size,
                int         image_num,
                bm_image *  dst
        );


**Input parameter description:**

* bm_handle_t handle

  Input parameters. Handle of bm_handle.

* void \*  p_jpeg_data[]

  Input parameter. The image data pointer to be decoded. It is a pointer array because the interface supports the decoding of multiple images.

* size_t \*in_size

  Input parameter. The size of each image to be decoded (in bytes) is stored in the pointer, that is, the size of the space pointed to by each dimensional pointer of p_jpeg_data.

* int  image_num

  Input parameter. The number of input image, up to 4.

* bm_image\* dst

  Output parameter. The pointer of output bm_image. Users can choose whether or not to call bm_image_create to create dst bm_image. If users only declare but do not create, it will be automatically created by the interface according to the image information to be decoded. The default format is shown in the following table. When it is no longer needed, users still needs to call bm_image_destroy to destroy it.

  +------------+---------------------+
  | Code Stream|Default Output Format|
  +============+=====================+
  |  YUV420    |  FORMAT_YUV420P     |
  +------------+---------------------+
  |  YUV422    |  FORMAT_YUV422P     |
  +------------+---------------------+
  |  YUV444    |  FORMAT_YUV444P     |
  +------------+---------------------+
  |  YUV400    |  FORMAT_GRAY        |
  +------------+---------------------+



**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. If users do not use bmcv_image_create to create bm_image of dst, they need to set the space pointed by the parameter input pointer to 0.


2. At present, the image formats supported by decoding and their output formats are shown in the following table. If users need to specify one of the following output formats, they can use bmcv_image_create to create their own dst bm_image, so as to decode the picture to one of the following corresponding formats.

   +------------------+------------------------+
   | Code Stream      | Default Output Format  |
   +==================+========================+
   |                  |  FORMAT_YUV420P        |
   +  YUV420          +------------------------+
   |                  |  FORMAT_NV12           |
   +                  +------------------------+
   |                  |  FORMAT_NV21           |
   +------------------+------------------------+
   |                  |  FORMAT_YUV422P        |
   +  YUV422          +------------------------+
   |                  |  FORMAT_NV16           |
   +                  +------------------------+
   |                  |  FORMAT_NV61           |
   +------------------+------------------------+
   |  YUV444          |  FORMAT_YUV444P        |
   +------------------+------------------------+
   |  YUV400          |  FORMAT_GRAY           |
   +------------------+------------------------+


**Sample code**


    .. code-block:: c

        size_t size = 0;
        // read input from picture
        FILE *fp = fopen(filename, "rb+");
        assert(fp != NULL);
        fseek(fp, 0, SEEK_END);
        *size = ftell(fp);
        u8* jpeg_data = (u8*)malloc(*size);
        fseek(fp, 0, SEEK_SET);
        fread(jpeg_data, *size, 1, fp);
        fclose(fp);

        // create bm_image used to save output
        bm_image dst;
        memset((char*)&dst, 0, sizeof(bm_image));
        // if you not create dst bm_image it will create automatically inside.
        // you can also create dst bm_image here, like this:
        // bm_image_create(handle, IMAGE_H, IMAGE_W, FORMAT_YUV420P,
        //         DATA_TYPE_EXT_1N_BYTE, &dst);

        // decode input
        int ret = bmcv_image_jpeg_dec(handle, (void**)&jpeg_data, &size, 1, &dst);
        free(jpeg_data);
        bm_image_destory(dst);




