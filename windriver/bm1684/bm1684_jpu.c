#include "../bm_common.h"
#include "bm1684_jpu.tmh"

static int dbg_enable = 1;

#ifndef VM_RESERVED /* for kernel up to 3.7.0 version */
//#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define false 0
#define true 1


static int jpu_reg_base[MAX_NUM_JPU_CORE] = {
	JPU_CORE0_REG,
	JPU_CORE1_REG,
	JPU_CORE2_REG,
	JPU_CORE3_REG,
};


static int jpu_reset[MAX_NUM_JPU_CORE] = {
	JPU_CORE0_RST,
	JPU_CORE1_RST,
	JPU_CORE2_RST,
	JPU_CORE3_RST,
};

static int jpu_irq[MAX_NUM_JPU_CORE] = {
	JPU_CORE0_IRQ,
	JPU_CORE1_IRQ,
	JPU_CORE2_IRQ,
	JPU_CORE3_IRQ,
};

static jpudrv_register_info_t bm_jpu_en_apb_clk[MAX_NUM_JPU_CORE] = {
	{JPU_TOP_CLK_EN_REG1, CLK_EN_APB_VD0_JPEG0_GEN_REG1},
	{JPU_TOP_CLK_EN_REG1, CLK_EN_APB_VD0_JPEG1_GEN_REG1},
	{JPU_TOP_CLK_EN_REG2, CLK_EN_APB_VD1_JPEG0_GEN_REG2},
	{JPU_TOP_CLK_EN_REG2, CLK_EN_APB_VD1_JPEG1_GEN_REG2},
};


static unsigned int get_max_num_jpu_core(struct bm_device_info *bmdi) {
       if(bmdi->cinfo.chip_id == 0x1686) {
            return MAX_NUM_JPU_CORE_BM1686;
       }
       return MAX_NUM_JPU_CORE;
}

static int jpu_core_reset(struct bm_device_info *bmdi, int core)
{
	int val = 0;
	//int offset = bmdi->cinfo.chip_id == 0x1686 ? jpu_reset_1684x[core] : jpu_reset[core];

	val = bm_read32(bmdi, JPU_RST_REG);
	val &= ~(1 << jpu_reset[core]);
	bm_write32(bmdi, JPU_RST_REG, val);
	bm_udelay(10);
	val |= (1 << jpu_reset[core]);
	bm_write32(bmdi, JPU_RST_REG, val);

	return 0;
}

static int jpu_cores_reset(struct bm_device_info *bmdi)
{
	unsigned int core_idx;

	ExAcquireFastMutex(&bmdi->jpudrvctx.jpu_core_lock);
	for (core_idx = 0; core_idx < get_max_num_jpu_core(bmdi); core_idx++)
		jpu_core_reset(bmdi, core_idx);
	ExReleaseFastMutex(&bmdi->jpudrvctx.jpu_core_lock);

	return 0;
}

static int jpu_top_remap(struct bm_device_info *bmdi,
		unsigned int core_idx,
		unsigned long long read_addr,
		unsigned long long write_addr)
{
	int val, read_high, write_high, shift_w, shift_r;

	read_high = read_addr >> 32;
	write_high = write_addr >> 32;

	if (core_idx >= get_max_num_jpu_core(bmdi)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_JPU,"jpu top remap core_idx :%d error\n", core_idx);
		return -1;
	}

    if(bmdi->cinfo.chip_id == 0x1686) {
	    shift_w = core_idx << 4;
	    shift_r = (core_idx << 4) + 4;
    } else { 
	    shift_w = core_idx << 3;
	    shift_r = (core_idx << 3) + 4;
    }

	val = bm_read32(bmdi, JPU_CONTROL_REG);
	val &= ~(7 << shift_w);
	val &= ~(7 << shift_r);
	val |= read_high << shift_r;
	val |= write_high << shift_w;
	bm_write32(bmdi, JPU_CONTROL_REG, val);

	return 0;
}

//int bm_jpu_open(struct inode *inode, struct file *filp)
//{
//	return 0;
//}

int bm_jpu_release(struct bm_device_info *bmdi, _In_ WDFFILEOBJECT filp)
{
	jpudrv_instance_list_t *jil, *n;

	ExAcquireFastMutex(&bmdi->jpudrvctx.jpu_core_lock);

	list_for_each_entry_safe(jil, n, &bmdi->jpudrvctx.inst_head, list, jpudrv_instance_list_t) {
		if (jil->filp == filp) {
			if (jil->inuse == 1) {
				jil->inuse = 0;
				bm_mdelay(100);
				jpu_core_reset(bmdi, jil->core_idx);
				KeReleaseSemaphore(&bmdi->jpudrvctx.jpu_sem, 0, 1, FALSE);
			}
			jil->filp = NULL;
		}
	}
	ExReleaseFastMutex(&bmdi->jpudrvctx.jpu_core_lock);

	return 0;
}

int bm_jpu_ioctl(struct bm_device_info *bmdi, _In_ WDFREQUEST Request, _In_ size_t OutputBufferLength,
                 _In_ size_t InputBufferLength, _In_ u32 IoControlCode)
{
	int ret = 0;
	size_t bufSize= 0;
	NTSTATUS Status = STATUS_SUCCESS;
	jpu_register_info_t reg, reg1;
	PVOID    inDataBuffer;
	PVOID    outDataBuffer;
	HANDLE   open_pid;
	HANDLE   process_id;
	u64 count;

	open_pid = PsGetCurrentThreadId();
	process_id = PsGetCurrentProcessId();

	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBufferLength);

	WDFFILEOBJECT filp = WdfRequestGetFileObject(Request);

	switch (IoControlCode) {
	case JDI_IOCTL_GET_INSTANCE_CORE_INDEX: {
		jpudrv_instance_list_t *jil, *n;
		jpudrv_remap_info_t     info, info1;
		int inuse = -1, core_idx = -1;
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU, "[jpudrv]:GET JPU CORE\n");
		TraceEvents( TRACE_LEVEL_INFORMATION, TRACE_JPU, "get threadid = 0x%llx  processid = 0x%llx\n", (u64)open_pid, (u64)process_id);

		Status = WdfRequestRetrieveInputBuffer(Request, sizeof(jpudrv_remap_info_t), &inDataBuffer, &bufSize);
		if (!NT_SUCCESS(Status)) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveInputBuffer failed\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}
		RtlCopyMemory(&info, inDataBuffer, bufSize);

		count = KeReadStateSemaphore(&bmdi->jpudrvctx.jpu_sem);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU,"[jpudrv]:The Semaphore count is %lld\n", count);
		Status = KeWaitForSingleObject(&bmdi->jpudrvctx.jpu_sem, Executive, KernelMode, TRUE, NULL);
		if (Status != STATUS_SUCCESS) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:KeWaitForSingleObject failed,maybe signal_pending\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}

		ExAcquireFastMutex(&bmdi->jpudrvctx.jpu_core_lock);
		list_for_each_entry_safe(jil, n, &bmdi->jpudrvctx.inst_head, list, jpudrv_instance_list_t) {
			if (!jil->inuse) {
				jil->inuse = 1;
				jil->filp = filp;
				inuse = 1;
				core_idx = jil->core_idx;
				jpu_top_remap(bmdi, core_idx, info.read_addr, info.write_addr);
				bmdi->jpudrvctx.core[core_idx]++;
				bmdi->jpudrvctx.s_jpu_usage_info.jpu_open_status[core_idx] = 1;
				RemoveEntryList(&jil->list);
				InsertTailList(&bmdi->jpudrvctx.inst_head, &jil->list);
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU,"[jpudrv]:core_idx=%d, filp=%p\n", (int)jil->core_idx, filp);
				break;
			}
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU,"[jpudrv]:jil->inuse == 1,filp=%p\n", filp);
		}
		ExReleaseFastMutex(&bmdi->jpudrvctx.jpu_core_lock);

		if (inuse == 1) {
			info.core_idx = core_idx;
			Status = WdfRequestRetrieveOutputBuffer(Request, sizeof(jpudrv_remap_info_t), &outDataBuffer, NULL);
			if (!NT_SUCCESS(Status)) {
				TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveOutputBuffer failed\n");
				WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
				break;
			}
			info1 = info;
			RtlCopyMemory(outDataBuffer, &info1, sizeof(jpudrv_remap_info_t));
			KeClearEvent(&bmdi->jpudrvctx.interrupt_wait_q[core_idx]);
		}
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(jpudrv_remap_info_t));
		break;
	}

	case JDI_IOCTL_CLOSE_INSTANCE_CORE_INDEX: {
		int core_idx;
		jpudrv_instance_list_t *jil, *n;
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU, "[jpudrv]:CLOSE JPU CORE\n");
		TraceEvents( TRACE_LEVEL_INFORMATION, TRACE_JPU, "get threadid = 0x%llx  processid = 0x%llx\n", (u64)open_pid, (u64)process_id);

		Status = WdfRequestRetrieveInputBuffer(Request, sizeof(u32), &inDataBuffer, &bufSize);
		if (!NT_SUCCESS(Status)) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveInputBuffer failed\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}
		RtlCopyMemory(&core_idx, inDataBuffer, bufSize);

		ExAcquireFastMutex(&bmdi->jpudrvctx.jpu_core_lock);

		list_for_each_entry_safe(jil, n, &bmdi->jpudrvctx.inst_head, list, jpudrv_instance_list_t) {
			if (jil->core_idx == core_idx && jil->filp == filp) {
				jil->inuse = 0;
				bmdi->jpudrvctx.s_jpu_usage_info.jpu_open_status[core_idx] = 0;
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU,"[jpudrv]:core_idx=%d, filp=%p\n", core_idx, filp);
				break;
			}
		}

		KeReleaseSemaphore(&bmdi->jpudrvctx.jpu_sem, 0, 1, FALSE);
		ExReleaseFastMutex(&bmdi->jpudrvctx.jpu_core_lock);
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);

		break;
	}

	case JDI_IOCTL_WAIT_INTERRUPT: {
		jpudrv_intr_info_t  info;
		LARGE_INTEGER li;
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU, "[jpudrv]:WAIT FOR JPU INTERRUPT\n");
		TraceEvents( TRACE_LEVEL_INFORMATION, TRACE_JPU, "get threadid = 0x%llx  processid = 0x%llx\n", (u64)open_pid, (u64)process_id);

		Status = WdfRequestRetrieveInputBuffer(Request, sizeof(jpudrv_intr_info_t), &inDataBuffer, &bufSize);
		if (!NT_SUCCESS(Status)) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveInputBuffer failed\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}
		RtlCopyMemory(&info, inDataBuffer, bufSize);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU,"[jpudrv]:info.core_idx %d, info.timeout %d\n",
					info.core_idx, info.timeout);

		li.QuadPart = WDF_REL_TIMEOUT_IN_MS(info.timeout);
		InterlockedExchange(&bmdi->jpudrvctx.s_jpu_usage_info.jpu_busy_status[info.core_idx],1);
		Status = KeWaitForSingleObject(&bmdi->jpudrvctx.interrupt_wait_q[info.core_idx], Executive, KernelMode, TRUE, &li);
		if (Status == STATUS_TIMEOUT) {
			TraceEvents(TRACE_LEVEL_ERROR,TRACE_JPU,"[jpudrv]:jpu wait_event_interruptible timeout,Status %d\n",Status);
			WdfRequestCompleteWithInformation(Request, STATUS_DRIVER_CANCEL_TIMEOUT, 0);
			InterlockedExchange(&bmdi->jpudrvctx.s_jpu_usage_info.jpu_busy_status[info.core_idx],0);
			break;
		}
		if((Status == STATUS_ALERTED) || (Status == STATUS_USER_APC))
		{
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:KeWaitForSingleObject failed, signal_pending\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			InterlockedExchange(&bmdi->jpudrvctx.s_jpu_usage_info.jpu_busy_status[info.core_idx],0);
			break;
		}

		Status = WdfRequestRetrieveOutputBuffer(Request, sizeof(u32), &outDataBuffer, NULL);
		if (!NT_SUCCESS(Status)) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveOutputBuffer failed\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			InterlockedExchange(&bmdi->jpudrvctx.s_jpu_usage_info.jpu_busy_status[info.core_idx],0);
			break;
		}
		RtlCopyMemory(outDataBuffer, &bmdi->jpudrvctx.pic_status[info.core_idx] , sizeof(u32));

		InterlockedExchange(&bmdi->jpudrvctx.s_jpu_usage_info.jpu_busy_status[info.core_idx],0);
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));
		break;
	}

	case JDI_IOCTL_RESET: {
		int core_idx;

		Status = WdfRequestRetrieveInputBuffer(Request, sizeof(u32), &inDataBuffer, &bufSize);
		if (!NT_SUCCESS(Status)) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveInputBuffer failed\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}
		RtlCopyMemory(&core_idx, inDataBuffer, bufSize);

		ExAcquireFastMutex(&bmdi->jpudrvctx.jpu_core_lock);
		jpu_core_reset(bmdi, core_idx);
		ExReleaseFastMutex(&bmdi->jpudrvctx.jpu_core_lock);
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU, "[jpudrv]:RESET JPU CORE %ld\n",core_idx);

		break;
	}

	case JDI_IOCTL_GET_REGISTER_INFO: {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU, "[jpudrv]:GET_REGISTER_INFO\n");

		Status = WdfRequestRetrieveOutputBuffer(Request, sizeof(jpudrv_buffer_t)*get_max_num_jpu_core(bmdi), &outDataBuffer, NULL);
		if (!NT_SUCCESS(Status)) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveOutputBuffer failed\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}
		RtlCopyMemory(outDataBuffer, &bmdi->jpudrvctx.jpu_register, sizeof(jpudrv_buffer_t)*get_max_num_jpu_core(bmdi));
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(jpudrv_buffer_t)*get_max_num_jpu_core(bmdi));

		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU,"[jpudrv]:JDI_IOCTL_GET_REGISTER_INFO: phys_addr==0x%llx, virt_addr=0x%llx, size=%d\n",
				bmdi->jpudrvctx.jpu_register[0].phys_addr,
				bmdi->jpudrvctx.jpu_register[0].virt_addr,
				bmdi->jpudrvctx.jpu_register[0].size);
		break;
	}

	case JDI_IOCTL_WRITE_VMEM: {
		struct dma_params {
			u64 src;
			u64 dst;
			u32 size;
		} dma_param;
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU, "[jpudrv]:JDI_IOCTL_WRITE_VMEM\n");

		if (!bmdi){
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}

		Status = WdfRequestRetrieveInputBuffer(Request, sizeof(dma_param), &inDataBuffer, &bufSize);
		if (!NT_SUCCESS(Status)) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveInputBuffer failed\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}
		RtlCopyMemory(&dma_param, inDataBuffer, bufSize);

		ret = bmdev_memcpy_s2d(bmdi, NULL, dma_param.dst, (void *)dma_param.src,
				dma_param.size, true, KERNEL_NOT_USE_IOMMU);
		if (ret) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:JDI_IOCTL_WRITE_MEM failed\n");
			Status = STATUS_UNSUCCESSFUL;
		}
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);

		break;
	}

	case JDI_IOCTL_READ_VMEM: {
		struct dma_params {
			u64 src;
			u64 dst;
			u32 size;
		} dma_param;
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU, "[jpudrv]:JDI_IOCTL_READ_VMEM\n");

		if (!bmdi){
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}

		Status = WdfRequestRetrieveInputBuffer(Request, sizeof(dma_param), &inDataBuffer, &bufSize);
		if (!NT_SUCCESS(Status)) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveInputBuffer failed\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}
		RtlCopyMemory(&dma_param, inDataBuffer, bufSize);

		ret = bmdev_memcpy_d2s(bmdi, NULL, (void *)dma_param.dst, dma_param.src,
				dma_param.size, true, KERNEL_NOT_USE_IOMMU);
		if (ret != 0) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:JDI_IOCTL_READ_MEM failed\n");
			Status = STATUS_UNSUCCESSFUL;
		}
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);

		break;
	}
	case JDI_IOCTL_READ_REGISTER: {
		Status = WdfRequestRetrieveInputBuffer(Request, sizeof(jpu_register_info_t), &inDataBuffer, &bufSize);
		if (!NT_SUCCESS(Status)) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveInputBuffer failed\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}
		RtlCopyMemory(&reg, inDataBuffer, bufSize);

		reg.data = bm_read32(bmdi, jpu_reg_base[reg.core_idx] + reg.address_offset);

		Status = WdfRequestRetrieveOutputBuffer(Request, sizeof(jpu_register_info_t), &outDataBuffer, NULL);
		if (!NT_SUCCESS(Status)) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveOutputBuffer failed\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}
		reg1 = reg;
		RtlCopyMemory(outDataBuffer, &reg1, sizeof(jpu_register_info_t));
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(jpu_register_info_t));

//		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU, "READ, reg.core_idx 0x%lx, reg.address_offset 0x%lx ,reg.data 0x%lx, jpu_reg_base[reg.core_idx] 0x%lx\n",
//			reg.core_idx,reg.address_offset,reg.data,jpu_reg_base[reg.core_idx]);

		break;
	}
	case JDI_IOCTL_WRITE_REGISTER: {

		Status = WdfRequestRetrieveInputBuffer(Request, sizeof(jpu_register_info_t), &inDataBuffer, &bufSize);
		if (!NT_SUCCESS(Status)) {
			TraceEvents( TRACE_LEVEL_ERROR, TRACE_JPU, "[jpudrv]:WdfRequestRetrieveInputBuffer failed\n");
			WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
			break;
		}
		RtlCopyMemory(&reg, inDataBuffer, bufSize);
		bm_write32(bmdi, jpu_reg_base[reg.core_idx] + reg.address_offset, reg.data);
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);

//		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU, "WRITE, reg.core_idx 0x%lx, reg.address_offset 0x%lx ,reg.data 0x%lx, jpu_reg_base[reg.core_idx] 0x%lx\n",
//			reg.core_idx,reg.address_offset,reg.data,jpu_reg_base[reg.core_idx]);

		break;
	}
	default:
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_JPU,"[jpudrv]:No such ioctl, cmd is %d\n", IoControlCode);
		Status = STATUS_UNSUCCESSFUL;
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
		break;
	}

	return Status;
}

static void bm1684_clear_jpuirq(struct bm_device_info *bmdi, int core)
{
	u32 val = 0;
	val = bm_read32(bmdi, jpu_reg_base[core] + MJPEG_PIC_STATUS_REG);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU,"%!FUNC!, MJPEG_PIC_STATUS_REG: 0x%lx\n",val);

	bmdi->jpudrvctx.pic_status[core] = val;
	if (val != 0)
		bm_write32(bmdi, jpu_reg_base[core] + MJPEG_PIC_STATUS_REG, val);

	return;
}
static void bm_jpu_irq_handler(struct bm_device_info *bmdi)
{
	unsigned int core = 0;
	int irq =  bmdi->cinfo.irq_id;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU,"[jpudrv]:irq handler card :%d irq :%d\n", bmdi->dev_index, irq);
	for (core = 0; core < get_max_num_jpu_core(bmdi); core++) {
		if (bmdi->jpudrvctx.jpu_irq[core] == irq)
			break;
	}
	bm1684_clear_jpuirq(bmdi, core);

	KeSetEvent(&bmdi->jpudrvctx.interrupt_wait_q[core], 0, FALSE);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU,"%!FUNC!, core id %d\n",core);

	return;
}

static void bmdrv_jpu_irq_handler(struct bm_device_info *bmdi)
{
	bm_jpu_irq_handler(bmdi);
}

void bm_jpu_request_irq(struct bm_device_info *bmdi)
{
	unsigned int i = 0;

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++)
		bmdrv_submodule_request_irq(bmdi, bmdi->jpudrvctx.jpu_irq[i], bmdrv_jpu_irq_handler);
}

void bm_jpu_free_irq(struct bm_device_info *bmdi)
{
	unsigned int i = 0;

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++)
		bmdrv_submodule_free_irq(bmdi, bmdi->jpudrvctx.jpu_irq[i]);
}
#if 0
int bm_jpu_addr_judge(unsigned long long addr, struct bm_device_info *bmdi)
{
	int i = 0;

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++) {
		if ((bmdi->jpudrvctx.jpu_register[i].phys_addr >> PAGE_SHIFT) == addr)
			return 0;
	}

	return -1;
}
#endif

static int jpu_reg_base_address_init(struct bm_device_info *bmdi)
{
	unsigned int i = 0;
    int ret = 0;
	u32 offset = 0;
	u64 bar_paddr = 0;
	u8 *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++) {
		//int base_addr = bmdi->cinfo.chip_id == 0x1686 ? jpu_reg_base_1684x[i] : jpu_reg_base[i];

		bm_get_bar_offset(pbar_info, jpu_reg_base[i], &bar_vaddr, &offset);
		bm_get_bar_base(pbar_info, jpu_reg_base[i], &bar_paddr);

		bmdi->jpudrvctx.jpu_register[i].phys_addr = (u64)(bar_paddr + offset);
		bmdi->jpudrvctx.jpu_register[i].virt_addr = (u64)(bar_vaddr + offset);
		bmdi->jpudrvctx.jpu_register[i].size = JPU_REG_SIZE;
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU,"[jpudrv]:jpu reg base addr=0x%llx, virtu base addr=0x%llx, size=%u\n",
				bmdi->jpudrvctx.jpu_register[i].phys_addr,
				bmdi->jpudrvctx.jpu_register[i].virt_addr,
				bmdi->jpudrvctx.jpu_register[i].size);
	}

	bm_get_bar_offset(pbar_info, JPU_CONTROL_REG, &bar_vaddr, &offset);
	bm_get_bar_base(pbar_info, JPU_CONTROL_REG, &bar_paddr);

	bmdi->jpudrvctx.jpu_control_register.phys_addr = (unsigned long long)(bar_paddr + offset);
	bmdi->jpudrvctx.jpu_control_register.virt_addr = (unsigned long long)(bar_vaddr + offset);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU,"[jpudrv]:jpu control reg base addr=0x%llx,virtual base=0x%llx\n",
			bmdi->jpudrvctx.jpu_control_register.phys_addr,
			bmdi->jpudrvctx.jpu_control_register.virt_addr);

	return ret;
}

static char check_jpu_core_busy(struct bm_device_info *bmdi, int coreIdx)
{
	char ret = 0;
	//jpudrv_register_info_t reg_info = bmdi->cinfo.chip_id == 0x1686 ? bm_jpu_en_apb_clk_1684x[coreIdx] : bm_jpu_en_apb_clk[coreIdx];

	unsigned int enable = bm_read32(bmdi, bm_jpu_en_apb_clk[coreIdx].reg);

	if (bmdi->cinfo.chip_id == 0x1682)
		return ret;

	if (enable & (1 << bm_jpu_en_apb_clk[coreIdx].bit_n)){
		if (bmdi->jpudrvctx.s_jpu_usage_info.jpu_open_status[coreIdx] == 1){
			if (InterlockedCompareExchange(&bmdi->jpudrvctx.s_jpu_usage_info.jpu_busy_status[coreIdx],1,1) == 1)
				ret = 1;
		}
	}
	return ret;
}


int bm_jpu_check_usage_info(struct bm_device_info *bmdi)
{
	int ret = 0;
    unsigned int i = 0;

	jpu_statistic_info_t *jpu_usage_info = &bmdi->jpudrvctx.s_jpu_usage_info;


	ExAcquireFastMutex(&bmdi->jpudrvctx.jpu_proc_lock);
	/* update usage */
	for (i = 0; i < get_max_num_jpu_core(bmdi); i++){
		char busy = check_jpu_core_busy(bmdi, i);
		int jpu_core_usage = 0;
		int j;
		jpu_usage_info->jpu_status_array[i][jpu_usage_info->jpu_status_index[i]] = busy;
		jpu_usage_info->jpu_status_index[i]++;
		jpu_usage_info->jpu_status_index[i] %= MAX_JPU_STAT_WIN_SIZE;

		if (busy == 1)
			jpu_usage_info->jpu_working_time_in_ms[i] += jpu_usage_info->jpu_instant_interval / MAX_JPU_STAT_WIN_SIZE;
		jpu_usage_info->jpu_total_time_in_ms[i] += jpu_usage_info->jpu_instant_interval / MAX_JPU_STAT_WIN_SIZE;

		for (j = 0; j < MAX_JPU_STAT_WIN_SIZE; j++)
			jpu_core_usage += jpu_usage_info->jpu_status_array[i][j];

		jpu_usage_info->jpu_core_usage[i] = jpu_core_usage;
	}
	ExReleaseFastMutex(&bmdi->jpudrvctx.jpu_proc_lock);

	return ret;
}

int bm_jpu_set_interval(struct bm_device_info *bmdi, int time_interval)
{
	jpu_statistic_info_t *jpu_usage_info = &bmdi->jpudrvctx.s_jpu_usage_info;

	ExAcquireFastMutex(&bmdi->jpudrvctx.jpu_proc_lock);
	jpu_usage_info->jpu_instant_interval = time_interval;
	ExReleaseFastMutex(&bmdi->jpudrvctx.jpu_proc_lock);

	return 0;
}

static void bm_jpu_usage_info_init(struct bm_device_info *bmdi)
{
	jpu_statistic_info_t *jpu_usage_info = &bmdi->jpudrvctx.s_jpu_usage_info;

	memset(jpu_usage_info, 0, sizeof(jpu_statistic_info_t));

	jpu_usage_info->jpu_instant_interval = 500;

	bm_jpu_check_usage_info(bmdi);

	return;
}

int bmdrv_jpu_init_bm1686(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.chip_id == 0x1686) {
		jpu_reg_base[0] = JPU_CORE0_REG;
		jpu_reg_base[1] = JPU_CORE2_REG;
		jpu_reset[0] = JPU_CORE0_RST;
		jpu_reset[1] = JPU_CORE2_RST;
		jpu_irq[0] = JPU_CORE0_IRQ;
		jpu_irq[1] = JPU_CORE2_IRQ;
		bm_jpu_en_apb_clk[0].reg   = JPU_TOP_CLK_EN_REG1;
		bm_jpu_en_apb_clk[0].bit_n = CLK_EN_APB_VD0_JPEG0_GEN_REG1;
		bm_jpu_en_apb_clk[1].reg   = JPU_TOP_CLK_EN_REG2;
		bm_jpu_en_apb_clk[1].bit_n = CLK_EN_APB_VD1_JPEG0_GEN_REG2;
	}
	return 0;
}


int bmdrv_jpu_init(struct bm_device_info *bmdi)
{
	unsigned int i;
	jpudrv_instance_list_t *jil;
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU, "%!FUNC! enter\n");
	bmdrv_jpu_init_bm1686(bmdi);

	memset(&bmdi->jpudrvctx, 0, sizeof(jpu_drv_context_t));
	memcpy(&bmdi->jpudrvctx.jpu_irq, &jpu_irq, sizeof(jpu_irq));
	InitializeListHead(&bmdi->jpudrvctx.inst_head);
	ExInitializeFastMutex(&bmdi->jpudrvctx.jpu_core_lock);
	KeInitializeSemaphore(&bmdi->jpudrvctx.jpu_sem, get_max_num_jpu_core(bmdi), get_max_num_jpu_core(bmdi));
	ExInitializeFastMutex(&bmdi->jpudrvctx.jpu_proc_lock);

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++)
		KeInitializeEvent(&bmdi->jpudrvctx.interrupt_wait_q[i], SynchronizationEvent, FALSE);

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++) {
		jil = (jpudrv_instance_list_t *)ExAllocatePool(PagedPool,sizeof(jpudrv_instance_list_t));
		if (!jil)
			return STATUS_FAILED_STACK_SWITCH ;

		memset(jil, 0, sizeof(jpudrv_instance_list_t));

		jil->core_idx = i;
		jil->inuse = 0;
		jil->filp = NULL;

		ExAcquireFastMutex(&bmdi->jpudrvctx.jpu_core_lock);
		InsertTailList(&bmdi->jpudrvctx.inst_head, &jil->list);
		ExReleaseFastMutex(&bmdi->jpudrvctx.jpu_core_lock);
	}

	jpu_cores_reset(bmdi);
	jpu_reg_base_address_init(bmdi);
	/* init jpu_usage_info structure */
	bm_jpu_usage_info_init(bmdi);

	return 0;
}

int bmdrv_jpu_exit(struct bm_device_info *bmdi)
{

	if (bmdi->jpudrvctx.jpu_register[0].virt_addr)
		bmdi->jpudrvctx.jpu_register[0].virt_addr = 0;

	if (bmdi->jpudrvctx.jpu_control_register.virt_addr)
		bmdi->jpudrvctx.jpu_control_register.virt_addr = 0;

	jpu_cores_reset(bmdi);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_JPU, "%!FUNC! exit\n");

	return 0;
}
#if 0
static ssize_t info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int len = 0, err = 0, i = 0;
	char data[512] = { 0 };
	struct bm_device_info *bmdi;

	bmdi = PDE_DATA(file_inode(file));
	len = strlen(data);
	sprintf(data + len, "\njpu ctl reg base addr:0x%x\n", JPU_CONTROL_REG);
	len = strlen(data);
	for (i = 0; i < get_max_num_jpu_core(bmdi); i++) {
		sprintf(data + len, "\njpu core[%d] base addr:0x%x, size: 0x%x\n",
				i, jpu_reg_base[i], JPU_REG_SIZE);
		len = strlen(data);
	}
	len = strlen(data);
	if (*ppos >= len)
		return 0;
	err = copy_to_user(buf, data, len);
	if (err)
		return 0;
	*ppos = len;

	return len;
}

static ssize_t info_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	int err = 0, i = 0;
	char data[256] = { 0 };
	unsigned long long val = 0, addr = 0;
	struct bm_device_info *bmdi;

	bmdi = PDE_DATA(file_inode(file));

	err = copy_from_user(data, buf, size);
	if (err)
		return -EFAULT;

	err = kstrtoul(data, 16, &val);
	if (err < 0)
		return -EFAULT;
	if (val == 1)
		dbg_enable = 1;
	else if (val == 0)
		dbg_enable = 0;
	else
		addr = val;

	if (val == 0 || val == 1)
		return size;

	if (addr == JPU_CONTROL_REG)
		goto valid_address;

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++)
		if (addr >= jpu_reg_base[i] &&
				(addr <= jpu_reg_base[i] + JPU_REG_SIZE))
			goto valid_address;

	pr_err("jpu proc get addres: 0x%lx invalid\n", addr);
	return -EFAULT;

valid_address:
	val = bm_read32(bmdi, addr);
	pr_info("jpu get address :0x%lx value: 0x%lx\n", addr, val);
	return size;
}

const struct file_operations bmdrv_jpu_file_ops = {
	.read = info_read,
	.write = info_write,
};
#endif
