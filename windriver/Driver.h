/*++

Module Name:

    driver.h

Abstract:

    This file contains the driver definitions.

Environment:

    Kernel-mode Driver Framework

--*/
#ifndef _DRIVER_H_
#define _DRIVER_H_

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include "device.h"
#include "queue.h"
#include "trace.h"

EXTERN_C_START

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD windriverEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP windriverEvtDriverContextCleanup;

EXTERN_C_END

#endif  // !_DRIVER_H_