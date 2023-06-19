#include "cpu_layer.h"
#include "bmcpu.h"
#ifdef __linux__
#include <dlfcn.h>
#else
#include <windows.h>
#endif
//#define BMCPU_LAYER_DEBUG

#ifdef BMCPU_LAYER_DEBUG
#include "cpu_utils.h"
#endif

using bmcpu::cpu_layer;
using bmcpu::CpuLayerRegistry;

typedef void* (*t_bmcpu_user_init)();
typedef void (*t_bmcpu_user_uninit)(void *);
typedef int (*t_bmcpu_user_process)(void *, void *,
                                const vector<float *>&, const vector<vector<int>>&,
                                const vector<float *>&, vector<vector<int>>&);
typedef int (*t_bmcpu_user_reshape)(void *, void *,
                                    const vector<vector<int>>&,
                                    vector<vector<int>>&);
typedef int (*t_bmcpu_user_dtype)(void *, void *,
                                  const vector<int>&,
                                  vector<int>&);

typedef struct {
  std::map<int, std::shared_ptr<cpu_layer>> cpu_layers_;

  void *user_cpu_handle;
  t_bmcpu_user_init    bmcpu_user_init_;
  t_bmcpu_user_uninit  bmcpu_user_uninit_;
  t_bmcpu_user_process bmcpu_user_process_;
  t_bmcpu_user_reshape bmcpu_user_reshape_;
  t_bmcpu_user_dtype   bmcpu_user_dtype_;
} bmcpu_handle_t;

/* instance all cpu layer */
void* bmcpu_init()
{
    bmcpu_handle_t *bmcpu_handle_ = new bmcpu_handle_t();
    auto& cpu_layers_ = bmcpu_handle_->cpu_layers_;

    cpu_layers_.clear();
    for (int layer_idx = 0; layer_idx < CPU_LAYER_NUM; layer_idx++) {

        if (layer_idx != CPU_USER_DEFINED) {
            cpu_layers_[layer_idx] = CpuLayerRegistry::createlayer(layer_idx);
        } else {
            printf("bmcpu init: skip cpu_user_defined\n");
        }
    }
    cpu_layers_[CPU_DEBUG] = CpuLayerRegistry::createlayer(CPU_DEBUG);

    #ifdef __linux__
    t_bmcpu_user_init    bmcpu_user_init_      =  (t_bmcpu_user_init)dlsym(NULL,    "user_cpu_init");
    t_bmcpu_user_uninit  bmcpu_user_uninit_    =  (t_bmcpu_user_uninit)dlsym(NULL,  "user_cpu_uninit");
    t_bmcpu_user_process bmcpu_user_process_   =  (t_bmcpu_user_process)dlsym(NULL, "user_cpu_process");
    t_bmcpu_user_reshape bmcpu_user_reshape_   =  (t_bmcpu_user_reshape)dlsym(NULL, "user_cpu_reshape");
    t_bmcpu_user_dtype   bmcpu_user_dtype_     =  (t_bmcpu_user_dtype)dlsym(NULL,   "user_cpu_dtype");
    if (bmcpu_user_init_ == NULL) {
        void *user_so_handle_ = dlopen("libusercpu.so", RTLD_LAZY);
        if(user_so_handle_) {
          bmcpu_user_init_    =  (t_bmcpu_user_init)dlsym(user_so_handle_,    "user_cpu_init");
          bmcpu_user_uninit_  =  (t_bmcpu_user_uninit)dlsym(user_so_handle_,  "user_cpu_uninit");
          bmcpu_user_process_ =  (t_bmcpu_user_process)dlsym(user_so_handle_, "user_cpu_process");
          bmcpu_user_reshape_ =  (t_bmcpu_user_reshape)dlsym(user_so_handle_, "user_cpu_reshape");
          bmcpu_user_dtype_   =  (t_bmcpu_user_dtype)dlsym(user_so_handle_,   "user_cpu_dtype");
    #else
    t_bmcpu_user_init    bmcpu_user_init_      =  (t_bmcpu_user_init)GetProcAddress(NULL,    "user_cpu_init");
    t_bmcpu_user_uninit  bmcpu_user_uninit_    =  (t_bmcpu_user_uninit)GetProcAddress(NULL,  "user_cpu_uninit");
    t_bmcpu_user_process bmcpu_user_process_   =  (t_bmcpu_user_process)GetProcAddress(NULL, "user_cpu_process");
    t_bmcpu_user_reshape bmcpu_user_reshape_   =  (t_bmcpu_user_reshape)GetProcAddress(NULL, "user_cpu_reshape");
    t_bmcpu_user_dtype   bmcpu_user_dtype_     =  (t_bmcpu_user_dtype)GetProcAddress(NULL,   "user_cpu_dtype");
    if (bmcpu_user_init_ == NULL) {
        auto user_so_handle_ = LoadLibrary("libusercpu.dll");
        if(user_so_handle_) {
          bmcpu_user_init_    =  (t_bmcpu_user_init)GetProcAddress(user_so_handle_,    "user_cpu_init");
          bmcpu_user_uninit_  =  (t_bmcpu_user_uninit)GetProcAddress(user_so_handle_,  "user_cpu_uninit");
          bmcpu_user_process_ =  (t_bmcpu_user_process)GetProcAddress(user_so_handle_, "user_cpu_process");
          bmcpu_user_reshape_ =  (t_bmcpu_user_reshape)GetProcAddress(user_so_handle_, "user_cpu_reshape");
          bmcpu_user_dtype_   =  (t_bmcpu_user_dtype)GetProcAddress(user_so_handle_,   "user_cpu_dtype");
    #endif
          CPU_ASSERT(bmcpu_user_init_ != NULL);
          CPU_ASSERT(bmcpu_user_uninit_ != NULL);
          CPU_ASSERT(bmcpu_user_process_ != NULL);
          CPU_ASSERT(bmcpu_user_reshape_ != NULL);
          CPU_ASSERT(bmcpu_user_dtype_ != NULL);
        } else {
          #ifdef __linux__
          printf("Cannot open libusercpu.so, disable user cpu layer.\n");
          #else
          printf("Cannot open libusercpu.dll, disable user cpu layer.\n");
          #endif
        }
    } else {
        #ifdef __linux__
        cout << "libusercpu.so already exist, user cpu layer is enable" << endl;
        #else
        cout << "libusercpu.dll already exist, user cpu layer is enable" << endl;
        #endif
    }

    if (bmcpu_user_init_ != NULL) {
        bmcpu_handle_->bmcpu_user_init_     = bmcpu_user_init_;
        bmcpu_handle_->bmcpu_user_uninit_   = bmcpu_user_uninit_;
        bmcpu_handle_->bmcpu_user_process_  = bmcpu_user_process_;
        bmcpu_handle_->bmcpu_user_reshape_  = bmcpu_user_reshape_;
        bmcpu_handle_->bmcpu_user_dtype_    = bmcpu_user_dtype_;
        bmcpu_handle_->user_cpu_handle      = bmcpu_user_init_();
        #ifdef __linux__
        printf("open usercpu.so, init user_cpu_init \n");
        #else
        printf("open usercpu.dll, init user_cpu_init \n");
        #endif
    }

    return (void *)bmcpu_handle_;
}

void bmcpu_uninit(void* bmcpu_handle)
{
    bmcpu_handle_t * bmcpu_handle_ = (bmcpu_handle_t *)bmcpu_handle;
    t_bmcpu_user_uninit bmcpu_user_uninit_    =  bmcpu_handle_->bmcpu_user_uninit_;

    if (bmcpu_user_uninit_ != NULL) {
        bmcpu_user_uninit_(bmcpu_handle_->user_cpu_handle);
    }

    delete bmcpu_handle_;
}

/* call op_type cpu layer's process handle.
 * return 0 if sucesse, else return -1. 
 * separate user defined layer
 * open libuser
 */
int bmcpu_process(void* bmcpu_handle, int op_type,
                  void *param, int param_size,
                  const vector<float *>& input_tensors,
                  const vector<vector<int>>& input_shapes,
                  const vector<float *>& output_tensors,
                  vector<vector<int>>& output_shapes
                  )
{
#ifdef BMCPU_LAYER_DEBUG
    auto in_dtypes = get_input_dtype_vector((CPU_LAYER_TYPE_T)op_type);
    static int cpu_layer_count = 0;
    for(size_t i = 0; i<input_tensors.size(); i++){
        string filename(get_cpu_layer_name((CPU_LAYER_TYPE_T)op_type));
        filename += "_in"+ std::to_string(i)+".log";
        string index_str = "00000" + std::to_string(cpu_layer_count);
        index_str = index_str.substr(index_str.length()-5);
        filename = index_str+"_"+filename;
        auto dtype = in_dtypes.size()>i?in_dtypes[i]: in_dtypes[0];
        cpu_print_tensor_ex(input_tensors[i], input_shapes[i].data(), input_shapes[i].size(), dtype, filename.c_str());
    }
#endif
    bmcpu_handle_t * bmcpu_handle_ = (bmcpu_handle_t *)bmcpu_handle;
    int result_code = -1;
    if (op_type == (int)CPU_USER_DEFINED) {

        t_bmcpu_user_process bmcpu_user_process_ = bmcpu_handle_->bmcpu_user_process_;
        if (bmcpu_user_process_ != NULL) {
            printf("process: user define cpu layer!!\n");
            result_code = bmcpu_user_process_(bmcpu_handle_->user_cpu_handle, param,
                                       input_tensors, input_shapes, output_tensors, output_shapes);
        } else {
            cout << "do not support usercpu" << endl;
            result_code = -1;
        }

    } else {
        auto& cpu_layers_ = bmcpu_handle_->cpu_layers_;
        map<int, std::shared_ptr<cpu_layer> >::iterator iter = cpu_layers_.find(op_type);
        if (iter != cpu_layers_.end()) {
            iter->second->set_common_param(input_tensors,  input_shapes,
                                       output_tensors, output_shapes);
            result_code = iter->second->process(param, param_size);
        } else {
            cout << "unknown cpu_layer" << endl;
            result_code = -1;
        }
    }
#ifdef BMCPU_LAYER_DEBUG
    if (result_code!=-1){
        auto out_dtypes = get_output_dtype_vector((CPU_LAYER_TYPE_T)op_type);
        for(size_t i = 0; i<output_tensors.size(); i++){
            string filename(get_cpu_layer_name((CPU_LAYER_TYPE_T)op_type));
            filename += "_out"+ std::to_string(i)+".log";
            string index_str = "00000" + std::to_string(cpu_layer_count);
            index_str = index_str.substr(index_str.length()-5);
            filename = index_str+"_"+filename;
            auto dtype = out_dtypes.size()>i?out_dtypes[i]: out_dtypes[0];
            cpu_print_tensor_ex(output_tensors[i], output_shapes[i].data(), output_shapes[i].size(), dtype, filename.c_str());
        }
    }
    cpu_layer_count ++;
#endif
    return result_code;
}


int bmcpu_user_process(void *bmcpu_handle, void *param,
                  const vector<float *>& input_tensors,
                  const vector<vector<int>>& input_shapes,
                  const vector<float *>& output_tensors,
                  vector<vector<int>>& output_shapes
                  )
{
    return bmcpu_process(bmcpu_handle, CPU_USER_DEFINED, param, 0, input_tensors, input_shapes, output_tensors, output_shapes);
}

/* call op_type cpu layer's reshape handle.
 * return 0 if sucesse, else return -1. 
 */
int bmcpu_reshape(void* bmcpu_handle, int op_type,
                  void *param, int param_size,
                  const vector<vector<int>>& input_shapes,
                  vector<vector<int>>& output_shapes
                  )
{
    bmcpu_handle_t * bmcpu_handle_ = (bmcpu_handle_t *)bmcpu_handle;
    if (op_type == (int)CPU_USER_DEFINED) {

        t_bmcpu_user_reshape bmcpu_user_reshape_ = bmcpu_handle_->bmcpu_user_reshape_;
        if (bmcpu_user_reshape_ != NULL) {
            printf("reshape: user define cpu layer!!\n");
            return bmcpu_user_reshape_(bmcpu_handle_->user_cpu_handle, param,
                                       input_shapes, output_shapes);
        } else {
            cout << "do not support usercpu reshape" << endl;
            return -1;
        }

    } else {
        auto& cpu_layers_ = bmcpu_handle_->cpu_layers_;
        map<int, std::shared_ptr<cpu_layer> >::iterator iter = cpu_layers_.find(op_type);
        if (iter != cpu_layers_.end()) {
            return iter->second->reshape(param, param_size, input_shapes, output_shapes);
        }
    }

    return -1;
}

int bmcpu_user_reshape(void* bmcpu_handle, void *param,
                       const vector<vector<int>>& input_shapes,
                       vector<vector<int>>& output_shapes
                       )
{
    return bmcpu_reshape(bmcpu_handle, CPU_USER_DEFINED, param, 0, input_shapes, output_shapes);
}

int  bmcpu_dtype(void* bmcpu_handle, int op_type, const void *param, size_t param_size,
                 const vector<int> &input_dtypes,
                 vector<int> &output_dtypes) {
    bmcpu_handle_t * bmcpu_handle_ = (bmcpu_handle_t *)bmcpu_handle;
    if (op_type == (int)CPU_USER_DEFINED) {

        t_bmcpu_user_dtype bmcpu_user_dtype_ = bmcpu_handle_->bmcpu_user_dtype_;
        if (bmcpu_user_dtype_ != NULL) {
            printf("dtype: user define cpu layer!!\n");
            return bmcpu_user_dtype_(bmcpu_handle_->user_cpu_handle, const_cast<void *>(param),
                                     input_dtypes, output_dtypes);
        } else {
            cout << "do not support usercpu dtype" << endl;
            return -1;
        }

    } else {
        auto& cpu_layers_ = bmcpu_handle_->cpu_layers_;
        map<int, std::shared_ptr<cpu_layer> >::iterator iter = cpu_layers_.find(op_type);
        if (iter != cpu_layers_.end()) {
            return iter->second->dtype(param, param_size, input_dtypes, output_dtypes);
        }
    }

    return -1;
}
int  bmcpu_user_dtype(void* bmcpu_handle, void *param,
                      const vector<int> &input_dtypes,
                      vector<int> &output_dtypes) {
    return bmcpu_dtype(bmcpu_handle, CPU_USER_DEFINED, param, 0, input_dtypes, output_dtypes);
}

