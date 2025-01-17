#include <stdio.h>
#include <fcntl.h>
#include <atomic>
#include <sys/prctl.h>
#include"bmlib_internal.h"
#include "bmlib_log.h"
#include "api.h"
#include "bmlib_memory.h"
#include "bmlib_utils.h"
#include "bmlib_interface.h"
#include "bmlib_runtime.h"
#include "bmcpu_app.h"

#ifdef _WIN32
#include <stdlib.h>
#include <sys/stat.h>
#include <iostream>
#include <SetupAPI.h>
#pragma comment(lib, "SetupAPI.Lib")
#else
#include <unistd.h>
#include "bmlib_mmpool.h"
#include "ion.h"
#ifdef USING_CMODEL
#include "bmlib_device.h"
//	#include "cmodel_runtime.h"
/* global dummy bm device manager control */
// bm_device_manager_control bm_dev_mgr_ctrl;
#endif
#endif

#define BMLIB_RUNTIME_LOG_TAG "bmlib_runtime"
static bmlib_api_dbg_callback api_debug_callback = NULL;


#ifdef _WIN32
// Define an Interface Guid so that apps can find the device and talk to it.
// {84703EC3-9B1B-49D7-9AA6-0C42C6465681}
DEFINE_GUID(GUID_DEVINTERFACE_bm_sophon0,
          0x84703ec3,
          0x9b1b,
          0x49d7,
          0x9a,
          0xa6,
          0xc,
          0x42,
          0xc6,
          0x46,
          0x56,
          0x81);

// {DCCB6586-7AF8-40A4-97B6-4D1C8E52718D}
DEFINE_GUID(GUID_DEVINTERFACE_bm_sophon1,
          0xdccb6586,
          0x7af8,
          0x40a4,
          0x97,
          0xb6,
          0x4d,
          0x1c,
          0x8e,
          0x52,
          0x71,
          0x8d);

// {11AA8451-91B0-4135-9263-65EA72685A5A}
DEFINE_GUID(GUID_DEVINTERFACE_bm_sophon2,
          0x11aa8451,
          0x91b0,
          0x4135,
          0x92,
          0x63,
          0x65,
          0xea,
          0x72,
          0x68,
          0x5a,
          0x5a);


const GUID* g_guid_interface[] = {&GUID_DEVINTERFACE_bm_sophon0,
                                &GUID_DEVINTERFACE_bm_sophon1,
                                &GUID_DEVINTERFACE_bm_sophon2};

BOOL GetDeviceHandle(bm_context* ctx) {
  BOOL status = TRUE;

  if (ctx->pDeviceInterfaceDetail == NULL) {
      status = FALSE;
  }

  if (status) {
      //  Get handle to device.
      ctx->hDevice = CreateFile(ctx->pDeviceInterfaceDetail->DevicePath,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                0,
                                NULL);

      if (ctx->hDevice == INVALID_HANDLE_VALUE) {
          if (ctx->pDeviceInterfaceDetail)
              free(ctx->pDeviceInterfaceDetail);
          status = FALSE;
          printf("CreateFile failed.  Error:%u", GetLastError());
      }
  }
  return status;
}

BOOL GetDevicePath(bm_context* ctx) {
  SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
  SP_DEVINFO_DATA          DeviceInfoData;

  ULONG size;
  BOOL  status = TRUE;

  //
  //  Retreive the device information for all PLX devices.
  //
  ctx->hDevInfo = SetupDiGetClassDevs(g_guid_interface[0],
                                      NULL,
                                      NULL,
                                      DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
  if (INVALID_HANDLE_VALUE == ctx->hDevInfo) {
      printf("No sophon devices interface class are in the system.\n");
      return FALSE;
  }
  //
  //  Initialize the appropriate data structures in preparation for
  //  the SetupDi calls.
  //
  DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
  DeviceInfoData.cbSize      = sizeof(SP_DEVINFO_DATA);

  if (!SetupDiEnumDeviceInterfaces(ctx->hDevInfo,
                                    NULL,
                                    (LPGUID)g_guid_interface[0],
                                    ctx->dev_id,
                                    &DeviceInterfaceData)) {
      printf("No sophon devices SetupDiEnumDeviceInterfaces for dev%d.\n", ctx->dev_id);
      goto Error;
  }

  //
  // Determine the size required for the DeviceInterfaceData
  //
  status = SetupDiGetDeviceInterfaceDetail(ctx->hDevInfo, &DeviceInterfaceData, NULL, 0, &size, NULL);

  if (!status && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      printf("SetupDiGetDeviceInterfaceDetail failed, Error: %u", GetLastError());
      goto Error;
  }
  ctx->pDeviceInterfaceDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(size);

  if (!ctx->pDeviceInterfaceDetail) {
      printf("Insufficient memory.\n");
      goto Error;
  }

  //
  // Initialize structure and retrieve data.
  //
  ctx->pDeviceInterfaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
  status = SetupDiGetDeviceInterfaceDetail(ctx->hDevInfo,
                                            &DeviceInterfaceData,
                                            ctx->pDeviceInterfaceDetail,
                                            size,
                                            NULL,
                                            &DeviceInfoData);

  if (!status) {
      printf("SetupDiGetDeviceInterfaceDetail failed, Error: %u", GetLastError());
      goto Error;
  }
  SetupDiDestroyDeviceInfoList(ctx->hDevInfo);
  return status;

Error:
  if (ctx->pDeviceInterfaceDetail)
      free(ctx->pDeviceInterfaceDetail);
  SetupDiDestroyDeviceInfoList(ctx->hDevInfo);
  return FALSE;
}
#endif


std::atomic<int> bmcpu_app_live(0);

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
              "bmdev request arm reserverd failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_MEM_ADDR_NULL;
  #endif
  }

void bm_gmem_arm_reserved_release(bm_handle_t handle) {
  #if defined USING_CMODEL
  handle->bm_dev->bm_device_arm_reserved_rel();
  #else
  int ret;
  ret = platform_ioctl(handle, BMDEV_RELEASE_ARM_RESERVED, 0);
  if (ret != 0)
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "bmdev release arm reserverd failed, ioclt ret = %d %d\n", ret, __LINE__);
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
          "bm_update_firmware_a9 param err\n");
    return BM_ERR_PARAM;
  }

  ret = bm_get_chipid(handle, &chip_id);
  if (ret != BM_SUCCESS)
    return ret;

  bool should_profile = handle->profile != nullptr;
  if (should_profile) bm_profile_deinit(handle);

  ioclt_ret = platform_ioctl(handle, BMDEV_UPDATE_FW_A9, pfw);
  if (ioclt_ret == 0) {
      if(should_profile) bm_profile_init(handle, true);
      return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "bmdev update firmware A9 failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  }
#endif

void P(int semid) {
    struct sembuf sb = {0, -1, 0};
    semop(semid, &sb, 1);
}

void V(int semid) {
    struct sembuf sb = {0, 1, 0};
    semop(semid, &sb, 1);
}


bm_status_t bm_send_api_to_core( bm_handle_t handle, int api_id, const u8 *api, u32 size, int core_id) {
  if (handle == nullptr) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
                "handle is nullptr %s: %s: %d\n", __FILE__, __func__, __LINE__);
      // return BM_ERR_DEVNOTREADY;
  }
#ifdef USING_CMODEL
return handle->bm_dev->bm_device_send_api(api_id, api, size, core_id);
#endif

  pthread_t tid=pthread_self();
  // pid_t pid = getpid();
  char proj_id[10];
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

  // open message queue
  // bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO,
            // "open message queue.\n");
  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = 20;
  attr.mq_msgsize = sizeof(bm_api_to_bmcpu_t);
  attr.mq_curmsgs = 0;
  mqd_t mq1 = mq_open(QUEUE1_NAME, O_WRONLY | O_CREAT, 0644, &attr);
  // mqd_t mq1 = mq_open(QUEUE1_NAME, O_WRONLY);
  if (mq1 == (mqd_t)-1) {
      perror("mq_open queue");
      return BM_ERR_FAILURE;
  }

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
      printf("New semaphore created, semid: %d\n", semid);
  }

  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG,
                "send data to mquequ.....\n");
  if (mq_send(mq1, (const char*)&bm_api, sizeof(bm_api_to_bmcpu_t), 0) == -1) {
      perror("mq_send header");
      mq_close(mq1);
      return BM_ERR_FAILURE;
  }

  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG,
                "mq_send success! send_size %d, thread_id %d, wait for sem_key %ld \n",
                sizeof(bm_api_to_bmcpu_t), tid, sem_key);
  P(semid);

  // get result from message queue
  // bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO,
  // 							"start handle result from bmcpu.....\n");
  struct mq_attr attr2;
  attr2.mq_flags = 0;
  attr2.mq_maxmsg = 20;
  attr2.mq_msgsize = sizeof(bm_ret);
  attr2.mq_curmsgs = 0;
  mqd_t mq2 = mq_open(QUEUE2_NAME, O_RDONLY | O_CREAT, 0644, &attr2);
  // mqd_t mq2 = mq_open(QUEUE2_NAME, O_RDONLY);
  if (mq2 == (mqd_t)-1) {
      perror("mq_open queue2");
      mq_close(mq1);
      return BM_ERR_FAILURE;
  }

  mq_receive(mq2, (char*)&bm_ret, sizeof(struct bm_ret), NULL);
  if (mq2 == (mqd_t)-1) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,"mq_receive failed....");
      return BM_ERR_FAILURE;
  }

  mq_close(mq1);
  mq_close(mq2);
  if (bm_ret.result!=0) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "api result = %u\n", bm_ret.result);
    return BM_ERR_FAILURE;
  }

  if (api_id==BM_API_ID_TPUSCALER_GET_FUNC) {
    int read_f_id;
    a53lite_get_func_t *api_new=(a53lite_get_func_t*)api;
    if (sscanf(bm_ret.msg, "f_id value: %d", &read_f_id) == 1) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO, "Read f_id: %d\n", read_f_id);
    }
    api_new->f_id=read_f_id;
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
  return bm_send_api_to_core(handle, api_id, api, size, 0);
  }

bm_status_t bm_device_sync(bm_handle_t handle) {
  int ret;
  #ifdef USING_CMODEL
  return handle->bm_dev->bm_device_sync();
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }
  ret = platform_ioctl(handle, BMDEV_DEVICE_SYNC_API, 0);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "bmdev device sync api failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_handle_sync_from_core(bm_handle_t handle, int core_id) {
  int ret;
  #ifdef USING_CMODEL
  return handle->bm_dev->bm_device_sync();
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }
  ret = platform_ioctl(handle, BMDEV_HANDLE_SYNC_API, &core_id);
  if (ret == 0) {
  return BM_SUCCESS;
  }
  else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "bmdev handle sync api failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
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
  int ret;
  #ifdef USING_CMODEL
    status =  handle->bm_dev->bm_device_thread_sync_from_core(core_id);
  #else
    if (handle == nullptr) {
        bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
                  "handle is nullptr %s: %s: %d\n",
                  __FILE__, __func__, __LINE__);
        status = BM_ERR_DEVNOTREADY;
    } else {
    ret = platform_ioctl(handle, BMDEV_THREAD_SYNC_API, &core_id);
    if (ret == 0) {
          status = BM_SUCCESS;
    } else {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "bmdev handle sync api failed, ioclt ret = %d %d\n", ret, __LINE__);
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

bm_status_t bm_reset_tpu(bm_handle_t handle){

  #ifdef USING_CMODEL
  return BM_SUCCESS;
  #else
  int ret;

  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG,
              BMLIB_LOG_ERROR,
              "handle is nullptr %s: %s: %d\n",
              __FILE__,
              __func__,
              __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_FORCE_RESET_TPU, NULL);
    if (ret == 0) {
        return BM_SUCCESS;
    } else {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
              "bmdev force reset tpu failed, ioclt ret = %d %d\n", ret, __LINE__);
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
  #ifdef SOC_MODE
  snprintf(devname, sizeof(devname), "/dev/bm-tpu%d", devid);
  #else
  snprintf(devname, sizeof(devname), "/dev/bm-sophon%d", devid);
  #endif
  fd = open(devname, O_RDWR);
  if (fd < 0) {
    printf("Create BM Handle Failed for dev%d\n", devid);
    return BM_ERR_BUSY;
  }
  ctx->dev_fd = fd;
  ctx->vpp_fd.dev_fd = -1;

  #ifdef SOC_MODE
  fd = open("/dev/ion", O_RDWR);
  if (fd > 0) {
    ctx->ion_fd = fd;
    ctx->heap_cnt = 1;
    // bm_get_carveout_heap_id(ctx);
  }

  #endif
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
  if (ctx->vpp_fd.dev_fd > 0) {
    close(ctx->vpp_fd.dev_fd);
    free(ctx->vpp_fd.name);
  }
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

  #ifdef __linux__
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
                "bmdev get misc info failed, ioclt ret = %d %d\n", ret, __LINE__);
    }

    // long id=0;
    // ret = platform_ioctl(ctx, BMDEV_GET_BMCPU_APP_THREAD, &id);
    // printf("the bmcpu app thread id is %ld\n", id);
    if ( bmcpu_app_live.load() == 0 ) {
    // pid_t bmcpu = fork();
    // printf("the bmcpu app id is %ld\n", bmcpu);
    // if (prctl(PR_SET_NAME, "bmcpu", 0, 0, 0) == -1) {
    //     perror("prctl failed");
    //     exit(EXIT_FAILURE);
    // }
    // if(bmcpu <0){
    //   bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "bmcpu fork failed");
    //   return BM_ERR_FAILURE;
    // }
    // if(bmcpu == 0){
    //   bmcpu_thread();
    //   exit(0);
    // }

      bmcpu_app_live.fetch_add(1);
      pthread_t bmcpu;
      if(pthread_create(&bmcpu, NULL, bmcpu_thread, static_cast<void*>(&ctx->dev_fd)) != 0) {
          perror("pthread_create");
          return BM_ERR_FAILURE;
      }
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO, "bmcpu app thread id is %ld\n", bmcpu);
      pthread_detach(bmcpu);

    // ret = ioctl(ctx->dev_fd, BMDEV_STORE_BMCPU_APP_THREAD, (int)bmcpu);
    // if(ret!=0){
    //   bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
    //         "bmdev store bmcpu app thread id failed, ioclt ret = %d %d\n", ret, __LINE__);
    //   return BM_ERR_FAILURE;
    // }
    }

    // add drvier version check here???? according to sc5 version format
    switch (ctx->misc_info.chipid) {
      case CHIP_ID:
        // do something here.
        break;
      default:
        return BM_ERR_PARAM;
    }
    *handle = ctx;
    #ifdef SMMU_MODE
      bm_enable_iommu(*handle);
    #else
      bm_disable_iommu(*handle);
    #endif
  #endif
  #else
  if (!GetDevicePath(ctx)) {
      free(ctx);
      printf("Create device %d failed", ctx->dev_id);
      return BM_ERR_FAILURE;
  }

  if (!GetDeviceHandle(ctx)) {
      free(ctx->pDeviceInterfaceDetail);
      free(ctx);
      printf("Create device %d failed", ctx->dev_id);
      return BM_ERR_FAILURE;
  }

  ret = platform_ioctl(ctx, BMDEV_GET_MISC_INFO, &ctx->misc_info);
  if (ret == 0) {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG,
                BMLIB_LOG_INFO,
                "driver version is %1d.%1d.%1d\n",
                ctx->misc_info.driver_version >> 16,
                (ctx->misc_info.driver_version >> 8) & 0xff,
                ctx->misc_info.driver_version & 0xff);
      bmlib_log(BMLIB_RUNTIME_LOG_TAG,
                BMLIB_LOG_INFO,
                "the chip id is 0x%x, pcie_soc_mode is %s\n",
                ctx->misc_info.chipid,
                (ctx->misc_info.pcie_soc_mode == 0) ? "PCIE" : "SOC");
  } else {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get misc info failed, ioclt ret = %d %d\n", ret, __LINE__);
  }
  // add drvier version check here???? according to sc5 version format
  switch (ctx->misc_info.chipid) {
      default:
          bmlib_log(
              BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "invalid chip id!\n");
          return BM_ERR_PARAM;
  }
  *handle = ctx;
  bm_disable_iommu(*handle);
  printf("Create sophon device %d success\n", ctx->dev_id);
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
    switch (handle->misc_info.chipid) {
      case CHIP_ID:
        // do something here.
        break;
      default:
        bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "invalid chip id: 0x%x!\n", handle->misc_info.chipid);
        break;
    }
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

  ret = platform_ioctl(handle, BMDEV_GET_PROFILE, profile);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get profile failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_boot_loader_version(bm_handle_t handle, boot_loader_version *version) {
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

  ret = platform_ioctl(handle, BMDEV_GET_VERSION, version);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "bmdev get version failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
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
            "bmdev get smi attr failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_vpu_instant_usage(bm_handle_t handle, int *vpu_usage) {
  #ifdef USING_CMODEL
  UNUSED(handle);

  return BM_SUCCESS;
  #else
  struct bm_smi_attr_t smi_attr;
  unsigned int chip_id;

  bm_get_smi_attr(handle, &smi_attr);
  bm_get_chipid(handle, &chip_id);


  memcpy(vpu_usage, smi_attr.vpu_instant_usage, 5 * sizeof(int));

  return BM_SUCCESS;
  #endif
  }

bm_status_t bm_get_jpu_core_usage(bm_handle_t handle, int *jpu_usage) {
  #ifdef USING_CMODEL
  UNUSED(handle);

  return BM_SUCCESS;
  #else
  struct bm_smi_attr_t smi_attr;
  unsigned int chip_id;

  bm_get_smi_attr(handle, &smi_attr);
  bm_get_chipid(handle, &chip_id);


    memcpy(jpu_usage, smi_attr.jpu_core_usage, 4 * sizeof(int));

  return BM_SUCCESS;
  #endif
  }

bm_status_t bm_get_vpp_instant_usage(bm_handle_t handle, int *vpp_usage) {
  #ifdef USING_CMODEL
  UNUSED(handle);

  return BM_SUCCESS;
  #else
  struct bm_smi_attr_t smi_attr;

  bm_get_smi_attr(handle, &smi_attr);
  memcpy(vpp_usage, smi_attr.vpp_instant_usage, 2 * sizeof(int));

  return BM_SUCCESS;
  #endif
  }

bm_status_t bm_trace_enable(bm_handle_t handle) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);

  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_TRACE_ENABLE, 0);
  if (ret == 0) {
    return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev trance enable failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_trace_disable(bm_handle_t handle) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);

  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_TRACE_DISABLE, 0);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev trance disable failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_traceitem_number(bm_handle_t handle, long long *number) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(number);

  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (number == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "tarce_data is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_TRACEITEM_NUMBER, number);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev tranceitem number failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_trace_dump(bm_handle_t handle, struct bm_trace_item_data *trace_data) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(trace_data);

  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (trace_data == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "tarce_data is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_TRACE_DUMP, trace_data);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev trance dump failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_trace_dump_all(bm_handle_t handle,
                              struct bm_trace_item_data *trace_data) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(trace_data);

  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (trace_data == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "tarce_data is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_TRACE_DUMP_ALL, trace_data);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev trance dump all failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
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
            "bmdev get misc info failed, ioclt ret = %d %d\n", ret, __LINE__);
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
            "bmdev get boot info failed, ioclt ret = %d %d\n", ret, __LINE__);
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
            "bmdev update boot info failed, ioclt ret = %d %d\n", ret, __LINE__);
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
            "tpu clk divider = %d not support\n", val);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_SET_TPU_DIVIDER, &val);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev set tpu divider failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_set_clk_tpu_freq(bm_handle_t handle, int freq) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(freq);

  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_SET_TPU_FREQ, &freq);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev set tpu freq failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_clk_tpu_freq(bm_handle_t handle, int *freq) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(freq);
  *freq = 1000;
  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_GET_TPU_FREQ, freq);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get tpu freq failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
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
            "module reset id = %d is large \n", val);
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
            "bmdevset moudle reset failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_device_time_us(bm_handle_t handle, unsigned long *time_us) {
  int ret;
  #ifdef USING_CMODEL
  UNUSED(handle);
  *time_us = 0;

  return BM_SUCCESS;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_GET_DEVICE_TIME, time_us);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get device time failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
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
            "bmdev set reg failed, ioclt ret = %d %d\n", ret, __LINE__);
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
            "bmdev get reg failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
}

#ifdef __linux__
bm_status_t bm_get_last_api_process_time_us(bm_handle_t    handle,
                                            unsigned long *time_us) {
#else
bm_status_t bm_get_last_api_process_time_us(bm_handle_t handle,
                                            unsigned long long *time_us) {
#endif
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
  #ifdef __linux__
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
  } else
  #endif
  {
  ret = platform_ioctl(handle, BMDEV_GET_DEV_STAT, stat);
    if (ret == 0) {
      return BM_SUCCESS;
    } else {
      bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "get dev stat error, ioclt ret = %d %d\n",
          ret, __LINE__);
      return BM_ERR_FAILURE;
    }
  }
  #endif
  }

#define BMLIB_VPP_TRIGGER_LOG_TAG "bmlib_vpp_trigger"
#define BMLIB_SPACC_TRIGGER_LOG_TAG "bmlib_spacc_trigger"

#if defined(__cplusplus)
extern "C" {
  #endif

  bm_status_t bm_trigger_vpp(bm_handle_t handle, struct vpp_batch_n* batch) {
  #ifdef USING_CMODEL
    UNUSED(handle);
    UNUSED(batch);
    return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_VPP_TRIGGER_LOG_TAG, BMLIB_LOG_ERROR,
        "handle is nullptr %s: %s: %d\n", __FILE__, __func__, __LINE__);
        return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_TRIGGER_VPP, batch);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev trigger vpp failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

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
            "bmdev trigger spacc failed, ioclt ret = %d %d\n", ret, __LINE__);
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
            "bmdev seckey valid failed, ioclt ret = %d %d\n", ret, __LINE__);
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
            "bmdev efuse write failed, ioclt ret = %d %d\n", ret, __LINE__);
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
            "bmdev efuse read failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }



  #if defined(__cplusplus)
  }
#endif

bm_status_t bm_handle_i2c_read(bm_handle_t handle, struct bm_i2c_param *i2c_param) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(i2c_param);

  return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_I2C_READ_SLAVE, i2c_param);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev i2c read slave failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_handle_i2c_write(bm_handle_t handle, struct bm_i2c_param *i2c_param) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(i2c_param);

  return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_I2C_WRITE_SLAVE, i2c_param);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev i2c write slave failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_handle_i2c_access(bm_handle_t handle, struct bm_i2c_smbus_ioctl_info *i2c_buf) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  //UNUSED(i2c_param);

  return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_I2C_SMBUS_ACCESS, i2c_buf);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev i2c smbus access failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_tpu_current(bm_handle_t handle, unsigned int *tpuc) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(tpuc);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (tpuc == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "tpuc is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_TPUC, tpuc);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get tpuc failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_board_max_power(bm_handle_t handle, unsigned int *maxp) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(maxp);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (maxp == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "maxp is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_MAXP, maxp;
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get maxp failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_board_power(bm_handle_t handle, unsigned int *boardp) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(boardp);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (boardp == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "boardp is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_BOARDP, boardp);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get boardp failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_fan_speed(bm_handle_t handle, unsigned int *fan) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(fan);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (fan == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "fan is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  if (0 == platform_ioctl(handle, BMDEV_GET_FAN, fan))
    return BM_SUCCESS;
  else
    return BM_ERR_FAILURE;
  #endif
  }

bm_status_t bm_get_ecc_correct_num(bm_handle_t handle, unsigned long *ecc_correct_num) {

  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(ecc_correct_num);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (ecc_correct_num == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "ecc_correct_num is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_CORRECTN, ecc_correct_num);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get coppectn failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_12v_atx(bm_handle_t handle, int *atx_12v) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(atx_12v);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (atx_12v == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "atx_12v is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_12V_ATX, atx_12v);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get 12v atx failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_product_sn(char *product_sn) {
  #ifdef USING_CMODEL
  UNUSED(product_sn);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
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

  memset (&rd_header, 0, RDBUF_SIZE);
  //open boot1 device
  fd = open(boot1, O_RDWR);
  if (fd < 0) {
    printf("open boot failed when get product sn !\n");
    return BM_ERR_FAILURE;
  }

  cnt = read(fd, &rd_header, RDBUF_SIZE);
  if (cnt !=RDBUF_SIZE) {
    printf("read rdbuf failed!\n");
    close(fd);
    return BM_ERR_DATA;
  }
  snprintf(product_sn, 18, "%s", rd_header.manufacture);
  close(fd);

  return BM_SUCCESS;
  #else
  return BM_NOT_SUPPORTED;
  #endif
  }

bm_status_t bm_get_sn(bm_handle_t handle, char *sn) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(sn);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
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
        printf("open /dev/mmcblk0boot1 failed!\n");
        return BM_ERR_FAILURE;
    }
    cnt = read(fd, sn, 17);
    if (cnt < 0) {
        printf("read /dev/mmcblk0boot1 failed!\n");
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
      printf("open boot failed when get sn!\n");
      return BM_ERR_FAILURE;
    }

    cnt = read(fd, &rd_header, RDBUF_SIZE);
    if (cnt != RDBUF_SIZE) {
      printf("read rdbuf failed!\n");
      close(fd);
      return BM_ERR_DATA;
    }
    snprintf(sn, 18, "%s", rd_header.sn);
    close(fd);
  }
  return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (sn == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "sn is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_SN, sn);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get SN failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
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
          "bmdev get status failed, ioclt ret = %d %d\n", ret, __LINE__);
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
            "bmdev get tpu minclk failed, ioclt ret = %d %d\n", ret, __LINE__);
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
            "bmdev get tpu maxclk failed, ioclt ret = %d %d\n", ret, __LINE__);
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
            "bmdev get driver version failed, ioclt ret = %d %d\n", ret, __LINE__);
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
            "bmdev get board type failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_board_temp(bm_handle_t handle, unsigned int *board_temp) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(board_temp);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (board_temp == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "boardT is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_BOARDT, board_temp);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get boardt failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_chip_temp(bm_handle_t handle, unsigned int *chip_temp) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(chip_temp);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (chip_temp == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "chipT is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_CHIPT, chip_temp);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get chip failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_tpu_power(bm_handle_t handle, float *tpu_power) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(tpu_power);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  u32 tpu_p = 0;
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (tpu_power == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "TPU_P is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_TPU_P, &tpu_p);
  if (ret == 0) {
  #ifdef __linux__
      *tpu_power = ((float)(tpu_p) / (1000 * 1000));
  #else
      *tpu_power = (float)tpu_p;
  #endif
    return BM_SUCCESS;
  }
  else
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get tpu power failed, ioclt ret = %d %d\n", ret, __LINE__);
    return BM_ERR_FAILURE;
  #endif
  }

bm_status_t bm_get_tpu_volt(bm_handle_t handle, unsigned int *tpu_volt) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(tpu_volt);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (tpu_volt == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "TPU_V is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_TPU_V, tpu_volt);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get tpu volt failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_card_id(bm_handle_t handle, unsigned int *card_id) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(card_id);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (card_id == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "card_id is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_CARD_ID, card_id);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get card id failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_get_card_num(unsigned int *card_num) {
  #ifdef USING_CMODEL
  UNUSED(card_num);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  if (card_num == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "card_num is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }
  #ifdef __linux__
  int fd;
  fd = open("/dev/bmdev-ctl", O_RDWR);
  if (fd == -1) return BM_ERR_FAILURE;
  if (ioctl(fd, BMCTL_GET_CARD_NUM, card_num)) {
    close(fd);
    return BM_ERR_FAILURE;
  }
  close(fd);
  #endif
  return BM_SUCCESS;
  #endif
  }

bm_status_t bm_get_chip_num_from_card(unsigned int card_id, unsigned int *chip_num, unsigned int *dev_start_index) {
  #ifdef USING_CMODEL
  UNUSED(card_id);
  UNUSED(chip_num);
  UNUSED(dev_start_index);
  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  if (chip_num == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "chip_num is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }
  if (dev_start_index == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "dev_start_index is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  struct bm_card bmcd;
  #ifdef __linux__
  int fd;
  fd = open("/dev/bmdev-ctl", O_RDWR);
  if (fd == -1) return BM_ERR_FAILURE;
  bmcd.card_index = card_id;
  if (ioctl(fd, BMCTL_GET_CARD_INFO, &bmcd)) {
    close(fd);
    return BM_ERR_FAILURE;
  }
  *chip_num = bmcd.chip_num;
  *dev_start_index = bmcd.dev_start_index;
  close(fd);
  #endif
  return BM_SUCCESS;
  #endif
  }

bm_status_t bm_get_dynfreq_status(bm_handle_t handle, int *dynfreq_status) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(dynfreq_status);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  if (dynfreq_status == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          "dynfreq_status is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = platform_ioctl(handle, BMDEV_GET_DYNFREQ_STATUS, dynfreq_status);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev get dynamic frequence status failed, ioclt ret = %d %d\n",
            ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }

bm_status_t bm_change_dynfreq_status(bm_handle_t handle, int new_status) {
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(new_status);

  return BM_NOT_SUPPORTED;
  #elif SOC_MODE
  return BM_NOT_SUPPORTED;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_CHANGE_DYNFREQ_STATUS, &new_status);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev change dynamic frequence status failed, ioclt ret = %d %d\n",
            ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
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
      case 3:
        *fd = handle->vpp_fd.dev_fd;
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
  #ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(bm_api_cfg_pwr_ctrl);
  return BM_SUCCESS;
  #else
  int ret;
  if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "handle is nullptr %s: %s: %d\n",
    __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  ret = platform_ioctl(handle, BMDEV_PWR_CTRL, bm_api_cfg_pwr_ctrl);
  if (ret == 0) {
  return BM_SUCCESS;
  } else {
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "bmdev pwr ctrl failed, ioclt ret = %d %d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
  }
  #endif
  }
DECL_EXPORT int bm_is_dynamic_loading(bm_handle_t handle) {
  #ifdef USING_CMODEL
  int arch_code = handle->bm_dev->chip_id;
  #else
  int arch_code = handle->misc_info.chipid;
  #endif
  return arch_code == 0x1686a200;
  }
  /*
  bm_status_t bm_vAddr_to_pAddr(bm_handle_t handle, void *virt_addr,unsigned long *p_addr){
    if (handle == nullptr) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
            "handle is nullptr %s: %s: %d\n",
            __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }

  struct virAddrToPhyAddr data={(unsigned long)virt_addr,(unsigned long)(p_addr),getpagesize()};
  bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_INFO,"virt_addr = %lu\n",
          (unsigned long)virt_addr);

  int ret=platform_ioctl(handle, BMDEV_GET_PHYS_ADDR, &data);
  if(ret !=0){
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,"bmdev get phys addr failed, ioclt ret = %d\n",ret);
    return BM_ERR_FAILURE;
  }
  *p_addr=data.phyAddr;
  return BM_SUCCESS;
  }
  */