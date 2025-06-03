#include "../include/bm_smi_mem_info.hpp"
#include "version.h"

static int                     dev_cnt;
static int                     start_dev;
static int                     g_driver_version;
static int                     text_lines;
static int                     win_y_offset;
static int                     proc_y;
static bool                    proc_show;
static char                    board_name[25] = "";
static std::ofstream           target_file;
static struct bm_smi_attr      g_attr[64];
static struct bm_smi_proc_gmem proc_gmem[64];
#ifdef __linux__
static std::string file_path((getenv("HOME")));
#else
static std::string file_path = "C:/test";
static int         err_count = 0;
#define MAX_ERROR_COUNT 10
#endif

#ifdef __linux__
#ifdef SOC_MODE
static int bm_smi_dev_request(bm_handle_t *handle) {
	bm_context_t *ctx = new bm_context_t{};
	if (!ctx)
		return -1;

	int fd = open("/dev/ion", O_RDWR);
	if (fd == -1) {
		printf("Cannot open /dev/ion\n");
		exit(-1);
	}
	ctx->ion_fd = fd;
	bm_get_carveout_heap_id(ctx);
	*handle = ctx;
	return 0;
}

static int bm_smi_get_board_name(void) {
	bm_handle_t handle = NULL;
	int ret = bm_dev_request(&handle, 0);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret = %d\n", ret);
		return -1;
	}
	bm_get_board_name(handle, board_name);
	bm_dev_free(handle);
	return 0;
}

static void bm_smi_dev_free(bm_handle_t ctx) {
	if (ctx->ion_fd)
		close(ctx->ion_fd);
	delete ctx;
}
#endif
#endif


static int read_ionmem_info(const char *path, u64 *value) {
	int fd, ret;
	char mem_info[16];
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("open %s failed\n", path);
		return -1;
	}
	ret = read(fd, mem_info, 10);
	if (ret < 0) {
		printf("read %s failed\n", path);
		return -1;
	}
	close(fd);
	*value = atol(mem_info);
	return 0;
}

static int read_procmem_info(const char *cmd, u32 *value) {
    char ret[30] = {0};
    FILE *fd = popen(cmd, "r");
    if (!fd) {
        return -1;
    }

    size_t nread = fread(ret, 1, sizeof(ret)-1, fd);
    pclose(fd);
    if (nread == 0) {
        return -1;
    }

    char *temp = strtok(ret, " ");
    while (temp != NULL && !isdigit(*temp)) {
        temp = strtok(NULL, " ");
    }

    if (temp == NULL) {
        return -1;
    }

    *value = atoi(temp);
    return 0;
}

/* get attibutes for the specified device*/
static void bm_smi_get_meminfo(bm_handle_t handle, int bmctl_fd, int dev_id) {
#ifdef SOC_MODE
    u64 ion_mem_total, ion_mem_alloc;
	u64 vpp_mem_total, vpp_mem_alloc, npu_mem_total, npu_mem_alloc;
	u32 mem_total, mem_free, cma_mem_total, cma_mem_free, ddr_size;
	const char *npu_total_path = NPU_TOTAL_PATH;
    const char *vpp_total_path = VPP_TOTAL_PATH;
    const char *npu_alloc_path = NPU_ALLOC_PATH;
	const char *vpp_alloc_path = VPP_ALLOC_PATH;
	//char ddr_size[4] = {0};
	int ret = 0;
	//FILE *fp;
    if (handle->ion_fd > 0) {
		ret = read_ionmem_info(npu_total_path, &npu_mem_total);
		if (ret < 0) {
			printf("read npu_mem_total failed\n");
		}
		ret = read_ionmem_info(vpp_total_path, &vpp_mem_total);
		if (ret < 0) {
			printf("read npu_mem_total failed\n");
		}
		ret = read_ionmem_info(npu_alloc_path, &npu_mem_alloc);
		if (ret < 0) {
			printf("read npu_mem_total failed\n");
		}
		ret = read_ionmem_info(vpp_alloc_path, &vpp_mem_alloc);
		if (ret < 0) {
			printf("read npu_mem_total failed\n");
		}
		ion_mem_total = vpp_mem_total + npu_mem_total;
		ion_mem_alloc = vpp_mem_alloc + npu_mem_alloc;

        g_attr[dev_id].ion_mem_total = ion_mem_total / 1024 / 1024;
        g_attr[dev_id].ion_mem_used  = ion_mem_alloc / 1024 / 1024;
        g_attr[dev_id].npu_mem_total = npu_mem_total / 1024 / 1024;
        g_attr[dev_id].npu_mem_used  = npu_mem_alloc / 1024 / 1024;
		g_attr[dev_id].vpp_mem_total = vpp_mem_total / 1024 / 1024;
        g_attr[dev_id].vpp_mem_used  = vpp_mem_alloc / 1024 / 1024;
    }

	ret = read_procmem_info("cat /proc/meminfo | grep MemTotal", &mem_total);
	if (ret < 0) {
		printf("read mem_total failed\n");
	}
	ret = read_procmem_info("cat /proc/meminfo | grep MemFree", &mem_free);
	if (ret < 0) {
		printf("read mem_total failed\n");
	}
	ret = read_procmem_info("cat /proc/meminfo | grep CmaTotal", &cma_mem_total);
	if (ret < 0) {
		printf("read mem_total failed\n");
	}
	ret = read_procmem_info("cat /proc/meminfo | grep CmaFree", &cma_mem_free);
	if (ret < 0) {
		printf("read mem_total failed\n");
	}
	ret = read_procmem_info("show_ddr |awk -F 'SIZE:' '{print $2}' | awk -F 'GB' '{print $1}'" , &ddr_size);
	if (ret < 0) {
		printf("read mem_total failed\n");
	}

	g_attr[dev_id].mem_total = mem_total / 1024;
	g_attr[dev_id].mem_used = (mem_total - mem_free) / 1024;
	g_attr[dev_id].cma_mem_total = cma_mem_total / 1024;
	g_attr[dev_id].cma_mem_used = cma_mem_total / 1024 - cma_mem_free / 1024;
	g_attr[dev_id].system_mem = SYSTEM_MEM_SIZE;
	g_attr[dev_id].ddr_size = ddr_size;
#endif
}

/* display mem_info */
//-----------------------------
//ddr_size:
//
//system_mem_size:
//Total_Memory_Usage:
//Cma_Memory_Usage:
//Ion_Memory_Usage:
//	vpp_heap:
//	npu_heap:
static void bm_smi_display_meminfo(int            dev_id,
								int            dis_slot,
								std::ofstream &file,
								bool           save_file) {
    char line_str[BUFFER_LEN]{};
	char head_str[BUFFER_LEN]{};
    int str_length;
	int attr_y;
	int i = 0;
	
    attr_y -= win_y_offset;

	snprintf(line_str, BUFFER_LEN, "ddr_size:%dGB\n\n", g_attr->ddr_size);
	if (file.is_open() && save_file) {
		file << line_str;
	}
	if (attr_y >= 0) {
		if (move(attr_y + 0, 0) == OK) {
			clrtoeol();
			attron(COLOR_PAIR(0));
			printw("%s", line_str);
			attroff(COLOR_PAIR(0));
		}
	}
	snprintf(line_str, BUFFER_LEN, "system_mem_size:%dMB\n", g_attr->system_mem);
	if (file.is_open() && save_file) {
		file << line_str;
	}
	if (attr_y >= 0) {
		if (move(attr_y + 1, 0) == OK) {
			clrtoeol();
			attron(COLOR_PAIR(0));
			printw("%s", line_str);
			attroff(COLOR_PAIR(0));
		}
	}
	snprintf(line_str, BUFFER_LEN, "total_memory_usage:%dMB/%dMB\n", g_attr->mem_used, g_attr->mem_total);
	if (file.is_open() && save_file) {
		file << line_str;
	}
	if (attr_y >= 0) {
		if (move(attr_y + 2, 0) == OK) {
			clrtoeol();
			attron(COLOR_PAIR(0));
			printw("%s", line_str);
			attroff(COLOR_PAIR(0));
		}
	}
	snprintf(line_str, BUFFER_LEN, "cma_memory_usage:%dMB/%dMB\n", g_attr->cma_mem_used, g_attr->cma_mem_total);
	if (file.is_open() && save_file) {
		file << line_str;
	}
	if (attr_y >= 0) {
		if (move(attr_y + 3, 0) == OK) {
			clrtoeol();
			attron(COLOR_PAIR(0));
			printw("%s", line_str);
			attroff(COLOR_PAIR(0));
		}
	}
	snprintf(line_str, BUFFER_LEN, "ion_memory_usage:%dMB/%dMB\n", g_attr->ion_mem_used, g_attr->ion_mem_total);
	if (file.is_open() && save_file) {
		file << line_str;
	}
	if (attr_y >= 0) {
		if (move(attr_y + 4, 0) == OK) {
			clrtoeol();
			attron(COLOR_PAIR(0));
			printw("%s", line_str);
			attroff(COLOR_PAIR(0));
		}
	}
	snprintf(line_str, BUFFER_LEN, "    vpp_heap:%dMB/%dMB\n", g_attr->vpp_mem_used, g_attr->vpp_mem_total);
	if (file.is_open() && save_file) {
		file << line_str;
	}
	if (attr_y >= 0) {
		if (move(attr_y + 5, 0) == OK) {
			clrtoeol();
			attron(COLOR_PAIR(0));
			printw("%s", line_str);
			attroff(COLOR_PAIR(0));
		}
	}
	snprintf(line_str, BUFFER_LEN, "    npu_heap:%dMB/%dMB\n", g_attr->npu_mem_used, g_attr->npu_mem_total);
	if (file.is_open() && save_file) {
		file << line_str;
	}
	if (attr_y >= 0) {
		if (move(attr_y + 6, 0) == OK) {
			clrtoeol();
			attron(COLOR_PAIR(0));
			printw("%s", line_str);
			attroff(COLOR_PAIR(0));
		}
	}

}

/* ncurses init screen before display */
static void bm_smi_init_scr() {
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	clear();
	if (!has_colors()) {
		endwin();
		printf("error - no color support on this terminal");
		exit(1);
	}
	if (start_color() != OK) {
		endwin();
		printf("error - could not initialize colors\n");
		exit(2);
	}
	start_color();
	init_pair(0, COLOR_WHITE, COLOR_BLACK);
	init_pair(1, COLOR_YELLOW, COLOR_BLACK);
	init_pair(2, COLOR_RED, COLOR_BLACK);
}

#ifndef SOC_MODE
#ifdef __linux__
static void bm_smi_fetch_all(int fd, int dev_cnt, int start_dev)
#else
static void bm_smi_fetch_all(HANDLE bmctl_device, int dev_cnt, int start_dev)
#endif
#else
static void bm_smi_fetch_all(bm_handle_t handle,
							int         fd,
							int         dev_cnt,
							int         start_dev)
#endif
{

	for (int i = start_dev; i < start_dev + dev_cnt; i++) {
#ifdef SOC_MODE
		bm_smi_get_meminfo(handle, fd, i);
#endif
	}
}
/* display all the information: attributes and proc gmem */
static void bm_smi_display_all(std::ofstream &target_file,
							bool           save_file,
							int            dev_cnt,
							int            start_dev) {

	for (int i = start_dev; i < start_dev + dev_cnt; i++) {
		bm_smi_display_meminfo(i, i - start_dev, target_file, save_file);
	}

	refresh();
}

#ifndef SOC_MODE
#ifdef __linux__
static void bm_smi_print_text_info(int fd, int start_dev, int last_dev) {
#else
static void bm_smi_print_text_info(HANDLE bmctl_device, int start_dev, int last_dev) {
#endif
	int  i = 0;
	char tpu_util_s[6];
	char tpu_util0_s[6];
	char tpu_util1_s[6];

#ifdef __linux__
	bm_smi_fetch_all(fd, last_dev - start_dev + 1, start_dev);
#else
	bm_smi_fetch_all(bmctl_device, last_dev - start_dev + 1, start_dev);
#endif

}
#endif

bm_smi_mem_info::bm_smi_mem_info(bm_smi_cmdline &cmdline) : bm_smi_test(cmdline) {
	if (bm_dev_getcount(&dev_cnt) != BM_SUCCESS) {
		printf("get devcount failed!\n");
	}
	start_dev        = 0;
	g_driver_version = 0;
	text_lines       = 0;
	win_y_offset     = 0;
	proc_y           = 0;
	proc_show        = true;
#ifdef __linux__
	static std::ifstream system_os("/etc/issue", std::ios::in);
	static char buffer[30] = "";

	if (system_os.is_open()) {
		system_os.getline(buffer, 30);
		if (strstr(buffer, "Ubuntu")) {
			setenv("TERM", "xterm-color", 1);
			setenv("TERMINFO", "/lib/terminfo", 1);
		} else if (strstr(buffer, "Debian")) {
			setenv("TERMINFO", "/lib/terminfo", 1);
		} else if (strstr(buffer, "Kylin")) {
			setenv("TERM", "xterm-color", 1);
			setenv("TERMINFO", "/lib/terminfo", 1);
		} else {
			setenv("TERM", "xterm-color", 1);
			setenv("TERMINFO", "/usr/share/terminfo", 1);
		}
		system_os.close();
	} else {
		setenv("TERM", "xterm-color", 1);
		setenv("TERMIFNO", "/usr/share/terminfo", 1);
		system_os.close();
	}
#endif
}

bm_smi_mem_info::~bm_smi_mem_info() {}

int bm_smi_mem_info::run_opmode() {
	int  ch;
	bool loop      = true;
	bool save_file = false;  // tell if save file for next display (not every
							// display file is saved)

#ifdef __linux__
	fd = bm_smi_open_bmctl(&g_driver_version);
#else
	bmctl_device = bm_smi_open_bmctl(&g_driver_version);
	if (!bmctl_device) {
		printf("open bmctl failed!, do not get driver version\n");
		return -1;
	}
#endif

#ifndef SOC_MODE
	int chip_mode;
	/* check board mode */
	struct bm_misc_info misc_info;
	bm_handle_t handle1;
	bm_status_t ret1 = BM_SUCCESS;

	ret1 = bm_dev_request(&handle1, start_dev);
	if (ret1 != BM_SUCCESS)
		return -EINVAL;

	ret1 = bm_get_misc_info(handle1, &misc_info);
	if (ret1 != BM_SUCCESS)
		return -EINVAL;

	chip_mode = misc_info.pcie_soc_mode;
	bm_dev_free(handle1);
	if (g_cmdline.m_text_format) {
		if (chip_mode == 1) {
			printf("text_format failed!\n");
			printf("this parameter is not support on SOC mode\n");
			return -1;
		} else {
			if ((g_cmdline.m_start_dev == 0xff) &&
				(g_cmdline.m_last_dev == 0xff)) {
				printf("error dev!please input start_dev and last_dev.\n");
				return -EINVAL;
			} else {
#ifdef __linux__
				bm_smi_print_text_info(
					fd, g_cmdline.m_start_dev, g_cmdline.m_last_dev);
#else
				bm_smi_print_text_info(bmctl_device, g_cmdline.m_start_dev,
									g_cmdline.m_last_dev);
#endif
				return 0;
			}
		}
	}
#endif
	// init screen here; or need handle exception
	bm_smi_init_scr();

#ifdef SOC_MODE
	bm_handle_t handle;
	bm_smi_dev_request(&handle);
	bm_smi_get_board_name();
#endif

	// loop getting chars until an invalid char is got;
	// if no char is get, display all information.ch = getch();
	int sleep_cnt = 0;  // count of 100us display interval
	/* open ctrl node and get dev_cnt*/
	while (loop) {
#ifndef __linux__
		resize_term(getcurx(stdscr), getcury(stdscr));
		refresh();
#endif
		ch = getch();
		switch (ch) {
			case ERR:  // no input char
				if (sleep_cnt++ == 0) {
					// fetch attributes only at certain loops
#ifdef SOC_MODE
					bm_smi_fetch_all(handle, fd, dev_cnt, start_dev);
#endif
					// save file if attributes are fetched
					save_file = true;
				}

				// display in every loop; save file when needed
				bm_smi_display_all(target_file, save_file, dev_cnt, start_dev);

				if (save_file)
					save_file = false;

				if (sleep_cnt == g_cmdline.m_lms / 20 - (g_cmdline.m_lms / 500))
					sleep_cnt = 0;

				if (g_cmdline.m_loop)
#ifdef __linux__
					usleep(20000);
#else
					Sleep(20);
#endif
				else
					loop = false;

				break;

			case KEY_RESIZE:  // resize detected
				clear();
				refresh();
				break;

			case KEY_UP:  // scroll up one line
			case KEY_SR:
				if (win_y_offset > 0) {
					win_y_offset--;
				}
				break;
			case KEY_DOWN:  // scroll down one line
			case KEY_SF:
				if (win_y_offset < (text_lines - LINES + 2)) {
					win_y_offset++;
				}
				break;
			case KEY_PPAGE:  // scroll up one page
				if (win_y_offset > LINES / 2) {
					win_y_offset -= LINES / 2;
				} else if (win_y_offset > 0) {
					win_y_offset = 0;
				}
				break;
			case KEY_NPAGE:  // scroll down one page
				if (win_y_offset < (text_lines - LINES + 2)) {
					win_y_offset += LINES / 2;
				}
				break;
			case KEY_CTAB:  // refresh window after resize detected
				clear();
				refresh();
				break;
			default:  // others exit
				loop = false;
				break;
		}
	}

	if (target_file.is_open()) {
		target_file.close();
	}

#ifdef __linux__
	close(fd);
#else
	CloseHandle(bmctl_device);
#ifdef SOC_MODE
	bm_smi_dev_free(handle);
#endif
#endif
	endwin();

	return 0;
}

int bm_smi_mem_info::validate_input_para() {
	return 0;
}

int bm_smi_mem_info::check_result() {
	return 0;
}

void bm_smi_mem_info::case_help() {
	printf("case_help\n");
}
