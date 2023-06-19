#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "bmlib_runtime.h"

#define BLOCK_SIZE 1024

int array_cmp_int(
		unsigned char *p_exp,
		unsigned char *p_got,
		int len,
		const char *info_label)
{
	int idx;
	for (idx = 0; idx < len; idx++) {
		if (p_exp[idx] != p_got[idx]) {
			printf("%s error at index %d exp %x got %x\n",
					info_label, idx, p_exp[idx], p_got[idx]);
			return -1;
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	bm_handle_t handle = NULL;
	bm_status_t ret = BM_ERR_FAILURE;
	unsigned char *sys_in_buffer = NULL;
	unsigned char *sys_out_buffer = NULL;
	unsigned int transfer_size = 0;
	unsigned int transfer_count = 0;
	int file_size = 0, check_data = 0;
	int consume = 0, cmp_ret = 0;
	float bandwidth = 0.0;
	size_t ret_size = 0;
	FILE *file_img = NULL;
	unsigned char *pbuf = NULL;
	long long dst_addr = -1;
	struct timeval tv_start;
	struct timeval tv_end;
	struct timeval timediff;

	if (argc != 4) {
		printf("Usage: ./pcie_download file_name dst_address check_enable\n");
		printf("eg: ./pcie_download fip.bin 0x110000000 1\n");
		printf("eg: ./pcie_download ramroot.itb 0x110080000 0\n");
		goto err;
	}
	dst_addr = strtoll(argv[2], NULL, 16);
	if (dst_addr < 0) {
		printf("dst address is invalid\n");
		goto err;
	}
	printf("transfer file :%s to dst_addr :0x%llx \n", argv[1], dst_addr);

	file_img = fopen(argv[1], "rb");
	if (!file_img) {
		perror("fopen");
		goto err;
	}

	fseek(file_img, 0, SEEK_END);
	file_size = ftell(file_img);

	if (file_size % sizeof(float))
		transfer_count = file_size / sizeof(float) + 1;
	else
		transfer_count = file_size / sizeof(float);

	transfer_size = sizeof(float) * transfer_count;

	printf("file_size :%d transfer_size :%d \n", file_size, transfer_size);
	sys_in_buffer = (unsigned char *)malloc(transfer_size);
	sys_out_buffer = (unsigned char *)malloc(transfer_size);
	if (!sys_in_buffer || !sys_out_buffer) {
		printf("malloc buffer for test failed\n");
		goto err;
	}

	memset(sys_in_buffer,  0, transfer_size);
	memset(sys_out_buffer, 0, transfer_size);

	fseek(file_img, 0, SEEK_SET);
	pbuf = sys_in_buffer;
	while (1) {
		ret_size = fread(pbuf, BLOCK_SIZE, 1, file_img);
		if (ret_size < 1)
			break;
		pbuf += BLOCK_SIZE;
	}
	ret = bm_dev_request(&handle, 0);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret = %d\n", ret);
		goto err;
	}

	gettimeofday(&tv_start, NULL);
	ret = bm_memcpy_s2d(handle,
			    bm_mem_from_device(dst_addr,transfer_size),
			    sys_in_buffer);
	if (ret != BM_SUCCESS) {
		printf("CDMA transfer from system to device failed, ret = %d\n", ret);
	}
	gettimeofday(&tv_end, NULL);
	timersub(&tv_end, &tv_start, &timediff);
	consume = timediff.tv_sec * 1000000 + timediff.tv_usec;
	if (consume > 0) {
		bandwidth = (float)transfer_size / (1024.0*1024.0) / (consume / 1000000.0);
		printf("S2D:Transfer size:0x%x byte. Cost time:%d us, Write Bandwidth:%.2f MB/s\n",
				transfer_size,
				consume,
				bandwidth);
	}
	check_data = atoi(argv[3]);
	if (check_data == 0)
		goto finish;

	gettimeofday(&tv_start, NULL);
	ret = bm_memcpy_d2s(handle,
			    sys_out_buffer,
			    bm_mem_from_device(dst_addr, transfer_size));
	if (ret != BM_SUCCESS) {
		printf("CDMA transfer from system to device failed, ret = %d\n", ret);
	}
	gettimeofday(&tv_end, NULL);
	timersub(&tv_end, &tv_start, &timediff);
	consume = timediff.tv_sec * 1000000 + timediff.tv_usec;
	if (consume > 0) {
		bandwidth = (float)transfer_size / (1024.0*1024.0) / (consume / 1000000.0);
		printf("D2S:Transfer size:0x%x byte. Cost time:%d us, Write Bandwidth:%.2f MB/s\n",
				transfer_size,
				consume,
				bandwidth);
	}

	cmp_ret = array_cmp_int(sys_in_buffer, sys_out_buffer, transfer_size, "pcie_download");
	printf("cdma transfer test %s.\n", cmp_ret ? "Failed" : "Success");

finish:
	bm_dev_free(handle);
	fclose(file_img);
	free(sys_in_buffer);
	free(sys_out_buffer);
	return 0;
err:
	if (handle)
		bm_dev_free(handle);
	if (file_img)
		fclose(file_img);
	if (sys_in_buffer)
		free(sys_in_buffer);
	if (sys_out_buffer)
		free(sys_out_buffer);
	return -1;
}
