#ifndef BOOT_TEST_H
#define BOOT_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#ifdef USE_NPU_LIB
#include "bm_types.h"
#include "bm_native_ref.h"
#include "bm_kernel.h"
#include "bm_intrinsics.h"
#include "native_util.h"

#include "bmkernel.h"
#include "gmem_utils.h"
#include "lmem_utils.h"
#endif

#include "common.h"
#include "system_common.h"
#include "fw_config.h"

#include "pll.h"
#include "timer.h"
#include "ddr.h"

#endif /* BOOT_TEST_H */
