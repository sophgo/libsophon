#include "../include/bm_smi_core_util.hpp"
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

/* get attibutes for the specified device*/
#ifndef SOC_MODE
#ifdef __linux__
static void bm_smi_get_util(int bmctl_fd, int dev_id) {
#else
static void bm_smi_get_util(HANDLE bmctl_device, int dev_id) {
#endif
#else
static void bm_smi_get_util(bm_handle_t handle, int bmctl_fd, int dev_id) {
#endif
	g_attr[dev_id].dev_id = dev_id;
#ifdef __linux__
	if (ioctl(bmctl_fd, BMCTL_GET_SMI_ATTR, &g_attr[dev_id]) < 0) {
#else
	DWORD bytesReceived = 0;
	BOOL  status        = TRUE;
	status = DeviceIoControl(bmctl_device,
							BMCTL_GET_SMI_ATTR,
							&dev_id,
							sizeof(s32),
							&g_attr[dev_id],
							sizeof(BM_SMI_ATTR),
							&bytesReceived,
							NULL);
	if (status == FALSE) {
		printf("DeviceIoControl BMCTL_GET_SMI_ATTR failed 0x%x\n",
			GetLastError());
		err_count++;
		if (err_count == MAX_ERROR_COUNT) {
			printf("driver may be removed or be abnormal\n");
			exit(-1);
		}
	}

	if (!status) {
#endif
		g_attr[dev_id].chip_id           = ATTR_FAULT_VALUE;
		g_attr[dev_id].status            = ATTR_FAULT_VALUE;
		g_attr[dev_id].tpu_util          = ATTR_FAULT_VALUE;
		g_attr[dev_id].tpu_util0         = ATTR_FAULT_VALUE;
		g_attr[dev_id].tpu_util1         = ATTR_FAULT_VALUE;
		g_attr[dev_id].card_index        = ATTR_FAULT_VALUE;
	}
}

#ifdef __linux__
/* get process gmem info for the specified device */
static void bm_smi_get_proc_gmem(int bmctl_fd, int dev_id) {
	proc_gmem[dev_id].dev_id = dev_id;
	if (ioctl(bmctl_fd, BMCTL_GET_PROC_GMEM, &proc_gmem[dev_id]) < 0) {
		proc_gmem[dev_id].proc_cnt = ATTR_FAULT_VALUE;
		return;
	}
}
#else
static void bm_smi_get_proc_gmem(HANDLE bmctl_device, int dev_id) {
	proc_gmem[dev_id].dev_id = dev_id;
	DWORD bytesReceived      = 0;
	BOOL  status             = TRUE;
	status                   = DeviceIoControl(bmctl_device,
							BMCTL_GET_PROC_GMEM,
							&dev_id,
							sizeof(s32),
							&proc_gmem[dev_id],
							sizeof(struct bm_smi_proc_gmem),
							&bytesReceived,
							NULL);
	if (status == FALSE) {
		printf("DeviceIoControl BMCTL_GET_PROC_GMEM failed 0x%x\n",
			GetLastError());
		//CloseHandle(bmctl_device);
		err_count++;
		if (err_count == (MAX_ERROR_COUNT)) {
			printf("driver may be removed or be abnormal\n");
			exit(-1);
		}
	}
}
#endif

/* convert tpu util to string*/
static void bm_smi_tpu_util_to_str(int dev_id, char *s) {
	if (g_attr[dev_id].tpu_util == ATTR_NOTSUPPORTED_VALUE) {
		snprintf(s, 6, "%s", " N/A ");
	} else if (g_attr[dev_id].tpu_util == 100) {
		snprintf(s, 6, "%s", "100% ");
	} else {
		snprintf(s, 4, "%d%%", g_attr[dev_id].tpu_util);
	}
}

/* convert tpu util0 to string*/
static void bm_smi_tpu_util0_to_str(int dev_id, char *s) {
	if (g_attr[dev_id].tpu_util0 == ATTR_NOTSUPPORTED_VALUE) {
		snprintf(s, 6, "%s", " N/A ");
	} else if (g_attr[dev_id].tpu_util0 == 100) {
		snprintf(s, 6, "%s", "100% ");
	} else {
		snprintf(s, 4, "%d%%", g_attr[dev_id].tpu_util0);
	}
}

/* convert tpu util1 to string*/
static void bm_smi_tpu_util1_to_str(int dev_id, char *s) {
	if (g_attr[dev_id].tpu_util1 == ATTR_NOTSUPPORTED_VALUE) {
		snprintf(s, 6, "%s", " N/A ");
	} else if (g_attr[dev_id].tpu_util1 == 100) {
		snprintf(s, 6, "%s", "100% ");
	} else {
		snprintf(s, 4, "%d%%", g_attr[dev_id].tpu_util1);
	}
}

/* convert api_num to string*/
static void bm_smi_api_num0_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].card_index == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 6, "%s", " N/A ");
    } else if (g_attr[dev_id].card_index == ATTR_FAULT_VALUE) {
        snprintf(s, 6, "%s", " F ");
    } else {
        snprintf(s, 6, "%x", g_attr[dev_id].card_index);
    }
}

/* convert api_num to string*/
static void bm_smi_api_num1_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].card_index == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 6, "%s", " N/A ");
    } else if (g_attr[dev_id].card_index == ATTR_FAULT_VALUE) {
        snprintf(s, 6, "%s", " F ");
    } else {
        snprintf(s, 6, "%x", g_attr[dev_id].card_index);
    }
}

/* display util */
//--------------------------
//core id    util    api_num
//   0        10%       0
//   1        10%       1
static void bm_smi_display_util(int            dev_id,
								int            dis_slot,
								std::ofstream &file,
								bool           save_file) {
	char tpu_util_s[2][6];
    char line_str[BUFFER_LEN]{};
	char head_str[BUFFER_LEN]{};
    int str_length;
	int core_num;
	int proc_display_y;
	int attr_y;
	int i = 0;
	static bool header_displayed = false;

	if (!strcmp(board_name, "BM1688-SOC")) {
		core_num = 2;
	} else if (!strcmp(board_name, "CV186AH-SOC")) {
		core_num = 1;
	} else {
		printf("illagal board name\n");
		return;
	}

	bm_smi_tpu_util0_to_str(dev_id, tpu_util_s[0]);
	bm_smi_tpu_util1_to_str(dev_id, tpu_util_s[1]);

	attr_y = dis_slot * BM_SMI_CORE_UTIL_HEIGHT;
    attr_y -= win_y_offset;

	if (!header_displayed) {
		snprintf(line_str, BUFFER_LEN, "core_id    util\n");
		printw("%s", line_str);
		if (file.is_open() && save_file) {
			file << line_str;
		}
		header_displayed = true;
	}

	for (i = 0; i < core_num; i++) {
		snprintf(line_str,
				BUFFER_LEN,
				"%4d%11s\n",
				i,
				tpu_util_s[i]);
		if (file.is_open() && save_file) {
			file << line_str;
		}
		if (attr_y >= 0) {
			if (move(attr_y + BM_SMI_CORE_UTIL_MEM + i, 0) == OK) {
				//printf("%s\n", line_str);
				clrtoeol();
				attron(COLOR_PAIR(0));
				printw("%s", line_str);
				attroff(COLOR_PAIR(0));
			}
		}
	}
}

#ifdef __linux__
/* get process name by pid */
static std::string get_process_name_by_pid(const int pid) {
	char               proc_name[128];
	std::ifstream      cmd_file;
	std::ostringstream cmd_file_name;
	cmd_file_name << "/proc/" << pid << "/cmdline";
	cmd_file.open(cmd_file_name.str());
	if (!cmd_file) {
		return {};
	}
	cmd_file.get(proc_name, 128, ' ');
	return proc_name;
}
#else
static std::string get_process_name_by_pid(const int pid) {
	char proc_name[128];
	HANDLE processhandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
	if (processhandle == NULL) {
		CloseHandle(processhandle);
		return {};
	}

	auto len = GetModuleBaseNameA(processhandle, NULL, proc_name, 128);
	if (len == 0) {
		CloseHandle(processhandle);
		return {};
	}

	CloseHandle(processhandle);
	return proc_name;
}
#endif

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

/* fetch all the attributes info from kernel */
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
		bm_smi_get_util(handle, fd, i);
#endif
		bm_smi_get_proc_gmem(fd, i);
		g_attr[i].board_endline = 1;
		g_attr[i].board_attr    = 1;
	}
}
/* display all the information: attributes and proc gmem */
static void bm_smi_display_all(std::ofstream &target_file,
							bool           save_file,
							int            dev_cnt,
							int            start_dev) {

	for (int i = start_dev; i < start_dev + dev_cnt; i++) {
		bm_smi_display_util(i, i - start_dev, target_file, save_file);
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

	for (i = start_dev; i < (last_dev + 1); i++) {
		bm_smi_tpu_util_to_str(i, tpu_util_s);
		bm_smi_tpu_util0_to_str(i, tpu_util_s);
		bm_smi_tpu_util1_to_str(i, tpu_util_s);

		printf("%s ", tpu_util_s);
		printf("%s ", tpu_util0_s);
		printf("%s ", tpu_util1_s);
	}
}
#endif

bm_smi_core_util::bm_smi_core_util(bm_smi_cmdline &cmdline) : bm_smi_test(cmdline) {
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

bm_smi_core_util::~bm_smi_core_util() {}

int bm_smi_core_util::run_opmode() {
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
#ifndef SOC_MODE
#ifdef __linux__
					bm_smi_fetch_all(fd, dev_cnt, start_dev);
#else
					bm_smi_fetch_all(bmctl_device, dev_cnt, start_dev);
#endif
#else
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

int bm_smi_core_util::validate_input_para() {
	return 0;
}

int bm_smi_core_util::check_result() {
	return 0;
}

void bm_smi_core_util::case_help() {
	printf("case_help\n");
}
