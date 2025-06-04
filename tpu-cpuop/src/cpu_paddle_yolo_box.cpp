#include "cpu_paddle_yolo_box.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <queue>

namespace bmcpu {
    
    template <typename T>
        inline T sigmoid(T x) {
            return 1.0 / (1.0 + std::exp(-x));
        }

    template <typename T>
        inline void GetYoloBox(T* box,  const T* x,  const int* anchors,  int i, 
                int j,  int an_idx,  int grid_size_h, 
                int grid_size_w,  int input_size_h, 
                int input_size_w,  int index,  int stride, 
                int img_height,  int img_width,  float scale, 
                float bias) {
            box[0] = (i + sigmoid<T>(x[index]) * scale + bias) * img_width / grid_size_w;
            box[1] = (j + sigmoid<T>(x[index + stride]) * scale + bias) * img_height /
                grid_size_h;
            box[2] = std::exp(x[index + 2 * stride]) * anchors[2 * an_idx] * img_width /
                input_size_w;
            box[3] = std::exp(x[index + 3 * stride]) * anchors[2 * an_idx + 1] *
                img_height / input_size_h;
        }
    
    inline int GetEntryIndex(int batch,  int an_idx,  int hw_idx, 
            int an_num,  int an_stride,  int stride, 
            int entry) {
        return (batch * an_num + an_idx) * an_stride + entry * stride + hw_idx;
    }
    
    template <typename T>
        inline void CalcDetectionBox(T* boxes,  T* box,  const int box_idx, 
                const int img_height, 
                const int img_width,  bool clip_bbox) {
            boxes[box_idx] = box[0] - box[2] / 2;
            boxes[box_idx + 1] = box[1] - box[3] / 2;
            boxes[box_idx + 2] = box[0] + box[2] / 2;
            boxes[box_idx + 3] = box[1] + box[3] / 2;
            
            if (clip_bbox) {
                boxes[box_idx] = boxes[box_idx] > 0 ? boxes[box_idx] : static_cast<T>(0);
                boxes[box_idx + 1] =
                    boxes[box_idx + 1] > 0 ? boxes[box_idx + 1] : static_cast<T>(0);
                boxes[box_idx + 2] = boxes[box_idx + 2] < img_width - 1 
                    ? boxes[box_idx + 2]
                    : static_cast<T>(img_width - 1);
                boxes[box_idx + 3] = boxes[box_idx + 3] < img_height - 1
                    ? boxes[box_idx + 3]
                    : static_cast<T>(img_height - 1);
            }
        }
    
    template <typename T>
        inline void CalcLabelScore(T* scores,  const T* input, 
                const int label_idx,  const int score_idx, 
                const int class_num,  const T conf, 
                const int stride) {
            for (int i = 0; i < class_num; i++) {
                scores[score_idx + i] = conf * sigmoid<T>(input[label_idx + i * stride]);
            }
        }

    int cpu_paddle_yolo_boxlayer::process(void *param, int psize) {
#ifdef CALC_TIME  
        struct timeval start,  stop;
        gettimeofday(&start,  0);
#endif
        setParam(param, psize);
        const int n = input_shapes_[0][0];
        const int h = input_shapes_[0][2];
        const int w = input_shapes_[0][3];
        int b_num = output_shapes_[0][0][1];
        const int an_num = (anchors_size_ >> 1);
        auto anchors_data = anchors_;
        
        int input_size_h = downsample_ratio_ * h;
        int input_size_w = downsample_ratio_ * w;
        const int stride = h * w;
        const int an_stride = (class_num_ + 5) * stride;
        
        const float* input_data = input_tensors_[0];
        const int* imgsize_data = reinterpret_cast<const int *>(input_tensors_[1]);




        float* boxes_data = output_tensors_[0];
        float* scores_data = output_tensors_[1];
        float box[4];
        memset(boxes_data,  0,  output_shapes_[0][0][0] * output_shapes_[0][0][1] * output_shapes_[0][0][2] * sizeof(float));
        memset(scores_data,  0,  output_shapes_[0][1][0] * output_shapes_[0][1][1] * output_shapes_[0][1][2] * sizeof(float));

        bool clip_bbox = clip_bbox_;
        float scale = scale_;
        float bias = -0.5 * (scale - 1.);
        for (int i = 0; i < n; i++) {
            int img_height = imgsize_data[2 * i]; 
            int img_width = imgsize_data[2 * i + 1];
            for (int j = 0; j < an_num; j++) {
                for (int k = 0; k < h; k++) {
                    for (int l = 0; l < w; l++) {
                        int obj_idx =
                            GetEntryIndex(i,  j,  k * w + l,  an_num,  an_stride,  stride,  4); 
                        float conf = sigmoid<float>(input_data[obj_idx]);
                        if (conf < conf_thresh_) {
                            continue;
                        }
                        int box_idx =
                            GetEntryIndex(i,  j,  k * w + l,  an_num,  an_stride,  stride,  0); 
                        GetYoloBox<float>(box,  input_data,  anchors_data,  l,  k,  j,  h,  w, 
                                input_size_h,  input_size_w,  box_idx,  stride, 
                                img_height,  img_width,  scale,  bias);
                        box_idx = (i * b_num + j * stride + k * w + l) * 4;
                        CalcDetectionBox<float>(boxes_data,  box,  box_idx,  img_height,  img_width, 
                                clip_bbox);
                        int label_idx =
                            GetEntryIndex(i,  j,  k * w + l,  an_num,  an_stride,  stride,  5); 
                        int score_idx = (i * b_num + j * stride + k * w + l) * class_num_;
                        CalcLabelScore<float>(scores_data,  input_data,  label_idx,  score_idx, 
                                class_num_,  conf,  stride);
                    }
                }
            }   
        }
 #ifdef CALC_TIME
        gettimeofday(&stop,  0);
        float timeuse = 1000000 * (stop.tv_sec - start.tv_sec) + stop.tv_usec - start.tv_usec;
        timeuse /= 1000;
        printf("**************cost time is %f ms\n",  timeuse);
#endif
        return 0;
    }

    void cpu_paddle_yolo_boxlayer::setParam(void *param, int psize) {
        layer_param_ = param;
        BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_paddle_yolo_box_param_t, yolo_box_param, layer_param_, psize);
        anchors_size_ = yolo_box_param->anchors_size;
        CPU_ASSERT(anchors_size_ < 100);
        memset(anchors_,  0,  sizeof(int) * 100);
        memcpy(anchors_,  yolo_box_param->anchors,  anchors_size_ * sizeof(int));
        conf_thresh_ = yolo_box_param->conf_thresh;
        class_num_ = yolo_box_param->class_num;
        downsample_ratio_ = yolo_box_param->downsample_ratio;
        iou_aware_ = yolo_box_param->iou_aware;
        clip_bbox_ = yolo_box_param->clip_bbox;
        iou_aware_factor_ = yolo_box_param->iou_aware_factor;
        scale_ = yolo_box_param->scale;
    }

    int cpu_paddle_yolo_boxlayer::reshape(
            void* param, int psize, 
            const vector<vector<int>>& input_shapes, 
            vector<vector<int>>& output_shapes) {
        BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_paddle_yolo_box_param_t, p, param, psize);
        
        vector<int> data_shape = input_shapes[0];
        const int out_dim1 = (data_shape[2]*data_shape[3]*p->anchors_size >> 1);

        output_shapes[0].resize(input_shapes[0].size()-1);
        output_shapes[1].resize(input_shapes[0].size()-1);

        output_shapes[0][0] = input_shapes[0][0];
        output_shapes[1][0] = input_shapes[0][0];
        output_shapes[0][1] = out_dim1;
        output_shapes[1][1] = out_dim1;
        output_shapes[0][2] = 4;
        output_shapes[1][2] = p->class_num;

        return 0;
    }

    int cpu_paddle_yolo_boxlayer::dtype(
            const void *param, size_t psize, 
            const vector<int>& input_dtypes, 
            vector<int>& output_dtypes) {
        CPU_ASSERT(input_dtypes.size() > 0);
        output_dtypes = {input_dtypes[0]};
        return 0;
    }


REGISTER_CPULAYER_CLASS(CPU_PADDLE_YOLO_BOX, cpu_paddle_yolo_box);
}/* namespace bmcpu*/
