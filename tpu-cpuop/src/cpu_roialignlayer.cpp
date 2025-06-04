#include <cmath>
#include "cpu_roialignlayer.h"
#include "bmcpu_macro.h"

namespace bmcpu {

int cpu_roialignlayer::process(void *raw_param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_roi_align_param_t, param, raw_param, param_size);
    int batch = input_shapes_[0][0];
    int unpooled_channels = input_shapes_[0][1];
    int height = input_shapes_[0][2];
    int width = input_shapes_[0][3];
    const float* input_ptr = input_tensors_[0];

    int roi_num = input_shapes_[1][0];
    int roi_len = input_shapes_[1][1];
    const float* roi_ptr = input_tensors_[1];

    int pooled_height = param->pooled_height;
    int pooled_width = param->pooled_width;
    float spatial_scale = param->spatial_scale;
    int sample_ratio = param->sampling_ratio;
    int position_sensitive = param->position_sensitive;
    int pooled_size = pooled_height * pooled_width;
    int channels = (position_sensitive)? unpooled_channels/pooled_size: unpooled_channels;

    (*output_shapes_)[0].resize(4);
    (*output_shapes_)[0][0] = roi_num;
    (*output_shapes_)[0][1] = channels;
    (*output_shapes_)[0][2] = pooled_height;
    (*output_shapes_)[0][3] = pooled_width;
    float* output_ptr = output_tensors_[0];

    CPU_ASSERT(roi_len == 5 || roi_len == 4);

    for(int n=0; n< roi_num; n++){
        int index_n = n * channels * pooled_size;

        const float* roi_data = roi_ptr + n * roi_len;
        int roi_batch_ind = 0;
        if(roi_len == 5){
            roi_batch_ind = roi_data[0];
            roi_data ++ ;
        }
        CPU_ASSERT_LT(roi_batch_ind, batch);
        float roi_start_w = roi_data[0] * spatial_scale;
        float roi_start_h = roi_data[1] * spatial_scale;
        float roi_end_w = roi_data[2] * spatial_scale;
        float roi_end_h = roi_data[3] * spatial_scale;

        float roi_height = std::max<float>(roi_end_h-roi_start_h, 1.0);
        float roi_width = std::max<float>(roi_end_w-roi_start_w, 1.0);

        float bin_height = roi_height/pooled_height;
        float bin_width = roi_width/pooled_width;

        int sample_h = (sample_ratio>0)? sample_ratio: std::ceil(bin_height);
        int sample_w = (sample_ratio>0)? sample_ratio: std::ceil(bin_width);

        vector<BilinearCoeff> coeffs;
        calc_bilinear_coeff(
                    height,
                    width,
                    pooled_height,
                    pooled_width,
                    roi_start_h,
                    roi_start_w,
                    bin_height,
                    bin_width,
                    sample_h,
                    sample_w,
                    coeffs
                    );
        int sample_size = sample_h * sample_w;
        for(int c = 0; c<channels; c++){
            int index_n_c = index_n + c * pooled_size;
            int coeff_idx = 0;
            for(int ph =0; ph < pooled_height; ph++){
                for(int pw = 0; pw < pooled_width; pw++){
                    int pooled_index = index_n_c + ph * pooled_width + pw;

                    int c_unpooled = c;
                    if(position_sensitive){
                        c_unpooled = c * pooled_size + ph * pooled_width + pw;
                    }
                    const float* feature_data = input_ptr +
                            (roi_batch_ind * unpooled_channels + c_unpooled) * height * width;
                    float& output_val = output_ptr[pooled_index];
                    output_val = 0;
                    for(int s=0; s<sample_size; s++){
                        BilinearCoeff& bc = coeffs[coeff_idx++];
                        // unify to bmnetc perfectly
                        float val = 0;
                        for(int bi = 0; bi<4; bi++){
                            val += bc.w[bi] * feature_data[bc.pos[bi]];
                        }
                        output_val += val;
                    }
                    output_val /= sample_size;
                }
            }
        }
    }
    return 0;
}

void cpu_roialignlayer::calc_bilinear_coeff(
        const int input_height,
        const int input_width,
        const int pooled_height,
        const int pooled_width,
        const float roi_start_h,
        const float roi_start_w,
        const float bin_height,
        const float bin_width,
        const int sample_h,
        const int sample_w,
        std::vector<BilinearCoeff>& coeffs
){
   int coeff_size = sample_h * sample_w * pooled_height * pooled_width;
   coeffs.resize(coeff_size);
   int coeff_idx = 0;
   for(int ph = 0; ph < pooled_height; ph++){
       for(int pw = 0; pw < pooled_width; pw++){
           for(int iy = 0; iy<sample_h; iy++){
               const float yy =  roi_start_h + ph * bin_height +
                                 (iy+.5f)*bin_height/sample_h;
               for(int ix = 0; ix<sample_w; ix++){
                   const float xx = roi_start_w + pw * bin_width +
                                      (ix+.5f)*bin_width/sample_w;
                   float x = xx;
                   float y = yy;
                   if(y<-1.0 || y > input_height || x<-1.0 || x>input_width){
                       BilinearCoeff& bc = coeffs[coeff_idx++];
                       memset(&bc, 0, sizeof(bc));
                       continue;
                   }
                   if(y < 0) y = 0;
                   if(x < 0) x = 0;
                   int x_low = (int)x;
                   int y_low = (int)y;
                   int x_high = x_low + 1;
                   int y_high = y_low + 1;
                   if(y_low>=input_height-1){
                       y_high = y_low = input_height-1;
                       y = y_low;
                   }
                   if(x_low>=input_width-1){
                       x_high = x_low = input_width-1;
                       x = x_low;
                   }
                   float ly = y - y_low;
                   float hy = 1 - ly;
                   float lx = x - x_low;
                   float hx = 1 - lx;
                   BilinearCoeff& bc = coeffs[coeff_idx++];
                   bc.w[0] = hy * hx;
                   bc.w[1] = hy * lx;
                   bc.w[2] = ly * hx;
                   bc.w[3] = ly * lx;
                   bc.pos[0] = y_low * input_width + x_low;
                   bc.pos[1] = y_low * input_width + x_high;
                   bc.pos[2] = y_high * input_width + x_low;
                   bc.pos[3] = y_high * input_width + x_high;
               }
           }
       }
   }
}

int cpu_roialignlayer::reshape(void* param, int param_size,
                               const vector<vector<int> >& input_shapes,
                               vector<vector<int>>& output_shapes) {
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_roi_align_param_t, l_param, param, param_size);
  int roi_num = input_shapes[1][0];

  int unpooled_channels = input_shapes[0][1];
  int pooled_height = l_param->pooled_height;
  int pooled_width = l_param->pooled_width;
  int position_sensitive = l_param->position_sensitive;
  int pooled_size = pooled_height * pooled_width;
  int channels = (position_sensitive)? unpooled_channels / pooled_size : 
                 unpooled_channels;
  output_shapes[0].clear();
  output_shapes[0].push_back(roi_num);
  output_shapes[0].push_back(channels);
  output_shapes[0].push_back(pooled_height);
  output_shapes[0].push_back(pooled_width);
  return 0;
}
int cpu_roialignlayer::dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
        vector<int> &output_dtypes) {
    output_dtypes.clear();
    output_dtypes.push_back(input_dtypes[0]);
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_ROIALIGN, cpu_roialign)

}
