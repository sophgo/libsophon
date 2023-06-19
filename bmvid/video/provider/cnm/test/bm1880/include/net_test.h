#ifndef NET_TEST_H
#define NET_TEST_H

typedef struct {
  u32 bdc;
  u32 arm;
  u32 gdma;
  u32 cdma;
} DESC_NUM;

typedef struct {
  u64 bdc;
  u64 arm;
  u64 gdma;
  u64 cdma;
} DESC_OFFSET;

typedef struct {
  u64         cmdbuf_addr;
  u64         weights_addr;
  u64         input_addr;
  u64         output_ref_addr;
  u64         output_addr;
  u32         output_size;
  u32         segments;
  DESC_OFFSET desc_off;
  DESC_NUM    desc_num;
} NET_INFO_T;

int do_net_test(NET_INFO_T *net, int with_segments);

#endif