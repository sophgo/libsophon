bmcv_nms_yolo
==============

该接口目前支持yolov3/yolov7，用于消除网络计算得到过多的物体框，并找到最佳物体框。


**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_nms_yolo(
                        bm_handle_t handle,
                        int 	input_num,
                        bm_device_mem_t bottom[3],
                        int 	batch_num,
                        int 	hw_shape[3][2],
                        int 	num_classes,
                        int 	num_boxes,
                        int 	mask_group_size,
                        float nms_threshold,
                        float confidence_threshold,
                        int 	keep_top_k,
                        float bias[18],
                        float anchor_scale[3],
                        float mask[9],
                        bm_device_mem_t output,
                        int 	yolo_flag,
                        int 	len_per_batch,
                        void *ext)

**参数说明:**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* int input_num

  输入参数。输入feature map数量。

* bm_device_mem_t bottom[3]

  输入参数。bottom的设备地址，需要调用 bm_mem_from_system()将数据地址转化成转化为 bm_device_mem_t 所对应的结构。

* int batch_num

  输入参数。batch 的数量。

* int hw_shape[3][2]

  输入参数。输入feature map的h、w。

* int num_classes

  输入参数。图片的类别数量。

* int num_boxes

  输入参数。每个网格包含多少个不同尺度的anchor box。

* int mask_group_size

  输入参数。掩膜的尺寸。

* float nms_threshold

  输入参数。过滤物体框的阈值，分数小于该阈值的物体框将会被过滤掉。

* int confidence_threshold

  输入参数。置信度。

* int keep_top_k

  输入参数。保存前 k 个数。

* int bias[18]

  输入参数。偏置。

* float anchor_scale[3]

  输入参数。anchor的尺寸。

* float mask[9]

  输入参数。掩膜。

* bm_device_mem_t output

  输入参数。输出的设备地址，需要调用 bm_mem_from_system()将数据地址转化成转化为 bm_device_mem_t 所对应的结构。

* int yolo_flag

  输入参数。yolov3时yolo_flag=0，yolov7时yolo_flag=2。

* int len_per_batch

  输入参数。该参数无效，仅为了维持接口的兼容性。

* int scale

  输入参数。目标尺寸。该参数仅在yolov7中生效。

* int \*orig_image_shape

  输入参数。原始图片的w/h, 按batch排布，比如batch4: w1 h1 w2 h2 w3 h3 w4 h4。该参数仅在yolov7中生效。

* int model_h

  输入参数。模型的shape h，该参数仅在yolov7中生效。

* int model_w

  输入参数。模型的shape w，该参数仅在yolov7中生效。

* void \*ext

  预留参数。如果需要新增参数，可以在这里新增。yolov7 中新增了4个参数为：

    .. code-block:: c

        typedef struct yolov7_info{
            int scale;
            int *orig_image_shape;
            int model_h;
            int model_w;
        } yolov7_info_t;

上面结构体中，int scale：scale_flag。int* orig_image_shape：原始图片的w/h, 按batch排布，比如batch4: w1 h1 w2 h2 w3 h3 w4 h4。int model_h：模型的shape h。int model_w：模型的shape w。这些参数仅在yolov7中生效。

**返回值:**

* BM_SUCCESS: 成功

* 其他: 失败

**代码示例:**


    .. code-block:: c

        #include <time.h>
        #include <random>
        #include <algorithm>
        #include <map>
        #include <vector>
        #include <iostream>
        #include <cmath>
        #include <getopt.h>
        #include "bmcv_api_ext.h"
        #include "bmcv_common_bm1684.h"
        #include "math.h"
        #include "stdio.h"
        #include "stdlib.h"
        #include "string.h"
        #include <iostream>
        #include <new>
        #include <fstream>

        #define KEEP_TOP_K    200
        #define Dtype float
        #define TIME_PROFILE

        typedef struct yolov7_info{
            int scale;
            int *orig_image_shape;
            int model_h;
            int model_w;
        } yolov7_info_t;

         int main(int argc, char *argv[]) {
            int DEV_ID = 0;
            int H = 16, W = 30;
            int bottom_num = 3;
            int dev_count;
            int f_data_from_file = 0;
            int f_tpu_forward = 1;

            bm_status_t ret = BM_SUCCESS;
            int batch_num = 32;
            int num_classes = 6;
            int num_boxes = 3;
            int yolo_flag = 0; //yolov3: 0, yolov7: 2
            int len_per_batch = 0;
            int keep_top_k = 100;
            float nms_threshold = 0.1;
            float conf_threshold = 0.98f;
            int mask_group_size = 3;
            float bias[18] = {10, 13, 16, 30, 33, 23, 30, 61, 62, 45, 59, 119, 116, 90, 156, 198, 373, 326};
            float anchor_scale[3] = {32, 16, 8};
            float mask[9] = {6, 7, 8, 3, 4, 5, 0, 1, 2};
            int scale = 0; //for yolov7 post handle
            int model_h = 0;
            int model_w = 0;
            int mode_value_end = 0;
            bm_dev_request(&handle, 0);
            int hw_shape[3][2] = {
                {H*1, W*1},
                {H*2, W*2},
                {H*4, W*4},
            };

            int size_bottom[3];
            float* data_bottom[3];
            int origin_image_shape[batch_num * 2] = {0};
            if (yolo_flag == 1){
                num_boxes = 1;
                len_per_batch = 12096 * 18;
                bottom_num = 1;
            } else if (yolo_flag == 2){
                //yolov7 post handle;
                num_boxes = 1;
                bottom_num = 3;
                mask_group_size = 1;
                scale = 1;
                model_h = 512;
                model_w = 960;
                for (int i = 0 ; i < 3; i++){
                mask[i] = i;
                }

                for (int i = 0; i < 6; i++)
                bias[i] = 1;

                for (int i = 0; i < 3; i++)
                anchor_scale[i] = 1;

                for (int i = 0; i < batch_num; i++){
                origin_image_shape[i*2 + 0] = 1920;
                origin_image_shape[i*2 + 1] = 1080;
                }
            }

            // alloc input data
            for (int i = 0; i < 3; ++i) {
                if (yolo_flag == 1){
                size_bottom[i] = batch_num * len_per_batch;
                } else {
                size_bottom[i] = batch_num * num_boxes *
                                (num_classes + 5) * hw_shape[i][0] * hw_shape[i][1];
                }
                try {
                data_bottom[i] = new float[size_bottom[i]];
                }
                catch(std::bad_alloc &memExp)
                {
                std::cerr<<memExp.what()<<std::endl;
                exit(-1);
                }
            }

            if (f_data_from_file) {
                #if defined(__aarch64__)
                #define DIR     "./imgs/"
                #else
                #define DIR     "test/test_api_bmdnn/bm1684/imgs/"
                #endif
                printf("reading data from: \"" DIR "\"\n");
                char path[256];
                if (yolo_flag == 1) {
                FILE* fp = fopen("./output_ref_data.dat.bmrt", "rb");
                size_t cnt = fread(data_bottom[0],
                        sizeof(float), size_bottom[0]*batch_num, fp);
                cnt = cnt;
                fclose(fp);
                } else {
                for (int i = 0; i < batch_num; ++i) {
                    sprintf(path, DIR "b%d_13.bin", i);
                    FILE* fp = fopen(path, "rb");
                    size_t cnt = fread(data_bottom[0] + i * size_bottom[0] / batch_num,
                        sizeof(float), size_bottom[0] / batch_num, fp);
                    cnt = cnt;
                    fclose(fp);

                    sprintf(path, DIR "b%d_26.bin", i);
                    fp = fopen(path, "rb");
                    cnt = fread(data_bottom[1] + i * size_bottom[1] / batch_num,
                        sizeof(float), size_bottom[1] / batch_num, fp);
                    cnt = cnt;
                    fclose(fp);

                    sprintf(path, DIR "b%d_52.bin", i);
                    fp = fopen(path, "rb");
                    cnt = fread(data_bottom[2] + i * size_bottom[2] / batch_num,
                        sizeof(float), size_bottom[2] / batch_num, fp);
                    cnt = cnt;
                    fclose(fp);
                }
                }
            } else {
                ofstream file_1("1.txt", std::ios::out);
                ofstream file_2("2.txt", std::ios::out);
                ofstream file_3("3.txt", std::ios::out);

                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<> dist(0, 1);

                // alloc and init input data
                for (int j = 0; j < size_bottom[0]; ++j){
                if (yolo_flag == 2){
                    data_bottom[0][j] = dist(gen);
                } else {
                    data_bottom[0][j] = (rand() % 1000 - 999.0f) / (124.0f);
                }
                file_1 << data_bottom[0][j] <<endl;
                }

                for (int j = 0; j < size_bottom[1]; ++j){
                if (yolo_flag == 2){
                    data_bottom[1][j] = dist(gen);
                } else {
                    data_bottom[1][j] = (rand() % 1000 - 999.0f) / (124.0f);
                }
                file_2 << data_bottom[1][j] <<endl;
                }

                for (int j = 0; j < size_bottom[2]; ++j){
                if (yolo_flag == 2){
                    data_bottom[2][j] = dist(gen);
                } else {
                    data_bottom[2][j] = (rand() % 1000 - 999.0f) / (124.0f);
                }
                file_3 << data_bottom[2][j] <<endl;
                }
            }

            // alloc output data
            float* output_bmdnn;
            float* output_native;
            try {
                output_bmdnn = new float[output_size];
                output_native = new float[output_size];
            }
            catch(std::bad_alloc &memExp)
            {
                std::cerr<<memExp.what()<<std::endl;
                exit(-1);
            }
            memset(output_bmdnn, 0, output_size * sizeof(float));
            memset(output_native, 0, output_size * sizeof(float));

            bm_dev_request(&handle, 0);
            bm_device_mem_t bottom[3] = {
                bm_mem_from_system((void*)data_bottom[0]),
                bm_mem_from_system((void*)data_bottom[1]),
                bm_mem_from_system((void*)data_bottom[2])
            };
            yolov7_info_t *ext = (yolov7_info_t*) malloc (sizeof(yolov7_info_t));
            ext->scale = scale;
            ext->orig_image_shape = origin_image_shape;
            ext->model_h = model_h;
            ext->model_w = model_w;

            ret = bmcv_nms_yolo(
            handle, bottom_num, bottom,
            batch_num, hw_shape, num_classes, num_boxes,
            mask_group_size, nms_threshold, conf_threshold,
            keep_top_k, bias, anchor_scale, mask,
            bm_mem_from_system((void*)output_bmdnn), yolo_flag, len_per_batch, (void*)ext);

            return 0;
         }
