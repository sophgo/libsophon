#pragma once
#include "bmruntime_common.h"
#include "backend/launcher.hpp"

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

namespace bmruntime {

struct net_stage_t;

struct ConversionParams {
  void *dst_cmd;
  const void *src_cmd;
  uint32_t num_cmd;
  uint64_t cmd_addr;
  uint64_t cmd_size;
  const net_stage_t *stage;
};

struct tag_t {
  uint32_t start;
  uint32_t len;
  uint64_t mask;
};
struct addr_t {
  tag_t offset;
  tag_t tag;
};

struct CmdInfo {
  uint8_t bits;
  uint32_t size;
  uint32_t cnt; // unit by 4byte

  CmdInfo(uint8_t bits, uint32_t size, uint32_t cnt)
      : bits(bits), size(size), cnt(cnt) {}

  explicit CmdInfo(uint8_t bits) : bits(bits) {
    this->size = 1 << bits;
    this->cnt = this->size >> 2;
  }
  CmdInfo() = delete;
};

class Backend {
public:
  Backend() = default;
  virtual ~Backend() = default;

  virtual uint64_t ctx_start_addr() const { return gmem_start_addr(); }
  virtual uint64_t gmem_start_addr() const = 0;
  virtual uint64_t gmem_offset() const { return 0; }
  virtual addr_t addr_layout() const {
    return addr_t{
        .offset = tag_t{0, 64, 0xffffffffffffffff},
        .tag = tag_t{0, 0, 0},
    };
  }

  virtual std::string name() const = 0;
  virtual bool name_verify(const std::string &model_name) const {
    return model_name == name();
  }

  // update tiu commands and gdma command,
  // you must override this function if the bachend has special convert_cmd
  virtual bool convert_bdc(ConversionParams &params) const {
    memcpy(params.dst_cmd, params.src_cmd, params.cmd_size);
    return true;
  }
  virtual bool convert_gdma(ConversionParams &params) const {
    memcpy(params.dst_cmd, params.src_cmd, params.cmd_size);
    return true;
  }
  virtual uint32_t aligned_cmd_words(ENGINE_ID engine) const {
    BMRT_LOG(FATAL, "The length of command is variable for this chip, and this "
                    "function can not be called\n");
    return 0;
  }

  virtual bool support_dynamic_loading() const { return false; }

  virtual uint32_t core_num() const { return core_num_; }
  // bm1688/cv1686 need get core num by bm_handle
  virtual void set_core_num(uint32_t num) { core_num_ = num; }

  virtual const std::unique_ptr<Launcher> &launcher() {
    BMRT_ASSERT(launcher_);
    return launcher_;
  }

  // STORE_MODE_1N, STORE_MODE_2N, STORE_MODE_4N
  virtual bool support_stmode() const { return false; }

protected:
  uint64_t update_gdma_cmd_addr(const net_stage_t *stage, uint64_t origin_addr, bool is_src) const;
  virtual uint32_t gdma_cmd_size(const uint32_t *cmd, uint64_t offset, bool last_cmd) const;
  uint32_t core_num_ = 1;
  std::unique_ptr<Launcher> launcher_;
};

class Backend_BM1682 : public Backend {
public:
  Backend_BM1682() { launcher_ = std::make_unique<Launcher_BM1682>(); }
  virtual ~Backend_BM1682() = default;

  virtual uint64_t ctx_start_addr() const override {
    return gmem_start_addr() + 0x5004000ul + 0x100000ul + 0x80000ul;
  }
  virtual uint64_t gmem_start_addr() const override { return 0x100000000ul; }
  virtual uint64_t gmem_offset() const {
#ifdef SOC_MODE
    return 0x100000000ul;
#else
    return 0;
#endif
  }
  virtual std::string name() const override { return "BM1682"; }

  virtual bool convert_bdc(ConversionParams &params) const override;
  virtual bool convert_gdma(ConversionParams &params) const override;
  virtual uint32_t aligned_cmd_words(ENGINE_ID engine) const override {
    uint32_t cmd_words = 0;
    if (engine == ENGINE_BD) {
      cmd_words = bdc_cmd_info.size >> 2;
    } else if (engine == ENGINE_GDMA) {
      cmd_words = gdma_cmd_info.size >> 2;
    } else {
      BMRT_LOG(FATAL, "Unsupported engine: %d\n", engine);
    }
    return cmd_words;
  }

private:
  const CmdInfo bdc_cmd_info = CmdInfo(8, 1 << 8, 24);
  const CmdInfo gdma_cmd_info = CmdInfo(6);
};

class Backend_BM1684 : public Backend {
public:
  Backend_BM1684() { launcher_ = std::make_unique<Launcher_BM1684>(); }
  virtual ~Backend_BM1684() = default;

  virtual uint64_t ctx_start_addr() const override {
    return gmem_start_addr() + 0x5000000ul + 0x100000ul;
  }
  virtual uint64_t gmem_start_addr() const override { return 0x100000000ul; }
  virtual std::string name() const override { return "BM1684"; }

  virtual bool convert_bdc(ConversionParams &params) const override;
  virtual bool convert_gdma(ConversionParams &params) const override;
  virtual uint32_t aligned_cmd_words(ENGINE_ID engine) const override {
    uint32_t cmd_words = 0;
    if (engine == ENGINE_BD) {
      cmd_words = bdc_cmd_info.size >> 2;
    } else if (engine == ENGINE_GDMA) {
      cmd_words = gdma_cmd_info.size >> 2;
    } else {
      BMRT_LOG(FATAL, "Unsupported engine:%d\n", engine);
    }
    return cmd_words;
  }

  virtual bool support_stmode() const override { return true; }

private:
  const CmdInfo bdc_cmd_info = CmdInfo(7);
  const CmdInfo gdma_cmd_info = CmdInfo(7);
};

class Backend_BM1684X : public Backend {
public:
  Backend_BM1684X() { launcher_ = std::make_unique<Launcher_BM1684X>(); }
  virtual ~Backend_BM1684X() = default;

  virtual uint64_t gmem_start_addr() const override { return 0x100000000ul; }
  virtual std::string name() const override { return "BM1684X"; }
  virtual bool name_verify(const std::string &model_name) const override {
    return model_name == name() || model_name == "BM1686";
  }

  virtual bool convert_gdma(ConversionParams &params) const override;

  virtual bool support_dynamic_loading() const override { return true; }
};

class Backend_BM1688 : public Backend {
public:
  Backend_BM1688() { launcher_ = std::make_unique<Launcher_BM1688>(); core_num_ = 2; }
  virtual ~Backend_BM1688() = default;

  virtual uint64_t gmem_start_addr() const override { return 0x100000000ul; }
  virtual addr_t addr_layout() const override {
    return addr_t{
        .offset = tag_t{.start = 0, .len = 35, .mask = (1ul << 35) - 1},
        .tag = tag_t{.start = 36, .len = 3, .mask = 0x7}};
  }

  virtual std::string name() const override { return "BM1688"; }
  virtual bool name_verify(const std::string &model_name) const override {
    return model_name == name() || model_name == "CV186X" ||
           model_name == "BM1686";
  }

  virtual bool convert_gdma(ConversionParams &params) const override;

  virtual bool support_dynamic_loading() const override { return true; }
};
class Backend_BM1690 : public Backend {
public:
  Backend_BM1690() { launcher_ = std::make_unique<Launcher_BM1690>("BM1690"); core_num_ = 8;}
  virtual ~Backend_BM1690() = default;

  virtual uint64_t gmem_start_addr() const override { return 0; }
  virtual addr_t addr_layout() const override {
    return addr_t{
        .offset = tag_t{.start = 0, .len = 40, .mask = (1ul << 40) - 1},
        .tag = tag_t{.start = 40, .len = 5, .mask = (1ul << 5) - 1}};
  }
  virtual std::string name() const override { return "BM1690"; }
};
class Backend_BM1690E : public Backend {
public:
  Backend_BM1690E() { launcher_ = std::make_unique<Launcher_BM1690>("BM1690E"); core_num_ = 4;}
  virtual ~Backend_BM1690E() = default;

  virtual uint64_t gmem_start_addr() const override { return 0; }
  virtual addr_t addr_layout() const override {
    return addr_t{
        .offset = tag_t{.start = 0, .len = 40, .mask = (1ul << 40) - 1},
        .tag = tag_t{.start = 40, .len = 5, .mask = (1ul << 5) - 1}};
  }

  virtual std::string name() const override { return "BM1690E"; }
};
class Backend_SG2380 : public Backend {
public:
  Backend_SG2380() = default;
  virtual ~Backend_SG2380() = default;

  virtual uint64_t gmem_start_addr() const override { return 0; }
  virtual addr_t addr_layout() const override {
    return addr_t{
        .offset = tag_t{.start = 0, .len = 40, .mask = (1ul << 40) - 1},
        .tag = tag_t{.start = 40, .len = 5, .mask = (1ul << 5) - 1}};
  }

  virtual std::string name() const override { return "SG2380"; }

  virtual uint32_t core_num() const { return 4; }
};
class Backend_CV184X : public Backend {
public:
  Backend_CV184X() { launcher_ = std::make_unique<Launcher_CV184X>(); }
  virtual ~Backend_CV184X() = default;

  virtual uint64_t gmem_start_addr() const override { return 0x80000000ul; }
  virtual addr_t addr_layout() const override {
    return addr_t{
        .offset = tag_t{.start = 0, .len = 40, .mask = (1ul << 40) - 1},
        .tag = tag_t{.start = 40, .len = 5, .mask = (1ul << 5) - 1}};
  }

  virtual std::string name() const override { return "CV184X"; }
  virtual bool name_verify(const std::string &model_name) const override {
    return model_name == name() || model_name == "MARS3";
  }

  virtual bool support_dynamic_loading() const override { return true; }
};
class Backend_BM1684X2 : public Backend {
public:
  Backend_BM1684X2() { launcher_ = std::make_unique<Launcher_BM1684X2>(); core_num_ = 4;}
  virtual ~Backend_BM1684X2() = default;

  virtual uint64_t gmem_start_addr() const override { return 0x1000000000ul; }
  virtual addr_t addr_layout() const override {
    return addr_t{
        .offset = tag_t{.start = 0, .len = 40, .mask = (1ul << 40) - 1},
        .tag = tag_t{.start = 40, .len = 5, .mask = (1ul << 5) - 1}};
  }

  virtual std::string name() const override { return "BM1684X2"; }
};

class Backend_SGTPUV8 : public Backend {
public:
  Backend_SGTPUV8() { launcher_ = std::make_unique<Launcher_SGTPUV8>(); }
  virtual ~Backend_SGTPUV8() = default;

  virtual uint64_t gmem_start_addr() const override { return 0x80000000ul; }
  virtual addr_t addr_layout() const override {
    return addr_t{
        .offset = tag_t{.start = 0, .len = 40, .mask = (1ul << 40) - 1},
        .tag = tag_t{.start = 40, .len = 5, .mask = (1ul << 5) - 1}};
  }

  virtual std::string name() const override { return "SGTPUV8"; }

  virtual bool support_dynamic_loading() const override { return true; }
};

static std::shared_ptr<Backend> register_backend(uint32_t chipip) {
  if (chipip == 0x1682) {
    return std::make_shared<Backend_BM1682>();
  } else if (chipip == 0x1684) {
    return std::make_shared<Backend_BM1684>();
  } else if (chipip == 0x1686) {
    return std::make_shared<Backend_BM1684X>();
  } else if (chipip == 0x1688 || chipip == 0x1686a200) {
    return std::make_shared<Backend_BM1688>();
  } else if (chipip == 0x184) {
    return std::make_shared<Backend_CV184X>();
  } else if (chipip == 0x16862) {
    return std::make_shared<Backend_BM1684X2>();
  } else if (chipip == 0x2260) {
    return std::make_shared<Backend_BM1690>();
  } else if (chipip == 0x2260e) {
    return std::make_shared<Backend_BM1690E>();
  } else if (chipip == 0x2380) {
    return std::make_shared<Backend_SG2380>();
  } else if (chipip == 0x8000) {
    return std::make_shared<Backend_SGTPUV8>();
  } else {
    BMRT_LOG(FATAL, "unknown chipip: %x", chipip);
  }
  return nullptr;
}
} // namespace bmruntime
