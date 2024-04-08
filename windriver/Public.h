/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

#ifndef _PUBLIC_H
#define _PUBLIC_H

//
// Define an Interface Guid so that apps can find the device and talk to it.
//

// {84703EC3-9B1B-49D7-9AA6-0C42C6465681}
DEFINE_GUID(GUID_DEVINTERFACE_bm_sophon0,
            0x84703ec3,
            0x9b1b,
            0x49d7,
            0x9a,
            0xa6,
            0xc,
            0x42,
            0xc6,
            0x46,
            0x56,
            0x81);


// {DCCB6586-7AF8-40A4-97B6-4D1C8E52718D}
DEFINE_GUID(GUID_DEVINTERFACE_bm_sophon1,
            0xdccb6586,
            0x7af8,
            0x40a4,
            0x97,
            0xb6,
            0x4d,
            0x1c,
            0x8e,
            0x52,
            0x71,
            0x8d);

// {11AA8451-91B0-4135-9263-65EA72685A5A}
DEFINE_GUID(GUID_DEVINTERFACE_bm_sophon2,
            0x11aa8451,
            0x91b0,
            0x4135,
            0x92,
            0x63,
            0x65,
            0xea,
            0x72,
            0x68,
            0x5a,
            0x5a);

// {7EB4D259-E6AC-2305-B061-98C0CA423A53}
DEFINE_GUID(GUID_DEVINTERFACE_bm_sophon_evb,
            0x7eb4d259,
            0xe6ac,
            0x2305,
            0xb0,
            0x61,
            0x98,
            0xc0,
            0xca,
            0x42,
            0x3a,
            0x53);

struct ion_allocation_data {
    u64 len;
    u32 heap_id_mask;
    u32 flags;
    u32 fd;
    u32 heap_id;
    u64 paddr;
};

#endif