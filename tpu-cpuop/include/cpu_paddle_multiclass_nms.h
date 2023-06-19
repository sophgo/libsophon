#ifndef _CPU_PADDLE_MULTICLASS_NMS_LAYER_H
#define _CPU_PADDLE_MULTICLASS_NMS_LAYER_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_paddle_multiclass_nmslayer : public cpu_layer {
public:
    cpu_paddle_multiclass_nmslayer() {}
    ~cpu_paddle_multiclass_nmslayer() {}
    int process(void *param, int psize);
    int reshape(void* param, int psize,
                const std::vector<std::vector<int>> &input_shapes,
                std::vector<std::vector<int>> &output_shapes);
    int dtype(const void *param, size_t psize,
              const std::vector<int> &input_dtypes,
              std::vector<int> &output_dtypes);

    void setParam(void *param, int psize);

protected:
    float score_threshold_;                           
    float nms_threshold_;
    float nms_eta_;
    int keep_top_k_;
    int nms_top_k_;
    int background_label_;                            
    bool normalized_;                                 
    std::vector<int> boxes_dims_;                     
    std::vector<int> score_dims_;
    bool return_index_;
    bool has_output_nms_num_;
    bool has_batch_;

    void GetMaxScoreIndex(const float* scores, 
            std::vector<std::pair<float,  int>>* sorted_indices);   
    float BBoxArea(const float* box);
    float JaccardOverlap(const float* box1,  const float* box2);
    void NMSFast(float* box,  float* score, 
            std::vector<int>* selected_indices);                   
    void MultiClassOutput(float* box, 
            float* score, 
            float* output, 
            int* output_index, 
            const std::map<int,  std::vector<int>>& selected_indices, 
            const int scores_size, 
            const int offset, 
            int& count);
    void MultiClassNMS(float* box,  float* score, 
            std::map<int,  std::vector<int>>* indices,               
            int* num_nmsed_out);
};
} // namespace bmcpu
#endif  /* _CPU_TENSORFLOW_NMS_V5_LAYER_H */
