#include "bmfunc/bmfunc.h"

namespace bmruntime {

bmfunc* bmfunc::sta_bmfunc_ptr;

bmfunc::bmfunc(const string& arch_name)
{
  sta_bmfunc_ptr = this;

  bmdnn_1684_fn = NULL;
  bmdnn_1682_fn = NULL;
  bmdnn_1880_fn = NULL;
  bmdnn_1684x_fn = NULL;
  bmdnn_1686_fn = NULL;
  bmdnn_fn = NULL;

  p_bmtpu_arch = new bmrt_arch_info(arch_name);

  bmtpu_arch_t arch = bmrt_arch_info::get_bmtpu_arch();
  if(arch == BM1684) {
    bmdnn_1684_fn = new bmdnn_func_1684();
    bmdnn_fn = bmdnn_1684_fn;
  } else if(arch == BM1880) {
    bmdnn_1880_fn = new bmdnn_func_1880();
    bmdnn_fn = bmdnn_1880_fn;
  } else if(arch == BM1682) {
    bmdnn_1682_fn = new bmdnn_func_1682();
    bmdnn_fn = bmdnn_1682_fn;
  } else if(arch == BM1684X) {
    bmdnn_1684x_fn = new bmdnn_func_1684x();
    bmdnn_fn = bmdnn_1684x_fn;
  } else if(arch == BM1686) {
    bmdnn_1686_fn = new bmdnn_func_1686();
    bmdnn_fn = bmdnn_1686_fn;
  } else {
    BMRT_LOG(FATAL, "Error: unkown architecture [%d]",  arch);
  }
}

bmfunc::~bmfunc()
{
  if(bmdnn_1684_fn != NULL) {
    delete bmdnn_1684_fn;
  }

  if(bmdnn_1880_fn != NULL) {
    delete bmdnn_1880_fn;
  }

  if(bmdnn_1682_fn != NULL) {
    delete bmdnn_1682_fn;
  }

  if(bmdnn_1684x_fn != NULL) {
    delete bmdnn_1684x_fn;
  }

  if(bmdnn_1686_fn != NULL) {
    delete bmdnn_1686_fn;
  }
  delete p_bmtpu_arch;
}

}
