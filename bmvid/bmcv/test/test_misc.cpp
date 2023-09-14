#define _WIN32_WINNT 0x0501
#include "test_misc.h"
#include <time.h>
#include <cstdint>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

using namespace std;

int array_cmp_(float *p_exp, float *p_got, int len, const char *info_label, float delta) {
  int max_error_count = 128;
  int idx = 0;
  int total = 0;
  int* p_exp_raw = (int*)(p_exp);
  int* p_got_raw = (int*)(p_got);
  bool only_warning = false;
  if (1e4 <= delta) {
    delta = 1e-2;
    only_warning = true;
  }
  for (idx = 0; idx < len; idx++) {
    if (bm_max(fabs(p_exp[idx]), fabs(p_got[idx])) > 1.0) {
      // compare rel
      if (bm_min(fabs(p_exp[idx]), fabs(p_got[idx])) < 1e-20) {
        printf("%s:%s(): %s rel warning at index %d exp %.20f got %.20f\n", __FILE__, __FUNCTION__, info_label, idx, p_exp[idx], p_got[idx]);
        total++;
        if (max_error_count < total && !only_warning) {return -1;}
      }
      if (fabs(p_exp[idx] - p_got[idx]) / bm_min(fabs(p_exp[idx]), fabs(p_got[idx])) > delta) {
        printf(
            "%s:%s(): %s rel warning at index %d exp %.20f(0x%08X) got %.20f(0x%08X), diff=%.20f\n",
            __FILE__, __FUNCTION__,
            info_label, idx,
            p_exp[idx], p_exp_raw[idx],
            p_got[idx], p_got_raw[idx],
            p_exp[idx] - p_got[idx]);
        total++;
        if (max_error_count < total && !only_warning) {return -1;}
      }
    } else {
      // compare abs
      if (fabs(p_exp[idx] - p_got[idx]) > delta) {
        printf(
            "%s:%s(): %s abs warning at index %d exp %.20f(0x%08X) got %.20f(0x%08X), diff=%.20f\n",
            __FILE__, __FUNCTION__,
            info_label, idx,
            p_exp[idx], p_exp_raw[idx],
            p_got[idx], p_got_raw[idx],
            p_exp[idx] - p_got[idx]);
        total++;
        if (max_error_count < total && !only_warning) {return -1;}
      }
    }

    IF_VAL if_val_exp, if_val_got;
    if_val_exp.fval = p_exp[idx];
    if_val_got.fval = p_got[idx];
    if (IS_NAN(if_val_got.ival) && !IS_NAN(if_val_exp.ival)) {
      printf("There are nans in %s idx %d\n", info_label, idx);
      printf("floating form exp %.10f got %.10f\n", if_val_exp.fval, if_val_got.fval);
      printf("hex form exp %8.8x got %8.8x\n", if_val_exp.ival, if_val_got.ival);
      return -2;
    }
  }
  if (0 < total && !only_warning) {return -1;}
  return 0;
}

int array_cmp_fix16b_(
    void *p_exp,
    void *p_got,
    int sign,  // 0: unsigned, 1: signed
    int len,
    const char *info_label,
    int delta) {
  int idx = 0;
  int first_error_idx = -1, first_expect_value = 0, first_got_value = 0;
  int max_error_idx   = -1, max_expect_value   = 0, max_got_value   = 0;
  int max_error_value = 0,  mismatch_cnt = 0;
  for (idx = 0; idx < len; idx++) {
    int error   = 0;
    int exp_int = 0;
    int got_int = 0;
    if (sign) {
      exp_int = (int)(*((short *)p_exp + idx));
      got_int = (int)(*((short *)p_got + idx));
    } else {
      exp_int = (int)(*((unsigned short *)p_exp + idx));
      got_int = (int)(*((unsigned short *)p_got + idx));
    }
    error = abs(exp_int - got_int);
    if (error > 0) {
      if (first_error_idx == -1) {
        first_error_idx = idx;
        first_expect_value = exp_int;
        first_got_value = got_int;
      }
      if(error > max_error_value) {
        max_error_idx = idx;
        max_error_value = error;
        max_expect_value = exp_int;
        max_got_value = got_int;
      }
      mismatch_cnt ++;
      //printf("%s mismatch at index %d exp %d got %d (delta %d)\n", info_label, idx, exp_int, got_int, delta); fflush(stdout);
    }
    if (error > delta) {
      printf("%s     error      at index %d exp %d got %d\n", info_label, idx, exp_int, got_int);
      printf("%s first mismatch at index %d exp %d got %d (delta %d)\n", info_label, first_error_idx, first_expect_value, first_got_value, delta);
      printf("%s total mismatch count %d (delta %d) \n", info_label, mismatch_cnt, delta);
      fflush(stdout);
      return -1;
    }
  }
  if(max_error_idx != -1) {
    printf("%s first mismatch at index %d exp %d got %d (delta %d)\n", info_label, first_error_idx, first_expect_value, first_got_value, delta);
    printf("%s  max  mismatch at index %d exp %d got %d (delta %d)\n", info_label, max_error_idx, max_expect_value, max_got_value, delta);
    printf("%s total mismatch count %d (delta %d) \n", info_label, mismatch_cnt, delta);
    fflush(stdout);
  }

  return 0;
}

int array_cmp_fix8b_(
    void *p_exp,
    void *p_got,
    int sign,  // 0: unsigned, 1: signed
    int len,
    const char *info_label,
    int delta) {

  // enable the following line to spot layers which compare with delta>0.
  // printf("array_cmp_fix8b(...,sign=%d, info=%s, delta=%d) gets called.\n", sign, info_label, delta); fflush(stdout);
#if (0)
  delta = 0;
#endif

  int idx = 0;
  int first_error_idx = -1, first_expect_value = 0, first_got_value = 0;
  int max_error_idx   = -1, max_expect_value   = 0, max_got_value   = 0;
  int max_error_value = 0,  mismatch_cnt = 0;
  for (idx = 0; idx < len; idx++) {
    int error   = 0;
    int exp_int = 0;
    int got_int = 0;
    if (sign) {
      exp_int = (int)(*((char *)p_exp + idx));
      got_int = (int)(*((char *)p_got + idx));
    } else {
      exp_int = (int)(*((unsigned char *)p_exp + idx));
      got_int = (int)(*((unsigned char *)p_got + idx));
    }
    error = abs(exp_int - got_int);
    if (error > 0) {
      if (first_error_idx == -1) {
        first_error_idx = idx;
        first_expect_value = exp_int;
        first_got_value = got_int;
      }
      if(error > max_error_value) {
        max_error_idx = idx;
        max_error_value = error;
        max_expect_value = exp_int;
        max_got_value = got_int;
      }
      mismatch_cnt ++;
      //printf("%s mismatch at index %d exp %d got %d (delta %d)\n", info_label, idx, exp_int, got_int, delta); fflush(stdout);
    }
    if (error > delta) {
      printf("%s     error      at index %d exp %d got %d\n", info_label, idx, exp_int, got_int);
      printf("%s first mismatch at index %d exp %d got %d (delta %d)\n", info_label, first_error_idx, first_expect_value, first_got_value, delta);
      printf("%s total mismatch count %d (delta %d) \n", info_label, mismatch_cnt, delta);
      fflush(stdout);
      FILE* ofp = fopen("out.dat", "w");
      FILE* rfp = fopen("ref.dat", "w");
      for(int i = 0; i< len; i++){
          fprintf(ofp, "0x%02x\n", 0xFF&((char*)p_got)[i]);
          fprintf(rfp, "0x%02x\n", 0xFF&((char*)p_exp)[i]);
      }
      fclose(ofp);
      fclose(rfp);
      return -1;
    }
  }
  if(max_error_idx != -1) {
    printf("%s first mismatch at index %d exp %d got %d (delta %d)\n", info_label, first_error_idx, first_expect_value, first_got_value, delta);
    printf("%s  max  mismatch at index %d exp %d got %d (delta %d)\n", info_label, max_error_idx, max_expect_value, max_got_value, delta);
    printf("%s total mismatch count %d (delta %d) \n", info_label, mismatch_cnt, delta);
    fflush(stdout);
  }

  return 0;
}

#ifdef _WIN32
#define BILLION (1E9)
static BOOL          g_first_time = 1;
static LARGE_INTEGER g_counts_per_sec;
int clock_gettime_win(int dummy, struct timespec* ct) {
  LARGE_INTEGER count;

  if (g_first_time) {
    g_first_time = 0;

    if (0 == QueryPerformanceFrequency(&g_counts_per_sec)) {
      g_counts_per_sec.QuadPart = 0;
    }
  }

  if ((NULL == ct) || (g_counts_per_sec.QuadPart <= 0) ||
      (0 == QueryPerformanceCounter(&count))) {
    return -1;
  }

  ct->tv_sec  = count.QuadPart / g_counts_per_sec.QuadPart;
  ct->tv_nsec = ((count.QuadPart % g_counts_per_sec.QuadPart) * BILLION) /
      g_counts_per_sec.QuadPart;

  return 0;
}

int gettimeofday_win(struct timeval* tp, void* tzp) {
  struct timespec clock;
  clock_gettime_win(0, &clock);
  tp->tv_sec = clock.tv_sec;
  tp->tv_usec = clock.tv_nsec / 1000;
  return 0;
}
#endif

int gettimeofday_(struct timeval* tp){
    #ifdef __linux__
      return gettimeofday(tp, NULL);
    #else
      return gettimeofday_win(tp,NULL);
    #endif
}

int clock_gettime_(int dummy, struct timespec* ct){
    #ifdef __linux__
      return clock_gettime(dummy, ct);
    #else
      return clock_gettime_win(dummy, ct);
    #endif
}

int dtype_size(data_type_t data_type) {
    int size = 0;
    switch (data_type) {
        case DT_FP32:   size = 4; break;
        case DT_UINT32: size = 4; break;
        case DT_INT32:  size = 4; break;
        case DT_FP16:   size = 2; break;
        case DT_BFP16:  size = 2; break;
        case DT_INT16:  size = 2; break;
        case DT_UINT16: size = 2; break;
        case DT_INT8:   size = 1; break;
        case DT_UINT8:  size = 1; break;
        default: break;
    }
    return size;
}

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

float fp16tofp32(fp16 h) {
    fp16_data dfp16;
    dfp16.ndata = h;
    fp32_data ret32;
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
            ret32.idata = *((uint32_t*)(&sign)) | (*((uint32_t*)(&mantissa))) | (*((uint32_t*)(&exponent)));
            return ret32.fdata;
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
        ret32.idata = *((uint32_t*)(&sign)) | (*((uint32_t*)(&mantissa))) | (*((uint32_t*)(&exponent)));
        return ret32.fdata;
    }
}
