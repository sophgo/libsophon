#include <stdio.h>
#include <fstream>
using namespace std;
#include <fcntl.h>
#include <unistd.h>
#include <sys/prctl.h>
#include "bmlib_internal.h"
#include "bmlib_log.h"
#include "api.h"
#include "bmlib_memory.h"
#include "bmlib_utils.h"
#include "bmlib_interface.h"
#include "bmlib_runtime.h"
#include "bmcpu_app.h"
#include "bmlib_mmpool.h"
#include "ion.h"
#ifdef USING_CMODEL
#include "bmlib_device.h"
//#include "cmodel_runtime.h"
/* global dummy bm device manager control */
// bm_device_manager_control bm_dev_mgr_ctrl;
#endif


#define BMLIB_RUNTIME_LOG_TAG "bmlib_runtime"
static bmlib_api_dbg_callback api_debug_callback = NULL;
static uint64_t tpu_usage = 0;
int bmcpu_app_live = 0; // Global variable to track if bmcpu app is live

bm_status_t find_lib_path(const char *lib_name, char **file_path) {
    const char *library_dirs[] = {
        "/lib", "/usr/lib", "/usr/lib64", "/lib64",
        "/usr/lib32", "/lib32", NULL
    };

    for (int i = 0; library_dirs[i] != NULL; i++) {
        size_t path_len = strlen(library_dirs[i]) + 1 + strlen(lib_name) + 1;
        char *path = (char *)malloc(path_len);
        if (path == NULL) {
            return BM_ERR_FAILURE;
        }

        snprintf(path, path_len, "%s/%s", library_dirs[i], lib_name);

        if (access(path, F_OK) == 0) {
            *file_path = path;
            return BM_SUCCESS;
        }
        free(path);
    }

    const char *ld_library_path = getenv("LD_LIBRARY_PATH");
    if (ld_library_path != NULL) {
        char *ld_copy = strdup(ld_library_path);
        if (ld_copy == NULL) {
            return BM_ERR_FAILURE;
        }

        char *dir = strtok(ld_copy, ":");
        while (dir != NULL) {
            size_t path_len = strlen(dir) + 1 + strlen(lib_name) + 1;
            char *path = (char *)malloc(path_len);
            if (path == NULL) {
                free(ld_copy);
                return BM_ERR_FAILURE;
            }

            snprintf(path, path_len, "%s/%s", dir, lib_name);

            if (access(path, F_OK) == 0) {
                *file_path = path;
                free(ld_copy);
                return BM_SUCCESS;
            }

            free(path);
            dir = strtok(NULL, ":");
        }

        free(ld_copy);
    }

    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d Library %s not found in system paths\n",
            __func__, __LINE__, lib_name);
    return BM_ERR_FAILURE;
}

void bm_flush(bm_handle_t handle) {
  bm_handle_sync(handle);
}

u64 bm_gmem_arm_reserved_request(bm_handle_t handle) {
  #if defined USING_CMODEL
  return handle->bm_dev->bm_device_arm_reserved_req();
  #else
  u64 val;
  int ret;
  ret = platform_ioctl(handle, BMDEV_REQUEST_ARM_RESERVED, &val);
  if (!ret)
    return val;
  else
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_MEM_ADDR_NULL;
  #endif
  }

void bm_gmem_arm_reserved_release(bm_handle_t handle) {
  #if defined USING_CMODEL
  handle->bm_dev->bm_device_arm_reserved_rel();
  #else
  
  #endif
}

#ifndef USING_CMODEL
bm_status_t bm_update_firmware_a9(bm_handle_t handle, pbm_fw_desc pfw) {
  unsigned int chip_id = 0;
  bm_status_t ret = BM_SUCCESS;
  int ioclt_ret;

  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "handle is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (pfw == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "%s:%d param err\n", __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = bm_get_chipid(handle, &chip_id);
  if (ret != BM_SUCCESS)
    return ret;

  bool should_profile = handle->profile != nullptr;
  if (should_profile) bm_profile_deinit(handle);

  ioclt_ret = platform_ioctl(handle, BMDEV_UPDATE_FW_A9, pfw);
  if (ioclt_ret == 0) {
      if (should_profile) bm_profile_init(handle, true);
      return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  }
#endif

void P(int semid) {
    struct sembuf sb = {0, -1, 0};
    if (semop(semid, &sb, 1) == -1) {
        perror("bmlib_runtime semop");
    }
}

void V(int semid) {
    struct sembuf sb = {0, 1, 0};
    if (semop(semid, &sb, 1) == -1) {
        perror("bmlib_runtime semop");
    }
}


bm_status_t bm_send_api_to_core(bm_handle_t handle, int api_id, const u8 *api, u32 size, int core_id) {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG, "enter %s\n", __func__);
  if (handle == nullptr) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
                "handle is nullptr %s: %s: %d\n", __FILE__, __func__, __LINE__);
      return BM_ERR_DEVNOTREADY;
  }
#ifdef USING_CMODEL
return handle->bm_dev->bm_device_send_api(api_id, api, size, core_id);
#endif

  pthread_t tid = pthread_self();
  char proj_id[10];
	int ret = 0;
  struct bm_ret bm_ret;
  std::hash<pthread_t> hasher;
  key_t sem_key = hasher(tid);

  // bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO,
  // 							"build message to bmcpu.\n");
  // message to bmcpu
  bm_api_to_bmcpu_t bm_api;
  bm_api.api_id = api_id;
  memcpy(bm_api.api_data, api, size);
  bm_api.api_size = size;
  bm_api.sem_key = sem_key;
  bm_api.tid = tid;


  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG,
                "create semget.....\n");
  int semid = semget(sem_key, 1, IPC_CREAT | IPC_EXCL | 0666);
  if (semid == -1) {
      if (errno == EEXIST) {
          semid = semget(sem_key, 1, 0666);
          if (semid == -1) {
              perror("Failed to get existing semaphore");
              return BM_ERR_FAILURE;
          }
          bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG, "Existing semaphore acquired, semid: %d\n", semid);
      } else {
          perror("Failed to create semaphore");
          return BM_ERR_FAILURE;
      }
  } else {
      if (semctl(semid, 0, SETVAL, 0) == -1) {
          perror("Failed to initialize semaphore");
          return BM_ERR_FAILURE;
      }
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO, "New semaphore created, semid: %d\n", semid);
  }

  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG,
                "send data to mquequ.....\n");

  pthread_mutex_lock(&uncomplete_msg_mtx);
  uncomplete_msg_queue.push(bm_api);
  pthread_mutex_unlock(&uncomplete_msg_mtx);
  // Notify the bmcpu thread that there is a new API request
  pthread_cond_signal(&uncomplete_msg_cv);

  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG,
          "mq_send success! api_id %x, thread_id %x, wait for sem_key %ld \n",
          api_id, tid, sem_key);
  P(semid);
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG, 
          "wakeup semid:%d, sem_key=%ld\n", semid, sem_key);

  pthread_mutex_lock(&complete_msg_mtx);
  for (std::list<bm_ret_t>::iterator it = complete_msg_queue.begin(); it != complete_msg_queue.end(); ++it) {
      if (it->tid == tid) {
          bm_ret = *it;
          complete_msg_queue.erase(it);
          break;
      }
  }
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG, "complete num size:%d\n", complete_msg_queue.size());
  pthread_mutex_unlock(&complete_msg_mtx);

  if (bm_ret.result != 0) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
        "%s:%d api result = %u\n", __func__, __LINE__, bm_ret.result);
      return BM_ERR_FAILURE;
  }

  if (api_id == BM_API_ID_TPUSCALER_GET_FUNC) {
    int read_f_id;
    a53lite_get_func_t *api_new = (a53lite_get_func_t*)api;
    if (sscanf(bm_ret.msg, "find_func_id value: %d", &read_f_id) == 1) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO, "Read f_id: %d\n", read_f_id);
    } else {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
                "%s:%d failed to read f_id from msg: %s\n",
                __func__, __LINE__, bm_ret.msg);
      return BM_ERR_FAILURE;
    }
    api_new->f_id = read_f_id;
  }

  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG,
                "api_id 0x%x done *************\n", api_id);
  return BM_SUCCESS;

}

bm_status_t bm_send_api_multicores(bm_handle_t handle, int api_id, tpu_launch_param_t *param_list, int param_num) {
  #ifdef USING_CMODEL
  return BM_SUCCESS;
  #else
  int i;
  int core_id;
  tpu_kernel_function_t func_id;
  void *param_data;
  unsigned int param_size;
  const u8 *api;

  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "handle is nullptr %s: %s: %d\n", __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  for (i = 0; i < param_num; i++) {
    api = (u8 *)&param_list[i];
    core_id = param_list[i].core_id;
    bm_profile_record_send_api(handle, api_id, api, core_id);
  }
  typedef struct bm_api {
    int core_id;
    int api_id;
    const u8* api_addr;
    u32 api_size;
  } bm_api_t;
  bm_api_t bm_api;
  memset(&bm_api, 0, sizeof(bm_api));
  bm_api.core_id = 0; // not use
  bm_api.api_id = api_id;
  bm_api.api_addr = (u8 *)&param_list[0];
  bm_api.api_size = (sizeof(tpu_launch_param_t) * param_num);
  if (0 == platform_ioctl(handle, BMDEV_SEND_API, &bm_api))
    return BM_SUCCESS;
  else
    return BM_ERR_FAILURE;
  #endif
}

bm_status_t bm_send_api(bm_handle_t handle, int api_id, const u8 *api, u32 size) {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG, "enter %s\n", __func__);
  return bm_send_api_to_core(handle, api_id, api, size, 0);
}

bm_status_t bm_device_sync(bm_handle_t handle) {
  #ifdef USING_CMODEL
  return handle->bm_dev->bm_device_sync();
  #else
  return BM_SUCCESS;
  #endif
}

bm_status_t bm_handle_sync_from_core(bm_handle_t handle, int core_id) {
  #ifdef USING_CMODEL
  return handle->bm_dev->bm_device_sync();
  #else
  return BM_SUCCESS;
  #endif
}

bm_status_t bm_handle_sync(bm_handle_t handle) {
    return bm_handle_sync_from_core(handle, 0);
}

u64 bm_get_version(bm_handle_t handle) {
  #ifdef USING_CMODEL
  UNUSED(handle);

  return 0x0001000100010001ul;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return 0;
  }
  unsigned int driver_version = handle->misc_info.driver_version;
  u64 bm_version = (((u64)BMLIB_VERSION) << 32) | driver_version;
  return bm_version;
  #endif
}

bm_status_t bm_thread_sync_from_core(bm_handle_t handle, int core_id) {
    bm_profile_record_sync_begin(handle, core_id);
    bm_status_t status = BM_SUCCESS;
  int ret = 0;
  #ifdef USING_CMODEL
    status =  handle->bm_dev->bm_device_thread_sync_from_core(core_id);
  #else
    if (handle == nullptr) {
        bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
                  "handle is nullptr %s: %s: %d\n",
                  __FILE__, __func__, __LINE__);
        status = BM_ERR_DEVNOTREADY;
    } else {
    // ret = platform_ioctl(handle, BMDEV_THREAD_SYNC_API, &core_id);
    if (ret == 0) {
          status = BM_SUCCESS;
    } else {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
      status = BM_ERR_FAILURE;
    }
  }
  #endif
    bm_profile_record_sync_end(handle, core_id);
    return status;
}

bm_status_t bm_thread_sync(bm_handle_t handle) {
    return bm_thread_sync_from_core(handle, 0);
}


bm_status_t bm_sync_api_from_core(bm_handle_t handle, int core_id) {
  return bm_thread_sync_from_core(handle, core_id);
}

bm_status_t bm_sync_api(bm_handle_t handle) {
  return bm_sync_api_from_core(handle, 0);
}

bm_status_t bm_reset_tpu(bm_handle_t handle) {

  #ifdef USING_CMODEL
  return BM_SUCCESS;
  #else
  int ret;

  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG,
              BMLIB_LOG_ERROR,
              "handle is nullptr %s: %s: %d\n",
              __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_FORCE_RESET_TPU, NULL);
    if (ret == 0) {
        return BM_SUCCESS;
    } else {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
        return BM_ERR_FAILURE;
    }
  #endif
}

bm_status_t bm_dev_getcount(int *count) {
  if (!count) return BM_ERR_PARAM;
  #ifdef USING_CMODEL
  *count = MAX_DEVICE_NUM;
  #else
  int fd;
  fd = open("/dev/bmdev-ctl", O_RDWR);
  if (fd == -1) return BM_ERR_FAILURE;
  if (ioctl(fd, BMCTL_GET_DEV_CNT, count) < 0) {
    close(fd);
    return BM_ERR_FAILURE;
  }
  close(fd);
  #endif
  return BM_SUCCESS;
}

bm_status_t bm_dev_query(int devid) {
  #ifdef USING_CMODEL
  UNUSED(devid);

  return BM_SUCCESS;
  #else
  char devname[20];
  int  fd;
  snprintf(devname, sizeof(devname), "/dev/bm-sophon%d", devid);
  fd = open(devname, O_RDWR);
  if (fd < 0) {
    snprintf(devname, sizeof(devname), "/dev/bm-tpu%d", devid);
    fd = open(devname, O_RDWR);
    if (fd < 0) return BM_ERR_BUSY;
  }
  close(fd);
  return BM_SUCCESS;
  #endif
}

#ifndef USING_CMODEL
bm_status_t bm_create_ctx(bm_context_t *ctx, int devid) {
  char devname[20];
  int fd;
  #ifdef __linux__
  snprintf(devname, sizeof(devname), "/dev/bm-tpu%d", devid);
  fd = open(devname, O_RDWR);
  if (fd < 0) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "Create BM Handle Failed for dev%d\n", devid);
    return BM_ERR_BUSY;
  }
  ctx->dev_fd = fd;

  fd = open("/dev/ion", O_RDWR);
  if (fd > 0) {
    ctx->ion_fd = fd;
    ctx->heap_cnt = 1;
    // bm_get_carveout_heap_id(ctx);
  }

  #endif

  return BM_SUCCESS;
}

void bm_destroy_ctx(bm_context_t *ctx) {
  if (ctx == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "ctx is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return;
  }
  #ifdef __linux__
  close(ctx->dev_fd);
  if (ctx->ion_fd) close(ctx->ion_fd);
  if (ctx->spacc_fd) close(ctx->spacc_fd);
  #endif
  }

void bm_enable_iommu(bm_handle_t handle) {
  handle->cdma_iommu_mode = BMLIB_USER_SETUP_IOMMU;
  }

void bm_disable_iommu(bm_handle_t handle) {
  handle->cdma_iommu_mode = BMLIB_NOT_USE_IOMMU;
  }

#endif

bm_status_t bm_dev_request(bm_handle_t *handle, int devid) {
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  bm_context_t *ctx = new bm_context_t;
  memset(ctx, 0, sizeof(bm_context_t));
  if (!ctx)
    return BM_ERR_NOMEM;
  ctx->dev_id = devid;

  #ifdef USING_CMODEL
    // ctx->device_mem_size = cmodel_get_global_mem_size(devid);
    bm_device_manager *bm_dev_mgr = bm_device_manager::get_dev_mgr();
    ASSERT(bm_dev_mgr);
    ctx->bm_dev = bm_dev_mgr->get_bm_device(devid);
    ASSERT(ctx->bm_dev);
    *handle = ctx;
  #else
    if (BM_SUCCESS != bm_create_ctx(ctx, devid)) {
      delete ctx;
      return BM_ERR_FAILURE;
    }

    ret = platform_ioctl(ctx, BMDEV_GET_MISC_INFO, &ctx->misc_info);

    if (ret == 0) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO,
            "driver version is %1d.%1d.%1d\n",
            ctx->misc_info.driver_version >> 16,
            (ctx->misc_info.driver_version >> 8) & 0xff,
            ctx->misc_info.driver_version & 0xff);

      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO,
            "the chip id is 0x%x, pcie_soc_mode is %s\n",
            ctx->misc_info.chipid,
            (ctx->misc_info.pcie_soc_mode == 0) ? "PCIE" : "SOC");
    } else {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
                "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
    }

    if (__atomic_load_n(&bmcpu_app_live, __ATOMIC_SEQ_CST) == 0 ) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO, "=======bmcpu app thread create======\n");
      __atomic_store_n(&bmcpu_app_live, 1, __ATOMIC_SEQ_CST);
      pthread_t bmcpu;
      if (pthread_create(&bmcpu, NULL, bmcpu_thread, static_cast<void*>(&ctx->dev_fd)) != 0) {
          perror("pthread_create");
          return BM_ERR_FAILURE;
      }
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO, "bmcpu app thread id is %ld\n", bmcpu);
      pthread_detach(bmcpu);
    }

    *handle = ctx;
    #ifdef SMMU_MODE
      bm_enable_iommu(*handle);
    #else
      bm_disable_iommu(*handle);
    #endif
  #endif
  if (get_env_bool_value("BMLIB_ENABLE_ALL_PROFILE", false)) {
    bm_profile_init(ctx, true);
  }
  ctx->enable_mem_guard = get_env_bool_value("BMLIB_ENABLE_MEM_GUARD", false);
  if (ctx->enable_mem_guard) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO, "mem guard mode is enabled\n");
  }
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO, "request success\n");
  return BM_SUCCESS;
}

void bm_dev_free(bm_handle_t handle) {
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "handle is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return;
  }
  if (handle->profile) {
      bm_profile_deinit(handle);
  }
  #if defined USING_CMODEL
    bm_device_manager *bm_dev_mgr = bm_device_manager::get_dev_mgr();
    bm_dev_mgr->free_bm_device(handle->dev_id);
    handle->bm_dev = nullptr;
  #else
    bm_destroy_ctx(handle);
  #endif
  delete handle;
  }

bm_status_t bm_get_profile(bm_handle_t handle, bm_profile_t *profile) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(profile);

  return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (profile == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "profile is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG, "bmdev get profile, send_api_count=%d, complete_api_count=%d, consume_time=%d\n", 
    bm_profile.sent_api_counter, bm_profile.completed_api_counter, bm_profile.tpu_process_time);
  // profile = &bm_profile;
  profile->sent_api_counter = bm_profile.sent_api_counter;
  profile->completed_api_counter = bm_profile.completed_api_counter;
  profile->tpu_process_time = bm_profile.tpu_process_time;
  return BM_SUCCESS;
  #endif
}

bm_status_t bm_get_boot_loader_version(bm_handle_t handle, boot_loader_version *version) {
  UNUSED(handle);

  return BM_SUCCESS;
}

bm_status_t bm_get_smi_attr(bm_handle_t handle, struct bm_smi_attr_t *smi_attr) {
  #ifdef USING_CMODEL
  UNUSED(handle);

  return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_GET_SMI_ATTR, smi_attr);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
}


bm_status_t bm_trace_enable(bm_handle_t handle) {
  UNUSED(handle);
  return BM_SUCCESS;
}

bm_status_t bm_trace_disable(bm_handle_t handle) {
  UNUSED(handle);

  return BM_SUCCESS;
}

bm_status_t bm_traceitem_number(bm_handle_t handle, long long *number) {
  UNUSED(handle);
  UNUSED(number);

  return BM_SUCCESS;
}

bm_status_t bm_trace_dump(bm_handle_t handle, struct bm_trace_item_data *trace_data) {
  UNUSED(handle);
  UNUSED(trace_data);

  return BM_SUCCESS;
}

bm_status_t bm_trace_dump_all(bm_handle_t handle,
                              struct bm_trace_item_data *trace_data) {
  UNUSED(handle);
  UNUSED(trace_data);

  return BM_SUCCESS;
}

bm_status_t bm_get_misc_info(bm_handle_t handle, bm_misc_info *pmisc_info) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(pmisc_info);

  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (pmisc_info == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "pmisc_info is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_MISC_INFO, pmisc_info);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d %d\n", __func__, __LINE__, ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_chipid(bm_handle_t handle, unsigned int *p_chipid) {
  #ifdef USING_CMODEL
  *p_chipid = handle->bm_dev->chip_id;
  UNUSED(handle);
  #else
  struct bm_misc_info misc_info;
  bm_status_t status = bm_get_misc_info(handle, &misc_info);
  if (status != BM_SUCCESS) return status;
  *p_chipid = misc_info.chipid;
  #endif
  return BM_SUCCESS;
  }

int bm_get_devid(bm_handle_t handle) {
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "handle is nullptr %s: %s: %d\n", __FILE__, __func__, __LINE__);
    return -1;
  }
  return handle->dev_id;
  }

bm_status_t bm_get_tpu_scalar_num(bm_handle_t handle, unsigned int *core_num) {
  #ifdef USING_CMODEL
  *core_num = handle->bm_dev->bm_core_num();
  #else
  struct bm_misc_info misc_info;
  bm_status_t status = bm_get_misc_info(handle, &misc_info);
  if (status != BM_SUCCESS) return status;
  *core_num = misc_info.tpu_core_num;
  #endif

  return BM_SUCCESS;
  }

bm_status_t bm_get_boot_info(bm_handle_t handle, bm_boot_info *pboot_info) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(pboot_info);
  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (pboot_info == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "pboot_info is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_BOOT_INFO, pboot_info);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_update_boot_info(bm_handle_t handle, bm_boot_info *pboot_info) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(pboot_info);
  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (pboot_info == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "pboot_info is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_UPDATE_BOOT_INFO, pboot_info);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_set_clk_tpu_divider(bm_handle_t handle, int divider) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(divider);
  return BM_SUCCESS;
  #else
  int val = divider;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (val <= 0) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d tpu clk divider = %d not support\n", __func__, __LINE__, val);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_SET_TPU_DIVIDER, &val);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_set_clk_tpu_freq(bm_handle_t handle, int freq) {
  UNUSED(handle);
  UNUSED(freq);

  return BM_SUCCESS;
}

bm_status_t bm_get_clk_tpu_freq(bm_handle_t handle, int *freq) {
  UNUSED(handle);
  UNUSED(freq);
  *freq = 1000;
  return BM_SUCCESS;
}

bm_status_t bm_set_module_reset(bm_handle_t handle, MODULE_ID module) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(module);

  return BM_SUCCESS;
  #else
  MODULE_ID val = module;

  if (val >= MODULE_END) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d module reset id = %d is large \n",
            __func__, __LINE__, val);
    return BM_ERR_PARAM;
  }

  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_SET_MODULE_RESET, &val);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_device_time_us(bm_handle_t handle, unsigned long *time_us) {
  UNUSED(handle);
  *time_us = 0;

  return BM_SUCCESS;
}

bm_status_t bm_set_reg(bm_handle_t handle, struct bm_reg *reg) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(reg);

  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret =  platform_ioctl(handle, BMDEV_SET_REG, reg);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_reg(bm_handle_t handle, struct bm_reg *reg) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(reg);

  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_GET_REG, reg);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
}

bm_status_t bm_get_last_api_process_time_us(bm_handle_t    handle,
                                            unsigned long *time_us) {
  // need implement for bm168x
  *time_us = 0;
  UNUSED(handle);
  return BM_SUCCESS;
}

void bmlib_set_api_dbg_callback(bmlib_api_dbg_callback callback) {
    api_debug_callback = callback;
  }

bm_status_t bm_get_stat(bm_handle_t handle, bm_dev_stat_t *stat) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(stat);

  return BM_SUCCESS;
  #else
  #ifdef __linux__
  struct ion_custom_data ion_data;
  struct bitmain_heap_info bm_heap_info;
  #endif
  u64 avail_size = 0;
  u64 total_size = 0;
  u64 used_size = 0;
  bm_dev_stat_t hstat;
  int ret;

  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }
  if (stat == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "profile is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }
  stat->mem_total = 0;
  stat->mem_used = 0;

  if (handle->ion_fd) {
    ion_data.cmd = ION_IOC_BITMAIN_GET_HEAP_INFO;
    ion_data.arg = (unsigned long)&bm_heap_info;
    stat->heap_num = handle->heap_cnt;

    for (int i = 0; i < handle->heap_cnt; i++) {
      bm_heap_info.id = handle->carveout_heap_id[i];
      if (0 != ioctl(handle->ion_fd, ION_IOC_CUSTOM, &ion_data))
        return BM_ERR_FAILURE;

      avail_size = bm_heap_info.avail_size;
      total_size = bm_heap_info.total_size;
      used_size = total_size - avail_size;
      stat->mem_total += total_size >> 20;
      stat->mem_used += used_size >> 20;

      stat->heap_stat[bm_heap_info.id].mem_avail = avail_size >> 20;
      stat->heap_stat[bm_heap_info.id].mem_total = total_size >> 20;
      stat->heap_stat[bm_heap_info.id].mem_used = used_size >> 20;
    }

    if (ioctl(handle->dev_fd, BMDEV_GET_DEV_STAT, &hstat) != 0) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "get dev stat error: %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
      return BM_ERR_FAILURE;
    }
    stat->tpu_util = hstat.tpu_util;
    return BM_SUCCESS;
  } else {
    ret = platform_ioctl(handle, BMDEV_GET_DEV_STAT, stat);
    if (ret == 0) {
      return BM_SUCCESS;
    } else {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "%s:%d error, ioclt ret = %d\n",
          __func__, __LINE__, ret);
      return BM_ERR_FAILURE;
    }
  }
  #endif
}

#define BMLIB_SPACC_TRIGGER_LOG_TAG "bmlib_spacc_trigger"

#if defined(__cplusplus)
extern "C" {
  #endif

  bm_status_t bm_trigger_spacc(bm_handle_t handle, struct spacc_batch* batch) {
  #ifdef USING_CMODEL
    UNUSED(handle);
    UNUSED(batch);
    return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_SPACC_TRIGGER_LOG_TAG, BMLIB_LOG_ERROR,
        "handle is nullptr %s: %s: %d\n", __FILE__, __func__, __LINE__);
        return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_TRIGGER_SPACC, batch);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }

  bm_status_t bm_is_seckey_valid(bm_handle_t handle, int* is_valid) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(is_valid);
  return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_SPACC_TRIGGER_LOG_TAG, BMLIB_LOG_ERROR,
        "handle is nullptr %s: %s: %d\n", __FILE__, __func__, __LINE__);
        return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_SECKEY_VALID, is_valid);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }

  bm_status_t bm_efuse_write(bm_handle_t handle, struct bm_efuse_param *efuse_param) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(efuse_param);
  return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_SPACC_TRIGGER_LOG_TAG, BMLIB_LOG_ERROR,
        "handle is nullptr %s: %s: %d\n", __FILE__, __func__, __LINE__);
        return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_EFUSE_WRITE, efuse_param);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }

  bm_status_t bm_efuse_read(bm_handle_t handle, struct bm_efuse_param *efuse_param) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(efuse_param);
  return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_SPACC_TRIGGER_LOG_TAG, BMLIB_LOG_ERROR,
        "handle is nullptr %s: %s: %d\n", __FILE__, __func__, __LINE__);
        return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_EFUSE_READ, efuse_param);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n", __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }



  #if defined(__cplusplus)
  }
#endif

bm_status_t bm_get_tpu_current(bm_handle_t handle, unsigned int *tpuc) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(tpuc);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;

}

bm_status_t bm_get_board_max_power(bm_handle_t handle, unsigned int *maxp) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(maxp);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;

}

bm_status_t bm_get_board_power(bm_handle_t handle, unsigned int *boardp) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(boardp);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;
  
}

bm_status_t bm_get_fan_speed(bm_handle_t handle, unsigned int *fan) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(fan);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;
}

bm_status_t bm_get_12v_atx(bm_handle_t handle, int *atx_12v) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(atx_12v);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;
}

bm_status_t bm_get_product_sn(char *product_sn) {
  #ifdef USING_CMODEL
  UNUSED(product_sn);

  return BM_NOT_SUPPORTED;
  #endif
  if (product_sn == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "product_sn is nullptr %s: %s: %d\n",
              __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  int cnt = 0;
  int fd;
  char boot1[] = "/sys/bus/nvmem/devices/1-006a0/nvmem";
  struct product_config rd_header;

  memset(&rd_header, 0, RDBUF_SIZE);

  //open boot1 device
  fd = open(boot1, O_RDWR);
  if (fd < 0) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "open boot failed when get product sn !\n");
    return BM_ERR_FAILURE;
  }

  cnt = read(fd, &rd_header, RDBUF_SIZE);
  if (cnt !=RDBUF_SIZE) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "read rdbuf failed!\n");
    close(fd);
    return BM_ERR_DATA;
  }
  snprintf(product_sn, 18, "%s", rd_header.manufacture);
  close(fd);

  return BM_SUCCESS;
}

bm_status_t bm_get_sn(bm_handle_t handle, char *sn) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(sn);

  return BM_NOT_SUPPORTED;
  #endif
  if (sn == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "sn is nullptr %s: %s: %d\n",
              __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  unsigned int chip_id = 0;
  bm_status_t ret = BM_SUCCESS;
  int fd = 0;
  int cnt = 0;

  ret = bm_get_chipid(handle, &chip_id);
  if (ret != BM_SUCCESS)
    return ret;

  if (chip_id == 0x1686a200) {
    fd = open("/dev/mmcblk0boot1", O_RDONLY);
    if (fd < 0) {
        bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "open /dev/mmcblk0boot1 failed!\n");
        return BM_ERR_FAILURE;
    }
    cnt = read(fd, sn, 17);
    if (cnt < 0) {
        bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "read /dev/mmcblk0boot1 failed!\n");
        close(fd);
        return BM_ERR_FAILURE;
    }
    close(fd);
  } else {
    char boot1[] = "/sys/bus/nvmem/devices/1-006a0/nvmem";
    struct product_config rd_header;

    memset(&rd_header, 0, RDBUF_SIZE);
    //open boot1 device
    fd = open(boot1, O_RDWR);
    if (fd < 0) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "open boot failed when get sn!\n");
      return BM_ERR_FAILURE;
    }

    cnt = read(fd, &rd_header, RDBUF_SIZE);
    if (cnt != RDBUF_SIZE) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "read rdbuf failed!\n");
      close(fd);
      return BM_ERR_DATA;
    }
    snprintf(sn, 18, "%s", rd_header.sn);
    close(fd);
  }
  return BM_SUCCESS;
}

bm_status_t bm_get_status(bm_handle_t handle, int *status) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(status);

  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (status == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "status is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_STATUS, status);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "%s:%d failed, ioclt ret = %d\n",
          __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_tpu_minclk(bm_handle_t handle, unsigned int *tpu_minclk) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(tpu_minclk);

  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (tpu_minclk == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "tpu_minclk is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_TPU_MINCLK, tpu_minclk);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d %d\n",
            __func__, __LINE__, ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_tpu_maxclk(bm_handle_t handle, unsigned int *tpu_maxclk) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(tpu_maxclk);

  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (tpu_maxclk == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "tpu_maxclk is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_TPU_MAXCLK, tpu_maxclk);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n",
            __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_driver_version(bm_handle_t handle, int *driver_version) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(driver_version);

  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (driver_version == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "driver_version is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_DRIVER_VERSION, driver_version);
  if (ret < 0) {
    *driver_version = 1<<16;
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d %d\n",
            __func__, __LINE__, ret, __LINE__);
    return BM_ERR_FAILURE;
  } else {
  return BM_SUCCESS;
  }
  #endif
  }

bm_status_t bm_get_board_name(bm_handle_t handle, char *name) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(name);

  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  //  if (name == nullptr) {
  //    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
  //          "name is nullptr %s: %s: %d\n",
  //          __FILE__, __func__, __LINE__);
  //    return BM_ERR_PARAM;
  //  }

  ret = platform_ioctl(handle, BMDEV_GET_BOARD_TYPE, name);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "%s:%d failed, ioclt ret = %d\n",
            __func__, __LINE__, ret);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_board_temp(bm_handle_t handle, unsigned int *board_temp) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(board_temp);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;

}

bm_status_t bm_get_chip_temp(bm_handle_t handle, unsigned int *chip_temp) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(chip_temp);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;
}

bm_status_t bm_get_tpu_power(bm_handle_t handle, float *tpu_power) {
#ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(tpu_power);

  return BM_NOT_SUPPORTED;
#endif
  return BM_NOT_SUPPORTED;
}

bm_status_t bm_get_tpu_volt(bm_handle_t handle, unsigned int *tpu_volt) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(tpu_volt);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;

}

bm_status_t bm_get_card_id(bm_handle_t handle, unsigned int *card_id) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(card_id);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;
}

bm_status_t bm_get_card_num(unsigned int *card_num) {
  #ifdef USING_CMODEL
  UNUSED(card_num);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;
}

bm_status_t bm_get_chip_num_from_card(unsigned int card_id, unsigned int *chip_num, unsigned int *dev_start_index) {
  #ifdef USING_CMODEL
  UNUSED(card_id);
  UNUSED(chip_num);
  UNUSED(dev_start_index);
  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;
}

bm_status_t bm_get_dynfreq_status(bm_handle_t handle, int *dynfreq_status) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(dynfreq_status);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;

}

bm_status_t bm_change_dynfreq_status(bm_handle_t handle, int new_status) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(new_status);

  return BM_NOT_SUPPORTED;
  #endif
  return BM_NOT_SUPPORTED;
}

bm_status_t bm_get_handle_fd(bm_handle_t handle, FD_ID id, int *fd) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(id);
  UNUSED(fd);
  return BM_NOT_SUPPORTED;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }
  #ifdef __linux__
  if (fd != nullptr) {
    switch (id) {
      case 0:
        *fd = handle->dev_fd;
      break;
      case 1:
        *fd = handle->ion_fd;
      break;
      case 2:
        *fd = handle->spacc_fd;
      break;
      default:
        bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "fd is not suported %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
        return BM_ERR_PARAM;
      break;
    }
  } else {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "fd is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }
  return BM_SUCCESS;
  #else
      return BM_NOT_SUPPORTED;
  #endif
  #endif
}

bm_status_t bm_pwr_ctrl(bm_handle_t handle, void *bm_api_cfg_pwr_ctrl) {
  UNUSED(handle);
  UNUSED(bm_api_cfg_pwr_ctrl);
  return BM_SUCCESS;
}

DECL_EXPORT int bm_is_dynamic_loading(bm_handle_t handle) {
  #ifdef USING_CMODEL
  int arch_code = handle->bm_dev->chip_id;
  #else
  int arch_code = handle->misc_info.chipid;
  #endif
  return arch_code == 0x1686a200;
}

#ifndef USING_CMODEL

bm_status_t bm_memcpy_s2s(uint64_t u64PhyDst, uint64_t u64PhySrc, uint64_t u64Size) {
  bm_handle_t handle;
  bm_status_t ret = BM_SUCCESS;
  int ret_ = 0;
  size_t mmap_sizes[5] = {0x10000, 0x30000, 0x10000, 0x1000, 0x80000}; // tup_sys size
  void *mapped_memory;
  sg_api_1d_memcpy api_mem_param;

  __atomic_store_n(&bmcpu_app_live, 1, __ATOMIC_SEQ_CST);
  ret = bm_dev_request(&handle, 0);
  if ((ret != BM_SUCCESS) || (handle == NULL)) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
      "%s:%d error, ret = %d\r\n", __func__, __LINE__, ret);
    return BM_ERR_FAILURE;
  }
  // open so file, the path is fixed /lib/libtpu_kernel_module.so
  char* lib_path = nullptr;
  ret = find_lib_path("libtpu_kernel_module.so" , &lib_path);
  if (ret != BM_SUCCESS) {
    bm_dev_free(handle);
    return BM_ERR_FAILURE;
  }

  void *handle_lib = dlopen(lib_path, RTLD_NOW);
  // mmap to firmware_core.so
  int (*CallPhysicalToVirtual)(void *, int) =
      (int (*)(void *, int))dlsym(handle_lib, "PhysicalToVirtual");
  if (!CallPhysicalToVirtual) {
    fprintf(stderr, "Failed to load PhysicalToVirtual\n");
    dlclose(handle_lib);
    bm_dev_free(handle);
    return BM_ERR_FAILURE;
  }

  for (int type = 0; type < 5; type++) {
    ret_ = ioctl(handle->dev_fd, BMDEV_SET_IOMAP_TPYE, type);
    if (ret != 0) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
        "%s:%d, ioctl failed\n", __func__, __LINE__);
      dlclose(handle_lib);
      bm_dev_free(handle);
      return BM_ERR_FAILURE;
    }
    mapped_memory = mmap(NULL, mmap_sizes[type], PROT_READ | PROT_WRITE, MAP_SHARED, handle->dev_fd, 0);
    if (mapped_memory == MAP_FAILED) {
      perror("bm_memcpy_s2s mmap failed");
      dlclose(handle_lib);
      bm_dev_free(handle);
      return BM_ERR_FAILURE;
    }
    ret_ = CallPhysicalToVirtual(mapped_memory, type);
  }

  // call copy function
  int (*f_ptr)(void *, unsigned int);
  f_ptr = (int (*)(void *, unsigned int))dlsym(handle_lib, "sg_api_1d_memcpy");
  if (!f_ptr) {
    fprintf(stderr, "Failed to load sg_api_1d_memcpy\n");
    dlclose(handle_lib);
    bm_dev_free(handle);
    return BM_ERR_FAILURE;
  }

  api_mem_param.data_type = 1; // 1:uint8
  api_mem_param.w_bytes = u64Size;
  api_mem_param.src_global_offset = u64PhySrc;
  api_mem_param.dst_global_offset = u64PhyDst;

  ret_ = f_ptr(&api_mem_param, sizeof(api_mem_param));
  if (ret_ != 0) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "sg_api_1d_memcpy failed!\n");
    dlclose(handle_lib);
    bm_dev_free(handle);
    return BM_ERR_FAILURE;
  }

  bm_dev_free(handle);
  dlclose(handle_lib);
  return BM_SUCCESS;
}

bm_status_t bm_memcpy_s2s_2d(sg_api_2d_memcpy_t *api_mem_param) {
  bm_handle_t handle;
  bm_status_t ret = BM_SUCCESS;
  int ret_ = 0;
  size_t mmap_sizes[5] = {0x10000, 0x30000, 0x10000, 0x1000, 0x80000}; // tup_sys size
  void *mapped_memory;


  __atomic_store_n(&bmcpu_app_live, 1, __ATOMIC_SEQ_CST);
  ret = bm_dev_request(&handle, 0);
  if ((ret != BM_SUCCESS) || (handle == NULL)) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "bm_dev_request error, ret = %d\r\n", ret);
    return BM_ERR_FAILURE;
  }
  // open so file, the path is fixed /lib/libtpu_kernel_module.so
  char* lib_path = nullptr;
  ret = find_lib_path("libtpu_kernel_module.so" , &lib_path);
  if (ret != BM_SUCCESS) {
    bm_dev_free(handle);
    return BM_ERR_FAILURE;
  }

  void *handle_lib = dlopen(lib_path, RTLD_NOW);
  // mmap to firmware_core.so
  int (*CallPhysicalToVirtual)(void *, int) =
      (int (*)(void *, int))dlsym(handle_lib, "PhysicalToVirtual");
  if (!CallPhysicalToVirtual) {
    fprintf(stderr, "Failed to load PhysicalToVirtual\n");
    dlclose(handle_lib);
    bm_dev_free(handle);
    return BM_ERR_FAILURE;
  }

  for (int type = 0; type < 5; type++) {
    ret_ = ioctl(handle->dev_fd, BMDEV_SET_IOMAP_TPYE, type);
    if (ret != 0) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "ioctl failed\n");
      dlclose(handle_lib);
      bm_dev_free(handle);
      return BM_ERR_FAILURE;
    }
    mapped_memory = mmap(NULL, mmap_sizes[type], PROT_READ | PROT_WRITE, MAP_SHARED, handle->dev_fd, 0);
    if (mapped_memory == MAP_FAILED) {
      perror("bm_memcpy_s2s_2d mmap failed");
      dlclose(handle_lib);
      bm_dev_free(handle);
      return BM_ERR_FAILURE;
    }
    ret_ = CallPhysicalToVirtual(mapped_memory, type);
  }

  // call copy function
  int (*f_ptr)(void *, unsigned int);
  f_ptr = (int (*)(void *, unsigned int))dlsym(handle_lib, "sg_api_2d_memcpy");
  if (!f_ptr) {
    fprintf(stderr, "Failed to load sg_api_1d_memcpy\n");
    dlclose(handle_lib);
    bm_dev_free(handle);
    return BM_ERR_FAILURE;
  }

  ret_ = f_ptr(api_mem_param, sizeof(sg_api_2d_memcpy_t));
  if (ret_ != 0) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "sg_api_2d_memcpy failed!\n");
    dlclose(handle_lib);
    bm_dev_free(handle);
    return BM_ERR_FAILURE;
  }

  bm_dev_free(handle);
  dlclose(handle_lib);
  return BM_SUCCESS;
}


#endif