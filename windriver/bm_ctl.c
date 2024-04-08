#include "bm_common.h"
#include "bm_ctl.tmh"


VOID bmctl_evtiodevicecontrol(IN WDFQUEUE   Queue,
                            IN WDFREQUEST Request,
                            IN size_t     OutputBufferLength,
                            IN size_t     InputBufferLength,
                            IN ULONG      IoControlCode)
/*++
Routine Description:

    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.

    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

   VOID

--*/
{
    NTSTATUS status = STATUS_SUCCESS;      // Assume success
    PCHAR    inBuf = NULL, outBuf = NULL;  // pointer to Input and output buffer
    size_t           bufSize;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_BMCTL_DEV,
                "bmctl_evtiodevicecontrol enter, IoControlCode = 0x%lx\n",
                IoControlCode);
    //
    // Determine which I/O control code was specified.
    //
    switch (IoControlCode) {
        case BMCTL_GET_DEV_CNT:

            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_BMCTL_DEV,
                        "Called BMCTL_GET_DEV_CNT\n");

            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(int), &outBuf, &bufSize);
            if (!NT_SUCCESS(status)) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            ASSERT(bufSize == OutputBufferLength);
            if (bmci == NULL) {
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, sizeof(int));
                return;
            }
            RtlCopyMemory(outBuf, &bmci->dev_count, sizeof(int));
            WdfRequestCompleteWithInformation(Request, status, sizeof(int));
            break;

        case BMCTL_GET_SMI_ATTR:
        {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_BMCTL_DEV,
                    "Called BMCTL_GET_SMI_ATTR\n");

            int              ret;
            int                 devid;
            struct bm_smi_attr *smi_attr;
            PHYSICAL_ADDRESS   pa;
            pa.QuadPart = 0xFFFFFFFFFFFFFFFF;

            smi_attr = (BM_SMI_PATTR)MmAllocateContiguousMemory(
                sizeof(BM_SMI_ATTR), pa);
            status = WdfRequestRetrieveInputBuffer(
                Request, sizeof(s32), &inBuf, &bufSize);
            if (!NT_SUCCESS(status)) {
                TraceEvents(
                    TRACE_LEVEL_ERROR,
                    TRACE_DRIVER,
                    "bmdev_ctl_ioctl: WdfRequestRetrieveOutputBuffer fail\n");
                WdfRequestCompleteWithInformation(Request, status, 0);
                break;
            }

            status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(BM_SMI_ATTR), &outBuf, &bufSize);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                    "bmdev_ctl_ioctl: WdfRequestRetrieveOutputBuffer fail\n");
                WdfRequestCompleteWithInformation(Request, status, 0);
                break;
            }

            RtlCopyMemory(&devid, inBuf, sizeof(s32));
            smi_attr->dev_id = devid;

            ret = bmctl_ioctl_get_attr(smi_attr);
            if (ret) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DRIVER,
                            "bmctl_ioctl_get_attr failed! ret = %d\n", ret);
                MmFreeContiguousMemory(smi_attr);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_UNSUCCESSFUL, sizeof(BM_SMI_ATTR));
                break;
            }

            RtlCopyMemory(outBuf, smi_attr, sizeof(BM_SMI_ATTR));

            WdfRequestCompleteWithInformation(
                Request, STATUS_SUCCESS, sizeof(BM_SMI_ATTR));
            MmFreeContiguousMemory(smi_attr);
            break;
        }
        case BMCTL_SET_LED:
            TraceEvents(TRACE_LEVEL_VERBOSE,
                        TRACE_BMCTL_DEV,
                        "Called BMCTL_SET_LED\n");

            status = WdfRequestRetrieveInputBuffer(
                Request, sizeof(u32), &inBuf, &bufSize);
            if (!NT_SUCCESS(status)) {
                TraceEvents(
                    TRACE_LEVEL_ERROR,
                    TRACE_DRIVER,
                    "bmctl_ioctl: WdfRequestRetrieveInputBuffer fail\n");
                WdfRequestCompleteWithInformation(Request, status, 0);
                break;
            }

            u32 arg;
            RtlCopyMemory(&arg, inBuf, sizeof(u32));
            if (bmctl_ioctl_set_led(arg)) {
                TraceEvents(
                    TRACE_LEVEL_ERROR,
                    TRACE_DRIVER,
                    "bmctl_ioctl: set_led failed\n");
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                break;
            }

            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
            break;

        case BMCTL_TEST_I2C1: {

            WdfRequestCompleteWithInformation(Request, status, 0);
            break;
        }

        case BMCTL_SET_ECC: {
            TraceEvents(
                TRACE_LEVEL_VERBOSE, TRACE_BMCTL_DEV, "Called BMCTL_SET_ECC\n");

            status = WdfRequestRetrieveInputBuffer(
                Request, sizeof(u32), &inBuf, &bufSize);
            if (!NT_SUCCESS(status)) {
                TraceEvents(
                    TRACE_LEVEL_ERROR,
                    TRACE_DRIVER,
                    "bmctl_ioctl: WdfRequestRetrieveInputBuffer fail\n");
                WdfRequestCompleteWithInformation(Request, status, 0);
                break;
            }

            u32 args;
            RtlCopyMemory(&args, inBuf, sizeof(u32));
            if (bmctl_ioctl_set_ecc(args)) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DRIVER,
                            "bmctl_ioctl: set_ecc failed\n");
                WdfRequestCompleteWithInformation(
                    Request, STATUS_UNSUCCESSFUL, 0);
                break;
            }
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
            break;
        }

        case BMCTL_GET_PROC_GMEM: {
            TraceEvents(
                TRACE_LEVEL_VERBOSE, TRACE_BMCTL_DEV, "Called BMCTL_GET_PROC_GMEM\n");
            int                 ret;
            int                 devid;
            struct bm_smi_proc_gmem *smi_gmem;
            PHYSICAL_ADDRESS    pa;
            pa.QuadPart = 0xFFFFFFFFFFFFFFFF;

            smi_gmem = (struct bm_smi_proc_gmem*)MmAllocateContiguousMemory(
                sizeof(struct bm_smi_proc_gmem), pa);
            status = WdfRequestRetrieveInputBuffer(
                Request, sizeof(s32), &inBuf, &bufSize);
            if (!NT_SUCCESS(status)) {
                TraceEvents(
                    TRACE_LEVEL_ERROR,
                    TRACE_DRIVER,
                    "bmdev_ctl_ioctl: WdfRequestRetrieveOutputBuffer fail\n");
                WdfRequestCompleteWithInformation(Request, status, 0);
                break;
            }

            status = WdfRequestRetrieveOutputBuffer(
                Request, sizeof(struct bm_smi_proc_gmem), &outBuf, &bufSize);
            if (!NT_SUCCESS(status)) {
                TraceEvents(
                    TRACE_LEVEL_ERROR,
                    TRACE_DRIVER,
                    "bmdev_ctl_ioctl: WdfRequestRetrieveOutputBuffer fail\n");
                WdfRequestCompleteWithInformation(Request, status, 0);
                break;
            }

            RtlCopyMemory(&devid, inBuf, sizeof(s32));
            smi_gmem->dev_id = devid;
            ret = bmctl_ioctl_get_proc_gmem(smi_gmem);
            if (ret) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DRIVER,
                            "bmdev_ctl_ioctl: get_proc_gmem failed!\n");
                MmFreeContiguousMemory(smi_gmem);
                WdfRequestCompleteWithInformation(
                    Request, STATUS_UNSUCCESSFUL, 0);
                break;
            }
            RtlCopyMemory(outBuf, smi_gmem, sizeof(struct bm_smi_proc_gmem));
            WdfRequestCompleteWithInformation(
                Request, STATUS_SUCCESS, sizeof(struct bm_smi_proc_gmem));
            MmFreeContiguousMemory(smi_gmem);
            break;
        }

        case BMCTL_DEV_RECOVERY: {

            WdfRequestCompleteWithInformation(Request, status, 0);
            break;
        }

        case BMCTL_GET_DRIVER_VERSION: {
            s32 driver_version;
            status =
                WdfRequestRetrieveOutputBuffer(Request, sizeof(s32), &outBuf, &bufSize);
            if (!NT_SUCCESS(status)) {
                WdfRequestCompleteWithInformation(Request, status, 0);
                break;
            }

            ASSERT(bufSize == OutputBufferLength);

            driver_version = BM_DRIVER_VERSION;
            RtlCopyMemory(outBuf, &driver_version, sizeof(s32));
            WdfRequestCompleteWithInformation(Request, status, sizeof(s32));
            break;
        }

        default:

            //
            // The specified I/O control code is unrecognized by this driver.
            //
            status = STATUS_INVALID_DEVICE_REQUEST;
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_BMCTL_DEV,
                        "ERROR: unrecognized IOCTL %x\n",
                        IoControlCode);
            break;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_BMCTL_DEV,
                "Completing Request %p with status %X",
                Request,
                status);
}

VOID bmctl_evtiostop(_In_ WDFQUEUE   Queue,
                          _In_ WDFREQUEST Request,
                          _In_ ULONG      ActionFlags)
/*++

Routine Description:

    This event is invoked for a power-managed queue before the device leaves the
working state (D0).

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    ActionFlags - A bitwise OR of one or more
WDF_REQUEST_STOP_ACTION_FLAGS-typed flags that identify the reason that the
callback function is being called and whether the request is cancelable.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_QUEUE,
                "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d",
                Queue,
                Request,
                ActionFlags);

    if (ActionFlags & WdfRequestStopActionSuspend) {
        WdfRequestStopAcknowledge(Request, FALSE);  // Don't requeue
    } else if (ActionFlags & WdfRequestStopActionPurge) {
        WdfRequestCancelSentRequest(Request);
    }
    return;
}


VOID bmctl_shutdown(WDFDEVICE Device)
/*++

Routine Description:
    Callback invoked when the machine is shutting down.  If you register for
    a last chance shutdown notification you cannot do the following:
    o Call any pageable routines
    o Access pageable memory
    o Perform any file I/O operations

    If you register for a normal shutdown notification, all of these are
    available to you.

    This function implementation does nothing, but if you had any outstanding
    file handles open, this is where you would close them.

Arguments:
    Device - The device which registered the notification during init

Return Value:
    None

  --*/

{
    UNREFERENCED_PARAMETER(Device);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BMCTL_DEV, "Entered bmctl_shutdown\n");
    return;
}

VOID bmctl_evtdriverunload(IN WDFDRIVER Driver)
/*++
Routine Description:

   Called by the I/O subsystem just before unloading the driver.
   You can free the resources created in the DriverEntry either
   in this routine or in the EvtDriverContextCleanup callback.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

Return Value:

    NTSTATUS

--*/
{
    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BMCTL_DEV, "Entered NonPnpDriverUnload\n");

    return;
}


NTSTATUS
bmdrv_add_ctl_device(IN WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit)
/*++

Routine Description:

    Called by the DriverEntry to create a control-device. This call is
    responsible for freeing the memory for DeviceInit.

Arguments:

    DriverObject - a pointer to the object that represents this device
    driver.

    DeviceInit - Pointer to a driver-allocated WDFDEVICE_INIT structure.

Return Value:

    STATUS_SUCCESS if initialized; an error otherwise.

--*/
{
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_IO_QUEUE_CONFIG   ioQueueConfig;
    WDFQUEUE              queue;
    WDFDEVICE             controlDevice;
    DECLARE_CONST_UNICODE_STRING(ntDeviceName, NTDEVICE_NAME_STRING);
    DECLARE_CONST_UNICODE_STRING(symbolicLinkName, SYMBOLIC_NAME_STRING);

    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_BMCTL_DEV,
                "NonPnpDeviceAdd DeviceInit %p\n",
                DeviceInit);
    //
    // Set exclusive to TRUE so that no more than one app can talk to the
    // control device at any time.
    //
    // WdfDeviceInitSetExclusive(DeviceInit, TRUE);

    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

    status = WdfDeviceInitAssignName(DeviceInit, &ntDeviceName);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_BMCTL_DEV,
                    "WdfDeviceInitAssignName failed %!STATUS!",
                    status);
        goto End;
    }

    WdfControlDeviceInitSetShutdownNotification(
        DeviceInit, bmctl_shutdown, WdfDeviceShutdown);
    //
    // In order to support METHOD_NEITHER Device controls, or
    // NEITHER device I/O type, we need to register for the
    // EvtDeviceIoInProcessContext callback so that we can handle the request
    // in the calling threads context.
    //
    //WdfDeviceInitSetIoInCallerContextCallback(DeviceInit,
    //                                          NonPnpEvtDeviceIoInCallerContext);

    //
    // Specify the size of device context
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            CONTROL_DEVICE_EXTENSION);

    status = WdfDeviceCreate(&DeviceInit, &attributes, &controlDevice);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_BMCTL_DEV,
                    "WdfDeviceCreate failed %!STATUS!",
                    status);
        goto End;
    }

    //
    // Create a symbolic link for the control object so that usermode can open
    // the device.
    //

    status = WdfDeviceCreateSymbolicLink(controlDevice, &symbolicLinkName);

    if (!NT_SUCCESS(status)) {
        //
        // Control device will be deleted automatically by the framework.
        //
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_BMCTL_DEV,
                    "WdfDeviceCreateSymbolicLink failed %!STATUS!",
                    status);
        goto End;
    }

    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                                           WdfIoQueueDispatchSequential);

    ioQueueConfig.EvtIoDeviceControl = bmctl_evtiodevicecontrol;
    ioQueueConfig.EvtIoStop          = bmctl_evtiostop;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    //
    // Since we are using Zw function set execution level to passive so that
    // framework ensures that our Io callbacks called at only passive-level
    // even if the request came in at DISPATCH_LEVEL from another driver.
    //
    // attributes.ExecutionLevel = WdfExecutionLevelPassive;

    //
    // By default, Static Driver Verifier (SDV) displays a warning if it
    // doesn't find the EvtIoStop callback on a power-managed queue.
    // The 'assume' below causes SDV to suppress this warning. If the driver
    // has not explicitly set PowerManaged to WdfFalse, the framework creates
    // power-managed queues when the device is not a filter driver.  Normally
    // the EvtIoStop is required for power-managed queues, but for this driver
    // it is not needed b/c the driver doesn't hold on to the requests or
    // forward them to other drivers. This driver completes the requests
    // directly in the queue's handlers. If the EvtIoStop callback is not
    // implemented, the framework waits for all driver-owned requests to be
    // done before moving in the Dx/sleep states or before removing the
    // device, which is the correct behavior for this type of driver.
    // If the requests were taking an indeterminate amount of time to complete,
    // or if the driver forwarded the requests to a lower driver/another stack,
    // the queue should have an EvtIoStop/EvtIoResume.
    //
    //__analysis_assume(ioQueueConfig.EvtIoStop != 0);
    status = WdfIoQueueCreate(controlDevice,
                              &ioQueueConfig,
                              &attributes,
                              &queue  // pointer to default queue
    );
    //__analysis_assume(ioQueueConfig.EvtIoStop == 0);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_BMCTL_DEV,
                    "WdfIoQueueCreate failed %!STATUS!",
                    status);
        goto End;
    }


    //
    // Control devices must notify WDF when they are done initializing.   I/O is
    // rejected until this call is made.
    //
    WdfControlFinishInitializing(controlDevice);

End:
    //
    // If the device is created successfully, framework would clear the
    // DeviceInit value. Otherwise device create must have failed so we
    // should free the memory ourself.
    //
    if (DeviceInit != NULL) {
        WdfDeviceInitFree(DeviceInit);
    }
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_BMCTL_DEV,
                "NonPnpDeviceAdd success exit\n");
    return status;
}

int bmdrv_init_bmci(WDFDRIVER Driver) {
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BMCTL_DEV, "init bmci!\n");
    PHYSICAL_ADDRESS pa;
    pa.QuadPart      = 0xFFFFFFFFFFFFFFFF;
    bmci             = (PBMCI)MmAllocateContiguousMemory(sizeof(BMCI), pa);
    if (!bmci) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BMCTL_DEV, "bmci alloc failed!\n");
        return -1;
    }
    bmci->ctl_driver = Driver;
    _snprintf_s(bmci->dev_ctl_name, 20, 20, "%s", (char *)NTDEVICE_NAME_STRING);
    bmci->dev_count    = 0;
    return 0;
}

int bmdrv_remove_bmci(void)
{
    if (bmci == NULL) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BMCTL_DEV, "bmci is NULL!\n");
        return -1;
    }
    bmci->ctl_driver = NULL;
    bmci->dev_count  = 0;
    _snprintf_s(bmci->dev_ctl_name, 20, 20, "");
    MmFreeContiguousMemory(bmci);
    bmci = NULL;
	return 0;
}

int bmdrv_ctrl_add_dev()
{
    if (bmci == NULL) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BMCTL_DEV, "bmci is NULL!\n");
        return -1;
    }
    /* record a reference in the bmdev list in bm_ctl */
    bmci->dev_count++;
	return 0;
}

int bmdrv_ctrl_del_dev()
{
    if (bmci == NULL) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BMCTL_DEV, "bmci is NULL!\n");
        return -1;
    }
	bmci->dev_count--;
	return 0;
}

static int bmctl_get_bus_id(struct bm_device_info *bmdi)
{
	return bmdi->misc_info.domain_bdf;
}

extern int find_real_dev_index(int dev_id);

struct bm_device_info *bmctl_get_bmdi(int dev_id)
{
    struct bm_device_info *bmdi;
    PHYSICAL_ADDRESS       pa;
    pa.QuadPart = 0xFFFFFFFFFFFFFFFF;
    PDRIVER_OBJECT pDrvObj;
    PDEVICE_OBJECT DeviceObject;
    WDFDEVICE      device;
    u32            i = 0;
    int            real_index = 0;
    if (bmci == NULL) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DRIVER,
                    "bcmi: invalid bcmi pointer %p!\n", bmci);
        return NULL;
    }

	if (dev_id >= (int)bmci->dev_count) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DRIVER,
                    "bmdrv: invalid device id %d!\n",
                    dev_id);
        return NULL;
    }
    real_index   = find_real_dev_index(dev_id);
    if (real_index == -1) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DRIVER,
                    "find_real_dev_index failed, device id = %d!\n",
                    dev_id);
        return NULL;
    }

    pDrvObj = WdfDriverWdmGetDriverObject(bmci->ctl_driver);
    DeviceObject = pDrvObj->DeviceObject;
    for (i = 0; i < bmci->dev_count+1; i++) {
        if (!DeviceObject) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DRIVER,
                        "bmdrv: bmdi not found! devid is %d\n",
                        dev_id);
            return NULL;
        }
        device = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DRIVER,
                    "bmctl:device = %p, dev_id = %d,real_index = %d\n",
                    device, dev_id,real_index);

        bmdi = Bm_Get_Bmdi(device);
        if (bmdi != NULL){
            if (bmdi->dev_index == real_index) {
                TraceEvents(TRACE_LEVEL_INFORMATION,
                            TRACE_DRIVER,
                            "bmdrv: bmdi has been found,devid = %d,real_index=%d\n",dev_id,real_index);
                return bmdi;
            } else {
                TraceEvents(
                    TRACE_LEVEL_INFORMATION,
                    TRACE_DRIVER,
                    "bmdrv: bmdi index= %d,devid = %d,real_index=%d\n",
                    bmdi->dev_index,
                    dev_id,
                    real_index);
                DeviceObject = DeviceObject->NextDevice;
            }
        } else {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "bmdi has been deleted,dev_id = %d ,count=%d\n",dev_id,i);
            DeviceObject = DeviceObject->NextDevice;
            return NULL;
        }
    }
	return NULL;
}

struct bm_device_info *bmctl_get_card_bmdi(struct bm_device_info *bmdi)
{
	int dev_id = 0;

	if ((bmdi->misc_info.domain_bdf & 0x7) == 0x0)
		return bmdi;

	if ((bmdi->misc_info.domain_bdf & 0x7) == 0x1
		|| (bmdi->misc_info.domain_bdf & 0x7) == 0x2) {
		if ((bmdi->misc_info.domain_bdf & 0x7) == 0x1)
			dev_id = bmdi->dev_index - 1;

		if ((bmdi->misc_info.domain_bdf & 0x7) == 0x2)
			dev_id = bmdi->dev_index - 2;

		if (dev_id < 0 || dev_id >= 0xff) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bmctl_get_card_bmdi fail, dev_id = 0x%x\n",
                        dev_id);
			return NULL;
		}

		return bmctl_get_bmdi(dev_id);
	}

	return NULL;
}

static int bmctl_get_smi_attr(struct bm_smi_attr *pattr)
{
	struct bm_device_info *bmdi;
	struct chip_info *cinfo;
	struct bm_chip_attr *c_attr;

	bmdi = bmctl_get_bmdi(pattr->dev_id);
    if (!bmdi) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "bmctl_get_bmdi fail! bmdi=%p\n",bmdi);
        return -1;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "bmctl_get_bmdi success! devid=%d chip_index=%d\n", pattr->dev_id, bmdi->cinfo.chip_index);


    cinfo            = &bmdi->cinfo;
    c_attr           = &bmdi->c_attr;
	pattr->chip_mode = bmdi->misc_info.pcie_soc_mode;
	if (pattr->chip_mode == 0)
		pattr->domain_bdf = bmctl_get_bus_id(bmdi);
	else
		pattr->domain_bdf = ATTR_NOTSUPPORTED_VALUE;
	pattr->chip_id = bmdi->cinfo.chip_id;
	pattr->status = bmdi->status;

	pattr->mem_total = (int)(bmdrv_gmem_total_size(bmdi)/1024/1024);
	pattr->mem_used = pattr->mem_total - (int)(bmdrv_gmem_avail_size(bmdi)/1024/1024);

	pattr->tpu_util = c_attr->bm_get_npu_util(bmdi);
    //pattr->tpu_util = 0;

	if (c_attr->bm_get_chip_temp != NULL)
		pattr->chip_temp =c_attr->chip_temp;
	else
		pattr->chip_temp = ATTR_NOTSUPPORTED_VALUE;
	if (c_attr->bm_get_board_temp != NULL)
		pattr->board_temp = c_attr->board_temp;
	else
		pattr->board_temp = ATTR_NOTSUPPORTED_VALUE;

	if (c_attr->bm_get_tpu_power != NULL) {
#ifndef SOC_MODE
		pattr->vdd_tpu_volt = c_attr->vdd_tpu_volt;
		pattr->vdd_tpu_curr = c_attr->vdd_tpu_curr;
#endif
		pattr->tpu_power = c_attr->tpu_power;
	} else {
		pattr->tpu_power = ATTR_NOTSUPPORTED_VALUE;
		pattr->vdd_tpu_volt = ATTR_NOTSUPPORTED_VALUE;
		pattr->vdd_tpu_curr = ATTR_NOTSUPPORTED_VALUE;
	}
	if (c_attr->bm_get_board_power != NULL) {
#ifndef SOC_MODE
        pattr->board_power = c_attr->board_power;
        pattr->atx12v_curr = c_attr->atx12v_curr;
#endif
	} else {
		pattr->board_power = ATTR_NOTSUPPORTED_VALUE;
		pattr->atx12v_curr = ATTR_NOTSUPPORTED_VALUE;
	}
#ifndef SOC_MODE
    memcpy(pattr->sn, cinfo->sn, 17);
#else
	memcpy(pattr->sn, "N/A", 3);
#endif

#ifndef SOC_MODE
	if (bmdi->cinfo.chip_id != 0x1682) {
		bm1684_get_board_type_by_id(bmdi, pattr->board_type,
			BM1684_BOARD_TYPE(bmdi));
	}
#else
	strncpy(pattr->board_type, "SOC", 3);
#endif
#ifndef SOC_MODE
	if (bmdi->cinfo.chip_id != 0x1682) {
		if (bmdi->bmcd != NULL) {
			pattr->card_index = bmdi->bmcd->card_index;
			pattr->chip_index_of_card = bmdi->dev_index - bmdi->bmcd->dev_start_index;
			if (pattr->chip_index_of_card < 0 || pattr->chip_index_of_card >  bmdi->bmcd->chip_num)
				pattr->chip_index_of_card = 0;
		}
	}
#else
	pattr->card_index = 0x0;
	pattr->chip_index_of_card = 0x0;
#endif

#ifndef SOC_MODE
	if (c_attr->fan_control)
        pattr->fan_speed = c_attr->fan_speed;
	else
		pattr->fan_speed = ATTR_NOTSUPPORTED_VALUE;
#else
	pattr->fan_speed = ATTR_NOTSUPPORTED_VALUE;
#endif

	switch (pattr->chip_id) {
	case 0x1682:
		pattr->tpu_min_clock = 687;
		pattr->tpu_max_clock = 687;
		pattr->tpu_current_clock = 687;
		pattr->board_max_power = 75;
		break;
	case 0x1684:
		if (pattr->chip_mode == 0) {
			pattr->tpu_min_clock = bmdi->boot_info.tpu_min_clk;
			pattr->tpu_max_clock = bmdi->boot_info.tpu_max_clk;
		} else {
			pattr->tpu_min_clock = 75;
			pattr->tpu_max_clock = 550;
		}

		pattr->tpu_current_clock = c_attr->tpu_current_clock;
		if (pattr->tpu_current_clock < pattr->tpu_min_clock
                           || (pattr->tpu_current_clock > pattr->tpu_max_clock)) {
			pattr->tpu_current_clock = (int)0xFFFFFC00;
		}
		break;
    case 0x1686:
        if (pattr->chip_mode == 0) {
            pattr->tpu_min_clock = bmdi->boot_info.tpu_min_clk;
            pattr->tpu_max_clock = bmdi->boot_info.tpu_max_clk;
        }
        else {
            pattr->tpu_min_clock = 75;
            pattr->tpu_max_clock = 1000;
        }
        pattr->tpu_current_clock = c_attr->tpu_current_clock;
        if ((pattr->tpu_current_clock < pattr->tpu_min_clock) || (pattr->tpu_current_clock > pattr->tpu_max_clock)) {
            pattr->tpu_current_clock = (int)0xFFFFFC00;
        }
        break;
	default:
	 	break;
	 }
	if (pattr->board_power != ATTR_NOTSUPPORTED_VALUE)
		pattr->board_max_power = bmdi->boot_info.max_board_power;
	else
		pattr->board_max_power = ATTR_NOTSUPPORTED_VALUE;
#ifdef SOC_MODE
		pattr->ecc_enable = ATTR_NOTSUPPORTED_VALUE;
		pattr->ecc_correct_num = ATTR_NOTSUPPORTED_VALUE;
#else
	pattr->ecc_enable = bmdi->misc_info.ddr_ecc_enable;
	if (pattr->ecc_enable == 0)
		pattr->ecc_correct_num = ATTR_NOTSUPPORTED_VALUE;
	else
		pattr->ecc_correct_num = bmdi->cinfo.ddr_ecc_correctN;
#endif
	return 0;
}

int bmctl_ioctl_get_attr(struct bm_smi_attr *smi_attr) {
	int ret = 0;

	ret = bmctl_get_smi_attr(smi_attr);
	if (ret)
		return ret;

	return 0;
}

static int bmctl_get_smi_proc_gmem(struct bm_smi_proc_gmem *smi_proc_gmem)
{
	struct bm_device_info *bmdi;
	struct bm_handle_info *h_info;
	int proc_cnt = 0;

	bmdi = bmctl_get_bmdi(smi_proc_gmem->dev_id);
	if (!bmdi)
		return -1;

    WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
	list_for_each_entry(h_info, &bmdi->handle_list, list, struct bm_handle_info) {
		smi_proc_gmem->pid[proc_cnt] = h_info->process_id;
		smi_proc_gmem->gmem_used[proc_cnt] = h_info->gmem_used / 1024 / 1024;
		proc_cnt++;
		if (proc_cnt == 128)
			break;
	}
    WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
	smi_proc_gmem->proc_cnt = proc_cnt;
	return 0;
}

int bmctl_ioctl_get_proc_gmem(struct bm_smi_proc_gmem *smi_gmem) {
	int ret = 0;

	if (!smi_gmem)
		return -1;

	ret = bmctl_get_smi_proc_gmem(smi_gmem);
	if (ret) {
		return ret;
	}

	return 0;
}

int bmctl_ioctl_set_led(unsigned long arg)
{
	int dev_id = arg & 0xff;
	int led_op = (arg >> 8) & 0xff;
    TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_DRIVER,
                "bmdrv: led_op=%d, devid=%d, led_arg=%lu\n",
                led_op, dev_id, arg);
	struct bm_device_info *bmdi = bmctl_get_bmdi(dev_id);
    if (!bmdi)
        return -1;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;

	if (bmdi->cinfo.chip_id == 0x1684 &&
			bmdi->cinfo.platform == DEVICE &&
			c_attr->bm_set_led_status) {
        ExAcquireFastMutex(&c_attr->attr_mutex);
		switch (led_op) {
		case 0:
			c_attr->bm_set_led_status(bmdi, LED_ON);
			c_attr->led_status = LED_ON;
			break;
		case 1:
			c_attr->bm_set_led_status(bmdi, LED_OFF);
			c_attr->led_status = LED_OFF;
			break;
		case 2:
			c_attr->bm_set_led_status(bmdi, LED_BLINK_ONE_TIMES_PER_2S);
			c_attr->led_status = LED_BLINK_ONE_TIMES_PER_2S;
			break;
		default:
			break;
		}
        ExReleaseFastMutex(&c_attr->attr_mutex);
	}
	return 0;
}

int bmctl_ioctl_set_ecc(unsigned long arg)
{
	int dev_id = arg & 0xff;
	int ecc_op = (arg >> 8) & 0xff;
	struct bm_device_info *bmdi = bmctl_get_bmdi(dev_id);

	if (!bmdi)
		return -1;

	if (bmdi->misc_info.chipid == 0x1684 || bmdi->misc_info.chipid == 0x1686)
		set_ecc(bmdi, ecc_op);

	return 0;
}

int bmctl_ioctl_recovery(unsigned long arg)
{
	int dev_id = arg & 0xff;
	int func_num = 0x0;
	struct bm_device_info *bmdi = bmctl_get_bmdi(dev_id);
    if (!bmdi)
        return -1;
	struct bm_device_info *bmdi_1 =  NULL;
	struct bm_device_info *bmdi_2 =  NULL;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS) {
		func_num = bmdi->misc_info.domain_bdf&0x7;
		if (func_num == 0) {
			bmdi = bmctl_get_bmdi(dev_id);
			bmdi_1 =  bmctl_get_bmdi(dev_id + 1);
			bmdi_2 =  bmctl_get_bmdi(dev_id + 2);

			if (bmdi_2 == NULL || bmdi_1 == NULL || bmdi == NULL)
				return -1;
		}

		if (func_num == 1) {
			bmdi = bmctl_get_bmdi(dev_id - 1);
			bmdi_1 =  bmctl_get_bmdi(dev_id);
			bmdi_2 =  bmctl_get_bmdi(dev_id + 1);

			if (bmdi_2 == NULL || bmdi_1 == NULL || bmdi == NULL)
				return -1;
		}

		if (func_num == 2) {
			bmdi = bmctl_get_bmdi(dev_id - 2);
			bmdi_1 =  bmctl_get_bmdi(dev_id - 1);
			bmdi_2 =  bmctl_get_bmdi(dev_id);
			if (bmdi_2 == NULL || bmdi_1 == NULL || bmdi == NULL)
				return -1;
		}

		if (BM1684_HW_VERSION(bmdi) == 0x0) {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"not support for sc5+ v1_0\n");
			return  -1;
		}

		if (bmdi->misc_info.chipid == 0x1684) {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"to reboot 1684, devid is %d\n", dev_id);
			bmdrv_wdt_start(bmdi);
		}

		//pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi_2->cinfo.device));
		//pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi_1->cinfo.device));
		//pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi->cinfo.device));

		bm_mdelay(6500);
	} else {
		if (bmdi->misc_info.chipid == 0x1684) {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"to reboot 1684, devid is %d\n", dev_id);
			bmdrv_wdt_start(bmdi);
		}
		//pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi->cinfo.device));

		bm_mdelay(6500);
	}

	//pci_lock_rescan_remove();
	//list_for_each_entry(root_bus, &pci_root_buses, node) {
	//	pci_rescan_bus(root_bus);
	//}
	//pci_unlock_rescan_remove();

	return 0;
}
