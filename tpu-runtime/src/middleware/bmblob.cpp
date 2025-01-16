#include <assert.h>
#include "bmblob.h"
#include "bmruntime.h"
#define __HANDLE     ((bm_handle_t)handle_)

using bmruntime::Bmruntime;
using bmruntime::bmrt_arch_info;
using bmruntime::bmfunc;

namespace bmcnn {

BMBlob::BMBlob(const Shape &shape, bm_data_type_t type, void *handle)
    : handle_((bm_handle_t)handle), shape_(shape),
      data_pos_(AIR), sys_data_(NULL), dev_mem_(NULL),st_mode_(0)
{
    capacity_ = 1;
    for(int i=0; i<shape.dims; i++){
        if(shape.data[i]<=0) assert(0);
        capacity_ *= shape.data[i];
    }
    if (handle_ == NULL) assert(0);
    mapped_data_ = NULL;
    data_type_ = type;
    data_type_size_ = bmrt_data_type_size(type);
    capacity_size_ = data_type_size_ * capacity_;
    batch_unit_ = batch_num();
}

BMBlob::~BMBlob()
{
#ifdef SOC_MODE
    if ((dev_mem_ != NULL) && (mapped_data_!= NULL))
        bm_mem_unmap_device_mem(__HANDLE, mapped_data_, capacity_size_);
#endif
    mapped_data_ = NULL;

    if (NULL != dev_mem_) {
        bm_free_device(__HANDLE, *dev_mem_);
        delete dev_mem_;
        dev_mem_ = NULL;
    }
    if (NULL != sys_data_) {
        free(sys_data_);
        sys_data_ = NULL;
    }
}

void BMBlob::Reshape(int n, int c, int h, int w)
{
    int shape_data[]={n,c,h,w};
    Reshape(shape_data, 4);
}

void BMBlob::Reshape(const int *shape_data, int len)
{

    int num_elements=1;
    shape_.dims = len;
    for(int i=0; i<len; i++){
        if(shape_data[i]<=0) assert(0);
        num_elements *= shape_data[i];
        shape_.data[i] = shape_data[i];
    }
    if (capacity_ < num_elements) {
#ifdef SOC_MODE
        if (bmrt_arch_info::is_soc_mode() && (dev_mem_ != NULL) && (mapped_data_!= NULL)) {
            bm_mem_unmap_device_mem(__HANDLE, mapped_data_, capacity_size_);
            mapped_data_ = NULL;
        }
#endif
        if (sys_data_ != NULL) {
            free(sys_data_);
            sys_data_ = NULL;
        }

        if (dev_mem_ != NULL) {
            bm_free_device(__HANDLE, *dev_mem_);
            delete dev_mem_;
            dev_mem_ = NULL;
        }
        // make sure input capacity is N times of origin bmodel input num
        capacity_ = ceiling_func(batch_num(), batch_unit_) * batch_unit_ * batch_length();
        capacity_size_ = capacity_ * data_type_size_;
    }
    /* data is in the air now. */
    data_pos_ = AIR;
}

void BMBlob::Reshape(const Shape &s)
{
   Reshape(s.data, s.dims);
}

void BMBlob::SyncShape(int n, int c, int h, int w)
{
    shape_.dims = 4;
    shape_.n = n;
    shape_.c = c;
    shape_.h = h;
    shape_.w = w;
}

void BMBlob::SyncShape(const Shape &s)
{
    shape_ = s;
}

void BMBlob::SyncShape(const int *shape_data, int len)
{
   memcpy(shape_.data, shape_data, len*sizeof(int));
   shape_.dims = len;
}

const void *BMBlob::cpu_data()
{
    if (data_pos_ == AIR)
        return NULL;
    if (data_pos_ == DEV)
        sync_d2s();
    return sys_data_;
}

void *BMBlob::mutable_cpu_data()
{
    if (sys_data_ == NULL)
        sys_data_ = malloc(capacity_size_);
    data_pos_ = SYS;
    return sys_data_;
}

bm_device_mem_t *BMBlob::mutable_dev_mem()
{
    if (dev_mem_ == NULL) {
        dev_mem_ = new bm_device_mem_t;
        bm_status_t status = bm_malloc_device_byte(__HANDLE, dev_mem_, capacity_size_);
        CHECK_status(status);
    }
    data_pos_ = DEV;
    return dev_mem_;
}

const bm_device_mem_t *BMBlob::dev_mem()
{
    if (data_pos_ == AIR)
        return NULL;
    if (data_pos_ == SYS)
        sync_s2d();
    return dev_mem_;
}

void BMBlob::set_store_mode(int stmode)
{
  if(stmode != st_mode_) {
    st_mode_ = stmode;
    if (sys_data_ != NULL) {
        free(sys_data_);
        sys_data_ = NULL;
    }
    if (dev_mem_ != NULL) {
        bm_free_device(__HANDLE, *dev_mem_);
        delete dev_mem_;
        dev_mem_ = NULL;
    }
    /* data is in the air now. */
    data_pos_ = AIR;
  }
}

void BMBlob::fill_cpu_data(void* in_data, int num_element) {
    if (num_element != this->num_elements()) {
        BMRT_LOG(FATAL, "Unexpected data shape when filling cpu data.");
    }

    /* why not effective memcpy ? no libc ? */
    void* blob_cpu_data_ptr = this->mutable_cpu_data();
    memcpy(blob_cpu_data_ptr, in_data, data_type_size_ * num_element);
}

void BMBlob::dump_cpu_data(void* out_data, int num_element) {
    if(num_element != this->num_elements()) {
        BMRT_LOG(FATAL, "Unexpected data shape when dumping cpu data.");
    }
    const void* blob_cpu_data_ptr = this->cpu_data();
    memcpy(out_data, blob_cpu_data_ptr, data_type_size_ * num_element);
}

/*
   this function is to be used in SOC mode, in which the device memory can be seen by A53,
   the device memory will be reserved and mapped in user space, and can be accessed by cpu directly
   without copy bm_memcpy_d2s
*/
void *BMBlob::mutable_mapped_output()
{
#ifdef SOC_MODE
    bm_device_mem_t *dmem = mutable_dev_mem();
    void *mapped_data = NULL;
    bm_flush(__HANDLE);

    if (mapped_data_ == NULL) {
        if (0 == bm_mem_mmap_device_mem(__HANDLE, dmem, (unsigned long long*)&mapped_data))
            mapped_data_ = mapped_data;
    } else {
        bm_mem_invalidate_device_mem(__HANDLE, dmem);
        mapped_data = mapped_data_;
    }

    return mapped_data;
#else
    return NULL;
#endif
}

void BMBlob::sync_s2d()
{
    if (!(data_pos_ & SYS))
        assert(0);
    if (data_pos_ & DEV)
        return;
    if (dev_mem_ == NULL) {
        dev_mem_ = new bm_device_mem_t;
        bm_status_t status = bm_malloc_device_byte(__HANDLE, dev_mem_, capacity_size_);
        CHECK_status(status);
    }
    bm_memcpy_s2d_partial(__HANDLE, *dev_mem_, sys_data_, data_type_size_ * num_elements());
    data_pos_ |= DEV;
}

void BMBlob::sync_d2s()
{
    if (!(data_pos_ & DEV))
        assert(0);
    if (data_pos_ & SYS)
        return;
    if (sys_data_ == NULL)
        sys_data_ = malloc(capacity_size_);

    bm_memcpy_d2s_partial(__HANDLE, sys_data_, *dev_mem_, data_type_size_ * num_elements());
    data_pos_ |= SYS;
}

} /* namespace bmcnn */
