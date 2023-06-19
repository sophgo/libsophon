#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "bm_video_interface.h"

#ifdef __linux__
#include <unistd.h>
#elif _WIN32
#include <windows.h>
#include "windows/libusb-1.0.18/examples/getopt/getopt.h"
#endif

#ifdef BM_PCIE_MODE
# if defined(CHIP_BM1682)
#  define VPU_DEVICE_NAME "/dev/bm1682-dev"
# elif defined(CHIP_BM1684)
#  define VPU_DEVICE_NAME "/dev/bm-sophon"
# else
#  error "PCIE mode is supported only in BM1682 and BM1684"
# endif
#else
#  define VPU_DEVICE_NAME "/dev/vpu"
#endif

static int devIdx  =  0;
static int coreIdx = -1;

static int parse_args(int argc, char* argv[]);
static int check_sophon_device(int devIdx);

int main(int argc, char* argv[])
{
    int ret;

    ret = parse_args(argc, argv);
    if (ret < 0)
        return -1;

    return BMVidVpuReset(devIdx, coreIdx);
}

static void usage(char* prog_name)
{
    static char options[] =
    "\t-d sophon device index. Reset VPU 0 at default. Only for PCIE mode.\n"
    "\t-c vpu core index.\n"
    "\t   for bm1684, 0-3: decoder, 4: encoder.\n"
    "\t   for bm1682, 0-2: decoder.\n"
    "\t   reset all cores at default.\n"
    "\t-h help\n"
    "\t-? help\n"
    "For example,\n"
    "\tvpureset           # reset all core of soc 0\n"
    "\tvpureset -d 1      # reset all core of soc 1\n"
    "\tvpureset -c 2      # reset the core 2 of soc 0\n"
    "\tvpureset -d 1 -c 4 # reset the core 4 of soc 1\n";

    fprintf(stderr, "Usage:\n\t%s [option]\n\noption:\n%s\n",
            prog_name, options);

    return ;
}

static int parse_args(int argc, char* argv[])
{
    int opt, ret;

    while (1)
    {
        opt = getopt(argc, argv, "d:c:h?");
        if (opt == -1)
            break;

        switch (opt)
        {
#ifdef BM_PCIE_MODE
        case 'd':
            devIdx = atoi(optarg);
            break;
#endif
        case 'c':
            coreIdx = atoi(optarg);
            break;
        case 'h':
        case '?':
            usage(argv[0]);
            return -1;
            break;
        default:
            break;
        }
    }

    ret = check_sophon_device(devIdx);
    if (ret < 0) {
        return -1;
    }

    return 0;
}

static int check_sophon_device(int devIdx)
{
    int vpu_fd;
#ifndef BM_PCIE_MODE
    vpu_fd = open(VPU_DEVICE_NAME, O_RDWR);
    if (vpu_fd < 0) {
        fprintf(stderr, "Invalid sophon device: %s\n", VPU_DEVICE_NAME);
        return -1;
    }
#else
    char vpu_dev_name[32] = {0};
    sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, devIdx);
    vpu_fd = open(vpu_dev_name, O_RDWR);
    if (vpu_fd < 0) {
        fprintf(stderr, "Invalid sophon device index: %d. "
                "Please check if %s exists.\n", devIdx, vpu_dev_name);
        return -1;
    }
#endif
    close(vpu_fd);
    return 0;
}
