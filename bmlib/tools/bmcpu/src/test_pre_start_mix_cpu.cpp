#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <bmlib_runtime.h>
#include <bmlib_internal.h>
#include <bmcpu_internal.h>
#include <arpa/inet.h>

#ifdef __linux__
#define MAX_CHIP_NUM 256

void test_msleep(int n_ms)
{
    int i = 0;
    for (i = 0; i < n_ms; i++)
        usleep(1000);
}

unsigned  int ipstr2num(char* str)
{
	int i = 0;
	int j = 0;
	char new_str[3];
	int num = 0;
	char ipstr[15];
	unsigned int result[4];
	unsigned  int ip = 0;

	while(*str != '\0')
	{
		while((*str != '.') && (*str != '\0'))
		{
			new_str[i] = *str;
			num = num * 10 + new_str[i] - '0';
			str += 1;
			i += 1;
		}
		result[j] = num;
		num = 0;
		if (*str == '\0')
		{
			break;
		}
		else
		{
			str += 1;
			i = 0;
			j += 1;
		}
	}

    ip |= ((result[3]&0xff)<<24);
    ip |= ((result[2]&0xff)<<16);
    ip |= ((result[1]&0xff)<<8);
    ip |= ((result[0]&0xff)<<0);

    return htonl(ip);
}

void *single_start_mixmode(int dev_id) {
    bm_handle_t handle;
    bm_status_t ret;
    const char* dev = "/dev/bm-sophon";
    const char*       kernel_path = "/opt/sophon/libsophon-current/data";
    char fip_path[100];
    char ramdisk_path[100];
    char dev_path[30];
    char cmd[100];
    bm_veth_ip_t ip_mask;
    bm_cpu_status_t status;

    sprintf(dev_path, "%s%d", dev, dev_id);
    sprintf(fip_path, "%s%s", kernel_path, "/fip.bin");
    sprintf(ramdisk_path, "%s%s", kernel_path, "/ramdisk_glibc_mix.itb");

    while (access(dev_path, F_OK) < 0) {
        sleep(1);
    }
    printf("%s exist! run continue\n", dev_path);

    printf("fip path: %s; \nkernel path: %s\n", fip_path, ramdisk_path);
    ret = bm_dev_request(&handle, dev_id);
    if ((ret != BM_SUCCESS) || (handle == NULL)) {
        printf("bm_dev_request error, ret = %d\r\n", ret);
        return (void *)BM_ERR_FAILURE;
    }

    ret = bm_get_mix_lock(handle);
    if (ret != BM_SUCCESS)
        printf("chip%d wait other thread enable mixmode\n", dev_id);

    while (ret != BM_SUCCESS) {
        test_msleep(10);
        ret = bm_get_mix_lock(handle);
    }

    if (handle->misc_info.chipid == 0x1686) {
        int cmd_ret;
        status = bmcpu_get_cpu_status(handle);
        if (status != BMCPU_IDLE) {
            ret = bmcpu_reset_cpu(handle);
            if (ret != BM_SUCCESS) {
                printf("reset cpu failed!\r\n");
                bm_free_mix_lock(handle);
                return (void *)BM_ERR_FAILURE;
            }
        }

        ret = bm_setup_veth(handle);
        if (ret != BM_SUCCESS)
        {
            bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                    BMLIB_LOG_ERROR,
                    "setup virtual ethernet error, ret %d\n",
                    ret);
            bm_free_mix_lock(handle);
            return (void *)BM_ERR_FAILURE;
        } else {
            printf("Setup veth%d success!\n", dev_id);
        }

        ret = bmcpu_start_mix_cpu(handle, fip_path, ramdisk_path);
        if ((ret != BM_SUCCESS) && (ret != BM_NOT_SUPPORTED)) {
            printf("start cpu %d failed!\r\n", dev_id);
            bm_free_mix_lock(handle);
            bm_dev_free(handle);
            return (void *)BM_ERR_FAILURE;
        }

        sprintf(cmd, "192.192.%d.2", dev_id);
        ip_mask.ip = ipstr2num(cmd);
        ip_mask.mask = 0xFFFFFF00;
        ret = bm_set_ip(handle, ip_mask);
        if (ret != BM_SUCCESS)
        {
            bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                    BMLIB_LOG_ERROR,
                    "set ip error, ret %d\n",
                    ret);
            bm_free_mix_lock(handle);
            return (void *)BM_ERR_FAILURE;
        } else {
            printf("set chip%d ip: %s\n", dev_id, cmd);
        }

        sprintf(cmd, "sudo ifconfig veth%d 192.192.%d.3", dev_id, dev_id);
        cmd_ret = system(cmd);
        if (cmd_ret == -1)
            printf("exec %s failed\n", cmd);

        sleep(20);

        bm_free_mix_lock(handle);
    }

    bm_dev_free(handle);
    return (void *)BM_SUCCESS;
}

void *bmcpu_pre_start(void *arg) {
    int dev_id = *(int *)arg;

    single_start_mixmode(dev_id);

    return (void *)BM_SUCCESS;
}

int main(int argc, char *argv[]) {
    int    dev_num;
    int    i;
    int    arg[MAX_CHIP_NUM];
    pthread_t threads[MAX_CHIP_NUM];
    int    ret;

    if (argc == 2) {
       single_start_mixmode(atoi(argv[1]));
       return 0;
    }

    if (BM_SUCCESS != bm_dev_getcount(&dev_num)) {
        printf("no sophon device found! when sophon device plugin in, sophon-rpc will run!\n");
        return 0;
    }

    if (dev_num > MAX_CHIP_NUM) {
        printf("too many device number %d!\n", dev_num);
        return -1;
    }

    for (i = 0; i < dev_num; i++) {
        arg[i] = i;
        ret = pthread_create(&threads[i], NULL, bmcpu_pre_start, (void *)&arg[i]);
        if (ret != 0) {
            printf("create thread %d error!\n", i);
            return -1;
        }
    }

    for (i = 0; i < dev_num; i++) {
        ret = pthread_join(threads[i], NULL);
        if (ret < 0) {
            printf("join thread %d error!\n", i);
            return -1;
        }
    }

    return 0;
}
#endif
