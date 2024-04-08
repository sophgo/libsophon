/*++

Module Name:

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "bm_common.h"
#include "driver.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, windriverQueueInitialize)
#endif

NTSTATUS
windriverQueueInitialize(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

     The I/O dispatch callbacks for the frameworks device object
     are configured in this function.

     A single default I/O Queue is configured for parallel request
     processing, and a driver context memory allocation is created
     to hold our structure QUEUE_CONTEXT.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    VOID

--*/
{
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;
    PBMDI               bmdi = NULL;

    PAGED_CODE();

    bmdi = Bm_Get_Bmdi(Device);
    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchParallel);

    queueConfig.EvtIoDeviceControl = windriverEvtIoDeviceControl;
    queueConfig.EvtIoStop = windriverEvtIoStop;

    status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &bmdi->IoctlQueue);

    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoctlQueueCreate failed %!STATUS!", status);
        return status;
    }

    status = WdfDeviceConfigureRequestDispatching(Device, bmdi->IoctlQueue, WdfRequestTypeDeviceControl);

    if (!NT_SUCCESS(status)) {
        //ASSERT(NT_SUCCESS(status));
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "Error in config'ing ioctl Queue %!STATUS!\n",
                    status);
        return status;
    }

//----------------------------------------------------------------------------------------------

    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchParallel);

    queueConfig.EvtIoDefault       = CreateQueueEvtIoDefault;
    queueConfig.EvtIoStop    = CreateQueueEvtIoStop;

    status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &bmdi->CreateQueue);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "WdfQueueCreate failed %!STATUS!\n",
                    status);
        return status;
    }

    status = WdfDeviceConfigureRequestDispatching(Device, bmdi->CreateQueue, WdfRequestTypeCreate);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "Error in config'ing creat Queue %!STATUS!\n",
                    status);
        return status;
    }

    return status;
}

VOID
windriverEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    OutputBufferLength - Size of the output buffer in bytes

    InputBufferLength - Size of the input buffer in bytes

    IoControlCode - I/O control code.

Return Value:

    VOID

--*/
{
    WDFDEVICE         device;
    PBMDI             bmdi = NULL;

    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    device = WdfIoQueueGetDevice(Queue);
    bmdi   = Bm_Get_Bmdi(device);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_QUEUE,
                "%!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d 0xIoControlCode %lx",
                Queue, Request, (int) OutputBufferLength, (int) InputBufferLength, IoControlCode>>2);

    // // Mark the request is cancelable
    //WdfRequestMarkCancelable(Request, EchoEvtRequestCancel);
    if (bmdi->is_surpriseremoved == 1) {
        WdfRequestCompleteWithInformation(Request, STATUS_CANCELLED, 0L);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! device is surpriseremoved %d", bmdi->is_surpriseremoved);
    }

    if ((((DWORD)(IoControlCode & 0xc00)) >> 10) == 3) {
        int ret = bm_vpu_ioctl(bmdi, Request, OutputBufferLength, InputBufferLength, IoControlCode);
        if (ret != 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "bm_vpu_ioctl return %d", ret);
        }
    } else if ((((DWORD)(IoControlCode & 0x3000)) >> 10) == 4) {
        int ret = bm_jpu_ioctl(bmdi, Request, OutputBufferLength, InputBufferLength, IoControlCode);
        if (ret != 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "bm_jpu_ioctl return %d", ret);
        }
    } else {
        int ret = bm_ioctl(bmdi, Request, OutputBufferLength, InputBufferLength, IoControlCode);
        if (ret != 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "bm_ioctl return %d", ret);
        }
    }

}

VOID
windriverEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
/*++

Routine Description:

    This event is invoked for a power-managed queue before the device leaves the working state (D0).

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    ActionFlags - A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS-typed flags
                  that identify the reason that the callback function is being called
                  and whether the request is cancelable.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d", Queue, Request, ActionFlags);

    if (ActionFlags & WdfRequestStopActionSuspend) {
        WdfRequestStopAcknowledge(Request, FALSE);  // Don't requeue
    } else if (ActionFlags & WdfRequestStopActionPurge) {
        BOOLEAN ret=WdfRequestCancelSentRequest(Request);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Queue 0x%p, Request 0x%p ret %d", Queue, Request, ret);
    }

    //
    // In most cases, the EvtIoStop callback function completes, cancels, or postpones
    // further processing of the I/O request.
    //
    // Typically, the driver uses the following rules:
    //
    // - If the driver owns the I/O request, it calls WdfRequestUnmarkCancelable
    //   (if the request is cancelable) and either calls WdfRequestStopAcknowledge
    //   with a Requeue value of TRUE, or it calls WdfRequestComplete with a
    //   completion status value of STATUS_SUCCESS or STATUS_CANCELLED.
    //
    //   Before it can call these methods safely, the driver must make sure that
    //   its implementation of EvtIoStop has exclusive access to the request.
    //
    //   In order to do that, the driver must synchronize access to the request
    //   to prevent other threads from manipulating the request concurrently.
    //   The synchronization method you choose will depend on your driver's design.
    //
    //   For example, if the request is held in a shared context, the EvtIoStop callback
    //   might acquire an internal driver lock, take the request from the shared context,
    //   and then release the lock. At this point, the EvtIoStop callback owns the request
    //   and can safely complete or requeue the request.
    //
    // - If the driver has forwarded the I/O request to an I/O target, it either calls
    //   WdfRequestCancelSentRequest to attempt to cancel the request, or it postpones
    //   further processing of the request and calls WdfRequestStopAcknowledge with
    //   a Requeue value of FALSE.
    //
    // A driver might choose to take no action in EvtIoStop for requests that are
    // guaranteed to complete in a small amount of time.
    //
    // In this case, the framework waits until the specified request is complete
    // before moving the device (or system) to a lower power state or removing the device.
    // Potentially, this inaction can prevent a system from entering its hibernation state
    // or another low system power state. In extreme cases, it can cause the system
    // to crash with bugcheck code 9F.

    return;
}

VOID CreateQueueEvtIoDefault(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request)
/*++

Routine Description:

    this function is called when CREATE irp comes.
    setup FileObject context fields, so it can be used to track MCN or exclusive
lock/unlock.

Arguments:

    Queue - Handle to device queue

    Request - the creation request

Return Value:

    None

--*/
{
    WDFFILEOBJECT           fileObject      = WdfRequestGetFileObject(Request);
    WDFDEVICE               device          = WdfIoQueueGetDevice(Queue);
    NTSTATUS                status          = STATUS_SUCCESS;

    PFILE_OBJECT_CONTEXT    fileObjectContext = NULL;
    PBMDI                   bmdi              = NULL;
    HANDLE                  open_pid;
    HANDLE                  process_id;
    struct bm_handle_info*  h_info;
    struct bm_thread_info*  thd_info = NULL;
    PHYSICAL_ADDRESS        pa;
    pa.QuadPart = 0xFFFFFFFFFFFFFFFF;

    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "create fileio entry,fileObject = 0x%p\n",fileObject);
    bmdi = Bm_Get_Bmdi(device);
    if (fileObject == NULL) {
        TraceEvents(TRACE_LEVEL_ERROR,
                     TRACE_QUEUE,
                    "Error: received a file create request with file object set to NULL\n");
        WdfRequestCompleteWithInformation(Request, STATUS_INTERNAL_ERROR, 0);
        return;
    }

    fileObjectContext = FileObjectGetContext(fileObject);

    // Initialize this WDFFILEOBJECT's context
    fileObjectContext->DeviceObject          = device;
    fileObjectContext->FileObject            = fileObject;

    open_pid = PsGetCurrentThreadId();
    process_id = PsGetCurrentProcessId();
    TraceEvents( TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "get threadid = 0x%llx  processid = 0x%llx\n", (u64)open_pid, (u64)process_id);
    h_info = MmAllocateContiguousMemory(sizeof(struct bm_handle_info), pa);
    if (!h_info) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "h_info is null\n");
        WdfRequestCompleteWithInformation(Request, STATUS_INTERNAL_ERROR, 0);
        return;
    }

    for (int i = 0; i < TABLE_NUM; i++)
        InitializeListHead(& h_info-> api_htable[i]);

    InitializeListHead(&h_info->list_gmem);
    h_info->fileObject     = fileObject;
    h_info->open_pid       = (u64)open_pid;
    h_info->process_id     = (u64)process_id;
    h_info->gmem_used      = 0ULL;
    h_info->h_send_api_seq = 0ULL;
    h_info->h_cpl_api_seq  = 0ULL;

    KeInitializeEvent(&h_info->h_msg_doneEvent, SynchronizationEvent, FALSE);

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = bmdi->WdfDevice;
    status = WdfSpinLockCreate(&attributes, &h_info->h_api_seq_spinlock);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "SpinLock Create fail\n");
        WdfRequestCompleteWithInformation(Request, STATUS_INTERNAL_ERROR, 0);
        return;
    }

    WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
    thd_info = bmdrv_create_thread_info(h_info, (u64)open_pid);
    if (!thd_info) {
        MmFreeContiguousMemory(h_info);
        WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "thd_info is null\n");
        WdfRequestCompleteWithInformation(Request, STATUS_INTERNAL_ERROR, 0);
        return;
    }
    WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);

    WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
    InsertTailList(&bmdi->handle_list, &h_info->list);//first para is list head
    WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);

#ifndef SOC_MODE
    if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
        bm_vpu_open(bmdi);
        //bm_jpu_open(inode, file);
    }
#endif

//#ifdef USE_RUNTIME_PM
//    pm_runtime_get_sync(bmdi->cinfo.device);
//#endif

    WdfRequestCompleteWithInformation(Request, status, 0);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "create fileio exit\n");

    return;
}


VOID CreateQueueEvtIoStop(_In_ WDFQUEUE   Queue,
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

VOID DeviceEvtFileClose(_In_ WDFFILEOBJECT FileObject)
/*++

Routine Description:

    this function is called when CLOSE irp comes.
    clean up MCN / Lock if necessary

Arguments:

    FileObject - WDF file object created for the irp.

Return Value:

    None

--*/
{
    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "fileclose entry\n");
    if (FileObject != NULL) {
        WDFDEVICE               device = WdfFileObjectGetDevice(FileObject);
        PBMDI                   bmdi   = Bm_Get_Bmdi(device);
        PFILE_OBJECT_CONTEXT fileObjectContext = FileObjectGetContext(FileObject);
        struct bm_handle_info* h_info;

        if (bmdev_gmem_get_handle_info(bmdi, fileObjectContext->FileObject, &h_info)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "DeviceEvtFileClose: file list is not found!\n");
            return;
        }

        #ifndef SOC_MODE
            if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
                //bm_vpu_release(bmdi, FileObject); //bm_vpu_release can only be called once,in windows Deviceevtfilecleanup must be called. This function may not be called
                bm_jpu_release(bmdi, FileObject);
            }
        #endif
        /* invalidate pending APIs in msgfifo */
        bmdev_invalidate_pending_apis(bmdi, h_info);

        WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
        bmdrv_delete_thread_info(h_info);
        RemoveEntryList(&h_info->list);
        MmFreeContiguousMemory(h_info);
        WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);

        //#ifdef USE_RUNTIME_PM
        //    pm_runtime_put_sync(bmdi->cinfo.device);
        //#endif
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "fileclose exit\n");
    return;
}

void bm_gmem_release(struct bm_device_info* bmdi, _In_ struct bm_handle_info *h_info) {
    int                ret = 0;
    gmem_buffer_pool_t *pool, *n;
    gmem_buffer_t       gb;

    /* found and free the not handled buffer by user applications */
    WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);

    list_for_each_entry_safe(pool, n, &h_info->list_gmem, list, gmem_buffer_pool_t) {
        if (pool->file == h_info->fileObject) {
            gb = pool->gb;
            if (gb.phys_addr) {
                ret = bmdrv_bgm_heap_free(&bmdi->gmem_info.common_heap[gb.heap_id], gb.phys_addr);
                if (ret != 0) {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "bm_gmem_release failed, heap=%d, addr = 0x%llx\n", gb.heap_id, gb.phys_addr);
                }
            }
            RemoveEntryList(&pool->list);
            BGM_P_FREE(pool);
        }
    }
    WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
    return;
}

VOID DeviceEvtFileCleanup(IN WDFFILEOBJECT FileObject)
/*++

   EvtFileClose is called when all the handles represented by the FileObject
   is closed and all the references to FileObject is removed. This callback
   may get called in an arbitrary thread context instead of the thread that
   called CloseHandle. If you want to delete any per FileObject context that
   must be done in the context of the user thread that made the Create call,
   you should do that in the EvtDeviceCleanp callback.

Arguments:

    FileObject - Pointer to fileobject that represents the open handle.

Return Value:

   VOID

--*/

{
    PBMDI                bmdi;
    struct bm_handle_info* h_info;

    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "EvtFileCleanup called entry\n");
    // KdPrint(("EvtFileCleanup called\n"));
    //
    // After cleanup callback is invoked, framework flushes all the queues
    // by fileobject to cancel irps belonging to the file handle being closed.
    // Since framework version 1.5 and less have a bug in that on cleanup-
    // flush the EvtIoCanceledOnQueue callback is not invoked, we will
    // retrive the request by fileobject ourselves and complete the request
    // and clear the NotificaitonIrp field.
    //
    bmdi = Bm_Get_Bmdi(WdfFileObjectGetDevice(FileObject));
    bm_vpu_release_byfilp(bmdi,FileObject);
    if (bmdev_gmem_get_handle_info(bmdi, FileObject, &h_info)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "DeviceEvtFileCleanup: file list is not found!\n");
        return;
    }
    bm_gmem_release(bmdi, h_info);
    bm_vpu_release(bmdi, FileObject);
    bm_jpu_release(bmdi, FileObject);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "EvtFileCleanup called exit\n");
    return;
}