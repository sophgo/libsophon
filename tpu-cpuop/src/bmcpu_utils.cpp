#include "bmcpu_utils.hpp"
namespace bmcpu {
int using_thread_num(int N){
  int nthreads = 1;
  char *nt = getenv("BM_CPU_LAYER_NUM_THREAD");
  if (nt != nullptr) nthreads= atoi(nt);
  if(N<nthreads){
    nthreads = N;
  }
  return nthreads;
}

int cpu_get_type_len(CPU_DATA_TYPE_T dtype) {
    if(dtype == CPU_DTYPE_FP32 || dtype == CPU_DTYPE_UINT32 || dtype == CPU_DTYPE_INT32){
        return 4;
    } else if(dtype == CPU_DTYPE_FP16 || dtype == CPU_DTYPE_BFP16 || dtype == CPU_DTYPE_INT16 || dtype == CPU_DTYPE_UINT16) {
        return 2;
    } else if(dtype == CPU_DTYPE_INT8 || dtype == CPU_DTYPE_UINT8) {
        return 1;
    }
    return 0;
}

}
