#include "bmcv_api_ext.h"
#include "bmlib_runtime.h"
#include <iostream>
#include <cmath>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#endif
#include "test_misc.h"

using namespace std;

typedef enum {
    DT_INT8   = (0 << 1) | 1,
    DT_UINT8  = (0 << 1) | 0,
    DT_INT16  = (3 << 1) | 1,
    DT_UINT16 = (3 << 1) | 0,
    DT_FP16   = (1 << 1) | 1,
    DT_BFP16  = (5 << 1) | 1,
    DT_INT32  = (4 << 1) | 1,
    DT_UINT32 = (4 << 1) | 0,
    DT_FP32   = (2 << 1) | 1
} data_type_t;

typedef data_type_t bm_data_type_t;


typedef struct
{
        unsigned short  manti : 10;
        unsigned short  exp : 5;
        unsigned short  sign : 1;
} fp16;

union fp16_data
{
        unsigned short idata;
        fp16 ndata;
};

typedef struct
{
        unsigned int manti : 23;
        unsigned int exp : 8;
        unsigned int sign : 1;
} fp32;

union fp32_data
{
        unsigned int idata;
        int idataSign;
        float fdata;
        fp32 ndata;
};

union {
    float f;
    uint32_t i;
} conv;

uint16_t fp32idata_to_fp16idata(uint32_t f,int round_method) {
  uint32_t f_exp;
  uint32_t f_sig;
  uint32_t h_sig=0x0u;
  uint16_t h_sgn, h_exp;

  h_sgn = (uint16_t)((f & 0x80000000u) >> 16);
  f_exp = (f & 0x7f800000u);

  /* Exponent overflow/NaN converts to signed inf/NaN */
  if (f_exp >= 0x47800000u) {
    if (f_exp == 0x7f800000u) {
      /* Inf or NaN */
      f_sig = (f & 0x007fffffu);
      if (f_sig != 0) {
        /* NaN */
        return 0x7fffu;
      } else {
        /* signed inf */
        return (uint16_t)(h_sgn + 0x7c00u);
      }
    } else {
      /* overflow to signed inf */
      return (uint16_t)(h_sgn + 0x7c00u);
    }
  }

/* Exponent underflow converts to a subnormal half or signed zero */
#ifdef CUDA_T
  if (f_exp <= 0x38000000u) ////exp= -15, fp16is denormal ,the smallest value of fp16 normal = 1x2**(-14)
#else
  if (f_exp < 0x38000000u)
#endif
  {
    /*
     * Signed zeros, subnormal floats, and floats with small
     * exponents all convert to signed zero half-floats.
     */
    if (f_exp < 0x33000000u) {
      return h_sgn;
    }
    /* Make the subnormal significand */
    f_exp >>= 23;
    f_sig = (0x00800000u + (f & 0x007fffffu));

/* Handle rounding by adding 1 to the bit beyond half precision */
    switch (round_method) {
      case 0:
// ROUND_TO_NEAREST_EVEN
    /*
     * If the last bit in the half significand is 0 (already even), and
     * the remaining bit pattern is 1000...0, then we do not add one
     * to the bit after the half significand.  In all other cases, we do.
     */
    if ((f_sig & ((0x01u << (127 - f_exp)) - 1)) != (0x01u << (125 - f_exp))) {
      f_sig += 1 << (125 - f_exp);
    }
    f_sig >>= (126 - f_exp);
    h_sig = (uint16_t)(f_sig);
    break;
  case 1:
    //JUST ROUND
    f_sig += 1 << (125 - f_exp);
    f_sig >>= (126 - f_exp);
    h_sig = (uint16_t)(f_sig);
    break;
   case 2:
// ROUND_TO_ZERO
    f_sig >>= (126 - f_exp);
    h_sig = (uint16_t)(f_sig);
    break;
   case 3:
    //JUST_FLOOR
      if ((f_sig & ((0x01u << (126 - f_exp)) - 1)) != 0x0u ) {
        if(h_sgn)
          f_sig  += 1 << (126 - f_exp);
      }
      f_sig >>= (126 - f_exp);
      h_sig = (uint16_t)(f_sig);
    break;
   case 4:
    //just_ceil
      if ((f_sig & ((0x01u << (126 - f_exp)) - 1)) != 0x0u ) {
        if( !h_sgn)
          f_sig  += 1 << (126 - f_exp);
      }
      f_sig >>= (126 - f_exp);
      h_sig = (uint16_t)(f_sig);
    break;
    //
   default:
// ROUND_TO_ZERO
      f_sig >>= (126 - f_exp);
      h_sig = (uint16_t)(f_sig);
      break;
    }
    /*
     * If the rounding causes a bit to spill into h_exp, it will
     * increment h_exp from zero to one and h_sig will be zero.
     * This is the correct result.
     */
    return (uint16_t)(h_sgn + h_sig);
  }

  /* Regular case with no overflow or underflow */
  h_exp = (uint16_t)((f_exp - 0x38000000u) >> 13);
  /* Handle rounding by adding 1 to the bit beyond half precision */
  f_sig = (f & 0x007fffffu);

    switch (round_method) {
      case 0:
          // ROUND_TO_NEAREST_EVEN
            /*
             * If the last bit in the half significand is 0 (already even), and
             * the remaining bit pattern is 1000...0, then we do not add one
             * to the bit after the half significand.  In all other cases, we do.
             */
            if ((f_sig & 0x00003fffu) != 0x00001000u) {
              f_sig = f_sig + 0x00001000u;
            }
              h_sig = (uint16_t)(f_sig >> 13);
              break;
      case 1:
          // JUST_ROUND
              f_sig = f_sig + 0x00001000u;
              h_sig = (uint16_t)(f_sig >> 13);
              break;
      case 2:
          // ROUND_TO_ZERO
            h_sig = (uint16_t)(f_sig >> 13);
            break;
      case 3:
          // JUST_FLOOR
            if ((f_sig & 0x00001fffu) != 0x00000000u) {
              if ( h_sgn ) {
                f_sig = f_sig + 0x00002000u;
              }
            }
            h_sig = (uint16_t)(f_sig >> 13);
            break;
      case 4:
          // JUST_CEIL
            if ((f_sig & 0x00001fffu) != 0x00000000u) {
              if ( !h_sgn )
                f_sig = f_sig + 0x00002000u;
            }
            h_sig = (uint16_t)(f_sig >> 13);
            break;
      default:
          // ROUND_TO_ZERO
            h_sig = (uint16_t)(f_sig >> 13);
            break;
    }
  /*
   * If the rounding causes a bit to spill into h_exp, it will
   * increment h_exp by one and h_sig will be zero.  This is the
   * correct result.  h_exp may increment to 15, at greatest, in
   * which case the result overflows to a signed inf.
   */

  return h_sgn + h_exp + h_sig;
}
static void set_denorm(void)
{

#ifdef __x86_64__
    const int MXCSR_DAZ = (1<<6);
    const int MXCSR_FTZ = (1<<15);

    unsigned int mxcsr = __builtin_ia32_stmxcsr ();
    mxcsr |= MXCSR_DAZ | MXCSR_FTZ;
    __builtin_ia32_ldmxcsr (mxcsr);
#endif

}

fp16 fp32tofp16 (float A,int round_method) {
    fp16_data res;
    fp32_data ina;

    ina.fdata = A;
    //// VppInfo("round_method =%d  \n",round_method);
    set_denorm();
    res.idata = fp32idata_to_fp16idata(ina.idata, round_method);
    return res.ndata;
}

float fp16tofp32(fp16 val) {
    fp16_data dfp16;
    dfp16.ndata = val;
    uint32_t sign = (dfp16.idata & 0x8000) << 16;
    uint32_t mantissa = (dfp16.idata & 0x03FF);
    uint32_t exponent = (dfp16.idata & 0x7C00) >> 10;

    if (exponent == 0) {
        if (mantissa == 0) {
            return *((float*)(&sign));
        }
        else {
            exponent = 1;
            while ((mantissa & 0x0400) == 0) {
                mantissa <<= 1;
                exponent++;
            }
            mantissa &= 0x03FF;
            exponent = (127 - 15 - exponent) << 23;
            mantissa <<= 13;
            conv.i = *((uint32_t*)(&sign)) | (*((uint32_t*)(&mantissa))) | (*((uint32_t*)(&exponent)));
            return conv.f;
        }
    }
    else if (exponent == 0x1F) {
        if (mantissa == 0) {
            return *((float*)(&sign)) / 0.0f;
        }
        else {
            return *((float*)(&sign)) / 0.0f;
        }
    }
    else {
        exponent = (exponent + (127 - 15)) << 23;
        mantissa <<= 13;
        conv.i = *((uint32_t*)(&sign)) | (*((uint32_t*)(&mantissa))) | (*((uint32_t*)(&exponent)));
        return conv.f;
    }
}

bm_status_t test(int dim) {
    int L = 1024 * 1024;
    float pnt32[8] = {0};
    for (int i = 0; i < dim; ++i)
        pnt32[i] = (rand() % 2 ? 1.f : -1.f) * (rand() % 100 + (rand() % 100) * 0.01);
    float *XHost = new float[L * dim];
    for (int i = 0; i < L * dim; ++i)
        XHost[i] = (rand() % 2 ? 1.f : -1.f) * (rand() % 100 + (rand() % 100) * 0.01);
    float *YHost = new float[L];
    float *YRef = new float[L];

    int dsize = 4;
    int round = 1;
    float max_error = 1e-5;
    fp16 *XHost16 = new fp16[L * dim];
    fp16 *YHost16 = new fp16[L];
    fp16 *pnt16 = new fp16[8];
    data_type_t dtype = rand() % 2 ? DT_FP16 : DT_FP32; // DT_FP32 DT_FP16;
    if (dtype == DT_FP16) {
        printf("Data type: DT_FP16\n");
        dsize = 2;
        max_error = 1e-3;
    } else {
        printf("Data type: DT_FP32\n");
    }

    bm_handle_t handle = nullptr;
    BM_CHECK_RET(bm_dev_request(&handle, 0));
    bm_device_mem_t XDev, YDev, pnt;
    BM_CHECK_RET(bm_malloc_device_byte(handle, &XDev, L * dim * dsize));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &YDev, L * dsize));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &pnt, 8 * dsize));
    if (dtype == DT_FP16) {
        for (int i = 0; i < L * dim; ++i)
            XHost16[i] = fp32tofp16(XHost[i], round);
        BM_CHECK_RET(bm_memcpy_s2d(handle, XDev, XHost16));
        for (int i = 0; i < dim; ++i){
            pnt16[i] = fp32tofp16(pnt32[i], round);
        }
        BM_CHECK_RET(bm_memcpy_s2d(handle, pnt, pnt16));
    } else {
        BM_CHECK_RET(bm_memcpy_s2d(handle, XDev, XHost));
        BM_CHECK_RET(bm_memcpy_s2d(handle, pnt, pnt32));
    }
    #ifdef __linux__
        struct timeval t1, t2;
        gettimeofday(&t1, NULL);
        BM_CHECK_RET(bmcv_distance_ext(handle,
                               XDev,
                               YDev,
                               dim,
                               pnt,
                               L,
                               dtype));
        gettimeofday(&t2, NULL);
        std::cout << "dim is " << dim << std::endl;
        std::cout << "distance TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << std::endl;
    #else
        struct timespec tp1, tp2;
        clock_gettime_win(0, &tp1);
        BM_CHECK_RET(bmcv_distance_ext(handle,
                               XDev,
                               YDev,
                               dim,
                               pnt,
                               L,
                               dtype));
        clock_gettime_win(0, &tp2);
        std::cout << "distance TPU using time: " << ((tp2.tv_sec - tp1.tv_sec) * 1000000 + (tp2.tv_nsec - tp1.tv_nsec)/1000) << "us" << std::endl;
    #endif
    if (dtype == DT_FP16)
        BM_CHECK_RET(bm_memcpy_d2s(handle, YHost16, YDev));
    else
        BM_CHECK_RET(bm_memcpy_d2s(handle, YHost, YDev));

    for (int i = 0; i < L; ++i) {
        YRef[i] = 0.f;
        for (int j = 0; j < dim; ++j) {
            if (dtype == DT_FP16) {
                XHost[i * dim + j] = fp16tofp32(XHost16[i * dim + j]);
                pnt32[j] = fp16tofp32(pnt16[j]);
            }
            YRef[i] += (XHost[i * dim + j] - pnt32[j]) * (XHost[i * dim + j] - pnt32[j]);
        }
        YRef[i] = std::sqrt(YRef[i]);
        if (dtype == DT_FP16){
            fp16 ff = fp32tofp16(YRef[i], round);
            YRef[i] = fp16tofp32(ff);
        }
    }

    for (int i = 0; i < L; ++i) {
        float err;
        if (dtype == DT_FP16)
            YHost[i] = fp16tofp32(YHost16[i]);
          #ifdef __linux__
          err = std::abs(YRef[i] - YHost[i]) / std::max(YRef[i], YHost[i]);
          #else
          err = std::abs(YRef[i] - YHost[i]) / (std::max)(YRef[i], YHost[i]);
          #endif
        if (err > max_error){
            std::cout << "error :" << YRef[i] << " vs " << YHost[i] << " at " << i << std::endl;
            break;
        }
    }
    delete [] XHost16;
    delete [] YHost16;
    delete [] pnt16;

    delete [] XHost;
    delete [] YHost;
    delete [] YRef;
    bm_free_device(handle, XDev);
    bm_free_device(handle, YDev);
    bm_free_device(handle, pnt);
    bm_dev_free(handle);
    return BM_SUCCESS;
}

int main(int argc, char *argv[]) {
    (void)(argc);
    (void)(argv);
    for (int i = 0; i < 8; ++i) {
        struct timespec tp;
        #ifdef __linux__
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
        #else
        clock_gettime_win(0, &tp);
        #endif
        srand(tp.tv_nsec);
        std::cout << "test " << i << ": random seed: " << tp.tv_nsec << std::endl;
        test(i + 1);
    }
    return 0;
}
