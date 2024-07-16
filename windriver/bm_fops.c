#include "bm_common.h"
#include "bm_fops.tmh"
#include "bm_attr.h"

//extern dev_t bm_devno_base;
//extern dev_t bm_ctl_devno_base;

//static int bmdev_open(struct inode *inode, struct file *file)
//{
//	struct bm_device_info *bmdi;
//	pid_t open_pid;
//	struct bm_handle_info *h_info;
//	struct bm_thread_info *thd_info = NULL;
//
//	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bmdev_open\n");
//	//bmdi = container_of(inode->i_cdev, struct bm_device_info, cdev);
//
//	//open_pid = current->pid;
//
//	//h_info = kmalloc(sizeof(struct bm_handle_info), GFP_KERNEL);
//	//if (!h_info)
//	//	return -1;
//	//hash_init(h_info->api_htable);
//	//h_info->file = file;
//	h_info->open_pid = open_pid;
//	h_info->gmem_used = 0ULL;
//	h_info->h_send_api_seq = 0ULL;
//	h_info->h_cpl_api_seq = 0ULL;
//	//init_completion(&h_info->h_msg_done);
//	//mutex_init(&h_info->h_api_seq_spinlock);
//
//	//mutex_lock(&bmdi->gmem_info.gmem_mutex);
//	thd_info = bmdrv_create_thread_info(h_info, open_pid);
//	if (!thd_info) {
//		//kfree(h_info);
//		//mutex_unlock(&bmdi->gmem_info.gmem_mutex);
//		return -1;
//	}
//	//mutex_unlock(&bmdi->gmem_info.gmem_mutex);
//
//	//mutex_lock(&bmdi->gmem_info.gmem_mutex);
//	//list_add(&h_info->list, &bmdi->handle_list);
//	//mutex_unlock(&bmdi->gmem_info.gmem_mutex);
//
//	//file->private_data = bmdi;
//
//	//if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
//	//	bm_vpu_open(inode, file);
//	//	bm_jpu_open(inode, file);
//	//}
//	return 0;
//}
//
//static ssize_t bmdev_read(struct file *filp, char  *buf, size_t len, loff_t *ppos)
//{
//#ifndef SOC_MODE
//	return bm_vpu_read(filp, buf, len, ppos);
//#else
//	return -1;
//#endif
//}
//
//static ssize_t bmdev_write(struct file *filp, const char  *buf, size_t len, loff_t *ppos)
//{
//#ifndef SOC_MODE
//	return bm_vpu_write(filp, buf, len, ppos);
//#else
//	return -1;
//#endif
//}
//
//static int bmdev_fasync(int fd, struct file *filp, int mode)
//{
//#ifndef SOC_MODE
//	return bm_vpu_fasync(fd, filp, mode);
//#else
//	return -1;
//#endif
//}
//
//static int bmdev_close(struct inode *inode, struct file *file)
//{
//	struct bm_device_info *bmdi = file->private_data;
//	struct bm_handle_info *h_info;
//
//	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
//		//TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "bmdrv: file list is not found!\n");
//		return -1;
//	}
//
//#ifndef SOC_MODE
//	if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
//		bm_vpu_release(inode, file);
//		bm_jpu_release(inode, file);
//	}
//#endif
//	/* invalidate pending APIs in msgfifo */
//	bmdev_invalidate_pending_apis(bmdi, h_info);
//
//	//mutex_lock(&bmdi->gmem_info.gmem_mutex);
//	bmdrv_delete_thread_info(h_info);
//	list_del(&h_info->list);
//	kfree(h_info);
//	//mutex_unlock(&bmdi->gmem_info.gmem_mutex);
//
//	file->private_data = NULL;
//
//#ifdef USE_RUNTIME_PM
//	pm_runtime_put_sync(bmdi->cinfo.device);
//#endif
//	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bmdev_close\n");
//	return 0;
//}

int bm_ioctl(struct bm_device_info *bmdi,
                  _In_ WDFREQUEST        Request,
                  _In_ size_t            OutputBufferLength,
                  _In_ size_t            InputBufferLength,
                  _In_ u32             IoControlCode)
{
    int ret = 0;
    //
//	if (bmdi->status_over_temp || bmdi->status_pcie) {
//        TraceEvents(TRACE_LEVEL_ERROR,
//                    TRACE_DEVICE,
//                    "the temperature is too high, causing the MCU to power off "
//                    "the board, causing the pcie link to be abnormal\n");
//		return -1;
//	}
//
    switch (IoControlCode) {
        //#ifndef SOC_MODE
        //	case BMDEV_I2C_READ_SLAVE:
        //		ret = bm_i2c_read_slave(bmdi, arg);
        //		break;
        //
        //	case BMDEV_I2C_WRITE_SLAVE:
        //		ret = bm_i2c_write_slave(bmdi, arg);
        //		break;
        //
        //#endif
    case BMDEV_TRIGGER_BMCPU:
        ret = bmdrv_trigger_bmcpu(bmdi, Request);
        break;

    case BMDEV_SET_BMCPU_STATUS:
        ret = bmdrv_set_bmcpu_status(bmdi, Request);
        break;

    case BMDEV_TRIGGER_VPP:
        ret = trigger_vpp(bmdi, Request, OutputBufferLength, InputBufferLength);
        break;

    case BMDEV_MEMCPY:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_MEMCPY] , command code is %d\n", IoControlCode);
        ret = bmdev_memcpy(bmdi, Request);
        break;

    case BMDEV_ALLOC_GMEM:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_ALLOC_GMEM] , command code is %d\n", IoControlCode);
        ret = bmdrv_gmem_ioctl_alloc_mem(
            bmdi, Request, OutputBufferLength, InputBufferLength);
        break;

        //case BMDEV_ALLOC_GMEM_ION:
        //	ret = bmdrv_gmem_ioctl_alloc_mem_ion(bmdi, file, arg);
        //	break;

    case BMDEV_FREE_GMEM:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_FREE_GMEM] , command code is %d\n", IoControlCode);
        ret = bmdrv_gmem_ioctl_free_mem(
            bmdi, Request, OutputBufferLength, InputBufferLength);
        break;

    case BMDEV_TOTAL_GMEM:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_TOTAL_GMEM] , command code is %d\n", IoControlCode);
        ret = bmdrv_gmem_ioctl_total_size(bmdi, Request, OutputBufferLength, InputBufferLength);
        break;

    case BMDEV_AVAIL_GMEM:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_AVAIL_GMEM] , command code is %d\n", IoControlCode);
        ret = bmdrv_gmem_ioctl_avail_size(
            bmdi, Request, OutputBufferLength, InputBufferLength);
        break;


    case BMDEV_SEND_API:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_SEND_API] , command code is %d\n", IoControlCode);
            if (bmdi->status_sync_api == 0) {
                ret = bmdrv_send_api(bmdi, Request, 0);
            }
            else {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "bm-sophon%d: tpu hang\n",
                    bmdi->dev_index);
                ret = -1;
            }
            break;
        }

    case BMDEV_SEND_API_EXT:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_SEND_API_EXT] , command code is %d\n", IoControlCode);
            if (bmdi->status_sync_api == 0) {
                ret = bmdrv_send_api(bmdi, Request, 1);
            }
            else {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "bm-sophon%d: tpu hang\n",
                    bmdi->dev_index);
                ret = -1;
            }
            break;
        }

    case BMDEV_QUERY_API_RESULT:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_QUERY_API_RESULT] , command code is %d\n", IoControlCode);
            if (bmdi->status_sync_api == 0) {
                ret = bmdrv_query_api(bmdi, Request);
            }
            else {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "bm-sophon%d: tpu hang\n",
                    bmdi->dev_index);
                ret = -1;
            }
            break;
        }

    case BMDEV_THREAD_SYNC_API:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_THREAD_SYNC_API] , command code is %d\n", IoControlCode);
            if (bmdi->status_sync_api == 0) {
                ret = bmdrv_thread_sync_api(bmdi, Request);
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d: finished [BMDEV_THREAD_SYNC_API] ret = %d \n", bmdi->dev_index, ret);
                bmdi->status_sync_api = ret;
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "[BMDEV_THREAD_SYNC_API] finished this api!!\n");
            }
            else {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "bm-sophon%d: tpu hang\n",
                    bmdi->dev_index);
                ret = -1;
            }
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "Get out of the ioctl function\n");
            break;
        }

    case BMDEV_HANDLE_SYNC_API:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_HANDLE_SYNC_API] , command code is %d\n", IoControlCode);
            if (bmdi->status_sync_api == 0) {
                ret = bmdrv_handle_sync_api(bmdi, Request);
                bmdi->status_sync_api = ret;
            }
            else {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "bm-sophon%d: tpu hang\n",
                    bmdi->dev_index);
                ret = -1;
            }
            break;
        }

    case BMDEV_DEVICE_SYNC_API:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_DEVICE_SYNC_API] , command code is %d\n", IoControlCode);
            if (bmdi->status_sync_api == 0) {
                ret = bmdrv_device_sync_api(bmdi, Request);
                bmdi->status_sync_api = ret;
            }
            else {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "bm-sophon%d: tpu hang\n",
                    bmdi->dev_index);
                ret = -1;
            }
            break;
        }
    case BMDEV_REQUEST_ARM_RESERVED:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_REQUEST_ARM_RESERVED] , command code is %d\n", IoControlCode);
            size_t bufSize;
            PVOID  DataBuffer;

            NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(u64), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
            }
            RtlCopyMemory(DataBuffer, &bmdi->gmem_info.resmem_info.armreserved_addr, sizeof(u64));

            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u64));
            ret = 0;
        break;
        }

	case BMDEV_RELEASE_ARM_RESERVED:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_RELEASE_ARM_RESERVED] , command code is %d\n", IoControlCode);
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
            ret = 0;
            break;
        }

	case BMDEV_UPDATE_FW_A9:
		{
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_UPDATE_FW_A9] , command code is %d\n", IoControlCode);
			bm_fw_desc fw;

            size_t bufSize;
            PVOID  inDataBuffer;

            NTSTATUS Status = WdfRequestRetrieveInputBuffer(
                Request, sizeof(bm_fw_desc), &inDataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                return -1;
            }
            RtlCopyMemory(&fw, inDataBuffer, sizeof(bm_fw_desc));

#if SYNC_API_INT_MODE == 1
			if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686)
				bm1684_pcie_msi_irq_enable(bmdi);
#endif

			ret = bmdrv_fw_load(bmdi, NULL, &fw);
            if (ret) {
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                return -1;
            } else {
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
                return 0;
            }
			break;
		}
//#ifndef SOC_MODE
	case BMDEV_PROGRAM_A53:
		{
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_PROGRAM_A53] , command code is %d\n", IoControlCode);
			struct bin_buffer bin_buf;
			u8 *kernel_bin_addr;
            size_t bufSize;
            PVOID  inDataBuffer;

            NTSTATUS Status = WdfRequestRetrieveInputBuffer(
                Request,
                sizeof(struct bin_buffer),
                &inDataBuffer,
                &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                return -1;
            }
            RtlCopyMemory(&bin_buf, inDataBuffer, sizeof(struct bin_buffer));

			kernel_bin_addr = MmAllocateNonCachedMemory(bin_buf.size);
            if (!kernel_bin_addr) {
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                return -1;
            }
            RtlCopyMemory(kernel_bin_addr, bin_buf.buf, bin_buf.size);
			bm_spi_init(bmdi);
			ret = bm_spi_flash_program(bmdi, kernel_bin_addr, bin_buf.target_addr, bin_buf.size);
			bm_spi_enable_dmmr(bmdi);
            MmFreeNonCachedMemory(kernel_bin_addr, bin_buf.size);
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
			break;
		}

	case BMDEV_PROGRAM_MCU:
		{
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_PROGRAM_MCU] , command code is %d\n", IoControlCode);
			struct bin_buffer bin_buf;
			u8 *kernel_bin_addr;
			u8 *read_bin_addr;
            size_t bufSize;
            PVOID  inDataBuffer;

            NTSTATUS Status = WdfRequestRetrieveInputBuffer(
                Request,
                sizeof(struct bin_buffer),
                &inDataBuffer,
                &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                return -1;
            }
            RtlCopyMemory(&bin_buf, inDataBuffer, sizeof(struct bin_buffer));

			kernel_bin_addr = MmAllocateNonCachedMemory(bin_buf.size);
            if (!kernel_bin_addr) {
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                return -1;
            }
            RtlCopyMemory(kernel_bin_addr, bin_buf.buf, bin_buf.size);

			ret = bm_mcu_program(bmdi, kernel_bin_addr, bin_buf.size, bin_buf.target_addr);
			if (ret) {
                MmFreeNonCachedMemory(kernel_bin_addr, bin_buf.size);
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                return -1;
			}
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"mcu program offset 0x%x size 0x%x complete\n", 
                bin_buf.target_addr, bin_buf.size);
			bm_mdelay(1500);

            read_bin_addr = MmAllocateNonCachedMemory(bin_buf.size);
            if (!read_bin_addr) {
                MmFreeNonCachedMemory(kernel_bin_addr, bin_buf.size);
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                return -1;
            }
			ret = bm_mcu_read(bmdi, read_bin_addr, bin_buf.size, bin_buf.target_addr);
			if (!ret) {
				ret = memcmp(kernel_bin_addr, read_bin_addr, bin_buf.size);
                if (ret) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,"read after program mcu and check failed!\n");
                    MmFreeNonCachedMemory(kernel_bin_addr, bin_buf.size);
                    MmFreeNonCachedMemory(read_bin_addr, bin_buf.size);
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                } else {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "read after program mcu and check succeeds.\n");
                    MmFreeNonCachedMemory(kernel_bin_addr, bin_buf.size);
                    MmFreeNonCachedMemory(read_bin_addr, bin_buf.size);
                    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
                    return 0;
                }
            } else {
                MmFreeNonCachedMemory(kernel_bin_addr, bin_buf.size);
                MmFreeNonCachedMemory(read_bin_addr, bin_buf.size);
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                return -1;
            }
			break;
		}

	case BMDEV_CHECKSUM_MCU:
		{
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_CHECKSUM_MCU] , command code is %d\n", IoControlCode);
			struct bin_buffer bin_buf;
			unsigned char cksum[16];

            size_t   bufSize;
            PVOID    inDataBuffer;
            NTSTATUS Status = WdfRequestRetrieveInputBuffer(Request, sizeof(struct bin_buffer), &inDataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                return -1;
            }

            size_t outbufSize;
            PVOID  outDataBuffer;
            Status = WdfRequestRetrieveOutputBuffer(Request, sizeof(struct bin_buffer), &outDataBuffer, &outbufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
            }

            RtlCopyMemory(&bin_buf, inDataBuffer, sizeof(struct bin_buffer));

			ret = bm_mcu_checksum(bmdi, bin_buf.target_addr, bin_buf.size, cksum);
            if (ret) {
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                return -1;
            }
            RtlCopyMemory(bin_buf.buf, cksum, sizeof(cksum));
            RtlCopyMemory(outDataBuffer, &bin_buf, sizeof(struct bin_buffer));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
			break;
		}

	case BMDEV_GET_BOOT_INFO:
		{
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_BOOT_INFO] , command code is %d\n", IoControlCode);
			struct bm_boot_info boot_info;
            size_t              bufSize;
            PVOID               DataBuffer;
            NTSTATUS            Status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(boot_info), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                return -1;
            }
			if (bm_spi_flash_get_boot_info(bmdi, &boot_info)) {
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                return -1;
            }
            RtlCopyMemory(DataBuffer, &boot_info, sizeof(boot_info));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(boot_info));
            ret = 0;
			break;
		}

	case BMDEV_UPDATE_BOOT_INFO:
		{
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_UPDATE_BOOT_INFO] , command code is %d\n", IoControlCode);
			struct bm_boot_info boot_info;
			size_t bufSize;
            PVOID  inDataBuffer;

            NTSTATUS Status = WdfRequestRetrieveInputBuffer(
                Request,
                sizeof(struct bm_boot_info),
                &inDataBuffer,
                &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                return -1;
            }
            RtlCopyMemory(&boot_info, inDataBuffer, sizeof(struct bm_boot_info));

			if (bm_spi_flash_update_boot_info(bmdi, &boot_info)) {
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                return -1;
            }
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
			break;
		}

//	case BMDEV_SET_REG:
//		{
//			struct bm_reg reg;
//
//			if (copy_from_user(&reg, (struct bm_reg  *)arg,
//						sizeof(struct bm_reg)))
//				return -1;
//
//			if (bm_set_reg(bmdi, &reg))
//				return -1;
//			break;
//		}
//
//	case BMDEV_GET_REG:
//		{
//			struct bm_reg reg;
//
//			if (copy_from_user(&reg, (struct bm_reg  *)arg,
//						sizeof(struct bm_reg)))
//				return -1;
//
//			if (bm_get_reg(bmdi, &reg))
//				return -1;
//
//			if (copy_to_user((struct bm_reg  *)arg, &reg,
//						sizeof(struct bm_reg)))
//				return -1;
//
//			break;
//		}

	case BMDEV_SN:
        ret = bm_burning_info_sn(bmdi, Request);
		break;
	case BMDEV_MAC0:
        ret = bm_burning_info_mac(bmdi, 0, Request);
		break;
	case BMDEV_MAC1:
        ret = bm_burning_info_mac(bmdi, 1, Request);
		break;
	case BMDEV_BOARD_TYPE:
        ret = bm_burning_info_board_type(bmdi, Request);
		break;
//#endif
	case BMDEV_ENABLE_PERF_MONITOR:
		{
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_ENABLE_PERF_MONITOR] , command code is %d\n", IoControlCode);
			struct bm_perf_monitor perf_monitor;
			size_t bufSize;
            PVOID  inDataBuffer;

            NTSTATUS Status = WdfRequestRetrieveInputBuffer(
                Request,
                sizeof(struct bm_perf_monitor),
                &inDataBuffer,
                &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                return -1;
            }

            RtlCopyMemory(&perf_monitor, inDataBuffer, sizeof(struct bm_perf_monitor));

 			if (bmdev_enable_perf_monitor(bmdi, &perf_monitor)) {
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                ret = -1;
            } else {
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
                ret = 0;
            }
			break;
		}

	case BMDEV_DISABLE_PERF_MONITOR:
		{
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_DISABLE_PERF_MONITOR] , command code is %d\n", IoControlCode);
			struct bm_perf_monitor perf_monitor;
			size_t bufSize;
            PVOID  inDataBuffer;

            NTSTATUS Status = WdfRequestRetrieveInputBuffer(
                Request,
                sizeof(struct bm_perf_monitor),
                &inDataBuffer,
                &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                return -1;
            }

            RtlCopyMemory(&perf_monitor, inDataBuffer, sizeof(struct bm_perf_monitor));

            if (bmdev_disable_perf_monitor(bmdi, &perf_monitor)) {
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                ret = -1;
            } else {
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
                ret = 0;
            }
			break;
		}

//	case BMDEV_GET_DEVICE_TIME:
//		{
//			unsigned long time_us = 0;
//
//			time_us = bmdev_timer_get_time_us(bmdi);
//
//			ret = copy_to_user((unsigned long  *)arg,
//					&time_us, sizeof(unsigned long));
//			break;
//		}

	case BMDEV_GET_PROFILE:
		{
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_PROFILE] , command code is %d\n", IoControlCode);
			struct bm_thread_info *thd_info;
            struct bm_handle_info *   h_info;

			HANDLE                 open_pid = PsGetCurrentThreadId();
            WDFFILEOBJECT file = WdfRequestGetFileObject(Request);
            UNICODE_STRING* fileObjectName = WdfFileObjectGetFileName(file);
            if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "get profile: file list is not found!,fileObject = %S.\n",
                            fileObjectName->Buffer);
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
				return -1;
			}
			WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
            thd_info = bmdrv_find_thread_info(h_info, (pid_t)open_pid);

			if (!thd_info) {
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                ret = -1;
            }
			else{
                size_t bufSize;
                PVOID  DataBuffer;

                NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
                    Request,
                    sizeof(bm_profile_t),
                    &DataBuffer,
                    &bufSize);
                if (!NT_SUCCESS(Status)) {
                    WdfRequestCompleteWithInformation(Request, Status, 0);
                    ret = -1;
                }
                RtlCopyMemory(DataBuffer, &thd_info->profile, sizeof(bm_profile_t));

                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(bm_profile_t));
                ret = 0;
			}
			WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
			break;
		}

    case BMDEV_LOADED_LIB:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_LOADED_LIB] , command code is %d\n", IoControlCode);
        ret = bmdrv_loaded_lib(bmdi, Request, OutputBufferLength, InputBufferLength);
        break;

	case BMDEV_GET_DEV_STAT:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_DEV_STAT] , command code is %d\n", IoControlCode);
		ret = bmdrv_gmem_ioctl_get_dev_stat(bmdi, Request, OutputBufferLength, InputBufferLength);
		break;

	case BMDEV_GET_HEAP_NUM:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_HEAP_NUM] , command code is %d\n", IoControlCode);
		ret = bmdrv_gmem_ioctl_get_heap_num(bmdi, Request, OutputBufferLength, InputBufferLength);
		break;

	case BMDEV_GET_HEAP_STAT_BYTE:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_HEAP_STAT_BYTE] , command code is %d\n", IoControlCode);
		ret = bmdrv_gmem_ioctl_get_heap_stat_byte_by_id(bmdi, Request, OutputBufferLength, InputBufferLength);
		break;

	case BMDEV_TRACE_ENABLE:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_TRACE_ENABLE] , command code is %d\n", IoControlCode);
            ret = bmdev_trace_enable(bmdi, Request);
		break;

	case BMDEV_TRACE_DISABLE:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_TRACE_DISABLE] , command code is %d\n", IoControlCode);
        ret = bmdev_trace_disable(bmdi, Request);
		break;

	case BMDEV_TRACEITEM_NUMBER:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_TRACEITEM_NUMBER] , command code is %d\n", IoControlCode);
		ret = bmdev_traceitem_number(bmdi, Request, OutputBufferLength, InputBufferLength);
		break;

	case BMDEV_TRACE_DUMP:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_TRACE_DUMP] , command code is %d\n", IoControlCode);
		ret = bmdev_trace_dump_one(bmdi,  Request, OutputBufferLength, InputBufferLength);
		break;

//	case BMDEV_TRACE_DUMP_ALL:
//		ret = bmdev_trace_dump_all(bmdi, file, arg);
//		break;
//
	case BMDEV_GET_MISC_INFO:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_MISC_INFO] , command code is %d\n", IoControlCode);
		ret = bmdrv_ioctl_get_misc_info(bmdi, Request,OutputBufferLength, InputBufferLength);
		break;

    case BMDEV_GET_TPUC:
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_TPUC] , command code is %d\n", IoControlCode);
        size_t bufSize;
        PVOID  DataBuffer;

        NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
            Request, sizeof(u32), &DataBuffer, &bufSize);
        if (!NT_SUCCESS(Status)) {
            WdfRequestCompleteWithInformation(Request, Status, 0);
            ret = -1;
            break;
        }

        if (bufSize != OutputBufferLength) {
            TraceEvents(
                TRACE_LEVEL_ERROR,
                TRACE_DEVICE,
                "bmlib: bufSize does not match bufferLength, bufSize=%lld, BufferLength=%lld\n", bufSize, OutputBufferLength);
            WdfRequestCompleteWithInformation(
                Request, STATUS_INFO_LENGTH_MISMATCH, 0);
            ret = -1;
            break;
        }

        RtlCopyMemory(DataBuffer, &bmdi->c_attr.vdd_tpu_curr, sizeof(u32));
        WdfRequestCompleteWithInformation(
            Request, STATUS_SUCCESS, sizeof(u32));

        break;
    }

    case BMDEV_GET_MAXP:
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_MAXP] , command code is %d\n", IoControlCode);
        size_t bufSize;
        PVOID  DataBuffer;

        NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
            Request, sizeof(u32), &DataBuffer, &bufSize);
        if (!NT_SUCCESS(Status)) {
            WdfRequestCompleteWithInformation(Request, Status, 0);
            ret = -1;
            break;
        }

        if (bufSize != OutputBufferLength) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bmlib: bufSize does not match bufferLength, "
                        "bufSize=%lld, BufferLength=%lld\n",
                        bufSize,
                        OutputBufferLength);
            WdfRequestCompleteWithInformation(
                Request, STATUS_INFO_LENGTH_MISMATCH, 0);
            ret = -1;
            break;
        }

        RtlCopyMemory(
            DataBuffer, &bmdi->boot_info.max_board_power, sizeof(u32));
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

        break;
    }

    case BMDEV_GET_BOARDP:
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_BOARDP] , command code is %d\n", IoControlCode);
        u32    board_power = 0;
        size_t bufSize;
        PVOID  DataBuffer;

        NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
            Request, sizeof(u32), &DataBuffer, &bufSize);
        if (!NT_SUCCESS(Status)) {
            WdfRequestCompleteWithInformation(Request, Status, 0);
            ret = -1;
            break;
        }

        if (bufSize != OutputBufferLength) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bmlib: bufSize does not match bufferLength, "
                        "bufSize=%lld, BufferLength=%lld\n",
                        bufSize,
                        OutputBufferLength);
            WdfRequestCompleteWithInformation(
                Request, STATUS_INFO_LENGTH_MISMATCH, 0);
            ret = -1;
            break;
        }

        if (bmdi->c_attr.bm_get_board_power != NULL) {
            bmdi->c_attr.bm_get_board_power(bmdi, &board_power);
        }

        RtlCopyMemory(
            DataBuffer, &board_power, sizeof(u32));
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

        break;
    }

    case BMDEV_GET_FAN:
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_FAN] , command code is %d\n", IoControlCode);
        u32    fan = 0;
        size_t bufSize;
        PVOID  DataBuffer;

        NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
            Request, sizeof(u32), &DataBuffer, &bufSize);
        if (!NT_SUCCESS(Status)) {
            WdfRequestCompleteWithInformation(Request, Status, 0);
            ret = -1;
            break;
        }

        if (bufSize != OutputBufferLength) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bmlib: bufSize does not match bufferLength, "
                        "bufSize=%lld, BufferLength=%lld\n",
                        bufSize,
                        OutputBufferLength);
            WdfRequestCompleteWithInformation(
                Request, STATUS_INFO_LENGTH_MISMATCH, 0);
            ret = -1;
            break;
        }

        if (bmdi->c_attr.bm_get_fan_speed != NULL) {
            fan = bmdi->c_attr.bm_get_fan_speed(bmdi);
        }

        RtlCopyMemory(DataBuffer, &fan, sizeof(u32));
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

        break;
    }

    case BMDEV_GET_CORRECTN:
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_CORRECTN] , command code is %d\n", IoControlCode);
        size_t bufSize;
        PVOID  DataBuffer;

        NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
            Request, sizeof(u64), &DataBuffer, &bufSize);
        if (!NT_SUCCESS(Status)) {
            WdfRequestCompleteWithInformation(Request, Status, 0);
            ret = -1;
            break;
        }

        if (bufSize != OutputBufferLength) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bmlib: bufSize does not match bufferLength, "
                        "bufSize=%lld, BufferLength=%lld\n",
                        bufSize,
                        OutputBufferLength);
            WdfRequestCompleteWithInformation(
                Request, STATUS_INFO_LENGTH_MISMATCH, 0);
            ret = -1;
            break;
        }

        RtlCopyMemory(DataBuffer, &bmdi->cinfo.ddr_ecc_correctN, sizeof(u32));
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

        break;
    }

    case BMDEV_GET_12V_ATX:
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_12V_ATX] , command code is %d\n", IoControlCode);
        u32 atx_curr;
        size_t bufSize;
        PVOID  DataBuffer;

        NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
            Request, sizeof(u32), &DataBuffer, &bufSize);
        if (!NT_SUCCESS(Status)) {
            WdfRequestCompleteWithInformation(Request, Status, 0);
            ret = -1;
            break;
        }

        if (bufSize != OutputBufferLength) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bmlib: bufSize does not match bufferLength, "
                        "bufSize=%lld, BufferLength=%lld\n",
                        bufSize,
                        OutputBufferLength);
            WdfRequestCompleteWithInformation(
                Request, STATUS_INFO_LENGTH_MISMATCH, 0);
            ret = -1;
            break;
        }

        ret = bm_read_mcu_current(bmdi, 0x28, &atx_curr);
        if (ret) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bmlib: read 12v_atx failed, 12v_atx=%d\n",
                        atx_curr);
            WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
            ret = -1;
            break;
        }

        RtlCopyMemory(DataBuffer, &atx_curr, sizeof(u32));
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

        break;
    }

    case BMDEV_GET_SN:
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_SN] , command code is %d\n", IoControlCode);
        char   card_sn[18];
        size_t bufSize;
        PVOID  DataBuffer;

        NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
            Request, sizeof(card_sn), &DataBuffer, &bufSize);
        if (!NT_SUCCESS(Status)) {
            WdfRequestCompleteWithInformation(Request, Status, 0);
            ret = -1;
            break;
        }

        if (bufSize != OutputBufferLength) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bmlib: bufSize does not match bufferLength, "
                        "bufSize=%lld, BufferLength=%lld\n",
                        bufSize,
                        OutputBufferLength);
            WdfRequestCompleteWithInformation(
                Request, STATUS_INFO_LENGTH_MISMATCH, 0);
            ret = -1;
            break;
        }

        ret = bm_get_sn(bmdi, card_sn);
        if (ret) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bmlib: read sn failed, sn=%s\n",
                        card_sn);
            WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
            ret = -1;
            break;
        }

        RtlCopyMemory(DataBuffer, card_sn, sizeof(card_sn));
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(card_sn));

        break;
    }

    case BMDEV_GET_STATUS:
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_STATUS] , command code is %d\n", IoControlCode);
        size_t bufSize;
        PVOID  DataBuffer;

        NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
            Request, sizeof(s32), &DataBuffer, &bufSize);
        if (!NT_SUCCESS(Status)) {
            WdfRequestCompleteWithInformation(Request, Status, 0);
            ret = -1;
            break;
        }

        if (bufSize != OutputBufferLength) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bmlib: bufSize does not match bufferLength, "
                        "bufSize=%lld, BufferLength=%lld\n",
                        bufSize,
                        OutputBufferLength);
            WdfRequestCompleteWithInformation(
                Request, STATUS_INFO_LENGTH_MISMATCH, 0);
            ret = -1;
            break;
        }

        RtlCopyMemory(DataBuffer, &bmdi->status, sizeof(s32));
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(s32));
        break;
    }

    case BMDEV_GET_TPU_MAXCLK:
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_TPU_MAXCLK] , command code is %d\n", IoControlCode);
        size_t bufSize;
        PVOID  DataBuffer;

        NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
            Request, sizeof(u32), &DataBuffer, &bufSize);
        if (!NT_SUCCESS(Status)) {
            WdfRequestCompleteWithInformation(Request, Status, 0);
            ret = -1;
            break;
        }

        if (bufSize != OutputBufferLength) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bmlib: bufSize does not match bufferLength, "
                        "bufSize=%lld, BufferLength=%lld\n",
                        bufSize,
                        OutputBufferLength);
            WdfRequestCompleteWithInformation(
                Request, STATUS_INFO_LENGTH_MISMATCH, 0);
            ret = -1;
            break;
        }

        RtlCopyMemory(DataBuffer, &bmdi->boot_info.tpu_max_clk, sizeof(u32));
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));
        break;
    }

        //	case BMDEV_SET_TPU_DIVIDER:
//		if (bmdi->misc_info.pcie_soc_mode == BM_DRV_SOC_MODE)
//			ret = -1;
//		else {
//			ExAcquireFastMutex(&bmdi->clk_reset_mutex);
//			ret = bmdev_clk_ioctl_set_tpu_divider(bmdi, arg);
//			ExReleaseFastMutex(&bmdi->clk_reset_mutex);
//		}
//		break;
//
//	case BMDEV_SET_TPU_FREQ:
//		ExAcquireFastMutex(&bmdi->clk_reset_mutex);
//		ret = bmdev_clk_ioctl_set_tpu_freq(bmdi, arg);
//		ExReleaseFastMutex(&bmdi->clk_reset_mutex);
//		break;
//
//	case BMDEV_GET_TPU_FREQ:
//		ExAcquireFastMutex(&bmdi->clk_reset_mutex);
//		ret = bmdev_clk_ioctl_get_tpu_freq(bmdi, arg);
//		ExReleaseFastMutex(&bmdi->clk_reset_mutex);
//		break;
//
	case BMDEV_SET_MODULE_RESET:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_SET_MODULE_RESET] , command code is %d\n", IoControlCode);
		if (bmdi->misc_info.pcie_soc_mode == BM_DRV_SOC_MODE)
			ret = -1;
		else {
			ExAcquireFastMutex(&bmdi->clk_reset_mutex);
			ret = bmdev_clk_ioctl_set_module_reset(bmdi, Request);
            ExReleaseFastMutex(&bmdi->clk_reset_mutex);
		}
		break;

//#ifndef SOC_MODE
//	case BMDEV_BASE64_PREPARE:
//		{
//			struct ce_base  test_base;
//
//			switch (bmdi->cinfo.chip_id) {
//			case 0x1682:
//                    TraceEvents(TRACE_LEVEL_INFORMATION,
//                                TRACE_DEVICE,
//                                "bm1682 not supported!\n");
//				break;
//			case 0x1684:
//				ret = copy_from_user(&test_base, (struct ce_base *)arg,
//						sizeof(struct ce_base));
//				if (ret) {
//                    TraceEvents(
//                        TRACE_LEVEL_ERROR, TRACE_DEVICE, "s2d failed\n");
//					return -1;
//				}
//				base64_prepare(bmdi, test_base);
//				break;
//			}
//			break;
//		}
//
//	case BMDEV_BASE64_START:
//		{
//			switch (bmdi->cinfo.chip_id) {
//			case 0x1682:
//                    TraceEvents(TRACE_LEVEL_INFORMATION,
//                                TRACE_DEVICE,
//                                "bm1682 not supported!\n");
//				break;
//			case 0x1684:
//				ret = 0;
//				base64_start(bmdi);
//				if (ret)
//					return -1;
//                TraceEvents(
//                    TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "setting ready\n");
//				break;
//			}
//			break;
//		}

	case BMDEV_BASE64_CODEC:
		{
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_BASE64_CODEC] , command code is %d\n", IoControlCode);
			struct ce_base test_base;

			switch (bmdi->cinfo.chip_id) {
			case 0x1682:
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bm1682 not supported!\n");
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    ret = -1;
				break;
            case 0x1684:
			case 0x1686:
                    size_t bufSize;
                    PVOID  DataBuffer;
                    NTSTATUS Status = WdfRequestRetrieveInputBuffer(Request, sizeof(struct ce_base), &DataBuffer, &bufSize);
                    if (!NT_SUCCESS(Status)) {
                        WdfRequestCompleteWithInformation(Request, Status, 0);
                        ret = -1;
                        break;
                    }
                    RtlCopyMemory(&test_base, DataBuffer, sizeof(struct ce_base));

                    ExAcquireFastMutex(&bmdi->spacc_mutex);
				    base64_prepare(bmdi, test_base);
				    base64_start(bmdi);
                    ExReleaseFastMutex(&bmdi->spacc_mutex);

                    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
                    ret = 0;
				    break;
            default:
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "chipid not supported!\n");
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                ret = -1;
                break;
			}
			break;
		}

//#endif
//
//#ifdef SOC_MODE
//	case BMDEV_INVALIDATE_GMEM:
//		{
//			u64 arg64;
//			u32 arg32l, arg32h;
//
//			if (get_user(arg64, (u64  *)arg)) {
//				dev_dbg(bmdi->dev, "cmd 0x%x get user failed\n", cmd);
//				return -1;
//			}
//			arg32l = (u32)arg64;
//			arg32h = (u32)((arg64 >> 32) & 0xffffffff);
//			bmdrv_gmem_invalidate(bmdi, ((unsigned long)arg32h)<<6, arg32l);
//			break;
//		}
//
//	case BMDEV_FLUSH_GMEM:
//		{
//			u64 arg64;
//			u32 arg32l, arg32h;
//
//			if (get_user(arg64, (u64  *)arg)) {
//				dev_dbg(bmdi->dev, "cmd 0x%x get user failed\n", cmd);
//				return -1;
//			}
//
//			arg32l = (u32)arg64;
//			arg32h = (u32)((arg64 >> 32) & 0xffffffff);
//			bmdrv_gmem_flush(bmdi, ((unsigned long)arg32h)<<6, arg32l);
//			break;
//		}
//#endif
//

	case BMDEV_GET_TPU_MINCLK:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_TPU_MINCLK] , command code is %d\n", IoControlCode);
            size_t bufSize;
            PVOID  DataBuffer;

            NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(u32), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
                break;
            }

            if (bufSize != OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bmlib: bufSize does not match bufferLength, "
                            "bufSize=%lld, BufferLength=%lld\n",
                            bufSize,
                            OutputBufferLength);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_INFO_LENGTH_MISMATCH, 0);
                ret = -1;
                break;
            }

            RtlCopyMemory(DataBuffer, &bmdi->boot_info.tpu_min_clk, sizeof(u32));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

            break;
        }

    case BMDEV_GET_BOARD_TYPE:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_BOARD_TYPE] , command code is %d\n", IoControlCode);
            char board_name[20];
            char board_type[20];
            size_t bufSize;
            PVOID  DataBuffer;

            NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(board_name), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
                break;
            }

            if (bufSize != OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bmlib: bufSize does not match bufferLength, "
                            "bufSize=%lld, BufferLength=%lld\n",
                            bufSize,
                            OutputBufferLength);
                WdfRequestCompleteWithInformation(Request, STATUS_INFO_LENGTH_MISMATCH, 0);
                ret = -1;
                break;
            }
		    bm1684_get_board_type_by_id(bmdi, board_type, BM1684_BOARD_TYPE(bmdi));
		    _snprintf(board_name, 20, "1684-%s", board_type);
            RtlCopyMemory(DataBuffer, board_name, sizeof(board_name));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(board_name));

            break;
        }

    case BMDEV_GET_BOARDT:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_BOARDT] , command code is %d\n", IoControlCode);
            size_t bufSize;
            PVOID  DataBuffer;

            NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(u32), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
                break;
            }

            if (bufSize != OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bmlib: bufSize does not match bufferLength, "
                            "bufSize=%lld, BufferLength=%lld\n",
                            bufSize,
                            OutputBufferLength);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_INFO_LENGTH_MISMATCH, 0);
                ret = -1;
                break;
            }

            int board_temp;
            ret = bmdi->c_attr.bm_get_board_temp(bmdi, &board_temp);
            if (ret)
                return -1;

            RtlCopyMemory(DataBuffer, &board_temp, sizeof(u32));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

            break;
        }

    case BMDEV_GET_CHIPT:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_CHIPT] , command code is %d\n", IoControlCode);
            size_t bufSize;
            PVOID  DataBuffer;

            NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(u32), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
                break;
            }

            if (bufSize != OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bmlib: bufSize does not match bufferLength, "
                            "bufSize=%lld, BufferLength=%lld\n",
                            bufSize,
                            OutputBufferLength);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_INFO_LENGTH_MISMATCH, 0);
                ret = -1;
                break;
            }

            int chip_temp;
            ret = bmdi->c_attr.bm_get_chip_temp(bmdi, &chip_temp);
            if (ret)
                return -1;

            RtlCopyMemory(DataBuffer, &chip_temp, sizeof(u32));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

            break;
        }

    case BMDEV_GET_TPU_P:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_TPU_P] , command code is %d\n", IoControlCode);
            size_t bufSize;
            PVOID  DataBuffer;

            NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(u32), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
                break;
            }

            if (bufSize != OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bmlib: bufSize does not match bufferLength, "
                            "bufSize=%lld, BufferLength=%lld\n",
                            bufSize,
                            OutputBufferLength);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_INFO_LENGTH_MISMATCH, 0);
                ret = -1;
                break;
            }

            RtlCopyMemory(DataBuffer, &bmdi->c_attr.tpu_power, sizeof(u32));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

            break;
        }

    case BMDEV_GET_TPU_V:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_TPU_V] , command code is %d\n", IoControlCode);
            size_t bufSize;
            PVOID  DataBuffer;

            NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(u32), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
                break;
            }

            if (bufSize != OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bmlib: bufSize does not match bufferLength, "
                            "bufSize=%lld, BufferLength=%lld\n",
                            bufSize,
                            OutputBufferLength);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_INFO_LENGTH_MISMATCH, 0);
                ret = -1;
                break;
            }

            RtlCopyMemory(
                DataBuffer, &bmdi->c_attr.vdd_tpu_volt, sizeof(u32));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));
            break;
        }

    case BMDEV_GET_CARD_ID:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_CARD_ID] , command code is %d\n", IoControlCode);
            size_t bufSize;
            PVOID  DataBuffer;

            NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(u32), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
                break;
            }

            if (bufSize != OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bmlib: bufSize does not match bufferLength, "
                            "bufSize=%lld, BufferLength=%lld\n",
                            bufSize,
                            OutputBufferLength);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_INFO_LENGTH_MISMATCH, 0);
                ret = -1;
                break;
            }

            RtlCopyMemory(DataBuffer, &bmdi->bmcd->card_index, sizeof(u32));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

            break;
        }

    case BMDEV_GET_DYNFREQ_STATUS:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_DYNFREQ_STATUS] , command code is %d\n", IoControlCode);
            size_t bufSize;
            PVOID  DataBuffer;

            NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(s32), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
                break;
            }

            if (bufSize != OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bmlib: bufSize does not match bufferLength, "
                            "bufSize=%lld, BufferLength=%lld\n",
                            bufSize,
                            OutputBufferLength);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_INFO_LENGTH_MISMATCH, 0);
                ret = -1;
                break;
            }

            RtlCopyMemory(DataBuffer, &bmdi->enable_dyn_freq, sizeof(s32));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(s32));

            break;
        }

    case BMDEV_CHANGE_DYNFREQ_STATUS:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_CHANGE_DYNFREQ_STATUS] , command code is %d\n", IoControlCode);
            size_t bufSize;
            PVOID  DataBuffer;

            NTSTATUS Status = WdfRequestRetrieveInputBuffer(
                Request, sizeof(s32), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
                break;
            }

            if (bufSize != InputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bmlib: bufSize does not match bufferLength, "
                            "bufSize=%lld, BufferLength=%lld\n",
                            bufSize,
                            InputBufferLength);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_INFO_LENGTH_MISMATCH, 0);
                ret = -1;
                break;
            }

            RtlCopyMemory(&bmdi->enable_dyn_freq, DataBuffer, sizeof(s32));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(s32));

            break;
        }

    case BMDEV_SET_TPU_FREQ:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_SET_TPU_FREQ] , command code is %d\n", IoControlCode);
            size_t bufSize;
            PVOID  DataBuffer;

            NTSTATUS Status = WdfRequestRetrieveInputBuffer(
                Request, sizeof(s32), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "retrieve input buffer failed!\n");
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
                break;
            }

            if (bufSize != InputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bmlib: bufSize does not match bufferLength, "
                            "bufSize=%lld, BufferLength=%lld\n",
                            bufSize,
                            InputBufferLength);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_INFO_LENGTH_MISMATCH, 0);
                ret = -1;
                break;
            }

            int tpu_freq;
            RtlCopyMemory(
                &tpu_freq, DataBuffer, sizeof(s32));

            ret = bmdrv_clk_set_tpu_target_freq(bmdi, tpu_freq);
            if (ret) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "set tpu freq failed!\n");
                WdfRequestCompleteWithInformation(
                    Request, STATUS_UNSUCCESSFUL, 0);
                ret = -1;
            }
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(s32));

            break;
        }

    case BMDEV_GET_TPU_FREQ:
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Start [BMDEV_GET_TPU_FREQ] , command code is %d\n", IoControlCode);
            size_t bufSize;
            PVOID  DataBuffer;
            int    freq = bmdev_clk_ioctl_get_tpu_freq(bmdi);

            NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(u32), &DataBuffer, &bufSize);
            if (!NT_SUCCESS(Status)) {
                WdfRequestCompleteWithInformation(Request, Status, 0);
                ret = -1;
                break;
            }

            if (bufSize != OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bmlib: bufSize does not match bufferLength, "
                            "bufSize=%lld, BufferLength=%lld\n",
                            bufSize,
                            OutputBufferLength);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_INFO_LENGTH_MISMATCH, 0);
                ret = -1;
                break;
            }

            RtlCopyMemory(DataBuffer, &freq, sizeof(u32));
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

            break;
        }

	default:
		//dev_err(bmdi->dev, "*************Invalid ioctl parameter************\n");
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "bm_fops.c: No such ioctl, cmd is %d\n", IoControlCode);
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
		return -1;
	}
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "bm_fops.c:get out of ioctl, cmd is %d\n", IoControlCode);
	return ret;
}
//
//static long bmdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
//{
//	struct bm_device_info *bmdi = (struct bm_device_info *)file->private_data;
//	int ret = 0;
//
//	if ((_IOC_TYPE(cmd)) == BMDEV_IOCTL_MAGIC)
//		ret = bm_ioctl(file, cmd, arg);
//#ifndef SOC_MODE
//	else if ((_IOC_TYPE(cmd)) == VDI_IOCTL_MAGIC)
//		ret = bm_vpu_ioctl(file, cmd, arg);
//	else if ((_IOC_TYPE(cmd)) == JDI_IOCTL_MAGIC)
//		ret = bm_jpu_ioctl(file, cmd, arg);
//#endif
//	else {
//		dev_dbg(bmdi->dev, "Unknown cmd 0x%x\n", cmd);
//		return -1;
//	}
//	return ret;
//}
//
//static int bmdev_ctl_open(struct inode *inode, struct file *file)
//{
//	struct bm_ctrl_info *bmci;
//
//	bmci = container_of(inode->i_cdev, struct bm_ctrl_info, cdev);
//	file->private_data = bmci;
//	return 0;
//}
//
//static int bmdev_ctl_close(struct inode *inode, struct file *file)
//{
//	file->private_data = NULL;
//	return 0;
//}
//
//static long bmdev_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
//{
//	struct bm_ctrl_info *bmci = file->private_data;
//	int ret = 0;
//
//	switch (cmd) {
//	case BMCTL_GET_DEV_CNT:
//		ret = put_user(bmci->dev_count, (int  *)arg);
//		break;
//
//	case BMCTL_GET_SMI_ATTR:
//		ret = bmctl_ioctl_get_attr(bmci, arg);
//		break;
//
//	case BMCTL_GET_PROC_GMEM:
//		ret = bmctl_ioctl_get_proc_gmem(bmci, arg);
//		break;
//
//	case BMCTL_SET_LED:
//#ifndef SOC_MODE
//		ret = bmctl_ioctl_set_led(bmci, arg);
//#endif
//		break;
//
//	case BMCTL_SET_ECC:
//#ifndef SOC_MODE
//		ret = bmctl_ioctl_set_ecc(bmci, arg);
//#endif
//		break;
//
//		/* test i2c1 slave */
//	case BMCTL_TEST_I2C1:
//		//	ret = bmctl_test_i2c1(bmci, arg);
//		break;
//
//	case BMCTL_DEV_RECOVERY:
//		ret = -1;
//#ifndef SOC_MODE
//		ret = bmctl_ioctl_recovery(bmci, arg);
//#endif
//		break;
//
//	case BMCTL_GET_DRIVER_VERSION:
//		ret = put_user(BM_DRIVER_VERSION, (int  *)arg);
//		break;
//
//	default:
//		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "*************Invalid ioctl parameter************\n");
//		return -1;
//	}
//	return ret;
//}
//
//static const struct file_operations bmdev_fops = {
//	.open = bmdev_open,
//	.read = bmdev_read,
//	.write = bmdev_write,
//	.fasync = bmdev_fasync,
//	.release = bmdev_close,
//	.unlocked_ioctl = bmdev_ioctl,
//	.mmap = bmdev_mmap,
//	.owner = THIS_MODULE,
//};
//
//static const struct file_operations bmdev_ctl_fops = {
//	.open = bmdev_ctl_open,
//	.release = bmdev_ctl_close,
//	.unlocked_ioctl = bmdev_ctl_ioctl,
//	.owner = THIS_MODULE,
//};
//
//int bmdev_register_device(struct bm_device_info *bmdi)
//{
//	bmdi->devno = MKDEV(MAJOR(bm_devno_base), MINOR(bm_devno_base) + bmdi->dev_index);
//	bmdi->dev = device_create(bmdrv_class_get(), bmdi->parent, bmdi->devno, NULL,
//			"%s%d", BM_CDEV_NAME, bmdi->dev_index);
//
//	cdev_init(&bmdi->cdev, &bmdev_fops);
//
//	bmdi->cdev.owner = THIS_MODULE;
//	cdev_add(&bmdi->cdev, bmdi->devno, 1);
//
//	dev_set_drvdata(bmdi->dev, bmdi);
//	dev_dbg(bmdi->dev, "%s\n", __func__);
//	return 0;
//}
//
//int bmdev_unregister_device(struct bm_device_info *bmdi)
//{
//	dev_dbg(bmdi->dev, "%s\n", __func__);
//	cdev_del(&bmdi->cdev);
//	device_destroy(bmdrv_class_get(), bmdi->devno);
//	return 0;
//}
//
//int bmdev_ctl_register_device(struct bm_ctrl_info *bmci)
//{
//	bmci->devno = MKDEV(MAJOR(bm_ctl_devno_base), MINOR(bm_ctl_devno_base));
//	bmci->dev = device_create(bmdrv_class_get(), NULL, bmci->devno, NULL,
//			"%s", BMDEV_CTL_NAME);
//	cdev_init(&bmci->cdev, &bmdev_ctl_fops);
//	bmci->cdev.owner = THIS_MODULE;
//	cdev_add(&bmci->cdev, bmci->devno, 1);
//
//	dev_set_drvdata(bmci->dev, bmci);
//
//	return 0;
//}
//
//int bmdev_ctl_unregister_device(struct bm_ctrl_info *bmci)
//{
//	cdev_del(&bmci->cdev);
//	device_destroy(bmdrv_class_get(), bmci->devno);
//	return 0;
//}
