#define _WIN32_WINNT 0x0501
#include "test_misc.h"
#include <time.h>
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