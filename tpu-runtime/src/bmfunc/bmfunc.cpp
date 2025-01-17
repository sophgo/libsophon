#include "bmfunc/bmfunc.h"

namespace bmruntime {

bmfunc *bmfunc::sta_bmfunc_ptr;

bmfunc::bmfunc(const string &arch_name) {
  sta_bmfunc_ptr = this;

  bmdnn_fn = NULL;

  p_bmtpu_arch = new bmrt_arch_info(arch_name);

  bmtpu_arch_t arch = bmrt_arch_info::get_bmtpu_arch();
  if (arch == BM1684) {
    bmdnn_fn = new bmdnn_func_1684();
  } else if (arch == BM1880) {
    bmdnn_fn = new bmdnn_func_1880();
  } else if (arch == BM1682) {
    bmdnn_fn = new bmdnn_func_1682();
  } else if (arch == BM1684X) {
    bmdnn_fn = new bmdnn_func_1684x();
  } else if (arch == BM1688) {
    bmdnn_fn = new bmdnn_func_1688();
  } else if (arch == BM1690) {
    bmdnn_fn = new bmdnn_func_2260();
  } else if (arch == SG2380) {
    bmdnn_fn = new bmdnn_func_2380();
  } else if (arch == MARS3) {
    bmdnn_fn = new bmdnn_func_mars3();
  } else if (arch == SGTPUV8) {
    bmdnn_fn = new bmdnn_func_sgtpuv8();
  } else {
    BMRT_LOG(FATAL, "Error: unkown architecture [%d]", arch);
  }
}

bmfunc::~bmfunc() {
  if (bmdnn_fn)
    delete bmdnn_fn;
  delete p_bmtpu_arch;
}

} // namespace bmruntime
