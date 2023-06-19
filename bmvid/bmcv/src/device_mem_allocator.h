#ifndef DEVICE_MEM_ALLOCATOR_H
#define DEVICE_MEM_ALLOCATOR_H

#include <set>
#include <map>
#include <functional>
#include "bmcv_api_ext.h"

static bool operator < (bm_device_mem_t m0, bm_device_mem_t m1){
    return bm_mem_get_device_addr(m0)<bm_mem_get_device_addr(m1);
}

class DeviceMemAllocator{
public:
    DeviceMemAllocator(bm_handle_t handle):
        _handle(handle) {}

    template<typename T>
    bm_device_mem_t alloc_on_device(size_t elem_size, std::function<size_t (T* ptr, size_t len)> init_func = nullptr){
        bm_device_mem_t mem;
        size_t byte_size = elem_size * sizeof(T);
        auto ret = bm_malloc_device_byte(_handle, &mem, byte_size);
        if(ret != BM_SUCCESS){
            destroy_mem();
            throw ret;
        }
        if(init_func){
            T* ptr = new T[elem_size];
            auto len = init_func(ptr, elem_size);
            auto ret = bm_memcpy_s2d_partial(_handle, mem, ptr, len*sizeof(T));
            delete [] ptr;
            if(ret != BM_SUCCESS){
                destroy_mem();
                throw ret;
            }
        }
        _mem_need_free.insert(mem);
        return mem;
    }

    void dealloc(bm_device_mem_t mem){
        if (bm_mem_get_type(mem) == BM_MEM_TYPE_SYSTEM){
            if(_mem_need_free.count(mem)){
                bm_free_device(_handle, mem);
                _mem_need_free.erase(mem);
            } else {
                destroy_mem();
                throw BM_ERR_FAILURE;
            }
        } else {
            auto ptr = (unsigned char*)bm_mem_get_system_addr(mem);
            if(_ptr_need_free.count(ptr)){
                delete [] ptr;
                _ptr_need_free.erase(ptr);
            } else {
                destroy_mem();
                throw BM_ERR_FAILURE;
            }
        }
    }

    template<typename T>
    bm_device_mem_t alloc_on_system(size_t elem_size, std::function<void (T* ptr, size_t len)> init_func = nullptr){
        size_t byte_size = elem_size*sizeof(T);
        unsigned char* ptr = new unsigned char[byte_size];
        if(ptr == nullptr){
            destroy_mem();
            throw BM_ERR_NOMEM;
        }
        if(init_func){
            init_func((T*)ptr, elem_size);
        }
        _ptr_need_free.insert(ptr);
        bm_device_mem_t mem;
        bm_mem_set_system_addr(&mem, ptr);
        return mem;
    }

    template<typename T>
    bm_device_mem_t map_input_to_device(bm_device_mem_t raw_mem, size_t elem_size){
        if (bm_mem_get_type(raw_mem) == BM_MEM_TYPE_SYSTEM){
            bm_device_mem_t new_mem = alloc_on_device<T>(elem_size);
            auto ret = bm_memcpy_s2d(_handle, new_mem, bm_mem_get_system_addr(raw_mem));
            if(ret != BM_SUCCESS){
                destroy_mem();
                throw ret;
            }
            return new_mem;
        }
        return raw_mem;
    }

    template<typename T>
    bm_device_mem_t map_output_to_device(bm_device_mem_t raw_mem, size_t elem_size){
        if (bm_mem_get_type(raw_mem) == BM_MEM_TYPE_SYSTEM){
            bm_device_mem_t new_mem = alloc_on_device<T>(elem_size);
            _post_copy_map[raw_mem] = new_mem;
            return new_mem;
        }
        return raw_mem;
    }

    ~DeviceMemAllocator(){
        for(auto m: _post_copy_map){
            bm_memcpy_d2s(_handle, bm_mem_get_system_addr(m.first), m.second);
        }
        destroy_mem();
    }

private:
    void destroy_mem(){
        for(auto p: _ptr_need_free){
            delete [] p;
        }
        _ptr_need_free.clear();
        for(auto m: _mem_need_free){
            bm_free_device(_handle, m);
        }
        _mem_need_free.clear();
    }
    bm_handle_t _handle;
    std::set<bm_device_mem_t> _mem_need_free;
    std::set<unsigned char*> _ptr_need_free;
    std::map<bm_device_mem_t, bm_device_mem_t> _post_copy_map;
};

#endif // BMCV_MEM_ALLOCATOR_H
