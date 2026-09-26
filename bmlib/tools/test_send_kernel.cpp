#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include <signal.h>
#include <sys/syscall.h>
#include <stdarg.h>

#define gettid() syscall(__NR_gettid)

volatile sig_atomic_t keep_running = 1;

void signal_handler(int signal) {
	if (signal == SIGINT) {
		printf("\n[PID:%d][TID:%ld] Received SIGINT signal, exiting gracefully...\n",
		       getpid(), (long)gettid());
		keep_running = 0;
	}
}

void print_with_id(const char *format, ...) {
	va_list args;
	va_start(args, format);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	struct tm *tm_info = localtime(&tv.tv_sec);

	printf("[PID:%d][TID:%ld][%02d:%02d:%02d.%03ld] ",
	       getpid(),
	       (long)gettid(),
	       tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
	       tv.tv_usec / 1000);

	vprintf(format, args);
	va_end(args);
}

int main(int argc, char *argv[])
{
	int chip_num = 0;
	bm_handle_t handle = NULL;
	bm_status_t ret = BM_SUCCESS;
	char *string = NULL;
	u32 size = 20;
	unsigned int count = 0;
	int block_num = 1;
	int group_num = 1;
	int loop_num = 1;
	int i;

	if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
		printf("Usage: %s [group_num] [block_num] [loop_num]\n", argv[0]);
		printf("  loop_num: times to send, default 1; <=0 means infinite until Ctrl+C\n");
		printf("Example: %s 1 4 1\n", argv[0]);
		return 0;
	}

	if (argc >= 3) {
		group_num = atoi(argv[1]);
		block_num = atoi(argv[2]);
		if ((block_num < 1 || block_num > 4 || group_num < 1)) {
			printf("invalid params,group_num:%d, block_num:%d\n", group_num, block_num);
			return -1;
		}
	}
	if (argc >= 4)
		loop_num = atoi(argv[3]);

	print_with_id("group_num= %d block_num= %d loop_num= %d\n", group_num, block_num, loop_num);

	signal(SIGINT, signal_handler);

	string = (char *)malloc(size);
	if (string == NULL) {
		print_with_id("Memory allocation failed!\n");
		return -1;
	}
	memset(string, 0, size);
	memcpy(string, "this is a test", 14);

	ret = bm_dev_request(&handle, chip_num);
	if (ret != BM_SUCCESS || handle == NULL) {
		print_with_id("bm_dev_request failed, ret = %d\n", ret);
		free(string);
		return -1;
	}

	for (i = 0; keep_running && (loop_num <= 0 || i < loop_num); i++) {
		ret = bm_send_api_to_multi_core(handle, 0x5a5a5a5a, (const u8 *)string, &size, group_num, block_num);
		if (ret != BM_SUCCESS) {
			print_with_id("bm_send_api failed, ret = %d\n", ret);
			break;
		}
		count++;
		if (count % 10 == 0) {
			print_with_id("start wait in bm_thread_sync_from_core\n");
			ret = bm_thread_sync_from_core(handle, 0);
			print_with_id("wait exit bm_thread_sync_from_core.\n");
			if (ret != BM_SUCCESS)
				break;
		}
	}

	if (ret == BM_SUCCESS && count > 0 && (count % 10 != 0)) {
		ret = bm_thread_sync_from_core(handle, 0);
		if (ret != BM_SUCCESS)
			print_with_id("final bm_thread_sync_from_core failed, ret = %d\n", ret);
	}

	bm_dev_free(handle);
	free(string);

	print_with_id("done, send_count=%u\n", count);
	return (ret == BM_SUCCESS) ? 0 : -1;
}
