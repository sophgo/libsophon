/*
 * Copyright (c) 2013-2015, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Top level SMC handler for SiP calls. Dispatch PM calls to PM SMC handler. */

#include <runtime_svc.h>
#include <uuid.h>
#include "pm_svc_main.h"

/* SMC function IDs for SiP Service queries */
#define ZYNQMP_SIP_SVC_CALL_COUNT    0x8200ff00
#define ZYNQMP_SIP_SVC_UID        0x8200ff01
#define ZYNQMP_SIP_SVC_VERSION        0x8200ff03

/* SiP Service Calls version numbers */
#define SIP_SVC_VERSION_MAJOR    0
#define SIP_SVC_VERSION_MINOR    1

/* These macros are used to identify PM calls from the SMC function ID */
#define PM_FID_MASK    0xf000u
#define PM_FID_VALUE    0u
#define is_pm_fid(_fid) (((_fid) & PM_FID_MASK) == PM_FID_VALUE)

/* SiP Service UUID */
DEFINE_SVC_UUID(zynqmp_sip_uuid,
        0x2a1d9b5c, 0x8605, 0x4023, 0xa6, 0x1b,
        0xb9, 0x25, 0x82, 0x2d, 0xe3, 0xa5);

/**
 * sip_svc_setup() - Setup SiP Service
 *
 * Invokes PM setup
 */
static int32_t sip_svc_setup(void)
{
    /* PM implementation as SiP Service */
    pm_setup();

    return 0;
}

/**
 * sip_svc_smc_handler() - Top-level SiP Service SMC handler
 *
 * Handler for all SiP SMC calls. Handles standard SIP requests
 * and calls PM SMC handler if the call is for a PM-API function.
 */
uint64_t sip_svc_smc_handler(uint32_t smc_fid,
                 uint64_t x1,
                 uint64_t x2,
                 uint64_t x3,
                 uint64_t x4,
                 void *cookie,
                 void *handle,
                 uint64_t flags)
{
    /* Let PM SMC handler deal with PM-related requests */
    if (is_pm_fid(smc_fid)) {
        return pm_smc_handler(smc_fid, x1, x2, x3, x4, cookie, handle,
                      flags);
    }

    switch (smc_fid) {
    case ZYNQMP_SIP_SVC_CALL_COUNT:
        /* PM functions + default functions */
        SMC_RET1(handle, PM_API_MAX + 2);

    case ZYNQMP_SIP_SVC_UID:
        SMC_UUID_RET(handle, zynqmp_sip_uuid);

    case ZYNQMP_SIP_SVC_VERSION:
        SMC_RET2(handle, SIP_SVC_VERSION_MAJOR, SIP_SVC_VERSION_MINOR);

    default:
        WARN("Unimplemented SiP Service Call: 0x%x\n", smc_fid);
        SMC_RET1(handle, SMC_UNK);
    }
}

/* Register PM Service Calls as runtime service */
DECLARE_RT_SVC(
        sip_svc,
        OEN_SIP_START,
        OEN_SIP_END,
        SMC_TYPE_FAST,
        sip_svc_setup,
        sip_svc_smc_handler);
