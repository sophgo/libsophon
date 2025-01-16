#include "../include/bm_smi_test.hpp"


bm_smi_test::bm_smi_test(bm_smi_cmdline &cmdline) : g_cmdline(cmdline) {}


bm_smi_test::~bm_smi_test() {}

int bm_smi_test::validate_input_para() { return 0; }

int bm_smi_test::run_opmode() { return 0; }

int bm_smi_test::check_result()  { return 0; }

void bm_smi_test::case_help() {}

#ifdef __linux__
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
#else
HANDLE bm_smi_open_bmctl(int *driver_version) {
    HANDLE bmdev_ctl;
    DWORD  errNum        = 0;
    BOOL   status        = TRUE;
    DWORD  bytesReceived = 0;
    UCHAR  outBuffer     = 0;
    int    ret           = 0;

    bmdev_ctl = CreateFileW(L"\\\\.\\bmdev_ctl",
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);

    if (bmdev_ctl == INVALID_HANDLE_VALUE) {
        errNum = GetLastError();
        if (!(errNum == ERROR_FILE_NOT_FOUND ||
              errNum == ERROR_PATH_NOT_FOUND)) {
            printf("CreateFile failed!  ERROR_FILE_NOT_FOUND = %d\n", errNum);
        } else {
            printf("CreateFile failed!  errNum = %d\n", errNum);
        }
        CloseHandle(bmdev_ctl);
    } else {
        status = DeviceIoControl(bmdev_ctl,
                                 BMCTL_GET_DRIVER_VERSION,
                                 NULL,
                                 0,
                                 driver_version,
                                 sizeof(s32),
                                 &bytesReceived,
                                 NULL);
        if (status == FALSE) {
            printf("DeviceIoControl BMCTL_GET_SMI_ATTR failed 0x%x\n",
                   GetLastError());
            CloseHandle(bmdev_ctl);
            ret              = -1;
            *driver_version  = 1 << 16;
        }
        return bmdev_ctl;
    }
    return NULL;
}

/* get number of devices in system*/
int bm_smi_get_dev_cnt(HANDLE bmctl_device) {
    int   dev_cnt;
    BOOL  status        = TRUE;
    DWORD bytesReceived = 0;

    status = DeviceIoControl(bmctl_device,
                             BMCTL_GET_DEV_CNT,
                             NULL,
                             0,
                             &dev_cnt,
                             sizeof(int),
                             &bytesReceived,
                             NULL);
    if (status == FALSE) {
        printf("DeviceIoControl BMCTL_GET_DEV_CNT failed 0x%x\n",
               GetLastError());
        CloseHandle(bmctl_device);
        return -EINVAL;
    }

    return dev_cnt;
}
#endif