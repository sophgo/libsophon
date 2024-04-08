/*++

Module Name:

    device.h

Abstract:

    This file contains the device definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#if !defined(_DEVICE_H_)
#define _DEVICE_H_

#include <initguid.h> // required for GUID definitions
#include <wdmguid.h> // required for WMILIB_CONTEXT
#include <wmistr.h>
#include <wmilib.h>
#include <ntintsafe.h>
#include "trace.h"
#include "public.h"

EXTERN_C_START

NTSTATUS windriverCreateDevice(_Inout_ WDFDRIVER          Driver,
                               _Inout_ PWDFDEVICE_INIT DeviceInit);

//NTSTATUS bm_drv_set_info(IN OUT PBMDI bmdi);
void bmdrv_pci_remove(struct bm_device_info *bmdi);
int bmdrv_pci_probe(struct bm_device_info *bmdi);
int bmdrv_pci_probe_delayed_function(struct bm_device_info *bmdi);
int bmdrv_get_card_start_index(struct bm_device_info *bmdi);


void bmdrv_pci_write_confg(struct bm_device_info *bmdi,
                           void *                 buffer,
                           u32                    offset,
                           u32                    length);

void bmdrv_pci_read_confg(struct bm_device_info *bmdi,
                          void *                 buffer,
                          u32                    offset,
                          u32                    length);

EXTERN_C_END
#endif
