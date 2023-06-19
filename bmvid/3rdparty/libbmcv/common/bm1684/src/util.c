#include "stdlib.h"

#ifdef _WIN32
#include "..\include_win\common_win.h"
#else
#include "common.h"
#endif

void dump_float_tensor(const char *filename, int length, float *dump_data) {
  FILE *outf = fopen(filename, "wb");
  if (outf == NULL) {
    printf("The float bin file can not be opened\n");
    exit(-1);
  }
  fwrite(dump_data, 4, length, outf);
  fclose(outf);
}

#define IS_NAN(x) ((((x >> 23) & 0xff) == 0xff) && ((x & 0x7fffff) != 0))
int array_cmp_abs(float *p_exp, float *p_got, int len, const char *info_label, int exactly_matched, float delta) {
  int idx;
  if (exactly_matched) {
    for (idx = 0; idx < len; idx++) {
      if (p_exp[idx] != p_got[idx]) {
        printf("%s error at index %d exp %.20f got %.20f\n", info_label, idx, p_exp[idx], p_got[idx]);
        return -1;
      }
    }
  } else {
    for (idx = 0; idx < len; idx++) {
      if (fabs(p_exp[idx] - p_got[idx]) > delta) {
        printf("%s error at index %d exp %.20f got %.20f\n", info_label, idx, p_exp[idx], p_got[idx]);
        return -1;
      }
    }
  }
  return 0;
}

int array_cmp_rel(float *p_exp, float *p_got, int len, const char *info_label, float delta) {
  int idx = 0;
  for (idx = 0; idx < len; idx++) {
    if (bm_min(fabs(p_exp[idx]), fabs(p_got[idx])) < 1e-20) {
      if (bm_max(fabs(p_exp[idx]), fabs(p_got[idx])) > 1e-19) {
        printf("%s error at index %d exp %.20f got %.20f\n", info_label, idx, p_exp[idx], p_got[idx]);
        return -1;
      }
    } else if (fabs(p_exp[idx] - p_got[idx]) / bm_min(fabs(p_exp[idx]), fabs(p_got[idx])) > delta) {
      printf("%s error at index %d exp %.20f got %.20f\n", info_label, idx, p_exp[idx], p_got[idx]);
      return -1;
    }
  }
  return 0;
}

int array_cmp_int8(char *p_exp, char *p_got, int len, const char *info_label, float delta) {
  UNUSED(delta);
  int idx = 0;
  for (idx = 0; idx < len; idx++) {
    if (p_exp[idx] != p_got[idx]) {
      printf("%s error at index %d exp %d got %d\n", info_label, idx, p_exp[idx], p_got[idx]);
      return -1;
    }
  }
  return 0;
}

int array_cmp_fix8b(
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

int array_cmp_fix16b(
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

int array_cmp(float *p_exp, float *p_got, int len, const char *info_label, float delta) {
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

int tri_array_cmp(
    float *p_exp, float *p_got, float *third_party, int len, const char *info_label, float delta, int *err_idx) {
  int idx       = 0;
  int err_count = 0;
  for (idx = 0; idx < len; idx++) {
    if (bm_max(fabs(p_exp[idx]), fabs(p_got[idx])) > 1.0) {
      // compare rel
      if (bm_min(fabs(p_exp[idx]), fabs(p_got[idx])) < 1e-20) {
        printf(
            "%s rel error at index %d exp %.20f got %.20f third_party %.20f\n", info_label, idx, p_exp[idx], p_got[idx],
            third_party[idx]);
        if (bm_min(fabs(p_exp[idx]), fabs(third_party[idx])) > 1e-20) {
          // only report error when exp agrees with third_party
          *err_idx = idx;
          err_count++;
        }
      }
      if (fabs(p_exp[idx] - p_got[idx]) / bm_min(fabs(p_exp[idx]), fabs(p_got[idx])) > delta) {
        printf(
            "%s rel error at index %d exp %.20f got %.20f third_party %.20f\n", info_label, idx, p_exp[idx], p_got[idx],
            third_party[idx]);
        if (fabs(p_exp[idx] - third_party[idx]) / bm_min(fabs(p_exp[idx]), fabs(third_party[idx])) < delta) {
          // only report error when exp agrees with third_party
          *err_idx = idx;
          err_count++;
        }
      }
    } else {
      // compare abs
      if (fabs(p_exp[idx] - p_got[idx]) > delta) {
        printf(
            "%s abs error at index %d exp %.20f got %.20f third_party %.20f\n", info_label, idx, p_exp[idx], p_got[idx],
            third_party[idx]);
        if (fabs(p_exp[idx] - third_party[idx]) < delta) {
          // only report error when exp agrees with third_party
          *err_idx = idx;
          err_count++;
        }
      }
    }
    // only report error when errer count is bigger than 0.001
    if ((float)err_count / (float)len > 0.001) {
      return -1;
    }
    IF_VAL if_val_exp, if_val_got;
    if_val_exp.fval = p_exp[idx];
    if_val_got.fval = p_got[idx];
    // IF_VAL if_val_3rd;
    // if_val_3rd.fval = third_party[idx];

    // if ( IS_NAN(if_val_exp.ival) || IS_NAN(if_val_got.ival) || IS_NAN(if_val_3rd.ival))
    if (IS_NAN(if_val_got.ival) && !IS_NAN(if_val_exp.ival)) {
      printf("There are nans in %s idx %d \n", info_label, idx);
      printf("floating form exp %.10f got %.10f 3rd %.10f\n", if_val_exp.fval, if_val_got.fval, if_val_got.fval);
      printf("hex form exp %8.8x got %8.8x 3rd %8.8x\n", if_val_exp.ival, if_val_got.ival, if_val_got.ival);
      return -2;
    }
  }
  return 0;
}

int array_cmp_int(int *p_exp, int *p_got, int len, const char *info_label) {
  int idx;
  for (idx = 0; idx < len; idx++) {
    if (p_exp[idx] != p_got[idx]) {
      printf("%s error at index %d exp %d got %d\n", info_label, idx, p_exp[idx], p_got[idx]);
      return -1;
    }
  }
  return 0;
}

int array_cmp_int32(int *p_exp,
                    int *p_got,
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
    exp_int = *(p_exp + idx);
    got_int = *(p_got + idx);
    error = abs(exp_int - got_int);
    if (error > 0) {
      if (first_error_idx == -1) {
        first_error_idx = idx;
        first_expect_value = exp_int;
        first_got_value = got_int;
      }
      if (error > max_error_value) {
        max_error_idx = idx;
        max_error_value = error;
        max_expect_value = exp_int;
        max_got_value = got_int;
      }
      mismatch_cnt++;
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

void dump_array_file(char *file, int row_num, int col_num, int transpose, float *parr) {
  FILE *f_matrix = fopen(file, "w");
  int row_idx, col_idx;
  fprintf(f_matrix, "row %d col %d\n", row_num, col_num);
  for (row_idx = 0; row_idx < row_num; row_idx++)
    for (col_idx = 0; col_idx < col_num; col_idx++) {
      int idx = transpose ? (col_idx * row_num + row_idx) : (row_idx * col_num + col_idx);
      fprintf(f_matrix, "row %d col %d %f\n", row_idx, col_idx, parr[idx]);
    }
  fclose(f_matrix);
}

void dump_hex(char *desc, void *addr, int len) {
  int i;
  unsigned char buff[17];
  unsigned char *pc = (unsigned char *)addr;

  /* Output description if given. */
  if (desc != NULL) printf("%s:\n", desc);

  /* Process every byte in the data. */
  for (i = 0; i < len; i++) {
    /* Multiple of 16 means new line (with line offset). */

    if ((i % 16) == 0) {
      /* Just don't print ASCII for the zeroth line. */
      if (i != 0) printf("  %s\n", buff);

      /* Output the offset. */
      printf("  %04x ", i);
    }

    /* Now the hex code for the specific character. */
    printf(" %02x", pc[i]);

    /* And store a printable ASCII character for later. */
    if ((pc[i] < 0x20) || (pc[i] > 0x7e))
      buff[i % 16] = '.';
    else
      buff[i % 16]     = pc[i];
    buff[(i % 16) + 1] = '\0';
  }

  /* Pad out last line if not exactly 16 characters. */
  while ((i % 16) != 0) {
    printf("   ");
    i++;
  }

  /* And print the final ASCII bit. */
  printf("  %s\n", buff);
}

void dump_data_float_abs(char *desc, void *addr, int n, int c, int h, int w) {
#define ABS_LEN 4
  int ni, ci, hi, wi;
  int off;
  float *data = (float *)addr;

  /* Output description if given. */
  if (desc != NULL) printf("%s: abs, n=%d, c=%d, h=%d, w=%d\n", desc, n, c, h, w);

  /* Process first and last 4 col and 4 raw in the data. */
  for (ni = 0; ni < n; ni++) {
    for (ci = 0; ci < c; ci++) {
      printf("=== n = %02d, c = %02d ===\n", ni, ci);
      for (hi = 0; hi < h; hi++) {
        if (hi == ABS_LEN) printf(" ... \n");
        if (hi >= ABS_LEN && hi <= h - ABS_LEN - 1) continue;
        printf("[ ");
        for (wi = 0; wi < w; wi++) {
          if (wi == ABS_LEN) printf(" ... ");
          if (wi >= ABS_LEN && wi <= w - ABS_LEN - 1) continue;
          off = ni * (c * h * w) + ci * (h * w) + hi * w + wi;
          if (data[off] >= 0) printf(" ");
          printf("%2.2f ", data[off]);
        }
        printf("]\n");
      }
    }
  }
}

/* dump full data, if w <= 16 && h <= 16 */
void dump_data_float(char *desc, void *addr, int n, int c, int h, int w) {
  int ni, ci, hi, wi;
  int off;
  float *data = (float *)addr;

  if (h > 16 || w > 16) {
    dump_data_float_abs(desc, addr, n, c, h, w);
    return;
  }

  /* Output description if given. */
  if (desc != NULL) printf("%s:\n", desc);

  /* Process every byte in the data. */
  for (ni = 0; ni < n; ni++) {
    for (ci = 0; ci < c; ci++) {
      printf("=== n = %02d, c = %02d ===\n", ni, ci);
      for (hi = 0; hi < h; hi++) {
        printf("[ ");
        for (wi = 0; wi < w; wi++) {
          off = ni * (c * h * w) + ci * (h * w) + hi * w + wi;
          if (data[off] >= 0) printf(" ");
          printf("%2.2f ", data[off]);
        }
        printf("]\n");
      }
    }
  }
}

/* dump full data, if w <= 16 && h <= 16 */
void dump_data_int(char *desc, void *addr, int n, int c, int h, int w) {
  int ni, ci, hi, wi;
  int off;
  int *data = (int *)addr;

  if (h > 16 || w > 16) {
    printf("data %s: exceed size, h %d, w %d\n", desc, h, w);
    return;
  }

  /* Output description if given. */
  if (desc != NULL) printf("%s:\n", desc);

  /* Process every byte in the data. */
  for (ni = 0; ni < n; ni++) {
    for (ci = 0; ci < c; ci++) {
      printf("=== n = %02d, c = %02d ===\n", ni, ci);
      for (hi = 0; hi < h; hi++) {
        printf("[ ");
        for (wi = 0; wi < w; wi++) {
          off = ni * (c * h * w) + ci * (h * w) + hi * w + wi;
          if (data[off] >= 0) printf(" ");
          printf("%02d ", data[off]);
        }
        printf("]\n");
      }
    }
  }
}

/* dump full matrix, if row <= 16 && col <= 16 */
void dump_matrix_float(char *desc, void *addr, int row, int col) {
  int ri, ci;
  int off;
  float *data = (float *)addr;

  if (row > 16 || col > 16) {
    printf("matrix %s: exceed size, row %d, col %d\n", desc, row, col);
    return;
  }

  /* Output description if given. */
  if (desc != NULL) printf("%s:\n", desc);

  /* Process every byte in the data. */
  for (ri = 0; ri < row; ri++) {
    printf("[ ");
    for (ci = 0; ci < col; ci++) {
      off = ri * col + ci;
      if (data[off] >= 0) printf(" ");
      printf("%2.2f ", data[off]);
    }
    printf("]\n");
  }
}

int conv_coeff_storage_convert(float *coeff_orig, float **coeff_reformat, u32 oc, u32 ic, u32 kh, u32 kw, u32 npu_num) {
  u32 local_used_num             = npu_num < oc ? npu_num : oc;
  u32 ksize                      = kh * kw;
  u32 coeff_num_in_one_local_mem = ceiling_func(oc, npu_num) * ic * ksize;
  u32 reformat_size              = coeff_num_in_one_local_mem * npu_num;
  *coeff_reformat                = (float *)malloc(reformat_size * sizeof(float));
  memset(*coeff_reformat, 0, reformat_size * sizeof(float));
  u32 oc_in_local_num;
  for (u32 local_idx = 0; local_idx < local_used_num; local_idx++) {
    for (u32 ic_in_local_idx = 0; ic_in_local_idx < ic; ic_in_local_idx++) {
      oc_in_local_num = (oc / npu_num) + (local_idx < (oc % npu_num));
      for (u32 oc_in_local_idx = 0; oc_in_local_idx < oc_in_local_num; oc_in_local_idx++) {
        u32 source_oc_idx = oc_in_local_idx * npu_num + local_idx;
        u32 source_ic_idx = ic_in_local_idx;
        float *src_addr   = coeff_orig + (source_oc_idx * ic + source_ic_idx) * ksize;
        float *dst_addr =
            (*coeff_reformat) + (coeff_num_in_one_local_mem * local_idx +
                                 ic_in_local_idx * (ceiling_func(oc, npu_num) * ksize) + oc_in_local_idx * ksize);
        memcpy(dst_addr, src_addr, ksize * sizeof(float));
      }
    }
  }
  return reformat_size;
}

void *create_cmd_id_node() {
  CMD_ID_NODE *pid_node = (CMD_ID_NODE *)calloc(1, sizeof(CMD_ID_NODE));
  return (void *)pid_node;
}

void destroy_cmd_id_node(void *pid_node) { free(pid_node); }

#ifdef BM_STAS_GEN
void set_cmd_id_cycle(void *pid_node, int val) { ((CMD_ID_NODE *)pid_node)->cycle_count = val; }
int get_cmd_id_cycle(void *pid_node) { return ((CMD_ID_NODE *)pid_node)->cycle_count; }
#endif
