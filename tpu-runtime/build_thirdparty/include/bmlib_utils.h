#ifndef BMLIB_UTILS_H
#define BMLIB_UTILS_H
#include <stdlib.h>
#include <string.h>

/*
 * Debug definitions for user app only
 * Copy from common.h
 * Don't include for internal usage
 */
#ifdef __cplusplus
extern "C" {
#endif

#define UNUSED(x)               (void)(x)
#define IS_NAN(x) ((((x >> 23) & 0xff) == 0xff) && ((x & 0x7fffff) != 0))

void dump_hex(char *desc, void *addr, int len);
void dump_data_float(char *desc, void *addr, int n, int c, int h, int w);
void dump_data_int(char *desc, void *addr, int n, int c, int h, int w);
void dump_matrix_float(char *desc, void *addr, int row, int col);
void dump_array_file(char * file, int row_num, int col_num, int transpose, float * parr);

/* dump to file */
void dump_float_tensor(const char * filename,
    int length, float * dump_data);

#ifdef __cplusplus
/* not available in C */
void random_param(
    int &n, int &c, int &h, int &w,
    int &kh, int &kw, int &ph, int &pw, int &sh, int &sw,
    int &oc);

void random_conv_param(
    int &n, int &ic, int &ih, int &iw, int &oc,
    int &kh, int &kw, int &dh, int &dw,
    int &ph, int &pw, int &sh, int &sw);
#endif

int conv_coeff_storage_convert(
    float * coeff_orig,
    float ** coeff_reformat,
    unsigned int oc,
    unsigned int ic,
    unsigned int kh,
    unsigned int kw,
    unsigned int npu_num);

static inline int get_env_int_value(const char* name, int default_value){
    const char* value_str = getenv(name);
    if(!value_str) return default_value;
    return atoi(value_str);
}

static inline int get_env_bool_value(const char* name, int default_value){
    const char* value_str = getenv(name);
    if(!value_str) return default_value;
    return strlen(value_str) && (value_str[0] == 'T' || value_str[0] == 't'
           || atoi(value_str) != 0);
}


#ifdef __cplusplus
}
#endif

#endif /* BMLIB_UTILS_H */
