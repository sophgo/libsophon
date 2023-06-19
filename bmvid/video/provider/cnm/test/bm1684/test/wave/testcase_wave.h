#ifndef _TESTCASE_CODA_H_
#define _TESTCASE_CODA_H_

struct decoder_param_s {
    int32_t boda_codec_type;
    int32_t boda_wtl_en;
    int32_t boda_map_typ;
    int32_t mvc_en;
    int32_t t2l_en;
    int32_t t2l_mode;
    int32_t cache_bypass;
    int32_t rotate_en;
    int32_t mirror_en;
    int32_t bwb_en;
    int32_t secaxi_en;
    int32_t mpeg4_class;
    int32_t wtl_mode;
    int32_t wtl_format;
};

typedef struct decoder_param_s    decoder_param_t;

int testcase_wave(void);

#endif
