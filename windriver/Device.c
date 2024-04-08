/*++

Module Name:

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/
#include "bm_common.h"
#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, windriverCreateDevice)
#endif

const GUID* g_guid_interface[] = {&GUID_DEVINTERFACE_bm_sophon0,&GUID_DEVINTERFACE_bm_sophon1,&GUID_DEVINTERFACE_bm_sophon2};
const GUID* g_guid_interface_evb[] = {&GUID_DEVINTERFACE_bm_sophon_evb};

VOID BmEvtDeviceContextCleanup(WDFOBJECT Device)
{
    PBMDI bmdi = NULL;

    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Entry");
    bmdi = Bm_Get_Bmdi((WDFDEVICE)Device);
    if (bmdi->Lookaside) {
        WdfObjectDelete(bmdi->Lookaside);
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Exit");
}

PCHAR
DbgDevicePowerString(IN WDF_POWER_DEVICE_STATE Type)
/*++

Updated Routine Description:
    DbgDevicePowerString does not change in this stage of the function driver.

--*/
{
    switch (Type) {
        case WdfPowerDeviceInvalid:
            return "WdfPowerDeviceInvalid";
        case WdfPowerDeviceD0:
            return "WdfPowerDeviceD0";
        case WdfPowerDeviceD1:
            return "WdfPowerDeviceD1";
        case WdfPowerDeviceD2:
            return "WdfPowerDeviceD2";
        case WdfPowerDeviceD3:
            return "WdfPowerDeviceD3";
        case WdfPowerDeviceD3Final:
            return "WdfPowerDeviceD3Final";
        case WdfPowerDevicePrepareForHibernation:
            return "WdfPowerDevicePrepareForHibernation";
        case WdfPowerDeviceMaximum:
            return "WdfPowerDeviceMaximum";
        default:
            return "UnKnown Device Power State";
    }
}

NTSTATUS
BmEvtDeviceD0Entry(IN WDFDEVICE Device, IN WDF_POWER_DEVICE_STATE PreviousState)
/*++

Routine Description:

    EvtDeviceD0Entry event callback must perform any operations that are
    necessary before the specified device is used.  It will be called every
    time the hardware needs to be (re-)initialized.  This includes after
    IRP_MN_START_DEVICE, IRP_MN_CANCEL_STOP_DEVICE, IRP_MN_CANCEL_REMOVE_DEVICE,
    IRP_MN_SET_POWER-D0.

    This function is not marked pageable because this function is in the
    device power up path. When a function is marked pagable and the code
    section is paged out, it will generate a page fault which could impact
    the fast resume behavior because the client driver will have to wait
    until the system drivers can service this page fault.

    This function runs at PASSIVE_LEVEL, even though it is not paged.  A
    driver can optionally make this function pageable if DO_POWER_PAGABLE
    is set.  Even if DO_POWER_PAGABLE isn't set, this function still runs
    at PASSIVE_LEVEL.  In this case, though, the function absolutely must
    not do anything that will cause a page fault.

Arguments:

    Device - Handle to a framework device object.

    PreviousState - Device power state which the device was in most recently.
        If the device is being newly started, this will be
        PowerDeviceUnspecified.

Return Value:

    NTSTATUS

--*/
{
    PBMDI bmdi = NULL;
    NTSTATUS status = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "-->PciDrvEvtDeviceD0Entry - coming from %s\n",
                DbgDevicePowerString(PreviousState));

    bmdi = Bm_Get_Bmdi(Device);

    ASSERT(PowerDeviceD0 != PreviousState);

    bmdi->DevicePowerState = PowerDeviceD0;

    if (bmdi->probed == 1) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "device has been inited\n");
        status = STATUS_SUCCESS;
    } else {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "device need to be reinited\n");
        if (bmdrv_pci_probe(bmdi)) {
            TraceEvents(
                TRACE_LEVEL_ERROR, TRACE_POWER, "deviced0 bmdrv_pci_probe failed\n");
            return STATUS_UNSUCCESSFUL;
        };

        if (bmdrv_pci_probe_delayed_function(bmdi)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_POWER,
                        "deviced0 delayed_function probe failed\n");
            return STATUS_UNSUCCESSFUL;
        } else {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "exit D0delayed_function probe\n");
        }
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "<--PciDrvEvtDeviceD0Entry\n");

    return status;
}

NTSTATUS
BmEvtDeviceD0Exit(IN WDFDEVICE Device, IN WDF_POWER_DEVICE_STATE TargetState)
/*++

Routine Description:

    This routine undoes anything done in EvtDeviceD0Entry.  It is called
    whenever the device leaves the D0 state, which happens when the device is
    stopped, when it is removed, and when it is powered off.

    The device is still in D0 when this callback is invoked, which means that
    the driver can still touch hardware in this routine.

    Note that interrupts have already been disabled by the time that this
    callback is invoked.

   EvtDeviceD0Exit event callback must perform any operations that are
   necessary before the specified device is moved out of the D0 state.  If the
   driver needs to save hardware state before the device is powered down, then
   that should be done here.

   This function runs at PASSIVE_LEVEL, though it is generally not paged.  A
   driver can optionally make this function pageable if DO_POWER_PAGABLE is set.

   Even if DO_POWER_PAGABLE isn't set, this function still runs at
   PASSIVE_LEVEL.  In this case, though, the function absolutely must not do
   anything that will cause a page fault.

Arguments:

    Device - Handle to a framework device object.

    TargetState - Device power state which the device will be put in once this
        callback is complete.

Return Value:

    Success implies that the device can be used.  Failure will result in the
    device stack being torn down.

--*/
{
    PBMDI bmdi = NULL;
    UNREFERENCED_PARAMETER(Device);
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "-->PciDrvEvtDeviceD0Exit - moving to %s\n",
                DbgDevicePowerString(TargetState));

    bmdi = Bm_Get_Bmdi(Device);
    bmdi->DevicePowerState = TargetState;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "<--PciDrvEvtDeviceD0Exit\n");
    return STATUS_SUCCESS;
}

NTSTATUS
PciDrvEvtDeviceSelfManagedIoSuspend(IN WDFDEVICE Device)
/*++

Routine Description:

    EvtDeviceSelfManagedIoSuspend is called by the Framework before the device
    leaves the D0 state.  Its job is to stop any I/O-related actions that the
    Framework isn't managing, and which cannot be handled when the device
    hardware isn't available.  In general, this means reversing anything that
    was done in EvtDeviceSelfManagedIoStart.

    If you allow the Framework to manage most or all of your queues, then when
    you build a driver from this sample, you can probably delete this function.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    NTSTATUS - Failures will result in the device stack being torn down.

--*/
{
    PBMDI bmdi = NULL;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "--> PciDrvEvtDeviceSelfManagedIoSuspend\n");

    bmdi = Bm_Get_Bmdi(Device);

    bm_monitor_thread_deinit(bmdi);
    bmdi->probed = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "<-- PciDrvEvtDeviceSelfManagedIoSuspend\n");

    return STATUS_SUCCESS;
}

NTSTATUS
PciDrvEvtDeviceSelfManagedIoRestart(IN WDFDEVICE Device)
/*++

Routine Description:

    EvtDeviceSelfManagedIoRestart is called by the Framework before the device
    is restarted for one of the following reasons:
    a) the PnP resources were rebalanced (framework received
        query-stop and stop IRPS )
    b) the device resumed from a low power state to D0.

    This function is not marked pagable because this function is in the
    device power up path. When a function is marked pagable and the code
    section is paged out, it will generate a page fault which could impact
    the fast resume behavior because the client driver will have to wait
    until the system drivers can service this page fault.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    NTSTATUS - Failure will cause the device stack to be torn down.

--*/
{
    PBMDI bmdi = NULL;
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "--> PciDrvEvtDeviceSelfManagedIoRestart\n");

    bmdi = Bm_Get_Bmdi(Device);
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "<-- PciDrvEvtDeviceSelfManagedIoRestart\n");

    return STATUS_SUCCESS;
}

VOID PciDrvEvtDeviceSelfManagedIoCleanup(IN WDFDEVICE Device)
/*++

Routine Description:

    EvtDeviceSelfManagedIoCleanup is called by the Framework when the device is
    being torn down, either in response to the WDM IRP_MN_REMOVE_DEVICE
    It will be called only once.  Its job is to stop all outstanding I/O in the
driver that the Framework is not managing.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    None

--*/
{
    PBMDI bmdi = NULL;
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "--> PciDrvEvtDeviceSelfManagedIoCleanup\n");

    bmdi = Bm_Get_Bmdi(Device);
    // Be sure to flush the DPCs as the READ/WRITE timer routine may still be
    // running
    // during device removal.  This call may take a while to complete.
    KeFlushQueuedDpcs();

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "<-- PciDrvEvtDeviceSelfManagedIoCleanup\n");
}

VOID PciDrvEvtDeviceSurpriseRemoval(_In_ WDFDEVICE Device)
/*++

Routine Description:

    this function is called when the device is surprisely removed.
    Stop all IO queues so that there will be no more request being sent down.

Arguments:

    Device - Handle to device object

Return Value:

    None.

--*/
{
    PBMDI bmdi = NULL;
    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "--> PciDrvEvtDeviceSurpriseRemoval\n");

    bmdi = Bm_Get_Bmdi(Device);
    bm_monitor_thread_deinit(bmdi);
    bmdi->probed = 0;
    bmdi->is_surpriseremoved = 1;
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "--> PciDrvEvtDeviceSurpriseRemoval,  WDFDEVICE %p\n",Device);

    return;
}

NTSTATUS
PciDrvEvtDeviceQueryStop(_In_ WDFDEVICE Device)

/*++

Routine Description:

    EvtDeviceQueryStop event callback function determines whether a specified
    device can be stopped so that the PnP manager can redistribute system
    hardware resources.

    SimBatt is designed to fail a rebalance operation, for reasons described
    below. Note however that this approach must *not* be adopted by an actual
    battery driver.

    SimBatt unregisters itself as a Battery driver by calling
    BatteryClassUnload() when IRP_MN_STOP_DEVICE arrives at the driver. It
    re-establishes itself as a Battery driver on arrival of IRP_MN_START_DEVICE.
    This results in any IOCTLs normally handeled by the Battery Class driver to
    be delivered to SimBatt. The IO Queue employed by SimBatt is power managed,
    it causes these IOCTLs to be pended when SimBatt is not in D0. Now if the
    device attempts to do a shutdown while an IOCTL is pended in SimBatt, it
    would result in a 0x9F bugcheck. By opting out of PNP rebalance operation
    SimBatt circumvents this issue.

Arguments:

    Device - Supplies a handle to a framework device object.

Return Value:

    NTSTATUS

--*/

{

    UNREFERENCED_PARAMETER(Device);
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "<-- PciDrvEvtDeviceQueryStop\n");
    return STATUS_SUCCESS;
}



NTSTATUS bm_drv_check_id(IN OUT PBMDI bmdi)
{
    NTSTATUS status = STATUS_SUCCESS;
    DECLSPEC_ALIGN(MEMORY_ALLOCATION_ALIGNMENT)
    UCHAR              buffer[64];
    PPCI_COMMON_CONFIG pPciConfig = (PPCI_COMMON_CONFIG)buffer;
    u32              bytesRead = 0;

    PAGED_CODE();

    RtlZeroMemory(buffer, sizeof(buffer));
    bytesRead = bmdi->BusInterface.GetBusData(
        bmdi->BusInterface.Context,
        PCI_WHICHSPACE_CONFIG,  // READ
        buffer,
        FIELD_OFFSET(PCI_COMMON_CONFIG, VendorID),
        64);

    if (bytesRead != 64) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    // Is this our device?
    //

    // if ((pPciConfig->VendorID != BITMAIN_VENDOR_ID ||
    //      pPciConfig->VendorID != SOPHGO_VENDOR_ID)  ||
    //     (pPciConfig->VendorID != BM1684_DEVICE_ID  ||
    //      pPciConfig->DeviceID != BM1684X_DEVICE_ID)) {
    if (pPciConfig->VendorID != SOPHGO_VENDOR_ID ||
        pPciConfig->DeviceID != BM1684X_DEVICE_ID) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DRIVER,
                    "VendorID/DeviceID don't match - %x/%x\n",
                    pPciConfig->VendorID,
                    pPciConfig->DeviceID);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    bmdi->vendor_id = pPciConfig->VendorID;
    bmdi->device_id = pPciConfig->DeviceID;

     return status;
}

NTSTATUS bm_drv_set_info(IN OUT PBMDI bmdi) {
    NTSTATUS status = STATUS_SUCCESS;
    u32    propertyAddress, BusNumber;
    ULONG  length;
    USHORT   FunctionNumber, DeviceNumber;

    //bmdi->dev_index = 0;

    //
    // Get the BUS_INTERFACE_STANDARD for our device so that we can
    // read & write to PCI config space.
    //
    status = WdfFdoQueryForInterface(bmdi->WdfDevice,
                                     &GUID_BUS_INTERFACE_STANDARD,
                                     (PINTERFACE)&bmdi->BusInterface,
                                     sizeof(BUS_INTERFACE_STANDARD),
                                     1,      // Version
                                     NULL);  // InterfaceSpecificData
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = bm_drv_check_id(bmdi);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Get the BusNumber. Be warned that bus numbers may be
    // dynamic and therefore subject to change unpredictably!!!
    IoGetDeviceProperty(WdfDeviceWdmGetPhysicalDevice(bmdi->WdfDevice),
                        DevicePropertyBusNumber,
                        sizeof(u32),
                        (PVOID)&BusNumber,
                        &length);

    // Get the DevicePropertyAddress
    IoGetDeviceProperty(WdfDeviceWdmGetPhysicalDevice(bmdi->WdfDevice),
                        DevicePropertyAddress,
                        sizeof(u32),
                        (PVOID)&propertyAddress,
                        &length);

    // For PCI, the DevicePropertyAddress has device number
    // in the high word and the function number in the low word.
    FunctionNumber = (USHORT)((propertyAddress)&0x0000FFFF);
    DeviceNumber   = (USHORT)(((propertyAddress) >> 16) & 0x0000FFFF);

    bmdi->bus_number = BusNumber;
    bmdi->device_number = DeviceNumber;
    bmdi->function_number = FunctionNumber;

    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "VendorID/DeviceID match success: 0x%x:0x%x\n", bmdi->vendor_id, bmdi->device_id);
    TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_DRIVER,
                "this sophon dev's BDF:%d-%d-%d\n",
                bmdi->bus_number,
                bmdi->device_number,
                bmdi->function_number);

    return status;
}

PVOID LocalMmMapIoSpace(_In_ PHYSICAL_ADDRESS PhysicalAddress,
                        _In_ SIZE_T           NumberOfBytes) {
    typedef PVOID (*PFN_MM_MAP_IO_SPACE_EX)(
        _In_ PHYSICAL_ADDRESS PhysicalAddress,
        _In_ SIZE_T           NumberOfBytes,
        _In_ u32            Protect);

    UNICODE_STRING         name;
    PFN_MM_MAP_IO_SPACE_EX pMmMapIoSpaceEx;

    RtlInitUnicodeString(&name, L"MmMapIoSpaceEx");
    pMmMapIoSpaceEx =
        (PFN_MM_MAP_IO_SPACE_EX)(ULONG_PTR)MmGetSystemRoutineAddress(&name);

    if (pMmMapIoSpaceEx != NULL) {
        //
        // Call WIN10 API if available
        //
        return pMmMapIoSpaceEx(
            PhysicalAddress, NumberOfBytes, PAGE_READWRITE | PAGE_NOCACHE);
    }

//
// Supress warning that MmMapIoSpace allocates executable memory.
// This function is only used if the preferred API, MmMapIoSpaceEx
// is not present. MmMapIoSpaceEx is available starting in WIN10.
//
#pragma warning(suppress : 30029)
    return MmMapIoSpace(PhysicalAddress, NumberOfBytes, MmNonCached);
}

NTSTATUS
BmMapHWResources(IN OUT PBMDI bmdi,
                  IN WDFCMRESLIST  ResourcesRaw,
                  IN WDFCMRESLIST  ResourcesTranslated)
{
    PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
    u32                           i;
    NTSTATUS                        status        = STATUS_SUCCESS;
    BOOLEAN                         bResInterrupt = FALSE;
    BOOLEAN                         bResMemory    = FALSE;
    u32                           numberOfBARs  = 0;

    UNREFERENCED_PARAMETER(ResourcesRaw);

    PAGED_CODE();

    for (i = 0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {

        descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);

        if (!descriptor) {
            TraceEvents(
                TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfResourceCmGetDescriptor");
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

        switch (descriptor->Type) {

            case CmResourceTypePort:
                 TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                             "We Have No I/O mapped Resource\n");
                 status = STATUS_DEVICE_CONFIGURATION_ERROR;
                 return status;
              break;

            case CmResourceTypeMemory:

                numberOfBARs++;

                if (numberOfBARs == 1) {
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                                TRACE_DEVICE,
                                "Memory mapped bar0:(0x%x:0x%x) Length:(0x%x)\n",
                                descriptor->u.Memory.Start.LowPart,
                                descriptor->u.Memory.Start.HighPart,
                                descriptor->u.Memory.Length);
                    //
                    // BAR0 memory space should be 0x400000 (4Mb) in size.
                    //
                    ASSERT(descriptor->u.Memory.Length == 0x400000);
                    bmdi->cinfo.bar_info.bar0_start =
                        ((LONG64)descriptor->u.Memory.Start.HighPart << 32) |
                        descriptor->u.Memory.Start.LowPart;
                    bmdi->cinfo.bar_info.bar0_len = descriptor->u.Memory.Length;
                    bmdi->cinfo.bar_info.bar0_vaddr =
                        LocalMmMapIoSpace(descriptor->u.Memory.Start,
                                          descriptor->u.Memory.Length);
                    if (!bmdi->cinfo.bar_info.bar0_vaddr) {
                        TraceEvents(TRACE_LEVEL_ERROR,
                                    TRACE_DEVICE,
                                    " - Unable to map bar0 memory "
                                    "%08I64X, length 0x%x",
                                    descriptor->u.Memory.Start.QuadPart,
                                    descriptor->u.Memory.Length);
                        return STATUS_INSUFFICIENT_RESOURCES;
                    }
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                                TRACE_DEVICE,
                                "bar0_vaddr=%p\n",
                                bmdi->cinfo.bar_info.bar0_vaddr);
                } else if (numberOfBARs == 2) {
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                                TRACE_DEVICE,
                                "Memory mapped bar1:(0x%x:0x%x) Length:(0x%x)\n",
                                descriptor->u.Memory.Start.LowPart,
                                descriptor->u.Memory.Start.HighPart,
                                descriptor->u.Memory.Length);
                    //
                    // BAR1 memory space should be 0x400000 (4Mb) in size.
                    //
                    ASSERT(descriptor->u.Memory.Length == 0x400000);
                    bmdi->cinfo.bar_info.bar1_start =
                        ((LONG64)descriptor->u.Memory.Start.HighPart << 32) |
                        descriptor->u.Memory.Start.LowPart;
                    bmdi->cinfo.bar_info.bar1_len = descriptor->u.Memory.Length;
                    bmdi->cinfo.bar_info.bar1_vaddr =
                        LocalMmMapIoSpace(descriptor->u.Memory.Start,
                                          descriptor->u.Memory.Length);
                    if (!bmdi->cinfo.bar_info.bar1_vaddr) {
                        TraceEvents(TRACE_LEVEL_ERROR,
                                    TRACE_DEVICE,
                                    " - Unable to map bar1 memory "
                                    "%08I64X, length 0x%x",
                                    descriptor->u.Memory.Start.QuadPart,
                                    descriptor->u.Memory.Length);
                        return STATUS_INSUFFICIENT_RESOURCES;
                    }
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                                TRACE_DEVICE,
                                "bar1_vaddr=%p\n",
                                bmdi->cinfo.bar_info.bar1_vaddr);
                } else if (numberOfBARs == 3) {
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                                TRACE_DEVICE,
                                "Memory mapped bar2:(0x%x:0x%x) Length:(0x%x)\n",
                                descriptor->u.Memory.Start.LowPart,
                                descriptor->u.Memory.Start.HighPart,
                                descriptor->u.Memory.Length);
                    //
                    // BAR2 memory space should be 0x100000 (1Mb) in size.
                    //
                    ASSERT(descriptor->u.Memory.Length == 0x100000);
                    bmdi->cinfo.bar_info.bar2_start =
                        ((LONG64)descriptor->u.Memory.Start.HighPart << 32) |
                        descriptor->u.Memory.Start.LowPart;
                    bmdi->cinfo.bar_info.bar2_len = descriptor->u.Memory.Length;
                    bmdi->cinfo.bar_info.bar2_vaddr =
                        LocalMmMapIoSpace(descriptor->u.Memory.Start,
                                          descriptor->u.Memory.Length);
                    if (!bmdi->cinfo.bar_info.bar2_vaddr) {
                        TraceEvents(TRACE_LEVEL_ERROR,
                                    TRACE_DEVICE,
                                    " - Unable to map bar2 memory "
                                    "%08I64X, length 0x%x",
                                    descriptor->u.Memory.Start.QuadPart,
                                    descriptor->u.Memory.Length);
                        return STATUS_INSUFFICIENT_RESOURCES;
                    }
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                                TRACE_DEVICE,
                                "bar2_vaddr=%p\n",
                                bmdi->cinfo.bar_info.bar2_vaddr);
                } else if (numberOfBARs == 4) {
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                                TRACE_DEVICE,
                                "Memory mapped bar4:(0x%x:0x%x) Length:(0x%x)\n",
                                descriptor->u.Memory.Start.LowPart,
                                descriptor->u.Memory.Start.HighPart,
                                descriptor->u.Memory.Length);
                    //
                    // BAR4 memory space should be 0x100000 (1Mb) in size.
                    //
                    ASSERT(descriptor->u.Memory.Length == 0x100000);
                    bmdi->cinfo.bar_info.bar4_start =
                        ((LONG64)descriptor->u.Memory.Start.HighPart << 32) |
                        descriptor->u.Memory.Start.LowPart;
                    bmdi->cinfo.bar_info.bar4_len = descriptor->u.Memory.Length;
                    bmdi->cinfo.bar_info.bar4_vaddr =
                        LocalMmMapIoSpace(descriptor->u.Memory.Start,
                                          descriptor->u.Memory.Length);
                    if (!bmdi->cinfo.bar_info.bar4_vaddr) {
                        TraceEvents(TRACE_LEVEL_ERROR,
                                    TRACE_DEVICE,
                                    " - Unable to map bar4 memory "
                                    "%08I64X, length 0x%x",
                                    descriptor->u.Memory.Start.QuadPart,
                                    descriptor->u.Memory.Length);
                        return STATUS_INSUFFICIENT_RESOURCES;
                    }
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                                TRACE_DEVICE,
                                "bar4_vaddr=%p\n",
                                bmdi->cinfo.bar_info.bar4_vaddr);
                    bResMemory = TRUE;
                }
                break;

            case CmResourceTypeInterrupt:

                ASSERT(!bResInterrupt);

                WDF_INTERRUPT_CONFIG interruptConfig;

                //
                // Create WDFINTERRUPT object.
                //
                WDF_INTERRUPT_CONFIG_INIT(&interruptConfig,
                                          BmEvtInterruptIsr,
                                          BmEvtInterruptDpc);

                //
                // These first two callbacks will be called at DIRQL.  Their
                // job is to enable and disable interrupts.
                //
                interruptConfig.EvtInterruptDisable = BmEvtInterruptDisable;
                interruptConfig.InterruptTranslated = descriptor;
                interruptConfig.InterruptRaw =
                    WdfCmResourceListGetDescriptor(ResourcesRaw, i);

                status = WdfInterruptCreate(bmdi->WdfDevice,
                                            &interruptConfig,
                                            WDF_NO_OBJECT_ATTRIBUTES,
                                            &bmdi->WdfInterrupt);

                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR,
                                TRACE_DEVICE,
                                "WdfInterruptCreate failed: %!STATUS!\n",
                                status);
                    return status;
                }
                

                bResInterrupt = TRUE;

                TraceEvents(TRACE_LEVEL_INFORMATION,
                            TRACE_DEVICE,
                            "Interrupt level: 0x%0x, Vector: 0x%0x\n",
                            descriptor->u.Interrupt.Level,
                            descriptor->u.Interrupt.Vector);

                break;

            default:
                //
                // This could be device-private type added by the PCI bus
                // driver. We shouldn't filter this or change the information
                // contained in it.
                //
                TraceEvents(TRACE_LEVEL_INFORMATION,
                            TRACE_DEVICE,
                            "Unhandled resource type (0x%x)\n",
                            descriptor->Type);
                break;
        }
    }

    //
    // Make sure we got all the 3 resources to work with.
    //
    if (!(bResInterrupt && bResMemory)) {
        status = STATUS_DEVICE_CONFIGURATION_ERROR;
        return status;
    }

    return status;
}

NTSTATUS
BmUnmapHWResources(IN OUT PBMDI bmdi)
{
    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "--> BmUnmapHWResources\n");
    //
    // Free hardware resources
    //
    if (bmdi->cinfo.bar_info.bar0_vaddr) {
        MmUnmapIoSpace(bmdi->cinfo.bar_info.bar0_vaddr,
                       bmdi->cinfo.bar_info.bar0_len);
    }

    if (bmdi->cinfo.bar_info.bar0_vaddr) {
        MmUnmapIoSpace(bmdi->cinfo.bar_info.bar1_vaddr,
                       bmdi->cinfo.bar_info.bar1_len);
    }

    if (bmdi->cinfo.bar_info.bar0_vaddr) {
        MmUnmapIoSpace(bmdi->cinfo.bar_info.bar2_vaddr,
                       bmdi->cinfo.bar_info.bar2_len);
    }

    if (bmdi->cinfo.bar_info.bar0_vaddr) {
        MmUnmapIoSpace(bmdi->cinfo.bar_info.bar4_vaddr,
                       bmdi->cinfo.bar_info.bar4_len);
    }
    TraceEvents(
        TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<-- BmUnmapHWResources\n");
    return STATUS_SUCCESS;
}

NTSTATUS
BmEvtDevicePrepareHardware(WDFDEVICE    Device,
                               WDFCMRESLIST Resources,
                               WDFCMRESLIST ResourcesTranslated)
{
    NTSTATUS  status  = STATUS_SUCCESS;
    PBMDI bmdi = NULL;

    UNREFERENCED_PARAMETER(Resources);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "--> EvtDevicePrepareHardware\n");

    bmdi = Bm_Get_Bmdi(Device);

    status = BmMapHWResources(bmdi, Resources, ResourcesTranslated);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "MapHWResources failed: %!STATUS!\n",
                    status);
        return status;
    }

    if (bmdrv_pci_probe(bmdi)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "bmdrv_pci_probe failed\n");
        return STATUS_UNSUCCESSFUL;
    };

	if (bmdrv_pci_probe_delayed_function(bmdi)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "delayed_function probe failed\n");
		return STATUS_UNSUCCESSFUL;
    } else {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "exit delayed_function probe\n");
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "<-- EvtDevicePrepareHardware\n");

    return status;
}

NTSTATUS
BmEvtDeviceReleaseHardware(IN WDFDEVICE    Device,
                               IN WDFCMRESLIST ResourcesTranslated)
{
    PBMDI bmdi = NULL;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "--> EvtDeviceReleaseHardware\n");

    bmdi = Bm_Get_Bmdi(Device);

    int ret = bmdrv_ctrl_del_dev();
    if (ret) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bmdrv_ctrl_del_dev failed\n");
    }else {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "dev_count=%d\n", bmci->dev_count);
            if (bmci->dev_count == 0) {
                bmdrv_clean_record();
                bmdrv_remove_bmci();
            }
    }

    bmdrv_pci_remove(bmdi);
    // Unmap any I/O ports. Disconnecting from the interrupt will be done
    // automatically by the framework.
    //
    BmUnmapHWResources(bmdi);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "<-- EvtDeviceReleaseHardware\n");

    return STATUS_SUCCESS;
}

NTSTATUS windriverCreateDevice(_Inout_ WDFDRIVER          Driver,
                               _Inout_ PWDFDEVICE_INIT DeviceInit) {
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    WDFDEVICE device;
    NTSTATUS status;
    PBMDI bmdi = NULL;
    WDF_FILEOBJECT_CONFIG        fileObjectConfig;
    int                          rc = 0x0;
    WDF_DEVICE_PNP_CAPABILITIES  pnpCaps;
    WDF_DEVICE_POWER_CAPABILITIES powerCaps;

    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();

    //
    // Zero out the PnpPowerCallbacks structure.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"[Device] Starting to add callbacks!!!\n");

    //
    // These two callbacks set up and tear down hardware state,
    // specifically that which only has to be done once.
    //
    pnpPowerCallbacks.EvtDevicePrepareHardware = BmEvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = BmEvtDeviceReleaseHardware;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"[Device] Finished hardware prepare!!!\n");
    //
    // These two callbacks set up and tear down hardware state that must be
    // done every time the device moves in and out of the D0-working state.
    //

    pnpPowerCallbacks.EvtDeviceD0Entry = BmEvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit  = BmEvtDeviceD0Exit;

    pnpPowerCallbacks.EvtDeviceD0EntryPostInterruptsEnabled = BmEvtDeviceD0EntryPostInterruptsEnabled;
    pnpPowerCallbacks.EvtDeviceD0ExitPreInterruptsDisabled =  BmEvtDeviceD0ExitPreInterruptsDisabled;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"[Device] Finished D0 entry point init!!!\n");

    //
    // This next group of five callbacks allow a driver to become involved in
    // starting and stopping operations within a driver as the driver moves
    // through various PnP/Power states.  These functions are not necessary
    // if the Framework is managing all the device's queues and there is no
    // activity going on that isn't queue-based.  This sample provides these
    // callbacks because it uses watchdog timer to monitor whether the device
    // is working or not and it needs to start and stop the timer when the device
    // is started or removed. It cannot start and stop the timers in the D0Entry
    // and D0Exit callbacks because if the device is surprise-removed, D0Exit
    // will not be called.
    //
    //pnpPowerCallbacks.EvtDeviceSelfManagedIoInit    = PciDrvEvtDeviceSelfManagedIoInit;
    pnpPowerCallbacks.EvtDeviceSelfManagedIoCleanup = PciDrvEvtDeviceSelfManagedIoCleanup;
    pnpPowerCallbacks.EvtDeviceSelfManagedIoSuspend = PciDrvEvtDeviceSelfManagedIoSuspend;
    pnpPowerCallbacks.EvtDeviceSelfManagedIoRestart = PciDrvEvtDeviceSelfManagedIoRestart;
    pnpPowerCallbacks.EvtDeviceSurpriseRemoval = PciDrvEvtDeviceSurpriseRemoval;
    pnpPowerCallbacks.EvtDeviceQueryStop = PciDrvEvtDeviceQueryStop;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"[Device] Start to set pnpPowerCallbacks!!!\n");
    //
    // Register the PnP and power callbacks. Power policy related callbacks will
    // be registered later.
    //
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    // Register FileObject related callbacks

        // Add FILE_OBJECT_EXTENSION as the context to the file object.
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, FILE_OBJECT_CONTEXT);

        // Set Entry points for Create and Close..

        // The framework doesn't sync the file create requests with pnp/power
        // state. Re-direct all the file create requests to a dedicated
        // queue, which will be purged manually during removal.
        WDF_FILEOBJECT_CONFIG_INIT(
            &fileObjectConfig,
            NULL,  // CreateQueueEvtIoDefault,
            DeviceEvtFileClose,
            DeviceEvtFileCleanup);

        fileObjectConfig.FileObjectClass = WdfFileObjectWdfCannotUseFsContexts;

        // Since we are registering file events and fowarding create request
        // ourself, we must also set AutoForwardCleanupClose so that cleanup
        // and close can also get forwarded.
        fileObjectConfig.AutoForwardCleanupClose = WdfTrue;
        deviceAttributes.SynchronizationScope    = WdfSynchronizationScopeNone;
        deviceAttributes.ExecutionLevel          = WdfExecutionLevelPassive;

        // Indicate that file object is optional.
        fileObjectConfig.FileObjectClass |= WdfFileObjectCanBeOptional;

        WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileObjectConfig, &deviceAttributes);


    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, BMDI);
    deviceAttributes.EvtCleanupCallback = BmEvtDeviceContextCleanup;

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status)) {
        //
        // Device creation is complete.
        // Get the DeviceExtension and initialize it.
        //
        bmdi = Bm_Get_Bmdi(device);
        if (!bmdi) {
            TraceEvents(
                TRACE_LEVEL_ERROR, TRACE_DRIVER, "[windriverCreateDevice] bmdi is null\n");
        }
        //RtlSecureZeroMemory(bmdi, sizeof(struct bm_device_info));
        //TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "bmdi value=%x\n", &bmdi->bmcd);
        bmdi->WdfDevice = device;
        bmdi->WdfDriver = Driver;

        status = WdfLookasideListCreate(WDF_NO_OBJECT_ATTRIBUTES,
                                        sizeof(struct bm_trace_item),
                                        NonPagedPoolNx,
                                        WDF_NO_OBJECT_ATTRIBUTES,
                                        BMPCI_POOL_TAG,
                                        & bmdi->Lookaside);

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "Couldn't allocate lookaside list status %!STATUS!\n",
                        status);
            return status;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DEVICE,
                    "PDO(0x%p) FDO(0x%p), Lower(0x%p) DevExt (0x%p)\n",
                    WdfDeviceWdmGetPhysicalDevice(device),
                    WdfDeviceWdmGetDeviceObject(device),
                    WdfDeviceWdmGetAttachedDevice(device),
                    bmdi);

        WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
        pnpCaps.SurpriseRemovalOK = WdfFalse;
        pnpCaps.LockSupported     = WdfTrue;
        pnpCaps.EjectSupported    = WdfFalse;
        pnpCaps.Removable         = WdfFalse;
        WdfDeviceSetPnpCapabilities(device, &pnpCaps);

        WDF_DEVICE_POWER_CAPABILITIES_INIT(&powerCaps);
        powerCaps.DeviceD1                          = WdfFalse;
        powerCaps.DeviceD2                          = WdfFalse;
        powerCaps.WakeFromD1                        = WdfFalse;
        powerCaps.DeviceState[PowerSystemWorking]   = PowerDeviceD0;
        powerCaps.DeviceState[PowerSystemSleeping1] = PowerDeviceUnspecified;
        powerCaps.DeviceState[PowerSystemSleeping2] = PowerDeviceUnspecified;
        powerCaps.DeviceState[PowerSystemSleeping3] = PowerDeviceUnspecified;
        powerCaps.DeviceState[PowerSystemHibernate] = PowerDeviceD3;
        powerCaps.DeviceState[PowerSystemShutdown]  = PowerDeviceD3;

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Start to set PowerCapabilities\n");
        WdfDeviceSetPowerCapabilities(device, &powerCaps);

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Start to set driver infomation\n");
        status = bm_drv_set_info(bmdi);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Setting driver infomation success\n");
        //
        // Create a device interface so that applications can find and talk
        // to us.
        //
        status = WdfDeviceCreateDeviceInterface(
            device,
            g_guid_interface[0],
            //g_guid_interface_evb[0],
            NULL // ReferenceString
            );

        if (!NT_SUCCESS(status)) {
            TraceEvents(
                    TRACE_LEVEL_ERROR, TRACE_DEVICE, "Create device interface failed!\n");
            bmdrv_remove_bmci();
        }


        if (NT_SUCCESS(status)) {
            //
            // Initialize the I/O Package and any Queues
            //
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Start to initialize the I/O Package and any Queues\n");
            status = windriverQueueInitialize(device);
        }


        if (!bmci) {
            rc = bmdrv_init_bmci(Driver);
            if (rc) {
                TraceEvents(
                    TRACE_LEVEL_ERROR, TRACE_DEVICE, "bmci init failed!\n");
                bmdrv_remove_bmci();
                return STATUS_UNSUCCESSFUL;
            }
        } else {
            TraceEvents(
                TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bmci is not null, bmci pointer=0x%llx\n", (u64)bmci);
        }

        rc = bmdrv_ctrl_add_dev();
        if (rc) {
            TraceEvents(
                TRACE_LEVEL_ERROR, TRACE_DEVICE, "bmci add dev failed!\n");
            bmdrv_ctrl_del_dev();
            return STATUS_UNSUCCESSFUL;
        } else {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_DEVICE,
                        "dev_count=%d\n",
                        bmci->dev_count);
        }
    }

    return status;
}
