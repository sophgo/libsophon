#ifndef __SOPHGO_DAHUA_DEVICE_COMMUNICATION_H_
#define __SOPHGO_DAHUA_DEVICE_COMMUNICATION_H_

#if defined(__cplusplus)
extern "C" {
#endif
#ifdef SOC_MODE
#include <sys/ioctl.h>
#include <asm/ioctl.h>

int sg_get_local_id(void);
int sg_read_data(int fd, char *buf, int len);
int sg_write_data(int fd, char *buf, int len);
int sg_read_msg(int fd, char *buf, int len);
int sg_write_msg(int fd, char *buf, int len);
#endif
#if defined(__cplusplus)
}
#endif

#endif
