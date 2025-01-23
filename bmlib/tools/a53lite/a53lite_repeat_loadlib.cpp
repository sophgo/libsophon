#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <setjmp.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "bmlib_memory.h"
#include "api.h"
#include <sys/mman.h>
#include <stdint.h>

#ifdef __linux__
#define MAX_CHIP_NUM 256

char g_library_file[100];

#define A53LITE_RUNTIME_LOG_TAG "a53lite_runtime"

jmp_buf jmp_env;


typedef enum {
	 DT_INT8  = (0 << 1) | 1,
	 DT_UINT8 = (0 << 1) | 0,
	 DT_INT16 = (3 << 1) | 1,
	 DT_UINT16 = (3 << 1) | 0,
	 DT_FP16  = (1 << 1) | 1,
	 DT_BFP16 = (5 << 1) | 1,
	 DT_INT32 = (4 << 1) | 1,
	 DT_UINT32 = (4 << 1) | 0,
	 DT_FP32  = (2 << 1) | 1,
	 DT_INT4  = (6 << 1) | 1,
	 DT_UINT4 = (6 << 1) | 0,
	 DT_FP8E5M2 = (0 << 5) | (7 << 1) | 1,
	 DT_FP8E4M3 = (1 << 5) | (7 << 1) | 1,
	 DT_FP20  = (8 << 1) | 1,
	 DT_TF32  = (9 << 1) | 1,
} data_type_t;

typedef struct sg_api_1d_memcpy {
  unsigned long long src_global_offset;
  unsigned long long dst_global_offset;
  int w_bytes;
  int stride_bytes_src;
  int stride_bytes_dst;
  data_type_t data_type;
} __attribute__((packed)) sg_api_1d_memcpy_t;

size_t virtual_to_physical(size_t addr)
{
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if(fd < 0)
    {
        printf("open '/proc/self/pagemap' failed!\n");
        return 0;
    }
    size_t pagesize = getpagesize();
    size_t offset = (addr / pagesize) * sizeof(uint64_t);
    if(lseek(fd, offset, SEEK_SET) < 0)
    {
        printf("lseek() failed!\n");
        close(fd);
        return 0;
    }
    uint64_t info;
    if(read(fd, &info, sizeof(uint64_t)) != sizeof(uint64_t))
    {
        printf("read() failed!\n");
        close(fd);
        return 0;
    }
    if((info & (((uint64_t)1) << 63)) == 0)
    {
        printf("page is not present!\n");
        close(fd);
        return 0;
    }
    size_t frame = info & ((((uint64_t)1) << 55) - 1);
    size_t phy = frame * pagesize + addr % pagesize;
    close(fd);
    return phy;
}

typedef struct {
    bm_handle_t handle;
    bm_device_mem_t* mem_need_free;
    size_t mem_need_free_size;
    unsigned char** ptr_need_free;
    size_t ptr_need_free_size;
    struct {
        bm_device_mem_t* keys;
        bm_device_mem_t* values;
        size_t size;
    } post_copy_map;
} DeviceMemAllocator;

void init_allocator(DeviceMemAllocator* allocator, bm_handle_t handle) {
    allocator->handle = handle;
    allocator->mem_need_free = NULL;
    allocator->mem_need_free_size = 0;
    allocator->ptr_need_free = NULL;
    allocator->ptr_need_free_size = 0;
    allocator->post_copy_map.keys = NULL;
    allocator->post_copy_map.values = NULL;
    allocator->post_copy_map.size = 0;
}

void destroy_mem(DeviceMemAllocator* allocator) {
    for (size_t i = 0; i < allocator->ptr_need_free_size; ++i) {
        free(allocator->ptr_need_free[i]);
    }
    free(allocator->ptr_need_free);
    allocator->ptr_need_free = NULL;
    allocator->ptr_need_free_size = 0;

    for (size_t i = 0; i < allocator->mem_need_free_size; ++i) {
        bm_free_device(allocator->handle, allocator->mem_need_free[i]);
    }
    free(allocator->mem_need_free);
    allocator->mem_need_free = NULL;
    allocator->mem_need_free_size = 0;

    free(allocator->post_copy_map.keys);
    free(allocator->post_copy_map.values);
    allocator->post_copy_map.keys = NULL;
    allocator->post_copy_map.values = NULL;
    allocator->post_copy_map.size = 0;
}

bm_device_mem_t alloc_on_device(DeviceMemAllocator* allocator, size_t elem_size, void (*init_func)(float*, size_t)) {
    bm_device_mem_t mem;
    size_t byte_size = elem_size;
    int ret = bm_malloc_device_byte(allocator->handle, &mem, byte_size);
    if (ret != BM_SUCCESS) {
        destroy_mem(allocator);
        longjmp(jmp_env, ret);
    }
    if (init_func) {
        float* ptr = (float*)malloc(byte_size);
        if (!ptr) {
            destroy_mem(allocator);
            longjmp(jmp_env, BM_ERR_NOMEM);
        }
        init_func(ptr,1);
        // ret = bm_memcpy_s2d_partial(allocator->handle, mem, ptr, byte_size);
        free(ptr);
        if (ret != BM_SUCCESS) {
            destroy_mem(allocator);
            longjmp(jmp_env, ret);
        }
    }
    allocator->mem_need_free = (bm_device_mem_t*)realloc(allocator->mem_need_free, (allocator->mem_need_free_size + 1) * sizeof(bm_device_mem_t));
    allocator->mem_need_free[allocator->mem_need_free_size++] = mem;
    return mem;
}

void dealloc(DeviceMemAllocator* allocator, const bm_device_mem_t* mem) {
    if (bm_mem_get_type(*mem) == BM_MEM_TYPE_SYSTEM) {
        for (size_t i = 0; i < allocator->mem_need_free_size; ++i) {
            if (allocator->mem_need_free[i].u.device.device_addr == mem->u.device.device_addr) {
                bm_free_device(allocator->handle, *mem);
                allocator->mem_need_free[i] = allocator->mem_need_free[--allocator->mem_need_free_size];
                return;
            }
        }
        destroy_mem(allocator);
        longjmp(jmp_env, BM_ERR_FAILURE);
    } else {
        unsigned char* ptr = (unsigned char*)bm_mem_get_system_addr(*mem);
        for (size_t i = 0; i < allocator->ptr_need_free_size; ++i) {
            if (allocator->ptr_need_free[i] == ptr) {
                free(ptr);
                allocator->ptr_need_free[i] = allocator->ptr_need_free[--allocator->ptr_need_free_size];
                return;
            }
        }
        destroy_mem(allocator);
        longjmp(jmp_env, BM_ERR_FAILURE);
    }
}

bm_device_mem_t alloc_on_system(DeviceMemAllocator* allocator, size_t elem_size, size_t elem_count, void (*init_func)(float*, size_t)) {
    size_t byte_size = elem_size * elem_count;
    unsigned char* ptr = (unsigned char*)malloc(byte_size);
    if (!ptr) {
        destroy_mem(allocator);
        longjmp(jmp_env, BM_ERR_NOMEM);
    }
    if (init_func) {
        init_func((float*)ptr, elem_count);
    }
    allocator->ptr_need_free = (unsigned char**)realloc(allocator->ptr_need_free, (allocator->ptr_need_free_size + 1) * sizeof(unsigned char*));
    allocator->ptr_need_free[allocator->ptr_need_free_size++] = ptr;
    bm_device_mem_t mem;
    bm_mem_set_system_addr(&mem, ptr);
    return mem;
}

bm_device_mem_t map_input_to_device(DeviceMemAllocator* allocator, const bm_device_mem_t* raw_mem, size_t elem_size) {
    if (bm_mem_get_type(*raw_mem) == BM_MEM_TYPE_SYSTEM) {
        bm_device_mem_t new_mem = alloc_on_device(allocator, elem_size, NULL);
				#ifndef USING_CMODEL
				bm_memcpy_s2d(allocator->handle, new_mem, bm_mem_get_system_addr(*raw_mem));
				// int ret=bm_mem_write_data_to_ion(allocator->handle, &new_mem, bm_mem_get_system_addr(*raw_mem), elem_size* sizeof(uint32_t));
				// // int ret=bm_vAddr_to_pAddr(allocator->handle, (void *)bm_mem_get_system_addr(*raw_mem),&new_mem.u.device.device_addr);
				// if (ret != BM_SUCCESS) {
				// 	destroy_mem(allocator);
        //     longjmp(jmp_env, ret);
        // }
				// else{
				// 	printf( "bm_mem_write_data_to_ion success! %lu\n",new_mem.u.device.device_addr);
				// } 

				uint32_t *read_buffer = (uint32_t *)malloc(elem_size);
				if (read_buffer == NULL) {
						perror("malloc failed11");
				}


				bm_status_t read_status = bm_mem_read_data_from_ion(allocator->handle, &new_mem, read_buffer, elem_size);
				if (read_status != BM_SUCCESS) {
						fprintf(stderr, "Failed to read data from device memory, error code: %d\n", read_status);
				} else {
						for (size_t i = 0; i < elem_size; i++) {
								if (read_buffer[i] != 1) {
										fprintf(stderr, "Data verification failed at index %zu: expected 1, got %u\n", i, read_buffer[i]);
										break;
								}
						}
						printf("Data verification succeeded.\n");
				}
				#else
				int ret = bm_memcpy_s2d(allocator->handle, new_mem, bm_mem_get_system_addr(*raw_mem));
        if (ret != BM_SUCCESS) {
            destroy_mem(allocator);
            longjmp(jmp_env, ret);
        }
        return new_mem;
				#endif
				
				return new_mem;
    }
    return *raw_mem;
}

bm_device_mem_t map_output_to_device(DeviceMemAllocator* allocator, const bm_device_mem_t* raw_mem, size_t elem_size, int is_inplace) {
    if (bm_mem_get_type(*raw_mem) == BM_MEM_TYPE_SYSTEM) {
        bm_device_mem_t new_mem = alloc_on_device(allocator, elem_size, NULL);
        allocator->post_copy_map.keys = (bm_device_mem_t*)realloc(allocator->post_copy_map.keys, (allocator->post_copy_map.size + 1) * sizeof(bm_device_mem_t));
        allocator->post_copy_map.values = (bm_device_mem_t*)realloc(allocator->post_copy_map.values, (allocator->post_copy_map.size + 1) * sizeof(bm_device_mem_t));
        allocator->post_copy_map.keys[allocator->post_copy_map.size] = *raw_mem;
        allocator->post_copy_map.values[allocator->post_copy_map.size++] = new_mem;
        if (is_inplace) {
            int ret = bm_memcpy_s2d(allocator->handle, new_mem, bm_mem_get_system_addr(*raw_mem));
            if (ret != BM_SUCCESS) {
                destroy_mem(allocator);
                longjmp(jmp_env, ret);
            }
        }
        return new_mem;
    }
    return *raw_mem;
}

unsigned long long map_input_to_device_addr(DeviceMemAllocator* allocator, void* raw_ptr, size_t elem_size) {
    bm_device_mem_t raw_mem = bm_mem_from_system(raw_ptr);
    bm_device_mem_t mem = map_input_to_device(allocator, &raw_mem, elem_size);
    return bm_mem_get_device_addr(mem);
}

unsigned long long map_output_to_device_addr(DeviceMemAllocator* allocator, const bm_device_mem_t* raw_mem, size_t elem_size, int is_inplace) {
    bm_device_mem_t mem = map_output_to_device(allocator, raw_mem, elem_size, is_inplace);
    return bm_mem_get_device_addr(mem);
}

unsigned long long map_buffer_to_device_addr(DeviceMemAllocator* allocator, size_t buffer_size) {
    if (buffer_size == 0) return (unsigned long long)-1;
    bm_device_mem_t mem = alloc_on_device(allocator, buffer_size, NULL);
    return bm_mem_get_device_addr(mem);
}

void flush_output(DeviceMemAllocator* allocator) {
    for (size_t i = 0; i < allocator->post_copy_map.size; ++i) {
        bm_memcpy_d2s(allocator->handle, bm_mem_get_system_addr(allocator->post_copy_map.keys[i]), allocator->post_copy_map.values[i]);
    }
    free(allocator->post_copy_map.keys);
    free(allocator->post_copy_map.values);
    allocator->post_copy_map.keys = NULL;
    allocator->post_copy_map.values = NULL;
    allocator->post_copy_map.size = 0;
}

void destroy_allocator(DeviceMemAllocator* allocator) {
    // flush_output(allocator);
    destroy_mem(allocator);
}

void init_input(uint32_t* ptr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ptr[i] = 1.0f;
    }
}

#define BM_ERR_PARAM -1
#define BMLIB_MEMORY_LOG_TAG "MemoryLog"


void bmlib_log(const char *tag, const char *level, const char *msg) {
    printf("[%s] %s: %s\n", tag, level, msg);
}

typedef enum {
    BM_DTYPE_INT,
    BM_DTYPE_FLOAT,
    BM_DTYPE_UINT32,
} bm_dtype_t;

int bm_mem_write_data_to_sys_ion(
	bm_handle_t      handle,
	bm_device_mem_t *dmem,
	bm_dtype_t       dtype,
	size_t           size,
	const char*      dump_filename) {

	void *mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, dmem->u.device.dmabuf_fd, 0);
	if (mapped_memory == MAP_FAILED) {
			bmlib_log(BMLIB_MEMORY_LOG_TAG, "ERROR", "mmap failed");
			return BM_ERR_PARAM;
	}

	FILE *file = fopen(dump_filename, "w");
	if (!file) {
			bmlib_log(BMLIB_MEMORY_LOG_TAG, "ERROR", "Failed to open file for dumping data");
			munmap(mapped_memory, size);
			return BM_ERR_PARAM;
	}

	size_t count = size;

	size_t print_count = count < 20 ? count : 20;

	switch (dtype) {
			case BM_DTYPE_INT: {
					int *data = (int *)mapped_memory;
					for (size_t i = 0; i < print_count; i++) {
							printf("Data[%zu]: %d\n", i, data[i]);
					}

					for (size_t i = 0; i < count; i++) {
							fprintf(file, "Data[%zu]: %d\n", i, data[i]);
					}
					break;
			}
			case BM_DTYPE_FLOAT: {
					float *data = (float *)mapped_memory;
					for (size_t i = 0; i < print_count; i++) {
							printf("Data[%zu]: %f\n", i, data[i]);
					}

					for (size_t i = 0; i < count; i++) {
							fprintf(file, "Data[%zu]: %f\n", i, data[i]);
					}
					break;
			}
			case BM_DTYPE_UINT32: {
					uint32_t *data = (uint32_t *)mapped_memory;
					for (size_t i = 0; i < print_count; i++) {
							printf("Data[%zu]: %u\n", i, data[i]);
					}

					for (size_t i = 0; i < count; i++) {
							fprintf(file, "Data[%zu]: %u\n", i, data[i]);
					}
					break;
			}
			default:
					bmlib_log(BMLIB_MEMORY_LOG_TAG, "ERROR", "Unsupported data type");
					fclose(file);
					munmap(mapped_memory, size);
					return BM_ERR_PARAM;
	}


	fclose(file);
	munmap(mapped_memory, size);
	return 0;
}

typedef struct sg_api_memcpy {
    unsigned long long src_global_offset;
    unsigned long long dst_global_offset;
    int input_n;
    int src_nstride;
    int dst_nstride;
    int count;
} __attribute__((packed)) sg_api_memcpy_t;

int main(int argc, char *argv[]) {
    int dev_num;
    int rel_num;
    int arg[MAX_CHIP_NUM];
    tpu_kernel_module_t bm_module;
    bm_handle_t handle;
    DeviceMemAllocator allocator;
    tpu_kernel_function_t f_id;
    bm_status_t ret = BM_SUCCESS;

    int i = 0;
    if (argc != 3) {
        printf("please input param just like: a53lite_load_lib 0 test.so\n");
        return -1;
    }

    if (BM_SUCCESS != bm_dev_getcount(&dev_num)) {
        printf("no sophon device found!\n");
        return -1;
    }

    if (dev_num > MAX_CHIP_NUM) {
        printf("too many device number %d!\n", dev_num);
        return -1;
    }

    rel_num = atoi(argv[1]);
    ret = bm_dev_request(&handle, rel_num);
    if ((ret != BM_SUCCESS) || (handle == NULL)) {
        printf("bm_dev_request error, ret = %d\r\n", ret);
        return BM_ERR_FAILURE;
    }
		init_allocator(&allocator, handle);


		bm_module = tpu_kernel_load_module_file(handle, argv[2]);
		if (bm_module == NULL) {
			printf("tpu_kernel_load_module_file failed, bm_module is null!\n");
			return BM_ERR_FAILURE;
		}else{
			printf("********************tpu_kernel_load_module_file success!\n");
		}


		f_id = tpu_kernel_get_function(handle, bm_module, "sg_api_1d_memcpy");
		if (f_id==-1)
		{
			printf("tpu_kernel_get_function failed...");
			bm_dev_free(handle);
			return BM_ERR_FAILURE;
		}else{
			printf("********************tpu_kernel_get_function success! f_id=%d\n", f_id);
		}


		// sg_api_memcpy_t api_mem_param;
    // api_mem_param.count = 100;
    // size_t shape_cnt = api_mem_param.count;
    // size_t sg_dtype_len = sizeof(uint32_t);
    // uint32_t* input = (uint32_t*)malloc(shape_cnt * sg_dtype_len);
    // if (!input) {
    //     fprintf(stderr, "Failed to allocate memory for input\n");
		// 		bm_dev_free(handle);
    //     return 1;
    // }
    // init_input(input, shape_cnt);
    // uint32_t* output = (uint32_t*)malloc(shape_cnt * sg_dtype_len);
    // if (!output) {
    //     fprintf(stderr, "Failed to allocate memory for output\n");
    //     free(input);
		// 		bm_dev_free(handle);
    //     return 1;
    // }

    // bm_device_mem_t input_mem = bm_mem_from_system(input);
    // bm_device_mem_t input_device_mem = map_input_to_device(&allocator, &input_mem, shape_cnt);
    // bm_device_mem_t output_mem = bm_mem_from_system(output);
    // bm_device_mem_t output_device_mem = map_output_to_device(&allocator, &output_mem, shape_cnt, 0);
    // api_mem_param.src_global_offset = bm_mem_get_device_addr(input_device_mem);
    // api_mem_param.dst_global_offset = bm_mem_get_device_addr(output_device_mem);

    // int status = bm_mem_write_data_to_sys_ion(allocator.handle, &output_device_mem, dtype, size, "output.dat");


		// api_mem_param.src_global_offset=virtual_to_physical((size_t)input);
		// api_mem_param.dst_global_offset=virtual_to_physical((size_t)output);
		// printf( "src_global_offset:%llu, dst_global_offset:%llu\n",api_mem_param.src_global_offset,api_mem_param.dst_global_offset);

		// ret=bm_vAddr_to_pAddr(handle, (void *)input,&api_mem_param.src_global_offset);
		// if (ret != BM_SUCCESS) {
		// 	printf( "bm_vAddr_to_pAddr failed!\n");
		// 	bm_dev_free(handle);
		// 	return BM_ERR_FAILURE;
		// }else{
		// 	printf( "bm_vAddr_to_pAddr success! %llu\n",api_mem_param.src_global_offset);
		// }
		// ret=bm_vAddr_to_pAddr(handle, (void *)output,&api_mem_param.dst_global_offset);
		// if (ret != BM_SUCCESS) {
		// 	printf( "bm_vAddr_to_pAddr failed!\n");
		// 	bm_dev_free(handle);
		// 	return BM_ERR_FAILURE;
		// }else{
		// 	printf( "bm_vAddr_to_pAddr success! %llu\n",api_mem_param.src_global_offset);
		// }

    size_t shape_cnt=100;
    sg_api_1d_memcpy api_mem_param;
    api_mem_param.data_type = DT_INT8;
    api_mem_param.w_bytes = shape_cnt;
    size_t sg_dtype_len = sizeof(uint8_t);

    // init src data
    uint8_t *input = (uint8_t *)malloc(shape_cnt * sg_dtype_len);
    if (!input) {
      fprintf(stderr, "Failed to allocate memory for input\n");
      return BM_ERR_FAILURE;
    }
    // init src data value
    for (size_t i = 0; i < shape_cnt * sg_dtype_len; i++) {
      input[i] = 1;
    }

    // init dst data
    uint32_t *output = (uint32_t *)malloc(shape_cnt * sg_dtype_len);
    if (!output) {
      fprintf(stderr, "Failed to allocate memory for output\n");
      free(input);
      bm_dev_free(handle);
      return BM_ERR_FAILURE;
    }

    bm_device_mem_t input_mem = bm_mem_from_system(input);
    bm_device_mem_t input_device_mem =
        map_input_to_device(&allocator, &input_mem, shape_cnt * sg_dtype_len);
    bm_device_mem_t output_mem = bm_mem_from_system(output);
    bm_device_mem_t output_device_mem = map_output_to_device(
        &allocator, &output_mem, shape_cnt * sg_dtype_len, 0);
    api_mem_param.src_global_offset = bm_mem_get_device_addr(input_device_mem);
    api_mem_param.dst_global_offset = bm_mem_get_device_addr(output_device_mem);


    ret = tpu_kernel_launch(handle, f_id, (void *)&api_mem_param, sizeof(sg_api_memcpy_t));
    if (ret != BM_SUCCESS) {
        printf( "tpu_kernel_launch failed !\n");
    }else{
        printf( "********************tpu_kernel_launch success!\n");
		}

		#ifndef USING_CMODEL
    int status = bm_mem_write_data_to_sys_ion(allocator.handle, &output_device_mem, BM_DTYPE_UINT32, 100, "output.txt");

    if(status == -1) printf("bm_mem_write_data_to_sys_ion exec error!!\n");
		#endif


		// printf("test unload module\n");
		ret=tpu_kernel_unload_module(handle, bm_module);
		if (ret != BM_SUCCESS) {
			printf( "tpu_kernel_unload_module failed, bm_module is null!\n");
			return BM_ERR_FAILURE;
		}else{
			printf("********************tpu_kernel_unload_module success!\n");
		}

		bm_dev_free(handle);
		free(input);
    free(output);
    // destroy_allocator(&allocator);
    return 0;

}
#endif
