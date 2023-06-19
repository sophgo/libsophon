#include "sgcpu_mix_device.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef SOC_MODE
#define SGDEV_COMM_READ_MSG _IOR('p', 0x0, unsigned long)
#define SGDEV_COMM_WRITE_MSG _IOR('p', 0x1, unsigned long)
#define SGDEV_COMM_GET_CARDID _IOR('p', 0x2, unsigned long)

struct sg_comm_msg_info {
    char *msg;
    int   len;
};

int sg_get_local_id(void) {
    int fd, id, ret;

    fd = open("/dev/comm", O_RDWR);
    if (fd < 0) {
        printf("%s open comm failed: %d\n", __func__, fd);
        return -1;
    }

    ret = ioctl(fd, SGDEV_COMM_GET_CARDID, &id);
    if (ret == 0)
        ret = id;
    else
        ret = -1;

    close(fd);
    return ret;
}

int sg_read_data(int fd, char *buf, int len) {
    return read(fd, buf, len);
}

int sg_write_data(int fd, char *buf, int len) {
    return write(fd, buf, len);
}

int sg_read_msg(int fd, char *buf, int len) {
    struct sg_comm_msg_info msg;

    msg.msg = buf;
    msg.len = len;

    return ioctl(fd, SGDEV_COMM_READ_MSG, &msg);
}

int sg_write_msg(int fd, char *buf, int len) {
    struct sg_comm_msg_info msg;

    msg.msg = buf;
    msg.len = len;

    return ioctl(fd, SGDEV_COMM_WRITE_MSG, &msg);
}
#endif
