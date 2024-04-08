#include "bm_common.h"

#include "bm_irq.tmh"

void bmdrv_enable_irq(struct bm_device_info *bmdi, int irq_num) {
    if (bmdi->cinfo.bmdrv_enable_irq)
        bmdi->cinfo.bmdrv_enable_irq(bmdi, irq_num, TRUE);
}

void bmdrv_disable_irq(struct bm_device_info *bmdi, int irq_num) {
    if (bmdi->cinfo.bmdrv_enable_irq)
        bmdi->cinfo.bmdrv_enable_irq(bmdi, irq_num, FALSE);
}

static void bmdrv_register_irq_handler(struct bm_device_info *     bmdi,
                                       int                         irq_num,
                                       bmdrv_submodule_irq_handler handler) {
    if (bmdi->cinfo.bmdrv_module_irq_handler)
        bmdi->cinfo.bmdrv_module_irq_handler[irq_num] = handler;
}

static void bmdrv_deregister_irq_handler(struct bm_device_info *bmdi,
                                         int                    irq_num) {
    if (bmdi->cinfo.bmdrv_module_irq_handler)
        bmdi->cinfo.bmdrv_module_irq_handler[irq_num] = NULL;
}

void bmdrv_submodule_request_irq(struct bm_device_info *     bmdi,
                                 int                         irq_num,
                                 bmdrv_submodule_irq_handler irq_handler) {
    bmdrv_register_irq_handler(bmdi, irq_num, irq_handler);
    bmdrv_enable_irq(bmdi, irq_num);
}

void bmdrv_submodule_free_irq(struct bm_device_info *bmdi, int irq_num) {
    bmdrv_deregister_irq_handler(bmdi, irq_num);
    bmdrv_disable_irq(bmdi, irq_num);
}

static void bmdrv_do_irq(struct bm_device_info *bmdi) {
    int i         = 0;
    int bitnum    = 0;
    u32 status[4] = {0};

    bmdi->cinfo.bmdrv_get_irq_status(bmdi, status);
    for (i = 0; i < 4; i++) {
        bitnum = 0x0;
        while (status[i] != 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "[bmdrv_do_irq] status[%d] is not zero\n", i);
            if (status[i] & 0x1) {
                if (bmdi->cinfo.bmdrv_module_irq_handler[i * 32 + bitnum] !=
                    NULL) {
                    bmdi->cinfo.irq_id = i * 32 + bitnum;
                    bmdi->cinfo.bmdrv_module_irq_handler[i * 32 + bitnum](bmdi);
                }
            }
            bitnum++;
            status[i] = status[i] >> 1;
        }
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "[bmdrv_do_irq] get out of the loop\n");
    if (bmdi->cinfo.bmdrv_unmaskall_intc_irq)
        bmdi->cinfo.bmdrv_unmaskall_intc_irq(bmdi);
}

void bmdrv_enable_msi(struct bm_device_info *bmdi) {
    USHORT command = 0;
    bmdrv_pci_read_confg(bmdi,
                         &command,
                         FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
                         sizeof(USHORT));

    command |= PCI_DISABLE_LEVEL_INTERRUPT;

    bmdrv_pci_write_confg(bmdi,
                          &command,
                          FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
                          sizeof(USHORT));

    bmdrv_pci_read_confg(bmdi,
                         &command,
                         FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
                         sizeof(USHORT));
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "bmdrv_enable_msi command=0x%x\n", command);
}

void bmdrv_disable_msi(struct bm_device_info *bmdi) {
    USHORT command = 0;
    bmdrv_pci_read_confg(bmdi,
                         &command,
                         FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
                         sizeof(USHORT));

    command &= ~PCI_DISABLE_LEVEL_INTERRUPT;

    bmdrv_pci_write_confg(bmdi,
                          &command,
                          FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
                          sizeof(USHORT));
}

int bmdrv_init_irq(struct bm_device_info *bmdi) {
    bmdrv_enable_msi(bmdi);

    if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686)
        bm1684_pcie_msi_irq_enable(bmdi);

    return 0;
}

void bmdrv_free_irq(struct bm_device_info *bmdi) {
    if (bmdi->cinfo.chip_id == 0x1684)
        bm1684_pcie_msi_irq_disable(bmdi);

    bmdrv_disable_msi(bmdi);
}

BOOLEAN BmEnableInterrupt(IN WDFINTERRUPT WdfInterrupt,
                                    IN WDFCONTEXT   Context) {
    PBMDI bmdi = (PBMDI)Context;

    UNREFERENCED_PARAMETER(bmdi);
    UNREFERENCED_PARAMETER(WdfInterrupt);

    //bmdrv_enable_msi(bmdi); // intc irq is enabled by bmdrv_do_irq's bmdrv_unmaskall_intc_irq

    return TRUE;
}

VOID BmDisableInterrupt(IN PBMDI bmdi) {
    UNREFERENCED_PARAMETER(bmdi);
    //bmdrv_disable_msi(bmdi);// intc irq is disabled by arm9
}

BOOLEAN
BmEvtInterruptIsr(IN WDFINTERRUPT Interrupt, IN ULONG MessageID)
{
    BOOLEAN   InterruptRecognized = FALSE;
    PBMDI bmdi             = NULL;
    //USHORT    IntStatus;
    BOOLEAN queueDpcSuccess;

    UNREFERENCED_PARAMETER(MessageID);
    UNREFERENCED_PARAMETER(InterruptRecognized);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "--> BmEvtInterruptIsr\n");

    bmdi = Bm_Get_Bmdi(WdfInterruptGetDevice(Interrupt));

    //
    // Disable the interrupt (will be re-enabled in BmEvtInterruptDpc
    //
    BmDisableInterrupt(bmdi);
    queueDpcSuccess=WdfInterruptQueueDpcForIsr(Interrupt);
    if (queueDpcSuccess == FALSE) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "queueDpcFail\n,<-- BmEvtInterruptIsr\n");
        return FALSE;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "<-- BmEvtInterruptIsr\n");

    return TRUE;
}

VOID BmEvtInterruptDpc(IN WDFINTERRUPT WdfInterrupt, IN WDFOBJECT WdfDevice)
{
    PBMDI bmdi = NULL;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "--> BmEvtInterruptDpc\n");

    bmdi = Bm_Get_Bmdi(WdfDevice);
    bmdrv_do_irq(bmdi);
    //
    // Re-enable the interrupt (disabled in MPIsr)
    //
    WdfInterruptSynchronize(WdfInterrupt, BmEnableInterrupt, bmdi);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "<-- BmEvtInterruptDpc\n");
}

NTSTATUS
BmEvtInterruptEnable(IN WDFINTERRUPT Interrupt, IN WDFDEVICE AssociatedDevice)
{
    PBMDI bmdi;

    UNREFERENCED_PARAMETER(Interrupt);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "--> BmEvtInterruptEnable\n");

    bmdi = Bm_Get_Bmdi(AssociatedDevice);

    bmdrv_init_irq(bmdi);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "<-- BmEvtInterruptEnable\n");

    return STATUS_SUCCESS;
}

NTSTATUS
BmEvtInterruptDisable(IN WDFINTERRUPT Interrupt, IN WDFDEVICE AssociatedDevice)
{
    PBMDI bmdi;

    UNREFERENCED_PARAMETER(Interrupt);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "--> BmEvtInterruptDisable\n");

    bmdi = Bm_Get_Bmdi(AssociatedDevice);

    bmdrv_free_irq(bmdi);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ISR, "<-- BmEvtInterruptDisable\n");

    return STATUS_SUCCESS;
}

NTSTATUS
BmEvtDeviceD0EntryPostInterruptsEnabled(
    IN WDFDEVICE Device, IN WDF_POWER_DEVICE_STATE PreviousState)
{
    PBMDI bmdi;

    UNREFERENCED_PARAMETER(PreviousState);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_ISR,
                "--> BmEvtDeviceD0EntryPostInterruptsEnabled\n");

    bmdi = Bm_Get_Bmdi(Device);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_ISR,
                "<-- BmEvtDeviceD0EntryPostInterruptsEnabled\n");

    return STATUS_SUCCESS;
}

NTSTATUS
BmEvtDeviceD0ExitPreInterruptsDisabled(IN WDFDEVICE              Device,
                                        IN WDF_POWER_DEVICE_STATE TargetState)
{
    PBMDI bmdi;

    UNREFERENCED_PARAMETER(TargetState);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_ISR,
                "--> BmEvtDeviceD0ExitPreInterruptsDisabled\n");

    bmdi = Bm_Get_Bmdi(Device);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_ISR,
                "<-- BmEvtDeviceD0ExitPreInterruptsDisabled\n");

    return STATUS_SUCCESS;
}