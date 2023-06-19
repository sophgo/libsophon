//
// Created by yuan on 8/24/21.
//

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef __linux__
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#endif

#include "bmvpp_int.h"

#ifdef _WIN32
#include "bmlib_runtime.h"
bm_handle_t sophon_dev_fd[BMVPP_MAX_NUM_SOPHONS] = { 0 };
#else //!_WIN32
#if defined(BM_PCIE_MODE)
#include "bmlib_runtime.h"
bm_handle_t sophon_dev_fd[BMVPP_MAX_NUM_SOPHONS] = { [0 ... (BMVPP_MAX_NUM_SOPHONS-1)] = 0 };
#else
int sophon_dev_fd[BMVPP_MAX_NUM_SOPHONS] = { [0 ... (BMVPP_MAX_NUM_SOPHONS-1)] = -1 };
#endif
#endif

static int sophon_dev_count[BMVPP_MAX_NUM_SOPHONS] = { 0 };

static int check_sophon_dev_fd(int soc_idx)
{
    if (sophon_dev_fd[soc_idx] <= 0)
        return BMVPP_ERR;

    return BMVPP_OK;
}

int bmvpp_open(int soc_idx)
{
    int ret;

    bmvpp_log_trace("[%s,%d] enter\n", __func__, __LINE__);
    ret = check_sophon_dev_fd(soc_idx);
    if (ret >= 0) {
        sophon_dev_count[soc_idx]++;
        bmvpp_log_trace("[%s,%d] leave\n", __func__, __LINE__);
        return BMVPP_OK;
    }

#ifdef BM_PCIE_MODE
    ret = bm_dev_request(&sophon_dev_fd[soc_idx], soc_idx);
    if (ret != BM_SUCCESS) {
        bmvpp_log_err("bm_dev_request() err=%d\n", ret);
        return BMVPP_ERR;
    }
#else
    char device_name[512] = {0};
    int dev_fd;
    sprintf(device_name, "/dev/bm-vpp");
     dev_fd = open(device_name, O_RDWR);
    if (dev_fd < 0) {
        bmvpp_log_err("open %s failed. errno=<%d, %s>\n",
                      device_name, errno, strerror(errno));
        return BMVPP_ERR;
    }
    sophon_dev_fd[soc_idx] = dev_fd;
    bmvpp_log_debug("open %s for bmvpp.\n", device_name);
#endif

    sophon_dev_count[soc_idx]++;
    char *val = getenv("BMVPP_LOG_LEVEL");
    if (val != NULL) {
        bmvpp_set_log_level(atoi(val));
    }

    bmvpp_log_trace("[%s,%d] leave\n", __func__, __LINE__);

    return BMVPP_OK;
}

int bmvpp_close(int soc_idx)
{
    int ret;

    bmvpp_log_trace("[%s,%d] enter\n", __func__, __LINE__);

    ret = check_sophon_dev_fd(soc_idx);
    if (ret < 0) {
        bmvpp_log_trace("[%s,%d] leave\n", __func__, __LINE__);
        return BMVPP_ERR;
    }

#ifdef BM_PCIE_MODE
#ifdef __linux__
    bmvpp_log_debug("close /dev/bm-sophon%d for bmvpp.\n", soc_idx);
#elif _WIN32
    bmvpp_log_debug("close handle%d for bmvpp.\n", soc_idx);
#endif
#else
    bmvpp_log_debug("close /dev/bm-vpp for bmvpp.\n");
#endif

    if (sophon_dev_count[soc_idx] > 0)
        sophon_dev_count[soc_idx]--;
    if (sophon_dev_count[soc_idx] == 0) {
#ifdef BM_PCIE_MODE
        bm_dev_free(sophon_dev_fd[soc_idx]);
        sophon_dev_fd[soc_idx] = NULL;
#else
        if (sophon_dev_fd[soc_idx] > 0) {
            close(sophon_dev_fd[soc_idx]);
            sophon_dev_fd[soc_idx] = -1;
        }
#endif
    }

    bmvpp_log_trace("[%s,%d] leave\n", __func__, __LINE__);

    return BMVPP_OK;
}

int bmvpp_get_status(int soc_idx)
{
#if !defined(BM_PCIE_MODE) && !defined(_WIN32)
    vpp_get_status(); //SOC
#else
     //TODO
#endif

    return BMVPP_OK;
}

void bmvpp_reset(int soc_idx)
{
#if !defined(BM_PCIE_MODE) && !defined(_WIN32)
    vpp_top_rst();
#else
    // TODO
#endif
}

int bmvpp_get_vppfd(int soc_idx, vpp_fd_s *vppfd)
{
    memset(vppfd, 0, sizeof(*vppfd));
#ifdef BM_PCIE_MODE
    vppfd->handle = sophon_dev_fd[soc_idx];
#else
    vppfd->dev_fd = sophon_dev_fd[soc_idx];
#endif

    return BMVPP_OK;
}
