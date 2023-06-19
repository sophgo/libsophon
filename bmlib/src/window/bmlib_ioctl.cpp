#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <iostream>
#include "bmlib_type.h"
#include"../bmlib_internal.h"
#include"../bmcpu_internal.h"
#include "../bmlib_log.h"
#include "../bmlib_memory.h"

int platform_ioctl(bm_handle_t handle, u32 commandID, void*param){
    BOOL  status        = TRUE;
    DWORD bytesReceived = 0;
    UCHAR outBuffer     = 0;
    switch(commandID)
    {
        case BMDEV_TRIGGER_VPP:
             struct vpp_batch_n {
               s32 num;
               void *cmd;
             };
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_TRIGGER_VPP,
                                     param,
                                     sizeof(struct vpp_batch_n),
                                     NULL,
                                     0,
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_TRIGGER_VPP failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
        break;
        case BMDEV_TRIGGER_BMCPU:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_TRIGGER_BMCPU,
                                     param,
                                     sizeof(int),
                                     NULL, 0, &bytesReceived, NULL);
            if (status == FALSE)
            {
                printf("DeviceIoControl BMDEV_TRIGGER_BMCPU failed 0x%x\n", GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;
        case BMDEV_SET_BMCPU_STATUS:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_SET_BMCPU_STATUS,
                                     param,
                                     sizeof(bm_cpu_status_t),
                                     NULL, 0, &bytesReceived, NULL);
            if (status == FALSE)
            {
                printf("DeviceIoControl BMDEV_TRIGGER_BMCPU failed 0x%x\n", GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;
//-------------------------------bmlib_runtime.cpp-------------------------------
        case BMDEV_SEND_API:
            typedef struct bm_api {
                sglib_api_id_t api_id;
                const u8*   api_addr;
                u32         api_size;
            } bm_api_t;
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_SEND_API,
                                     param,
                                     sizeof(bm_api_t),
                                     NULL,
                                     0,
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_SEND_API failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;
        case BMDEV_SEND_API_EXT:
            status = DeviceIoControl(handle->hDevice,
                                    BMDEV_SEND_API_EXT,
                                    param,
                                    sizeof(bm_api_ext_t),
                                    param,
                                    sizeof(bm_api_ext_t),
                                    &bytesReceived,
                                    NULL);
            if (status == FALSE)
            {
                printf("DeviceIoControl BMDEV_SEND_API_EXT failed 0x%x\n", GetLastError());
                return -1;
            }else
            {
                return 0;
            }
            break;

        case BMDEV_QUERY_API_RESULT:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_QUERY_API_RESULT,
                                     param,
                                     sizeof(bm_api_data_t),
                                     param,
                                     sizeof(bm_api_data_t),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE){
                printf("DeviceIoControl BMDEV_QUERY_API_RESULT failed!\n");
                return -1;
            }else{
                return 0;
            }
            break;
        case BMDEV_THREAD_SYNC_API:
            status           = DeviceIoControl(handle->hDevice,
                             BMDEV_THREAD_SYNC_API,
                             NULL,
                             0,
                             NULL,
                             0,
                             &bytesReceived,
                             NULL);
            if (status == FALSE){//FALSE = 0
                printf("DeviceIoControl BMDEV_THREAD_SYNC_API failed 0x%x\n",GetLastError());
                return -1;
            } else {
                return 0;
            }
        break;

        case BMDEV_GET_MISC_INFO:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_MISC_INFO,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(struct bm_misc_info),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_MISC_INFO failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_TRACE_ENABLE:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_TRACE_ENABLE,
                                     NULL,
                                     0,
                                     NULL,
                                     0,
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_TRACE_ENABLE failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_TRACE_DISABLE:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_TRACE_DISABLE,
                                     NULL,
                                     0,
                                     NULL,
                                     0,
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_TRACE_DISABLE failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_TRACE_DUMP:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_TRACE_DUMP,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(struct bm_trace_item_data),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_TRACE_DUMP failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_PROFILE:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_PROFILE,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(bm_profile_t),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_PROFILE failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

          case BMDEV_ENABLE_PERF_MONITOR:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_ENABLE_PERF_MONITOR,
                                     param,
                                     sizeof(bm_perf_monitor_t),
                                     NULL,
                                     0,
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_ENABLE_PERF_MONITOR failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

          case BMDEV_DISABLE_PERF_MONITOR:
              status = DeviceIoControl(handle->hDevice,
                                       BMDEV_DISABLE_PERF_MONITOR,
                                       param,
                                       sizeof(bm_perf_monitor_t),
                                       NULL,
                                       0,
                                       &bytesReceived,
                                       NULL);
              if (status == FALSE) {
                  printf(
                      "DeviceIoControl BMDEV_DISABLE_PERF_MONITOR failed 0x%x\n",
                      GetLastError());
                  return -1;
              } else {
                  return 0;
              }
          break;

          case BMDEV_REQUEST_ARM_RESERVED:
              status = DeviceIoControl(handle->hDevice,
                                       BMDEV_REQUEST_ARM_RESERVED,
                                       NULL,
                                       0,
                                       param,
                                       sizeof(u64),
                                       &bytesReceived,
                                       NULL);
              if (status == FALSE) {
                  printf("DeviceIoControl BMDEV_REQUEST_ARM_RESERVED failed 0x%x\n",
                         GetLastError());
                  return -1;
              } else {
                  return 0;
              }
              break;

          case BMDEV_RELEASE_ARM_RESERVED:
              status = DeviceIoControl(handle->hDevice,
                                       BMDEV_RELEASE_ARM_RESERVED,
                                       NULL,
                                       0,
                                       NULL,
                                       0,
                                       &bytesReceived,
                                       NULL);
              if (status == FALSE) {
                  printf(
                      "DeviceIoControl BMDEV_RELEASE_ARM_RESERVED failed "
                      "0x%x\n",
                      GetLastError());
                  return -1;
              } else {
                  return 0;
              }
              break;

          case BMDEV_DEVICE_SYNC_API:
              status = DeviceIoControl(handle->hDevice,
                                       BMDEV_DEVICE_SYNC_API,
                                       NULL,
                                       0,
                                       NULL,
                                       0,
                                       &bytesReceived,
                                       NULL);
              if (status == FALSE) {
                  printf(
                      "DeviceIoControl BMDEV_DEVICE_SYNC_API failed "
                      "0x%x\n",
                      GetLastError());
                  return -1;
              } else {
                  return 0;
              }
              break;

         case BMDEV_HANDLE_SYNC_API:
              status = DeviceIoControl(handle->hDevice,
                                       BMDEV_HANDLE_SYNC_API,
                                       NULL,
                                       0,
                                       NULL,
                                       0,
                                       &bytesReceived,
                                       NULL);
              if (status == FALSE) {
                  printf(
                      "DeviceIoControl BMDEV_HANDLE_SYNC_API failed "
                      "0x%x\n",
                      GetLastError());
                  return -1;
              } else {
                  return 0;
              }
              break;

          case BMDEV_TRACEITEM_NUMBER:
             status = DeviceIoControl(handle->hDevice,
                                      BMDEV_TRACEITEM_NUMBER,
                                      NULL,
                                      0,
                                      param,
                                      sizeof(u64),
                                      &bytesReceived,
                                      NULL);
             if (status == FALSE) {
                 printf(
                     "DeviceIoControl BMDEV_TRACEITEM_NUMBER failed 0x%x\n",
                     GetLastError());
                 return -1;
             } else {
                 return 0;
             }
             break;

        case BMDEV_GET_TPUC:
              status = DeviceIoControl(handle->hDevice,
                                       BMDEV_GET_TPUC,
                                       NULL,
                                       0,
                                       param,
                                       sizeof(u32),
                                       &bytesReceived,
                                       NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_TPUC failed 0x%x\n",
                         GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_MAXP:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_MAXP,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_MAXP failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_BOARDP:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_BOARDP,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_BOARDP failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_FAN:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_FAN,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_FAN failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_CORRECTN:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_CORRECTN,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u64),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_CORRECTN failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_12V_ATX:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_12V_ATX,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(s32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_12V_ATX failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_SN:
            char sn[18];
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_SN,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(sn),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_SN failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_STATUS:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_STATUS,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(s32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_STATUS failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_TPU_MAXCLK:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_TPU_MAXCLK,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_TPU_MAXCLK failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

         case BMDEV_GET_BOOT_INFO:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_BOOT_INFO,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(bm_boot_info),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_BOOT_INFO failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

         case BMDEV_UPDATE_BOOT_INFO:
             status = DeviceIoControl(handle->hDevice,
                                      BMDEV_UPDATE_BOOT_INFO,
                                      param,
                                      sizeof(bm_boot_info),
                                      NULL,
                                      0,
                                      &bytesReceived,
                                      NULL);
             if (status == FALSE) {
                 printf("DeviceIoControl BMDEV_UPDATE_BOOT_INFO failed 0x%x\n",
                        GetLastError());
                 return -1;
             } else {
                 return 0;
             }
             break;

         case BMDEV_SN:
             status = DeviceIoControl(handle->hDevice,
                                      BMDEV_SN,
                                      param,
                                      17,
                                      param,
                                      17,
                                      &bytesReceived,
                                      NULL);
             if (status == FALSE) {
                 printf("DeviceIoControl BMDEV_SN failed 0x%x\n",
                        GetLastError());
                 return -1;
             } else {
                 return 0;
             }
             break;

         case BMDEV_BOARD_TYPE:
             status = DeviceIoControl(handle->hDevice,
                                      BMDEV_BOARD_TYPE,
                                      param,
                                      sizeof(char),
                                      param,
                                      sizeof(char),
                                      &bytesReceived,
                                      NULL);
             if (status == FALSE) {
                 printf("DeviceIoControl BMDEV_BOARD_TYPE failed 0x%x\n",
                        GetLastError());
                 return -1;
             } else {
                 return 0;
             }
             break;

         case BMDEV_MAC0:
             status = DeviceIoControl(handle->hDevice,
                                      BMDEV_MAC0,
                                      param,
                                      6,
                                      param,
                                      6,
                                      &bytesReceived,
                                      NULL);
             if (status == FALSE) {
                 printf("DeviceIoControl BMDEV_MAC0 failed 0x%x\n",
                        GetLastError());
                 return -1;
             } else {
                 return 0;
             }
             break;

         case BMDEV_MAC1:
             status = DeviceIoControl(handle->hDevice,
                                      BMDEV_MAC1,
                                      param,
                                      6,
                                      param,
                                      6,
                                      &bytesReceived,
                                      NULL);
             if (status == FALSE) {
                 printf("DeviceIoControl BMDEV_MAC1 failed 0x%x\n",
                        GetLastError());
                 return -1;
             } else {
                 return 0;
             }
             break;

         case BMDEV_PROGRAM_MCU:
             typedef struct bin_buffer {
                 unsigned char *buf;
                 unsigned int   size;
                 unsigned int   target_addr;
             } Bin_buffer;
             status = DeviceIoControl(handle->hDevice,
                                      BMDEV_PROGRAM_MCU,
                                      param,
                                      sizeof(Bin_buffer),
                                      NULL,
                                      0,
                                      &bytesReceived,
                                      NULL);
             if (status == FALSE) {
                 printf("DeviceIoControl BMDEV_PROGRAM_MCU failed 0x%x\n",
                        GetLastError());
                 return -1;
             } else {
                 return 0;
             }
             break;

        case BMDEV_CHECKSUM_MCU:
             status = DeviceIoControl(handle->hDevice,
                                      BMDEV_CHECKSUM_MCU,
                                      param,
                                      sizeof(Bin_buffer),
                                      param,
                                      sizeof(Bin_buffer),
                                      &bytesReceived,
                                      NULL);
             if (status == FALSE) {
                 printf("DeviceIoControl BMDEV_CHECKSUM_MCU failed 0x%x\n",
                        GetLastError());
                 return -1;
             } else {
                 return 0;
             }
             break;

        case BMDEV_PROGRAM_A53:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_PROGRAM_A53,
                                     param,
                                     sizeof(Bin_buffer),
                                     NULL,
                                     0,
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_PROGRAM_A53 failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

            case BMDEV_UPDATE_FW_A9:
            status = DeviceIoControl(handle->hDevice,
                                    BMDEV_UPDATE_FW_A9,
                                    param,
                                    sizeof(struct bm_fw_desc),
                                    NULL,
                                    0,
                                    &bytesReceived,
                                    NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_UPDATE_FW_A9 failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            } break;

//-------------------------------bmlib_memory.cpp-------------------------------
        case BMDEV_MEMCPY:
            status = DeviceIoControl(handle->hDevice,
                             BMDEV_MEMCPY,
                             param,
                             sizeof(bm_memcpy_info_t),
                             &outBuffer,
                             sizeof(outBuffer),
                             &bytesReceived,
                             NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_MEMCPY failed 0x%x\n", GetLastError());
                return -1;
            } else {
                return 0;
            }
        break;
        case BMDEV_TOTAL_GMEM:
            status = DeviceIoControl(handle->hDevice,
                             BMDEV_TOTAL_GMEM,
                             NULL,
                             0,
                             param,
                             sizeof(u64),
                             &bytesReceived,
                             NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_TOTAL_GMEM failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
        break;
        case BMDEV_AVAIL_GMEM:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_AVAIL_GMEM,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u64),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_AVAIL_GMEM failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
        break;
        case BMDEV_ALLOC_GMEM:
            status = DeviceIoControl(handle->hDevice,
                             BMDEV_ALLOC_GMEM,
                             param,
                             sizeof(struct ion_allocation_data),
                             param,
                             sizeof(struct ion_allocation_data),
                             &bytesReceived,
                             NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_ALLOC_GMEM failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
        break;
        case BMDEV_FREE_GMEM:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_FREE_GMEM,
                                     param,
                                     sizeof(bm_device_mem_t),
                                     NULL,
                                     0,
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_FREE_GMEM failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
        break;

        case BMDEV_GET_TPU_MINCLK:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_TPU_MINCLK,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_TPU_MINCLK failed 0x%x",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_DEV_STAT:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_DEV_STAT,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(bm_dev_stat_t),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_DEV_STAT failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_BOARD_TYPE:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_BOARD_TYPE,
                                     NULL,
                                     0,
                                     param,
                                     20,
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_BOARD_TYPE failed 0x%x",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_BOARDT:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_BOARDT,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_BOARDT failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_HEAP_NUM:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_HEAP_NUM,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_HEAP_NUM failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_TPU_P:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_TPU_P,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_TPU_P failed 0x%x",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_TPU_V:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_TPU_V,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_TPU_V failed 0x%x",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_HEAP_STAT_BYTE:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_HEAP_STAT_BYTE,
                                     param,
                                     sizeof(bm_heap_stat_byte_t),
                                     param,
                                     sizeof(bm_heap_stat_byte_t),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_HEAP_STAT_BYTE failed 0x%x\n",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_CARD_ID:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_CARD_ID,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_CARD_ID failed 0x%x",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_DYNFREQ_STATUS:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_DYNFREQ_STATUS,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_DYNFREQ_STATUS failed 0x%x",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_CHANGE_DYNFREQ_STATUS:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_CHANGE_DYNFREQ_STATUS,
                                     param,
                                     sizeof(u32),
                                     NULL,
                                     0,
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_CHANGE_DYNFREQ_STATUS failed 0x%x",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_SET_TPU_FREQ:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_SET_TPU_FREQ,
                                     param,
                                     sizeof(u32),
                                     NULL,
                                     0,
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf(
                    "DeviceIoControl BMDEV_SET_TPU_FREQ failed 0x%x",
                    GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_TPU_FREQ:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_TPU_FREQ,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_TPU_FREQ failed 0x%x",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        case BMDEV_GET_CHIPT:
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_GET_CHIPT,
                                     NULL,
                                     0,
                                     param,
                                     sizeof(u32),
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_GET_CHIPT failed 0x%x",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

            case BMDEV_BASE64_CODEC:
            struct ce_base {
                u64  src;
                u64  dst;
                u64  len;
                int direction;
            };
            status = DeviceIoControl(handle->hDevice,
                                     BMDEV_BASE64_CODEC,
                                     param,
                                     sizeof(struct ce_base),
                                     NULL,
                                     0,
                                     &bytesReceived,
                                     NULL);
            if (status == FALSE) {
                printf("DeviceIoControl BMDEV_BASE64_CODEC failed 0x%x",
                       GetLastError());
                return -1;
            } else {
                return 0;
            }
            break;

        default:
            printf("invalidate ioctl command\n");
            return -1;
        break;
    }

}
