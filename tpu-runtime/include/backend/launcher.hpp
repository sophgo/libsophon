#pragma once

#include "bmruntime_common.h"
#include "bmruntime_cpp.h"

namespace bmruntime {

struct tpu_tensor_info_t {
  uint16_t dtype;
  /// storage mode of input/output tensors which are setted by user
  uint16_t user_stmode;
  /// storage mode of input/output tensors which are fixed when compiling
  uint16_t compiled_stmode;
  uint32_t tensor_byte_size;
  int32_t n;
  int32_t c;
  int32_t h;
  int32_t w;
  /// value of padding h for conv(only used for BM1684 conv 3ic)
  uint32_t padding_h;
  /// global addr that is malloced by user
  uint64_t user_global_addr;
  /// global addr of input/output tensors which are fixed when compiling
  uint64_t compiled_global_addr;
};
struct tpu_cmd_info_t {
  /// number of bdc command
  int32_t bdc_cmd_num;
  /// number of gdma command
  int32_t gdma_cmd_num;
  /// number of cdma command
  int32_t cdma_cmd_num;
  /// byte size of bdc command
  uint64_t bdc_cmd_byte_size;
  /// byte size of gdma command
  uint64_t gdma_cmd_byte_size;
  /// byte size of cdma command
  uint64_t cdma_cmd_byte_size;
};
struct reloc_entry_t {
  /// id of base-addrs(io-addrs).
  uint32_t base_addr_id;
  /// addr offset from the base-addr.
  uint32_t addr_offset;
};
struct cmd_reloc_entry_t {
  reloc_entry_t reloc_addr_info;
  /// cmd offset (in Byte) from the tpu_single_core_cmd_t.gdma_cmd_addr.
  uint64_t cmd_offset;
};

struct tpu_single_core_cmd_t {
  std::vector<tpu_cmd_info_t> cmd_info;
  /// global addr of bdc command
  uint64_t bdc_cmd_addr;
  /// global addr of gdma command
  uint64_t gdma_cmd_addr;
  /// global addr of cdma command
  uint64_t cdma_cmd_addr;
  /// global addr of hau command
  uint64_t hau_cmd_addr;
  //// global addr of sdma command
  uint64_t sdma_cmd_addr;
  /// reloc entries for gdma command
  std::vector<cmd_reloc_entry_t> gdma_reloc_entries;
};

typedef struct tpu_kernel_allreduce_1684x {
  u64 i_global_addr[8];
  u64 i_global_addr_1[8];
  u64 o_global_addr[8];
  u32 count;
  int dtype;
  int reduce_method;
  int group[8];
  int rank;
  int chip_num;
  int group_size;
} tpu_kernel_allreduce_1684x_t;

typedef struct tpu_kernel_global_move_1684x {
  u64 src_global_addr;
  u64 dst_global_addr;
  int num_dims;
  int shape[4];
  int src_stride[4];
  int dst_stride[4];
  int type_size;
} tpu_kernel_global_move_1684x_t;

typedef struct {
  std::vector<tpu_tensor_info_t> input_info;
  std::vector<tpu_tensor_info_t> output_info;
  std::vector<u64> reloc_base_addrs; // base io-addrs in io-reloc mode.
  std::vector<tpu_single_core_cmd_t> core_commands;
  std::vector<int32_t> core_list;
  /// kernel func id(used for dynamic loading)
  std::vector<tpu_kernel_function_t> kernel_func_ids;
  /// coeff start addr
  uint64_t coeff_start_addr = -1;
  /// neuron start addr
  std::vector<uint64_t> neuron_start_addr;
  int32_t do_allreduce = 0;
  tpu_kernel_allreduce_1684x_t allreduce_param;
  int32_t addr_mode;
} tpu_net_info_t;

struct tpu_dynamic_tensor_t {
  uint64_t addr;
  uint64_t buffer_addr; // BM1684 storage conversion
  bool buffer_used; // BM1684 storage conversion
  int shape[BM_MAX_DIMS_NUM];
  union {
    uint32_t packed;
    struct {
      uint16_t dim;
      uint8_t dtype;
      uint8_t stmode_conveter; // BM1684
    };
  };
  uint32_t elem_num; // elem_num may be different with shape dims
};
struct tpu_dynamic_net_info_t {
  uint64_t ir_addr;
  uint64_t ir_word_num; // unit word
  uint64_t ctx_start_addr;
  uint64_t coeff_offset_addr;
  uint64_t io_start_addr;
  uint64_t io_mem_offset;
  uint64_t output_shape_addr;
  std::vector<uint64_t> ctx_mem_borders;
  std::vector<uint64_t> ctx_mem_offsets;
  std::vector<tpu_dynamic_tensor_t> inputs;
  std::vector<tpu_dynamic_tensor_t> outputs;
  std::vector<tpu_kernel_function_t> kernel_func_ids; // dynamic loading
  std::vector<int32_t> core_ids; // multi cores
  tpu_kernel_allreduce_1684x_t *all_reduce_param;
};
class Launcher {
public:
  Launcher(){};
  virtual ~Launcher(){};

  virtual bm_status_t static_subnet(bm_handle_t handle,
                                    const tpu_net_info_t &net_info) = 0;
  virtual void fill_api_info(const tpu_net_info_t &net_info,
                             api_info_t &api_info) = 0;
  virtual bm_status_t
  dynamic_subnet(bm_handle_t handle,
                 const tpu_dynamic_net_info_t &net_info) = 0;
};

class Launcher_BM1682 : public Launcher {
public:
  Launcher_BM1682() {
    BM_API_ID_MULTI_FULLNET = 134;
    BM_API_ID_DYNAMIC_FULLNET = 135;
    BM_API_ID_MULTI_FULLNET_PROFILE = 137;
    MAX_API_MSG_SIZE = 1019 * sizeof(u32); // ref to 1682
  };
  void set_bmdnn_func_profile(bool enable) { b_enable_profile = enable; }
  virtual bm_status_t static_subnet(bm_handle_t handle,
                                    const tpu_net_info_t &net_info);
  virtual void fill_api_info(const tpu_net_info_t &net_info,
                             api_info_t &api_info);

  virtual bm_status_t
  dynamic_subnet(bm_handle_t handle,
                 const tpu_dynamic_net_info_t &net_info) override;

private:
  bool b_enable_profile = false;
  u32 BM_API_ID_MULTI_FULLNET_PROFILE;
  u32 BM_API_ID_MULTI_FULLNET;
  u32 BM_API_ID_DYNAMIC_FULLNET;
  u32 MAX_API_MSG_SIZE;
};

class Launcher_BM1684 : public Launcher {
public:
  Launcher_BM1684() {
    BM_API_ID_MULTI_FULLNET = 110;
    BM_API_ID_DYNAMIC_FULLNET = 111;
    BM_API_ID_SET_PROFILE_ENABLE = 986;
    BM_API_ID_GET_PROFILE_DATA = 987;
    MAX_API_MSG_SIZE = 1022 * sizeof(u32); // ref to 1684
  };
  virtual bm_status_t static_subnet(bm_handle_t handle,
                                    const tpu_net_info_t &net_info);
  virtual void fill_api_info(const tpu_net_info_t &net_info,
                             api_info_t &api_info);
  virtual bm_status_t
  dynamic_subnet(bm_handle_t handle,
                 const tpu_dynamic_net_info_t &net_info) override;

  bm_status_t _bmdnn_set_profile_enable_(bm_handle_t handle, bool enable);
  bm_status_t _bmdnn_get_profile_data_(
      bm_handle_t handle, unsigned long long output_global_addr,
      unsigned int output_max_size, unsigned int offset,
      unsigned int data_category // 0: profile time records, 1: extra data
  );

private:
  u32 BM_API_ID_MULTI_FULLNET;
  u32 BM_API_ID_DYNAMIC_FULLNET;
  u32 BM_API_ID_SET_PROFILE_ENABLE;
  u32 BM_API_ID_GET_PROFILE_DATA;
  u32 MAX_API_MSG_SIZE;
};

class Launcher_BM1684X : public Launcher {
public:
  Launcher_BM1684X() {
    BM_API_ID_MULTI_FULLNET = 0x0ffffffb;
    BM_API_ID_DYNAMIC_FULLNET = 0x0ffffffc;
    BM_API_ID_SET_PROFILE_ENABLE = 986;
    BM_API_ID_GET_PROFILE_DATA = 987;
    MAX_API_MSG_SIZE = 1016 * sizeof(u32);
  };
  virtual bm_status_t static_subnet(bm_handle_t handle,
                                    const tpu_net_info_t &net_info);
  virtual void fill_api_info(const tpu_net_info_t &net_info,
                             api_info_t &api_info);

  virtual bm_status_t
  dynamic_subnet(bm_handle_t handle,
                 const tpu_dynamic_net_info_t &net_info) override;

  bm_status_t _bmdnn_set_profile_enable_(bm_handle_t handle,
                                         tpu_kernel_function_t func_id,
                                         bool enable);
  bm_status_t _bmdnn_get_profile_data_(
      bm_handle_t handle, tpu_kernel_function_t func_id,
      unsigned long long output_global_addr, unsigned int output_max_size,
      unsigned int offset,
      unsigned int data_category // 0: profile time records, 1: extra data
  );

private:
  u32 BM_API_ID_MULTI_FULLNET;
  u32 BM_API_ID_DYNAMIC_FULLNET;
  u32 BM_API_ID_SET_PROFILE_ENABLE;
  u32 BM_API_ID_GET_PROFILE_DATA;
  u32 MAX_API_MSG_SIZE;
};

class Launcher_SGTPUV8 : public Launcher {
public:
  Launcher_SGTPUV8() {
    BM_API_ID_MULTI_FULLNET = 0x0ffffffb;
    BM_API_ID_DYNAMIC_FULLNET = 0x0ffffffc;
    BM_API_ID_SET_PROFILE_ENABLE = 986;
    BM_API_ID_GET_PROFILE_DATA = 987;
    MAX_API_MSG_SIZE = 1016 * sizeof(u32);
  };
  virtual bm_status_t static_subnet(bm_handle_t handle,
                                    const tpu_net_info_t &net_info);
  virtual void fill_api_info(const tpu_net_info_t &net_info,
                             api_info_t &api_info);

  virtual bm_status_t
  dynamic_subnet(bm_handle_t handle,
                 const tpu_dynamic_net_info_t &net_info) override;

  bm_status_t _bmdnn_set_profile_enable_(bm_handle_t handle,
                                         unsigned int enable);
  bm_status_t _bmdnn_get_profile_data_(
      bm_handle_t handle, unsigned long long output_global_addr,
      unsigned int output_max_size, unsigned int offset,
      unsigned int data_category // 0: profile time records, 1: extra data
  );

private:
  u32 BM_API_ID_MULTI_FULLNET;
  u32 BM_API_ID_DYNAMIC_FULLNET;
  u32 BM_API_ID_SET_PROFILE_ENABLE;
  u32 BM_API_ID_GET_PROFILE_DATA;
  u32 MAX_API_MSG_SIZE;
};

class Launcher_BM1688 : public Launcher {
public:
  Launcher_BM1688() { MAX_API_MSG_SIZE = 1016 * sizeof(u32); };
  virtual bm_status_t static_subnet(bm_handle_t handle,
                                    const tpu_net_info_t &net_info);
  virtual void fill_api_info(const tpu_net_info_t &net_info,
                             api_info_t &api_info);

  virtual bm_status_t
  dynamic_subnet(bm_handle_t handle,
                 const tpu_dynamic_net_info_t &net_info) override;

  bm_status_t _bmdnn_set_engine_profile_param_(bm_handle_t handle, int core,
                                               tpu_kernel_function_t func_id,
                                               int engine_type,
                                               unsigned long long addr,
                                               unsigned long long size);
  bm_status_t _bmdnn_set_profile_enable_(bm_handle_t handle, int core,
                                         tpu_kernel_function_t func_id,
                                         unsigned int enable);
  bm_status_t _bmdnn_get_profile_data_(
      bm_handle_t handle, int core, tpu_kernel_function_t func_id,
      unsigned long long output_global_addr, unsigned int output_max_size,
      unsigned int offset,
      unsigned int data_category // 0: profile time records, 1: extra data
  );

private:
  u32 MAX_API_MSG_SIZE;
};

class Launcher_BM1690 : public Launcher {
public:
  Launcher_BM1690(std::string arch) : arch_(arch) {
    BM_API_ID_MULTI_FULLNET = 0x0ffffffb;
    BM_API_ID_DYNAMIC_FULLNET = 0x0ffffffc;
    BM_API_ID_SET_PROFILE_ENABLE = 986;
    BM_API_ID_GET_PROFILE_DATA = 987;
    MAX_API_MSG_SIZE = 1016 * sizeof(u32);
  };
  virtual bm_status_t static_subnet(bm_handle_t handle,
                                    const tpu_net_info_t &net_info);
  virtual void fill_api_info(const tpu_net_info_t &net_info,
                             api_info_t &api_info);

  virtual bm_status_t
  dynamic_subnet(bm_handle_t handle,
                 const tpu_dynamic_net_info_t &net_info) override;

  bm_status_t _bmdnn_set_profile_enable_(bm_handle_t handle,
                                         unsigned int enable);
  bm_status_t _bmdnn_get_profile_data_(
      bm_handle_t handle, unsigned long long output_global_addr,
      unsigned int output_max_size, unsigned int offset,
      unsigned int data_category // 0: profile time records, 1: extra data
  );

private:
  std::string arch_;
  u32 BM_API_ID_MULTI_FULLNET;
  u32 BM_API_ID_DYNAMIC_FULLNET;
  u32 BM_API_ID_SET_PROFILE_ENABLE;
  u32 BM_API_ID_GET_PROFILE_DATA;
  u32 MAX_API_MSG_SIZE;
};

class Launcher_CV184X : public Launcher {
public:
  Launcher_CV184X() {
    BM_API_ID_MULTI_FULLNET = 0x0ffffffb;
    BM_API_ID_DYNAMIC_FULLNET = 0x0ffffffc;
    BM_API_ID_SET_PROFILE_ENABLE = 986;
    BM_API_ID_GET_PROFILE_DATA = 987;
    MAX_API_MSG_SIZE = 1016 * sizeof(u32);
  };
  virtual bm_status_t static_subnet(bm_handle_t handle,
                                    const tpu_net_info_t &net_info);
  virtual void fill_api_info(const tpu_net_info_t &net_info,
                             api_info_t &api_info);

  virtual bm_status_t
  dynamic_subnet(bm_handle_t handle,
                 const tpu_dynamic_net_info_t &net_info) override;

  bm_status_t _bmdnn_set_engine_profile_param_(bm_handle_t handle, int core,
                                               tpu_kernel_function_t func_id,
                                               int engine_type,
                                               unsigned long long addr,
                                               unsigned long long size);
  bm_status_t _bmdnn_set_profile_enable_(bm_handle_t handle, int core,
                                         tpu_kernel_function_t func_id,
                                         unsigned int enable);
  bm_status_t _bmdnn_get_profile_data_(
      bm_handle_t handle, int core, tpu_kernel_function_t func_id,
      unsigned long long output_global_addr, unsigned int output_max_size,
      unsigned int offset,
      unsigned int data_category // 0: profile time records, 1: extra data
  );

private:
  u32 BM_API_ID_MULTI_FULLNET;
  u32 BM_API_ID_DYNAMIC_FULLNET;
  u32 BM_API_ID_SET_PROFILE_ENABLE;
  u32 BM_API_ID_GET_PROFILE_DATA;
  u32 MAX_API_MSG_SIZE;
};

class Launcher_SG2380 : public Launcher {
public:
  Launcher_SG2380() {
    BM_API_ID_MULTI_FULLNET = 0x0ffffffb;
    BM_API_ID_DYNAMIC_FULLNET = 0x0ffffffc;
    BM_API_ID_SET_PROFILE_ENABLE = 986;
    BM_API_ID_GET_PROFILE_DATA = 987;
    MAX_API_MSG_SIZE = 1016 * sizeof(u32);
  };
  virtual bm_status_t static_subnet(bm_handle_t handle,
                                    const tpu_net_info_t &net_info);
  virtual void fill_api_info(const tpu_net_info_t &net_info,
                             api_info_t &api_info);

  virtual bm_status_t
  dynamic_subnet(bm_handle_t handle,
                 const tpu_dynamic_net_info_t &net_info) override;

  bm_status_t _bmdnn_set_engine_profile_param_(bm_handle_t handle, int core,
                                               tpu_kernel_function_t func_id,
                                               int engine_type,
                                               unsigned long long addr,
                                               unsigned long long size);
  bm_status_t _bmdnn_set_profile_enable_(bm_handle_t handle, int core,
                                         tpu_kernel_function_t func_id,
                                         unsigned int enable);
  bm_status_t _bmdnn_get_profile_data_(
      bm_handle_t handle, int core, tpu_kernel_function_t func_id,
      unsigned long long output_global_addr, unsigned int output_max_size,
      unsigned int offset,
      unsigned int data_category // 0: profile time records, 1: extra data
  );

private:
  u32 BM_API_ID_MULTI_FULLNET;
  u32 BM_API_ID_DYNAMIC_FULLNET;
  u32 BM_API_ID_SET_PROFILE_ENABLE;
  u32 BM_API_ID_GET_PROFILE_DATA;
  u32 MAX_API_MSG_SIZE;
};

class Launcher_BM1684X2 : public Launcher {
public:
  Launcher_BM1684X2() {
    BM_API_ID_MULTI_FULLNET = 0x0ffffffb;
    BM_API_ID_DYNAMIC_FULLNET = 0x0ffffffc;
    BM_API_ID_SET_PROFILE_ENABLE = 986;
    BM_API_ID_GET_PROFILE_DATA = 987;
    MAX_API_MSG_SIZE = 1016 * sizeof(u32);
  };
  virtual bm_status_t static_subnet(bm_handle_t handle,
                                    const tpu_net_info_t &net_info);
  virtual void fill_api_info(const tpu_net_info_t &net_info,
                             api_info_t &api_info);

  virtual bm_status_t
  dynamic_subnet(bm_handle_t handle,
                 const tpu_dynamic_net_info_t &net_info) override;

  bm_status_t _bmdnn_set_engine_profile_param_(bm_handle_t handle, int core,
                                               tpu_kernel_function_t func_id,
                                               int engine_type,
                                               unsigned long long addr,
                                               unsigned long long size);
  bm_status_t _bmdnn_set_profile_enable_(bm_handle_t handle, int core,
                                         tpu_kernel_function_t func_id,
                                         unsigned int enable);
  bm_status_t _bmdnn_get_profile_data_(
      bm_handle_t handle, int core, tpu_kernel_function_t func_id,
      unsigned long long output_global_addr, unsigned int output_max_size,
      unsigned int offset,
      unsigned int data_category // 0: profile time records, 1: extra data
  );

private:
  u32 BM_API_ID_MULTI_FULLNET;
  u32 BM_API_ID_DYNAMIC_FULLNET;
  u32 BM_API_ID_SET_PROFILE_ENABLE;
  u32 BM_API_ID_GET_PROFILE_DATA;
  u32 MAX_API_MSG_SIZE;
};

} // namespace bmruntime
