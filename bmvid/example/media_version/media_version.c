/*****************************************************************************
 *
 *    Copyright (C) 2025 Sophgo Technologies Inc.  All rights reserved.
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bmcv_api_ext_c.h"
#include "bm_vpudec_interface.h"
#include "bm_vpuenc_interface.h"
#include "bm_jpeg_interface.h"

int main(int argc, char **argv)
{
    const char *env_val = getenv("BMCV_PRINT_VERSION");
    if (setenv("BMCV_PRINT_VERSION", "1", 1) != 0) {
        perror("setenv BMCV_PRINT_VERSION failed");
        return 1;
    }
    bmcv_print_version();
    if (env_val == NULL || strcmp(env_val, "1") != 0) {
        setenv("BMCV_PRINT_VERSION", "0", 1);
    }
    printf("\n");
    bm_vpuapi_get_commit_version();
    bm_vpulite_get_commit_version();
    bm_video_get_commit_version();
    bm_jpuapi_get_commit_version();
    bm_jpulite_get_commit_version();

    return 0;
}