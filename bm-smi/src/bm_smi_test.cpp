#include "../include/bm_smi_test.hpp"


bm_smi_test::bm_smi_test(bm_smi_cmdline &cmdline) : g_cmdline(cmdline) {}


bm_smi_test::~bm_smi_test() {}

int bm_smi_test::validate_input_para() { return 0; }

int bm_smi_test::run_opmode() { return 0; }

int bm_smi_test::check_result()  { return 0; }

void bm_smi_test::case_help() {}

/* get number of devices in system*/
int bm_smi_get_dev_cnt(int bmctl_fd) {
    int dev_cnt;

    if (ioctl(bmctl_fd, BMCTL_GET_DEV_CNT, &dev_cnt) < 0) {
        perror("can not get devices number on this PC or Server\n");
        exit(EXIT_FAILURE);
    }

    return dev_cnt;
}

/* open bm-ctl device node for management*/
int bm_smi_open_bmctl(int *driver_version) {
    char dev_ctl_name[20];
    int  fd;

    snprintf(dev_ctl_name, sizeof(dev_ctl_name), BMDEV_CTL);
    fd = open(dev_ctl_name, O_RDWR);
    if (fd == -1) {
        perror("no sophon device found on this PC or Server\n");
        exit(EXIT_FAILURE);
    } else {
        if (ioctl(fd, BMCTL_GET_DRIVER_VERSION, driver_version) < 0) {
            *driver_version = 1 << 16;
        }
    }
    return fd;
}
