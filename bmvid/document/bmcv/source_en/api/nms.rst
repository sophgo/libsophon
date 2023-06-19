bmcv_nms
========

The interface is used to eliminate excessive object frames obtained by network calculation and find the best object frame.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_nms(bm_handle_t handle,
                             bm_device_mem_t input_proposal_addr,
                             int proposal_size,
                             float nms_threshold,
                             bm_device_mem_t output_proposal_addr)

**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t input_proposal_addr

  Input parameter. Input the address where the object box data is located, and input the data structure as face_rect_t. Please refer to the following data structure description for details. Users need to call bm_mem_from_system() to convert the data address to structure corresponding to bm_device_mem_t.

* int proposal_size

  Input parameter. The number of object frames.

* float nms_threshold

  Input parameter. The threshold value of the filtered object frame. The object frame whose score is less than the threshold value will be filtered out.

* bm_device_mem_t output_proposal_addr

  Output parameter. The address where the output object frame data is located, and the output object frame data structure is nms_proposal_t. Please refer to the following data structure description for details. Users need to call bm_mem_from_system() to convert the data address to structure corresponding to bm_device_mem_t.


**Return value:**

* BM_SUCCESS: success

* Other: failed


**Data type description:**

face_rect_t describes the coordinate position of an object frame and the corresponding fraction.

    .. code-block:: c

        typedef struct
        {
            float x1;
            float y1;
            float x2;
            float y2;
            float score;
        }face_rect_t;

    * x1 describes the abscissa of the left edge of the object frame

    * y1 describes the ordinate of the upper edge of the object frame

    * x2 describes the abscissa of the right edge of the object frame

    * y2 describes the ordinate of the lower edge of the object frame

    * score describes the score corresponding to the object frame


nms_proposal_t describes the information of the output object box.

    .. code-block:: c

        typedef struct
        {
            face_rect_t face_rect[MAX_PROPOSAL_NUM];
            int size;
            int capacity;
            face_rect_t *begin;
            face_rect_t *end;
        } nms_proposal_t;

      * face_rect describes the filtered object frame information

      * size describes the number of object frames obtained after filtering

      * capacity describes the maximum number of object frames after filtering

      * begin is not used temporarily

      * end is not used temporarily


**Code example:**

    .. code-block:: c

        face_rect_t *proposal_rand = new face_rect_t[MAX_PROPOSAL_NUM];
        nms_proposal_t *output_proposal = new nms_proposal_t;
        int proposal_size =32;
        float nms_threshold = 0.2;
        for (int i = 0; i < proposal_size; i++)
        {
            proposal_rand[i].x1 = 200;
            proposal_rand[i].x2 = 210 ;
            proposal_rand[i].y1 = 200;
            proposal_rand[i].y2 = 210;
            proposal_rand[i].score = 0.23;
        }
        bmcv_nms(handle,
                 bm_mem_from_system(proposal_rand),
                 proposal_size,
                 nms_threshold,
                 bm_mem_from_system(output_proposal));
        delete[] proposal_rand;
        delete output_proposal;


**Note:**

The maximum number of proposal that can be entered by this API is 56000.

