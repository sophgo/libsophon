#ifndef _BM_IRQ_H
#define _BM_IRQ_H
/*
BOOLEAN BmEvtInterruptIsr(IN WDFINTERRUPT Interrupt, IN u32 MessageID);

VOID BmEvtInterruptDpc(IN WDFINTERRUPT WdfInterrupt, IN WDFOBJECT WdfDevice);

NTSTATUS BmEvtInterruptEnable(IN WDFINTERRUPT Interrupt,
                              IN WDFDEVICE    AssociatedDevice);

NTSTATUS BmEvtInterruptDisable(IN WDFINTERRUPT Interrupt,
                               IN WDFDEVICE    AssociatedDevice);

NTSTATUS BmEvtDeviceD0EntryPostInterruptsEnabled(
    IN WDFDEVICE Device, IN WDF_POWER_DEVICE_STATE PreviousState);

NTSTATUS BmEvtDeviceD0ExitPreInterruptsDisabled(
        IN WDFDEVICE Device, IN WDF_POWER_DEVICE_STATE TargetState);*/

struct bm_device_info;
#define MSG_IRQ_ID_CHANNEL_XPU 48 
#define MSG_IRQ_ID_CHANNEL_CPU 49
#define CDMA_IRQ_ID 46
typedef void (*bmdrv_submodule_irq_handler)(struct bm_device_info *bmdi);
extern bmdrv_submodule_irq_handler bmdrv_module_irq_handler[128];
void bmdrv_enable_irq(struct bm_device_info *bmdi, int irq_num);
void bmdrv_disable_irq(struct bm_device_info *bmdi, int irq_num);
void bmdrv_submodule_request_irq(struct bm_device_info *        bmdi,
                                 int                            irq_num,
                                 bmdrv_submodule_irq_handler irq_handler);
void bmdrv_submodule_free_irq(struct bm_device_info *bmdi, int irq_num);
int  bmdrv_init_irq(struct bm_device_info *bmdi);
void bmdrv_free_irq(struct bm_device_info *bmdi);



EVT_WDF_INTERRUPT_ISR     BmEvtInterruptIsr;
EVT_WDF_INTERRUPT_DPC     BmEvtInterruptDpc;
EVT_WDF_INTERRUPT_ENABLE  BmEvtInterruptEnable;
EVT_WDF_INTERRUPT_DISABLE BmEvtInterruptDisable;

EVT_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED
    BmEvtDeviceD0EntryPostInterruptsEnabled;
EVT_WDF_DEVICE_D0_EXIT_PRE_INTERRUPTS_DISABLED
    BmEvtDeviceD0ExitPreInterruptsDisabled;

#endif