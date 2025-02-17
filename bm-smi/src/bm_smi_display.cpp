#include "../include/bm_smi_display.hpp"
#include "version.h"

static int                     dev_cnt;
static int                     start_dev;
static int                     g_driver_version;
static int                     text_lines;
static int                     win_y_offset;
static int                     proc_y;
static bool                    proc_show;
static char                    bm_smi_version[10] = PROJECT_VER;
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


/* get attibutes for the specified device*/
static void bm_smi_get_attr(bm_handle_t handle, int bmctl_fd, int dev_id) {
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
        g_attr[dev_id].chip_id = ATTR_FAULT_VALUE;
        g_attr[dev_id].status  = ATTR_FAULT_VALUE;
        g_attr[dev_id].chip_mode =
            ATTR_FAULT_VALUE;  // 0---pcie = ATTR_FAULT_VALUE; 1---soc
        g_attr[dev_id].domain_bdf        = ATTR_FAULT_VALUE;
        g_attr[dev_id].mem_used          = 0;
        g_attr[dev_id].mem_total         = 0;
        g_attr[dev_id].tpu_util          = ATTR_FAULT_VALUE;
        g_attr[dev_id].board_temp        = ATTR_FAULT_VALUE;
        g_attr[dev_id].chip_temp         = ATTR_FAULT_VALUE;
        g_attr[dev_id].board_power       = ATTR_FAULT_VALUE;
        g_attr[dev_id].tpu_power         = ATTR_FAULT_VALUE;
        g_attr[dev_id].fan_speed         = ATTR_FAULT_VALUE;
        g_attr[dev_id].vdd_tpu_volt      = ATTR_FAULT_VALUE;
        g_attr[dev_id].vdd_tpu_curr      = ATTR_FAULT_VALUE;
        g_attr[dev_id].atx12v_curr       = ATTR_FAULT_VALUE;
        g_attr[dev_id].tpu_min_clock     = ATTR_FAULT_VALUE;
        g_attr[dev_id].tpu_max_clock     = ATTR_FAULT_VALUE;
        // g_attr[dev_id].tpu_current_clock = ATTR_FAULT_VALUE;
        g_attr[dev_id].board_max_power   = ATTR_FAULT_VALUE;
        g_attr[dev_id].ecc_enable        = ATTR_FAULT_VALUE;
        g_attr[dev_id].ecc_correct_num   = ATTR_FAULT_VALUE;
        g_attr[dev_id].card_index        = ATTR_FAULT_VALUE;
    }

    u64 mem_total, mem_avail;
    if (handle->ion_fd > 0) {
        bm_total_gmem(handle, &mem_total);
        bm_avail_gmem(handle, &mem_avail);
        g_attr[dev_id].mem_total = mem_total / 1024 / 1024;
        g_attr[dev_id].mem_used  = ceil(mem_avail / 1024 / 1024);
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

// |-----------X------------>
// |
// |
// Y
// |
// |
// |
// \/

/*
  2   6       14      22     29    35      43    49    56        67  71 77 | TPU
ChipId  Status| boardT chipT boardP  TPU_P TPU_V 12V_ATX | ECC CorrectN SN |
*/

/* display attributes format header */

static void bm_smi_display_format(std::ofstream &file, bool save_file) {
    char   line_str[BUFFER_LEN]{};
    time_t now = time(0);
    char * dt  = ctime(&now);

    move(0, 0);

    for (int i = 0; i < BM_SMI_FORMAT_HEIGHT; i++) {
        switch (i) {
            case 0:
                snprintf(line_str, BUFFER_LEN, "%s", dt);
                break;
            case 1:
                snprintf(line_str,
                         BUFFER_LEN,
                         "+----------------------------------------------------"
                         "----------------------------------------------+\n");
                break;
            case 2:
                snprintf(
                    line_str,
                    BUFFER_LEN,
                    "| SDK Version:%9s             Driver Version:  "
                    "%1d.%1d.%1d                                         |\n",
                    bm_smi_version,
                    g_driver_version >> 16,
                    (g_driver_version >> 8) & 0xff,
                    g_driver_version & 0xff);
                break;
            case 3:
                snprintf(line_str,
                         BUFFER_LEN,
                         "+---------------------------------------+------------"
                         "----------------------------------------------+\n");
                break;
            case 4:
                snprintf(line_str,
                         BUFFER_LEN,
                         "|card  Name      Mode        SN         |TPU  boardT "
                         " chipT   TPU_P  TPU_V  ECC  CorrectN  Tpu-Util|\n");
                break;
            case 5:
                snprintf(line_str,
                         BUFFER_LEN,
                         "|12V_ATX  MaxP boardP Minclk Maxclk  Fan|Bus-ID      "
                         "Status   Currclk   TPU_C   Memory-Usage       |\n");
                break;
            case 6:
                snprintf(line_str,
                         BUFFER_LEN,
                         "|=======================================+============"
                         "==============================================|\n");
                break;
            default:
                break;
        }

        if (file.is_open() && save_file) {
            file << line_str;
        }
        if (i == 0 || LINES >= BM_SMI_FORMAT_HEIGHT) {
            printw("%s", line_str);
        }
    }
}

/* convert chipid to string*/
static void bm_smi_chipid_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].chip_id == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 6, "%s", "N/A");
    } else if (g_attr[dev_id].chip_id == ATTR_FAULT_VALUE) {
        snprintf(s, 6, "%s", " F ");
    } else {
        if (g_attr[dev_id].chip_id == 0x1686)
            snprintf(s, 6, "%s","1684X");
        else if (g_attr[dev_id].chip_id == 0x1686a200)
            snprintf(s, 6, "%s","1688");
        else
            snprintf(s, 6, "%x", g_attr[dev_id].chip_id);
    }
}

/* convert status to string*/
static void bm_smi_status_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].status == 0) {
        snprintf(s, 7, "%s", "Active");
    } else {
        snprintf(s, 7, "%s", "Fault ");
    }
}

/* convert board temperature to string*/
static void bm_smi_boardt_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].board_temp == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 5, "%s", "N/A");
    } else if (g_attr[dev_id].board_temp == ATTR_FAULT_VALUE) {
        snprintf(s, 5, "%s", " F ");
    } else {
        snprintf(s, 5, "%dC", g_attr[dev_id].board_temp);
    }
}

/* convert chip temperature to string*/
static void bm_smi_chipt_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].chip_temp == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 5, "%s", "N/A");
    } else if (g_attr[dev_id].chip_temp == ATTR_FAULT_VALUE) {
        snprintf(s, 5, "%s", " F ");
    } else {
        snprintf(s, 5, "%dC", g_attr[dev_id].chip_temp);
    }
}

/* convert board power to string*/
static void bm_smi_boardp_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].board_power == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 5, "%s", "N/A");
    } else if (g_attr[dev_id].board_power == ATTR_FAULT_VALUE) {
        snprintf(s, 5, "%s", " F ");
    } else {
        snprintf(s, 5, "%dW", g_attr[dev_id].board_power);
    }
}

/* convert tpu power to string*/
static void bm_smi_tpup_to_str(int dev_id, char *s) {
    if ((int)g_attr[dev_id].tpu_power == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 5, "%s", "N/A");
    } else if ((int)g_attr[dev_id].tpu_power == ATTR_FAULT_VALUE) {
        snprintf(s, 5, "%s", " F ");
    } else if ((int)g_attr[dev_id].tpu_power < 0) {
        snprintf(s, 5, "%s", " F ");
    } else {
        snprintf(s,
                 6,
                 "%4.1fW",
                 (float)(g_attr[dev_id].vdd_tpu_volt) *
                     g_attr[dev_id].vdd_tpu_curr / (1000 * 1000));
    }
}

/* convert tpu voltage to string*/
static void bm_smi_tpuv_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].vdd_tpu_volt == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 7, "%s", "N/A");
    } else if (g_attr[dev_id].vdd_tpu_volt == ATTR_FAULT_VALUE) {
        snprintf(s, 7, "%s", " F ");
    } else if (g_attr[dev_id].vdd_tpu_volt < 0) {
        snprintf(s, 5, "%s", " F ");
    } else {
        snprintf(s, 7, "%dmV", g_attr[dev_id].vdd_tpu_volt);
    }
}

/* convert 12vatx current to string*/
static void bm_smi_12v_curr_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].atx12v_curr == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 7, "%s", "N/A");
    } else if (g_attr[dev_id].atx12v_curr == ATTR_FAULT_VALUE) {
        snprintf(s, 7, "%s", " F ");
    } else {
        snprintf(s, 7, "%dmA", g_attr[dev_id].atx12v_curr);
    }
}

/* convert ecc on/off to string*/
static void bm_smi_ecc_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].ecc_enable == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 4, "%s", "N/A");
    } else if (g_attr[dev_id].ecc_enable == ATTR_FAULT_VALUE) {
        snprintf(s, 4, "%s", " F ");
    } else if (g_attr[dev_id].ecc_enable == 1) {
        snprintf(s, 4, "%s", "ON");
    } else {
        snprintf(s, 4, "%s", "OFF");
    }
}

/* convert ecc correct number to string*/
static void bm_smi_cnum_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].ecc_correct_num == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 5, "%s", "N/A");
    } else if (g_attr[dev_id].ecc_correct_num == ATTR_FAULT_VALUE) {
        snprintf(s, 5, "%s", " F ");
    } else {
        snprintf(s, 5, "%d", g_attr[dev_id].ecc_correct_num);
    }
}

/* convert serial number to string*/
static void bm_smi_sn_to_str(int dev_id, char *s) {
    g_attr[dev_id].sn[17] = '\0';
     if(!strncmp(g_attr[dev_id].sn,"N/A",3)){
        strncpy(g_attr[dev_id].sn,"    N/A         ",17);
    }
    snprintf(s, 18, "%s", g_attr[dev_id].sn);
}

/* convert board type to string*/
static void bm_smi_board_type_to_str(int dev_id, char *s) {
    snprintf(s, 6, "%s", g_attr[dev_id].board_type);
}

/*
  2             16    22     29     36      43   50              65 81       87
| Bus-ID        Mode| Minclk Maxclk Curclk  MaxP TPU_C         | Memory-Usage
Tpu-Util Fan   |
*/

/* convert busid to string*/
static void bm_smi_busid_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].domain_bdf == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 12, "%s", "N/A       ");
    } else if (g_attr[dev_id].domain_bdf == ATTR_FAULT_VALUE) {
        snprintf(s, 12, "%s", " F ");
    } else {
        snprintf(s,
                 12,
                 "%03x:%02x:%02x.%1x",
                 g_attr[dev_id].domain_bdf >> 16,
                 (g_attr[dev_id].domain_bdf & 0xffff) >> 8,
                 (g_attr[dev_id].domain_bdf & 0xff) >> 3,
                 g_attr[dev_id].domain_bdf & 0x7);
    }
}

/* convert mode to string*/
static void bm_smi_mode_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].chip_mode == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 5, "%s", "N/A");
    } else if (g_attr[dev_id].chip_mode == ATTR_FAULT_VALUE) {
        snprintf(s, 5, "%s", " F ");
    } else if (g_attr[dev_id].chip_mode == 0) {
        snprintf(s, 5, "%s", "PCIE");
    } else {
        snprintf(s, 5, "%s", "SOC");
    }
}

/* convert min clock to string*/
static void bm_smi_minclk_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].tpu_min_clock == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 5, "%s", "N/A");
    } else if (g_attr[dev_id].tpu_min_clock == ATTR_FAULT_VALUE) {
        snprintf(s, 5, "%s", " F ");
    } else {
        snprintf(s, 5, "%dM", g_attr[dev_id].tpu_min_clock);
    }
}

/* convert max clock to string*/
static void bm_smi_maxclk_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].tpu_max_clock == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 6, "%s", "N/A");
    } else if (g_attr[dev_id].tpu_max_clock == ATTR_FAULT_VALUE) {
        snprintf(s, 6, "%s", " F ");
    } else {
        snprintf(s, 6, "%dM", g_attr[dev_id].tpu_max_clock);
    }
}

/* convert current clock to string*/
static void bm_smi_currclk_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].tpu_current_clock == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 5, "%s", "N/A");
    } else if (g_attr[dev_id].tpu_current_clock == ATTR_FAULT_VALUE) {
        snprintf(s, 5, "%s", " F ");
    } else {
        snprintf(s, 6, "%dM", g_attr[dev_id].tpu_current_clock);
    }
}

/* convert max board power to string*/
static void bm_smi_maxpower_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].board_max_power == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 5, "%s", "N/A");
    } else if (g_attr[dev_id].board_max_power == ATTR_FAULT_VALUE) {
        snprintf(s, 5, "%s", " F ");
    } else {
        snprintf(s, 5, "%dW", g_attr[dev_id].board_max_power);
    }
}

/* convert tpu current to string*/
static void bm_smi_tpuc_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].vdd_tpu_curr == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 6, "%s", "N/A");
    } else if (g_attr[dev_id].vdd_tpu_curr == ATTR_FAULT_VALUE) {
        snprintf(s, 6, "%s", " F ");
    } else {
        snprintf(s, 6, "%4.1fA", (float)g_attr[dev_id].vdd_tpu_curr / 1000);
    }
}

/* convert fan speed to string*/
static void bm_smi_fan_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].fan_speed == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 4, "%s", "N/A");
    } else if ((g_attr[dev_id].fan_speed == ATTR_FAULT_VALUE) ||
               (g_attr[dev_id].fan_speed == 0)) {
        snprintf(s, 4, "%s", " F ");
    } else {
        snprintf(s, 4, "%d", g_attr[dev_id].fan_speed);
    }
}

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

/* convert card index to string*/
static void bm_smi_card_index_to_str(int dev_id, char *s) {
    if (g_attr[dev_id].card_index == ATTR_NOTSUPPORTED_VALUE) {
        snprintf(s, 6, "%s", " N/A ");
    } else if (g_attr[dev_id].card_index == ATTR_FAULT_VALUE) {
        snprintf(s, 6, "%s", " F ");
    } else {
        snprintf(s, 6, "%x", g_attr[dev_id].card_index);
    }
}

/* validate attributes fetched to determine whether to display */
static bool bm_smi_validate_attr(int dev_id) {
    bool valid = true;
    if (g_attr[dev_id].tpu_power == 0xffff)
        valid = false;
    if (g_attr[dev_id].vdd_tpu_volt == 0xffff)
        valid = false;
    if (g_attr[dev_id].vdd_tpu_curr >= 0xffff)
        valid = false;
    return valid;
}

/* display attribute of the device in specified slot */
static void bm_smi_display_attr(int            dev_id,
                                int            dis_slot,
                                std::ofstream &file,
                                bool           save_file) {
    int  attr_y;
    int  color_y;
    int  str_length;
    char color_str[BUFFER_LEN]{};
    char after_color_str[BUFFER_LEN]{};
    char line_str[BUFFER_LEN]{};
    char whole_str[BUFFER_LEN]{};
    char card_index_s[6];
    char status_s[7];
    char boardt_s[5];
    char chipt_s[5];
    char boardp_s[5];
    char tpup_s[6];
    char tpuv_s[7];
    char c12v_s[7];
    char ecc_s[4];
    char cnum_s[5];
    char sn_s[18];
    char busid_s[12];
    char mode_s[5];
    char minclk_s[5];
    char maxclk_s[6];
    char currclk_s[6];
    char maxp_s[5];
    char tpuc_s[6];
    char fan_s[4];
    char tpu_util_s[6];
    char board_type_s[7];

    bm_smi_card_index_to_str(dev_id, card_index_s);
    bm_smi_status_to_str(dev_id, status_s);
    bm_smi_boardt_to_str(dev_id, boardt_s);
    bm_smi_chipt_to_str(dev_id, chipt_s);
    bm_smi_boardp_to_str(dev_id, boardp_s);
    bm_smi_tpup_to_str(dev_id, tpup_s);
    bm_smi_tpuv_to_str(dev_id, tpuv_s);
    bm_smi_12v_curr_to_str(dev_id, c12v_s);
    bm_smi_ecc_to_str(dev_id, ecc_s);
    bm_smi_cnum_to_str(dev_id, cnum_s);
    bm_smi_sn_to_str(dev_id, sn_s);

    bm_smi_busid_to_str(dev_id, busid_s);
    bm_smi_mode_to_str(dev_id, mode_s);
    bm_smi_minclk_to_str(dev_id, minclk_s);
    bm_smi_maxclk_to_str(dev_id, maxclk_s);
    bm_smi_currclk_to_str(dev_id, currclk_s);
    bm_smi_maxpower_to_str(dev_id, maxp_s);
    bm_smi_tpuc_to_str(dev_id, tpuc_s);
    bm_smi_fan_to_str(dev_id, fan_s);
    bm_smi_tpu_util_to_str(dev_id, tpu_util_s);
    bm_smi_board_type_to_str(dev_id, board_type_s);

    attr_y = dis_slot * BM_SMI_DEVATTR_HEIGHT;
    attr_y -= win_y_offset;

    for (int i = 0; i < BM_SMI_DEVATTR_HEIGHT; i++, attr_y++) {
        switch (i) {
            case 0: {
                if (g_attr[dev_id].board_attr) {
                    snprintf(line_str,
                             BUFFER_LEN,
                             "|%2s %10s %5s %17s |%2d   %4s    %4s",
                             card_index_s,
                             board_name,
                             mode_s,
                             sn_s,
                             dev_id,
                             boardt_s,
                             chipt_s);
                    str_length = snprintf(color_str, BUFFER_LEN, " ");
                    snprintf(after_color_str,
                             BUFFER_LEN,
                             "%5s   %5s  %3s    %3s       %5s |\n",
                             tpup_s,
                             tpuv_s,
                             ecc_s,
                             cnum_s,
                             tpu_util_s);
                    snprintf(whole_str,
                             BUFFER_LEN,
                             "|%2s %10s %5s %17s |%2d   %4s    %4s   %5s  "
                             " %5s  %3s    %3s       %5s |\n",
                             card_index_s,
                             board_name,
                             mode_s,
                             sn_s,
                             dev_id,
                             boardt_s,
                             chipt_s,
                             tpup_s,
                             tpuv_s,
                             ecc_s,
                             cnum_s,
                             tpu_util_s);
                } else {
                    snprintf(line_str,
                             BUFFER_LEN,
                             "|                                       |%2d   "
                             "%4s    %4s",
                             dev_id,
                             boardt_s,
                             chipt_s);
                    str_length = snprintf(color_str, BUFFER_LEN, " ");
                    snprintf(after_color_str,
                             BUFFER_LEN,
                             "%5s   %5s  %3s    %3s       %5s |\n",
                             tpup_s,
                             tpuv_s,
                             ecc_s,
                             cnum_s,
                             tpu_util_s);
                    snprintf(whole_str,
                             BUFFER_LEN,
                             "|                                       |%2d   "
                             "%4s    %4s   %5s   %5s  %3s    %3s       %5s |\n",
                             dev_id,
                             boardt_s,
                             chipt_s,
                             tpup_s,
                             tpuv_s,
                             ecc_s,
                             cnum_s,
                             tpu_util_s);
                }
            } break;
            case 1:
                if (g_attr[dev_id].board_attr) {
            snprintf(line_str,
                             BUFFER_LEN,
                             "|%6s %5s  %4s  %4s  %7s %5s|%11s%7s ",
                             c12v_s,
                             maxp_s,
                             boardp_s,
                             minclk_s,
                             maxclk_s,
                             fan_s,
                             busid_s,
                             status_s);
                    str_length =
                        snprintf(color_str, BUFFER_LEN, "%7s", currclk_s);
                    snprintf(after_color_str,
                             BUFFER_LEN,
                             "   %7s %5dMB/%5dMB      |\n",
                             tpuc_s,
                             g_attr[dev_id].mem_used,
                             g_attr[dev_id].mem_total);
                    snprintf(whole_str,
                             BUFFER_LEN,
                             "|%6s %5s  %4s  %4s  %7s %5s|%11s%7s %7s   "
                             "%7s %5dMB/%5dMB      |\n",
                             c12v_s,
                             maxp_s,
                             boardp_s,
                             minclk_s,
                             maxclk_s,
                             fan_s,
                             busid_s,
                             status_s,
                             currclk_s,
                             tpuc_s,
                             g_attr[dev_id].mem_used,
                             g_attr[dev_id].mem_total);
                } else {
                    snprintf(
                        line_str,
                        BUFFER_LEN,
                        "|                                       |%11s%7s ",
                        busid_s,
                        status_s);
                    str_length =
                        snprintf(color_str, BUFFER_LEN, "%7s", currclk_s);
                    snprintf(after_color_str,
                             BUFFER_LEN,
                             "   %7s %5dMB/%5dMB      |\n",
                             tpuc_s,
                             g_attr[dev_id].mem_used,
                             g_attr[dev_id].mem_total);
                    snprintf(whole_str,
                             BUFFER_LEN,
                             "|                                       |%11s%7s "
                             "%7s   %7s %5dMB/%5dMB      |\n",
                             busid_s,
                             status_s,
                             currclk_s,
                             tpuc_s,
                             g_attr[dev_id].mem_used,
                             g_attr[dev_id].mem_total);
                }
                break;
            case 2: {
                if (g_attr[dev_id].board_endline) {
            snprintf(line_str,
                             BUFFER_LEN,
                             "+=======================================+========"
                             "===========");
                    str_length = snprintf(color_str, BUFFER_LEN, "=");
                    snprintf(after_color_str,
                             BUFFER_LEN,
                             "======================================+\n");
                    snprintf(
                        whole_str,
                        BUFFER_LEN,
                        "+=======================================+============="
                        "=============================================+\n");
                } else {
                    snprintf(line_str,
                             BUFFER_LEN,
                             "|                                       "
                             "|---------------------------------");
                    str_length = snprintf(color_str, BUFFER_LEN, "-");
                    snprintf(after_color_str,
                             BUFFER_LEN,
                             "--------------------------------------|\n");
                    snprintf(whole_str,
                             BUFFER_LEN,
                             "|                                       "
                             "|------------------------------------------------"
                             "----------|\n");
                }
            } break;
            default:
                break;
        }
        if (file.is_open() && save_file) {
            file << whole_str;
        }
        if (attr_y >= 0) {
            if (move(attr_y + BM_SMI_FORMAT_HEIGHT, 0) == OK) {
                clrtoeol();
                attron(COLOR_PAIR(0));
                printw("%s", line_str);
                attroff(COLOR_PAIR(0));
                color_y = getcury(stdscr);
                if (g_attr[dev_id].tpu_current_clock == 440) {
                    if (str_length > 2) {
                        attron(COLOR_PAIR(1));
                        mvprintw(color_y, 60, "%s", color_str);
                        attroff(COLOR_PAIR(1));
                    } else {
                        attron(COLOR_PAIR(0));
                        mvprintw(color_y, 60, "%s", color_str);
                        attroff(COLOR_PAIR(0));
                    }
                } else if (g_attr[dev_id].tpu_current_clock == 75) {
                    if (str_length > 2) {
                        attron(COLOR_PAIR(2));
                        mvprintw(color_y, 60, "%s", color_str);
                        attroff(COLOR_PAIR(2));
                    } else {
                        attron(COLOR_PAIR(0));
                        mvprintw(color_y, 60, "%s", color_str);
                        attroff(COLOR_PAIR(0));
                    }
                } else {
                    attron(COLOR_PAIR(0));
                    mvprintw(color_y, 60, "%s", color_str);
                    attroff(COLOR_PAIR(0));
                }
                attron(COLOR_PAIR(0));
                printw("%s", after_color_str);
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
/* display proc gmem header */
static void bm_smi_display_proc_gmem_header(int            dev_cnt,
                                            std::ofstream &file,
                                            bool           save_file) {
    char line_str[BUFFER_LEN]{};
#ifdef __linux__
    if (LINES >
        std::max((dev_cnt * BM_SMI_DEVATTR_HEIGHT + BM_SMI_FORMAT_HEIGHT +
                  BM_SMI_PROCHEADER_HEIGHT - win_y_offset),
                 BM_SMI_FORMAT_HEIGHT + BM_SMI_PROCHEADER_HEIGHT)) {
#else
    if (LINES > max((dev_cnt * BM_SMI_DEVATTR_HEIGHT + BM_SMI_FORMAT_HEIGHT +
                     BM_SMI_PROCHEADER_HEIGHT - win_y_offset),
                    BM_SMI_FORMAT_HEIGHT + BM_SMI_PROCHEADER_HEIGHT)) {
#endif
        proc_show = true;
        proc_y    = dev_cnt * BM_SMI_DEVATTR_HEIGHT + BM_SMI_FORMAT_HEIGHT +
                 BM_SMI_PROCHEADER_HEIGHT;
    } else {
        proc_show = false;
    }

    for (int i = 0; i < BM_SMI_PROCHEADER_HEIGHT; i++) {
        switch (i) {
            case 0:
                snprintf(line_str, BUFFER_LEN, "\n");
                break;
            case 1:
                snprintf(line_str,
                         BUFFER_LEN,
                         "+----------------------------------------------------"
                         "----------------------------------------------+\n");
                break;
            case 2:
                snprintf(line_str,
                         BUFFER_LEN,
                         "| Processes:                                         "
                         "                                   TPU Memory |\n");
                break;
            case 3:
                snprintf(line_str,
                         BUFFER_LEN,
                         "|  TPU-ID       PID   Process name                   "
                         "                                   Usage      |\n");
                break;
            case 4:
                snprintf(line_str,
                         BUFFER_LEN,
                         "|===================================================="
                         "==============================================|\n");
                break;
            default:
                break;
        }
        if (file.is_open() && save_file) {
            file << line_str;
        }
        if (proc_show)
            printw("%s", line_str);
    }
    clrtobot();
}
#ifdef __linux__
/* display proc gmem info for the device */
static void bm_smi_display_proc_gmem(int            dev_id,
                                     std::ofstream &file,
                                     bool           save_file) {
    std::string proc_name;
    int         proc_display_y;
    char        line_str[BUFFER_LEN]{};

    if (proc_gmem[dev_id].proc_cnt == ATTR_FAULT_VALUE)
        return;
    for (int i = 0; i < proc_gmem[dev_id].proc_cnt; i++) {
        proc_name = get_process_name_by_pid(proc_gmem[dev_id].pid[i]);
        if (proc_name.empty())
            continue;
        snprintf(line_str,
                 BUFFER_LEN,
                 "%9d%10d  %-45s        %16luMB\n",
                 dev_id,
                 proc_gmem[dev_id].pid[i],
                 proc_name.c_str(),
                 proc_gmem[dev_id].gmem_used[i]);
        if (file.is_open() && save_file) {
            file << line_str;
        }
        proc_display_y = proc_y - win_y_offset;
        if (proc_show &&
            (proc_display_y >=
             (BM_SMI_FORMAT_HEIGHT + BM_SMI_PROCHEADER_HEIGHT)) &&
            (move(proc_display_y, 0) == OK)) {
            printw("%s", line_str);
        }
        text_lines++;
        proc_y++;
    }
}
#else
static void bm_smi_display_proc_gmem(int            dev_id,
                                     std::ofstream &file,
                                     bool           save_file) {
    std::string proc_name;
    int         proc_display_y;
    char        line_str[BUFFER_LEN]{};

    if (proc_gmem[dev_id].proc_cnt == ATTR_FAULT_VALUE)
        return;
    for (int i = 0; i < proc_gmem[dev_id].proc_cnt; i++) {
        proc_name = get_process_name_by_pid(proc_gmem[dev_id].pid[i]);
        if (proc_name.empty())
            continue;
        snprintf(line_str,
                 BUFFER_LEN,
                 "%9d%10d  %-45s        %16luMB\n",
                 dev_id,
                 proc_gmem[dev_id].pid[i],
                 proc_name.c_str(),
                 proc_gmem[dev_id].gmem_used[i]);
        if (file.is_open() && save_file) {
            file << line_str;
        }
        proc_display_y = proc_y - win_y_offset;
        if (proc_show &&
            (proc_display_y >=
             (BM_SMI_FORMAT_HEIGHT + BM_SMI_PROCHEADER_HEIGHT)) &&
            (move(proc_display_y, 0) == OK)) {
            printw("%s", line_str);
        }
        text_lines++;
        proc_y++;
    }
}
#endif
/* ncurses init screen before display */
static void bm_smi_init_scr() {
    initscr();
    resize_term(24, 100);
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
static void bm_smi_fetch_all(bm_handle_t handle,
                             int         fd,
                             int         dev_cnt,
                             int         start_dev)
{
    for (int i = start_dev; i < start_dev + dev_cnt; i++) {
        bm_smi_get_attr(handle, fd, i);
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
    text_lines = 0;
    bm_smi_display_format(target_file, save_file);
    text_lines += BM_SMI_FORMAT_HEIGHT;
    for (int i = start_dev; i < start_dev + dev_cnt; i++) {
        if (bm_smi_validate_attr(i)) {
            bm_smi_display_attr(i, i - start_dev, target_file, save_file);
        }
    }
    text_lines += BM_SMI_DEVATTR_HEIGHT * dev_cnt;

    bm_smi_display_proc_gmem_header(dev_cnt, target_file, save_file);

    text_lines += BM_SMI_PROCHEADER_HEIGHT;

    for (int i = start_dev; i < start_dev + dev_cnt; i++) {
        bm_smi_display_proc_gmem(i, target_file, save_file);
    }

    if (target_file.is_open() && save_file) {
        target_file << "\n"
                    << "\n"
                    << "\n";
        target_file.flush();
    }
    refresh();
}



bm_smi_display::bm_smi_display(bm_smi_cmdline &cmdline) : bm_smi_test(cmdline) {
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

bm_smi_display::~bm_smi_display() {}

int bm_smi_display::validate_input_para() {


    /* check lms value */
    if (g_cmdline.m_lms < 300) {
        printf("invalid lsm = %d, it is less than 300\n", g_cmdline.m_lms);
        return -EINVAL;
    }

    if (!g_cmdline.m_file.empty()) {
        if (g_cmdline.m_file.at(0) == '~') {
            file_path = file_path + g_cmdline.m_file.erase(0, 1);
            printf("saved file path %s \n", file_path.c_str());
        } else {
            file_path = g_cmdline.m_file;
        }

        target_file.open(file_path.c_str(),
                         std::ofstream::out | std::ofstream::app);
        if (!target_file.is_open()) {
            printf("open %s failed\n", file_path.c_str());
            return -ENOENT;
        }
    }

    return 0;
}

int bm_smi_display::run_opmode() {
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

    // init screen here; or need handle exception
    bm_smi_init_scr();


    bm_handle_t handle;
    bm_smi_dev_request(&handle);
    bm_smi_get_board_name();


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
                    bm_smi_fetch_all(handle, fd, dev_cnt, start_dev);
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
    bm_smi_dev_free(handle);
#endif
    endwin();

    return 0;
}

int bm_smi_display::check_result() {
    return 0;
}

void bm_smi_display::case_help() {
    printf("case_help\n");
}
