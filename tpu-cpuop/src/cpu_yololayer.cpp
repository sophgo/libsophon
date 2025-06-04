#include "cpu_yololayer.h"
namespace bmcpu {

#if defined(__aarch64__)
#include <arm_neon.h>

#define c_exp_hi 88.3762626647949f
#define c_exp_lo -88.3762626647949f

#define c_cephes_LOG2EF 1.44269504088896341
#define c_cephes_exp_C1 0.693359375
#define c_cephes_exp_C2 -2.12194440e-4

#define c_cephes_exp_p0 1.9875691500E-4
#define c_cephes_exp_p1 1.3981999507E-3
#define c_cephes_exp_p2 8.3334519073E-3
#define c_cephes_exp_p3 4.1665795894E-2
#define c_cephes_exp_p4 1.6666665459E-1
#define c_cephes_exp_p5 5.0000001201E-1

void cpu_yololayer::activate_array(float *arr, const int n)
{
  int i,j=0;
  float *p0 = arr;
  float *p1 = arr;
  for (i = 0; i < (n/4); i++) {
    float32x4_t x = vld1q_f32 (p0);

    float32x4_t tmp, fx;

    float32x4_t one = vdupq_n_f32(1);
    x = vminq_f32(x, vdupq_n_f32(c_exp_hi));
    x = vmaxq_f32(x, vdupq_n_f32(c_exp_lo));

    /* express exp(x) as exp(g + n*log(2)) */
    fx = vmlaq_f32(vdupq_n_f32(0.5f), x, vdupq_n_f32(c_cephes_LOG2EF));

    /* perform a floorf */
    tmp = vcvtq_f32_s32(vcvtq_s32_f32(fx));

    /* if greater, substract 1 */
    uint32x4_t mask = vcgtq_f32(tmp, fx);
    mask = vandq_u32(mask, vreinterpretq_u32_f32(one));


    fx = vsubq_f32(tmp, vreinterpretq_f32_u32(mask));

    tmp = vmulq_f32(fx, vdupq_n_f32(c_cephes_exp_C1));
    float32x4_t z = vmulq_f32(fx, vdupq_n_f32(c_cephes_exp_C2));
    x = vsubq_f32(x, tmp);
    x = vsubq_f32(x, z);

    static const float cephes_exp_p[6] = { c_cephes_exp_p0, c_cephes_exp_p1, \
                                           c_cephes_exp_p2, c_cephes_exp_p3, \
                                           c_cephes_exp_p4, c_cephes_exp_p5 };
    float32x4_t y = vld1q_dup_f32(cephes_exp_p+0);
    float32x4_t c1 = vld1q_dup_f32(cephes_exp_p+1);
    float32x4_t c2 = vld1q_dup_f32(cephes_exp_p+2);
    float32x4_t c3 = vld1q_dup_f32(cephes_exp_p+3);
    float32x4_t c4 = vld1q_dup_f32(cephes_exp_p+4);
    float32x4_t c5 = vld1q_dup_f32(cephes_exp_p+5);

    y = vmulq_f32(y, x);
    z = vmulq_f32(x,x);
    y = vaddq_f32(y, c1);
    y = vmulq_f32(y, x);
    y = vaddq_f32(y, c2);
    y = vmulq_f32(y, x);
    y = vaddq_f32(y, c3);
    y = vmulq_f32(y, x);
    y = vaddq_f32(y, c4);
    y = vmulq_f32(y, x);
    y = vaddq_f32(y, c5);

    y = vmulq_f32(y, z);
    y = vaddq_f32(y, x);
    y = vaddq_f32(y, one);

    /* build 2^n */
    int32x4_t mm;
    mm = vcvtq_s32_f32(fx);
    mm = vaddq_s32(mm, vdupq_n_s32(0x7f));
    mm = vshlq_n_s32(mm, 23);
    float32x4_t pow2n = vreinterpretq_f32_s32(mm);

    y = vmulq_f32(y, pow2n);

    y = vaddq_f32(one , y);
    float32x4_t b = vrecpeq_f32(y);
    b = vmulq_f32(vrecpsq_f32(y, b), b);
    b = vmulq_f32(vrecpsq_f32(y, b), b);
    b = vmulq_f32(vrecpsq_f32(y, b), b);
    b = vsubq_f32(one, b);

    vst1q_f32(p1,b);
    p0 = p0 + 4;
    p1 = p1 + 4;
    j += 4;
  }
  for (i = j; i < n; i++) {
    *p1++ = 1.0f / (1.0f + exp(-(*p0++)));
  }
}
#else

void cpu_yololayer::activate_array(float *x, const int n)
{
  for(int i = 0; i < n; ++i) {
    x[i] = 1.0f / (1.0f + exp(-x[i]));
  }
}

#endif

int cpu_yololayer::process(void *param, int param_size)
{
  setParam(param, param_size);

  const float* bottom_data = input_tensors_[0];
  float* top_data = output_tensors_[0];
  int batch = input_shapes_[0][0];
  int channels = input_shapes_[0][1];
  int height = input_shapes_[0][2];
  int width = input_shapes_[0][3];

  memcpy(top_data, bottom_data, batch * channels * height * width * sizeof(float));

  //TODO: implement multi-thread optimization
  int outputs = channels * height * width;
  for (int b = 0; b < batch; ++b) {
    for(int n = 0; n < num_; ++n) {
      int index = 0;
      index = entry_index(width, height, outputs, classes_, b, n * width * height, 0);
      activate_array(top_data + index, 2 * width * height);
      index = entry_index(width, height, outputs, classes_, b, n * width * height, 4);
      activate_array(top_data + index, (1 + classes_) * width * height);
    }
  }

  return 0;
}

void cpu_yololayer::setParam(void *param, int param_size)
{
  layer_param_ = param;
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_yolo_param_t, yolo_param, layer_param_, param_size);
  classes_ = yolo_param->classes;
  num_ = yolo_param->num;
}

REGISTER_CPULAYER_CLASS(CPU_YOLO, cpu_yolo)

}
