#include "bm_common.h"
#include "bm_debug.tmh"
//static struct proc_dir_entry *bmsophon_total_node;
//static struct proc_dir_entry *bmsophon_proc_dir;
//static char debug_node_name[] = "bmsophon";
//extern struct bm_ctrl_info *bmci;
//extern int dev_count;
//
//static int bmdrv_all_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = NULL;
//	int i = 0;
//	char dev_name[] = BM_CDEV_NAME;
//#ifndef SOC_MODE
//	int mcu_volt[9] = {0};
//	int mcu_curr[9] = {0};
//	int j;
//	struct bm_chip_attr *c_attr = NULL;
//#endif
//
//	for (i = 0; i < dev_count; i++) {
//		bmdi = bmctl_get_bmdi(bmci, i);
//		seq_printf(m, "===========dev_name: %s-%d============\n", dev_name, i);
//		seq_printf(m, "chip_id: 0x%x, version: 0x%x\n", bmdi->cinfo.chip_id, bmdi->misc_info.driver_version);
//#ifndef SOC_MODE
//		c_attr = &bmdi->c_attr;
//		if (bmdi->boot_info.deadbeef == 0xdeadbeef) {
//			seq_printf(m, "ddr0a_size: 0x%llx, ddr0b_size: 0x%llx, ddr1_size: 0x%llx, ddr2_size: 0x%llx\n",
//				bmdi->boot_info.ddr_0a_size, bmdi->boot_info.ddr_0b_size,
//				bmdi->boot_info.ddr_1_size, bmdi->boot_info.ddr_2_size);
//		} else {
//			seq_printf(m, "ddr: boot info not update!!\n");
//		}
//
//		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_EVB ||
//				BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5 ||
//				BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS) {
//			seq_printf(m, "-----------MCU Voltage Info------------\n");
//
//			//mutex_lock(&c_attr->attr_mutex);
//			for (j = 0; j < 9; j++) {
//				if (bm_read_mcu_voltage(bmdi, 0x28 + 2 * j, &mcu_volt[j]) != 0)
//					break;
//			}
//			//mutex_unlock(&c_attr->attr_mutex);
//			if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_EVB ||
//				BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5) {
//				seq_printf(m, "12Vatx : %dmV\tvddio5 : %dmV\t\tvddio18 : %dmV\n",
//					mcu_volt[0], mcu_volt[1], mcu_volt[2]);
//				seq_printf(m, "vddio33 : %dmV\tvdd_phy: %dmV\t\tvdd_pcie : %dmV\n",
//					mcu_volt[3], mcu_volt[4], mcu_volt[5]);
//				seq_printf(m, "vdd_tpu_mem : %dmV\tddr_vddq : %dmV\tddr_vddqlp : %dmV\n",
//					mcu_volt[6], mcu_volt[7], mcu_volt[8]);
//			} else {
//				if (bmdi->boot_info.board_power_sensor_exist == 0x1)
//					seq_printf(m, "12Vatx : %dmV\tvddio33 : %dmV\n",
//						mcu_volt[0], mcu_volt[3]);
//				else
//					seq_printf(m, "12Vatx : N/A mV\tvddio33 : N/A mV\n");
//			}
//
//			seq_printf(m, "-----------MCU Current Info------------\n");
//
//			//mutex_lock(&c_attr->attr_mutex);
//			for (j = 0; j < 9; j++) {
//				if (bm_read_mcu_current(bmdi, 0x28 + 2 * j, &mcu_curr[j]) != 0)
//					break;
//			}
//			//mutex_unlock(&c_attr->attr_mutex);
//
//			if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_EVB ||
//				BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5) {
//				seq_printf(m, "12Vatx : %dmA\t\tvddio5 : %dmA\t\tvddio18 : %dmA\n",
//					mcu_curr[0], mcu_curr[1], mcu_curr[2]);
//				seq_printf(m, "vddio33 : %dmA\t\tvdd_phy: %dmA\t\tvdd_pcie : %dmA\n",
//					mcu_curr[3], mcu_curr[4], mcu_curr[5]);
//				seq_printf(m, "vdd_tpu_mem : %dmA\tddr_vddq : %dmA\tddr_vddqlp : %dmA\n",
//					mcu_curr[6], mcu_curr[7], mcu_curr[8]);
//			} else {
//				if (bmdi->boot_info.board_power_sensor_exist == 0x1)
//				seq_printf(m, "12Vatx : %dmA\t\tvddio33 : %dmA\n",
//					mcu_curr[0], mcu_curr[3]);
//				else
//					seq_printf(m, "12Vatx : N/A mA\tvddio33 : N/A mA\n");
//			}
//		}
//
//		seq_printf(m, "-------------MCU Version Info---------------\n");
//		seq_printf(m, "MCU firmware Version: %d\n", BM1684_MCU_VERSION(bmdi));
//
//#endif
//	}
//	return 0;
//}
//
//static int bmdrv_all_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_all_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations proc_debug_info_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_all_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//int bmdrv_proc_init(void)
//{
//	bmsophon_proc_dir = proc_mkdir(debug_node_name, NULL);
//	if (!bmsophon_proc_dir)
//		return -1;
//
//	bmsophon_total_node = proc_create(debug_node_name, 0444, bmsophon_proc_dir,
//		&proc_debug_info_ops);
//	if (!bmsophon_total_node) {
//		proc_remove(bmsophon_proc_dir);
//		//TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "proc_create fail, node name is %s\n", debug_node_name);
//		return -1;
//	}
//
//	return 0;
//}
//
//void bmdrv_proc_deinit(void)
//{
//	proc_remove(bmsophon_proc_dir);
//	proc_remove(bmsophon_total_node);
//}
//
//#define MAX_NAMELEN 128
//
//static int bmdrv_chipid_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "0x%x\n", bmdi->cinfo.chip_id);
//	return 0;
//}
//
//static int bmdrv_chipid_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_chipid_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_chipid_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_chipid_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_tpuid_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%d\n", bmdi->dev_index);
//	return 0;
//}
//
//static int bmdrv_tpuid_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_tpuid_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_tpuid_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_tpuid_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_mode_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%s\n", bmdi->misc_info.pcie_soc_mode ? "soc" : "pcie");
//	return 0;
//}
//
//static int bmdrv_mode_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_mode_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_mode_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_mode_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//#ifndef SOC_MODE
//static int bmdrv_dbdf_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%03x:%02x:%02x.%1x\n", bmdi->misc_info.domain_bdf>>16,
//		(bmdi->misc_info.domain_bdf&0xffff)>>8,
//		(bmdi->misc_info.domain_bdf&0xff)>>3,
//		(bmdi->misc_info.domain_bdf&0x7));
//	return 0;
//}
//
//static int bmdrv_dbdf_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_dbdf_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_dbdf_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_dbdf_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_status_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	int stat = bmdi->status;
//
//	if (stat == 0){
//		seq_printf(m, "%s\n", "Active");
//	} else {
//		seq_printf(m, "%s\n", "Fault");
//	}
//	return 0;
//}
//
//static int bmdrv_status_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_status_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_status_file_ops = {
//	.owner          = THIS_MODULE,
//        .open           = bmdrv_status_proc_open,
//        .read           = seq_read,
//        .llseek         = seq_lseek,
//        .release        = single_release,
//};
//
//static int bmdrv_tpu_minclk_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%dMHz\n", bmdi->boot_info.tpu_min_clk);
//	return 0;
//}
//
//static int bmdrv_tpu_minclk_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_tpu_minclk_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_tpu_minclk_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_tpu_minclk_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_tpu_maxclk_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%dMHz\n", bmdi->boot_info.tpu_max_clk);
//	return 0;
//}
//
//static int bmdrv_tpu_maxclk_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_tpu_maxclk_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_tpu_maxclk_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_tpu_maxclk_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_tpu_maxboardp_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_chip_attr *c_attr;
//
//	c_attr = &bmdi->c_attr;
//	if (c_attr->bm_get_board_power != NULL)
//		seq_printf(m, "%d W\n", bmdi->boot_info.max_board_power);
//	else
//		seq_printf(m, "N/A\n");
//	return 0;
//}
//
//static int bmdrv_tpu_maxboardp_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_tpu_maxboardp_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_tpu_maxboardp_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_tpu_maxboardp_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_ecc_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%s\n", bmdi->misc_info.ddr_ecc_enable ? "on" : "off");
//	return 0;
//}
//
//static int bmdrv_ecc_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_ecc_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_ecc_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_ecc_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_dynfreq_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%d\n", bmdi->enable_dyn_freq);
//	return 0;
//}
//
//static int bmdrv_dynfreq_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_dynfreq_proc_show, PDE_DATA(inode));
//}
//
//static ssize_t bmdrv_dynfreq_proc_write(struct file *file,
//	const char  *ubuf, size_t count, loff_t *ppos)
//{
//	char buf[2];
//	struct bm_device_info *bmdi = NULL;
//	struct seq_file *s = NULL;
//
//	s = file->private_data;
//	bmdi = s->private;
//
//	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
//		return -1;
//
//	if (!strncmp(buf, "0", 1))
//		bmdi->enable_dyn_freq = 0;
//	else if (!strncmp(buf, "1", 1))
//		bmdi->enable_dyn_freq = 1;
//	else
//        TraceEvents(
//            TRACE_LEVEL_ERROR, TRACE_DEVICE, "invalid val for dyn freq\n");
//	return count;
//}
//
//static const struct file_operations bmdrv_dynfreq_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_dynfreq_proc_open,
//	.read		= seq_read,
//	.write		= bmdrv_dynfreq_proc_write,
//};
//
//static int bmdrv_dumpreg_proc_show(struct seq_file *m, void *v)
//{
//    struct bm_device_info *bmdi = m->private;
//    int i=0;
//
//    if(bmdi->dump_reg_type == 0)
//        seq_printf(m, " echo parameter is 0 ,echo 0 don't dump register\n help tips: echo 1 dump tpu register; echo 2 dump gdma register; echo others not supported\n");
//    else if(bmdi->dump_reg_type == 1){
//        seq_printf(m, "DEV %d tpu command reg:\n", bmdi-> dev_index);
//        for(i=0; i<32; i++)
//          seq_printf(m, "BDC_CMD_REG %d: addr= 0x%08x, value = 0x%08x\n",i, bmdi->cinfo.bm_reg->tpu_base_addr + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr + i*4));
//
//        seq_printf(m, "DEV %d tpu control reg:\n", bmdi-> dev_index);
//        for(i=0; i<64; i++)
//          seq_printf(m, "BDC_CTL_REG %d: addr= 0x%08x, value = 0x%08x\n",i, bmdi->cinfo.bm_reg->tpu_base_addr + 0x100 + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr + 0x100 + i*4));
//    }
//    else if(bmdi->dump_reg_type == 2){
//        seq_printf(m, "DEV %d gdma all reg:\n", bmdi-> dev_index);
//        for(i=0; i<32; i++)
//          seq_printf(m, "GDMA_ALL_REG %d: addr= 0x%08x, value = 0x%08x\n",i, bmdi->cinfo.bm_reg->gdma_base_addr + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->gdma_base_addr + i*4));
//        }
//    else{
//        seq_printf(m, "invalid echo val for dumpreg,shoud be 0,1,2\n");
//        }
//
//    return 0;
//}
//
//static int bmdrv_dumpreg_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_dumpreg_proc_show, PDE_DATA(inode));
//}
//
//static ssize_t bmdrv_dumpreg_proc_write(struct file *file,
//	const char  *ubuf, size_t count, loff_t *ppos)
//{
//	char buf[2];
//	struct bm_device_info *bmdi = NULL;
//	struct seq_file *s = NULL;
//
//	s = file->private_data;
//	bmdi = s->private;
//
//	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
//		return -1;
//
//	if (!strncmp(buf, "0", 1))
//		bmdi->dump_reg_type = 0;
//	else if (!strncmp(buf, "1", 1))
//		bmdi->dump_reg_type = 1;
//	else if (!strncmp(buf, "2", 1))
//		bmdi->dump_reg_type = 2;
//	else
//		bmdi->dump_reg_type = 0xff;
//	return count;
//}
//
//
//static const struct file_operations bmdrv_dumpreg_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_dumpreg_proc_open,
//	.read		= seq_read,
//	.write		= bmdrv_dumpreg_proc_write,
//};
//
//static int bmdrv_fan_speed_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_chip_attr *c_attr;
//	u32 fan;
//
//	if (bmdi->boot_info.fan_exist) {
//		c_attr = &bmdi->c_attr;
//		//mutex_lock(&c_attr->attr_mutex);
//		fan = bm_get_fan_speed(bmdi);
//		//mutex_unlock(&c_attr->attr_mutex);
//
//		seq_printf(m, "duty  %d\n", fan);
//		seq_printf(m, "fan_speed  %d\n", c_attr->fan_rev_read);
//	} else {
//		seq_printf(m, "N/A\n");
//	}
//
//	return 0;
//}
//
//static int bmdrv_fan_speed_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_fan_speed_proc_show, PDE_DATA(inode));
//}
//
//static ssize_t bmdrv_fan_speed_proc_write(struct file *file,
//		const char  *ubuf, size_t count, loff_t *ppos)
//{
//	char buf[3];
//	struct bm_device_info *bmdi = NULL;
//	struct seq_file *s = NULL;
//	int res;
//
//	s = file->private_data;
//	bmdi = s->private;
//	if (bmdi->boot_info.fan_exist) {
//		if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf), count)))
//			return -1;
//
//		kstrtoint(buf, 10, &res); /* String to integer data */
//
//		if ((res < 0) || (res > 100)) {
//            TraceEvents(TRACE_LEVEL_ERROR,
//                        TRACE_DEVICE,
//                        "Error, valid value range is 0 ~ 100\n");
//			return -1;
//		} else {
//			bmdi->fixed_fan_speed = res;
//			return count;
//		}
//	} else {
//        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "not supprot\n");
//		return -1;
//	}
//}
//
//static const struct file_operations bmdrv_fan_speed_file_ops = {
//	.owner = THIS_MODULE,
//	.open = bmdrv_fan_speed_proc_open,
//	.read = seq_read,
//	.write = bmdrv_fan_speed_proc_write,
//};
//
//static int bmdrv_pcie_link_speed_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct pci_dev *pdev = bmdi->cinfo.pcidev;
//	u32 link_status = 0;
//
//	pci_read_config_dword(pdev, 0x80, &link_status);
//	link_status = (link_status >> 16) & 0xf;
//	seq_printf(m, "gen%d\n", link_status);
//	return 0;
//}
//
//static int bmdrv_pcie_link_speed_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_pcie_link_speed_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_pcie_link_speed_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_pcie_link_speed_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_pcie_link_width_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct pci_dev *pdev = bmdi->cinfo.pcidev;
//	u32 link_status = 0;
//
//	pci_read_config_dword(pdev, 0x80, &link_status);
//	link_status = (link_status >> 20) & 0x3f;
//	seq_printf(m, "x%d\n", link_status);
//	return 0;
//}
//
//static int bmdrv_pcie_link_width_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_pcie_link_width_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_pcie_link_width_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_pcie_link_width_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_pcie_cap_speed_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct pci_dev *pdev = bmdi->cinfo.pcidev;
//	u32 link_status = 0;
//
//	pci_read_config_dword(pdev, 0x7c, &link_status);
//	link_status = (link_status >> 0) & 0xf;
//	seq_printf(m, "gen%d\n", link_status);
//	return 0;
//}
//
//static int bmdrv_pcie_cap_speed_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_pcie_cap_speed_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_pcie_cap_speed_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_pcie_cap_speed_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_pcie_cap_width_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct pci_dev *pdev = bmdi->cinfo.pcidev;
//	u32 link_status = 0;
//
//	pci_read_config_dword(pdev, 0x7c, &link_status);
//	link_status = (link_status >> 4) & 0x3f;
//	seq_printf(m, "x%d\n", link_status);
//	return 0;
//}
//
//static int bmdrv_pcie_cap_width_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_pcie_cap_width_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_pcie_cap_width_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_pcie_cap_width_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_pcie_region_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_bar_info bar_info = bmdi->cinfo.bar_info;
//	int size = 1024 * 1024;
//
//	seq_printf(m, "%lldM, %lldM, %lldM, %lldM\n",
//		bar_info.bar0_len / size, bar_info.bar1_len / size,
//		bar_info.bar2_len / size, bar_info.bar4_len / size);
//	return 0;
//}
//
//static int bmdrv_pcie_region_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_pcie_region_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_pcie_region_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_pcie_region_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_tpu_volt_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_chip_attr *c_attr;
//	int vdd_tpu_volt = 0x0;
//
//	c_attr = &bmdi->c_attr;
//	if (c_attr->bm_get_tpu_power != NULL) {
//		//mutex_lock(&c_attr->attr_mutex);
//		bm_read_vdd_tpu_voltage(bmdi, &vdd_tpu_volt);
//		//mutex_unlock(&c_attr->attr_mutex);
//		seq_printf(m, "%d mV\n", vdd_tpu_volt);
//	} else {
//		seq_printf(m, "N/A\n");
//	}
//
//	return 0;
//}
//
//static int bmdrv_tpu_volt_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_tpu_volt_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_tpu_volt_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_tpu_volt_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_tpu_cur_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_chip_attr *c_attr;
//	int vdd_tpu_cur = 0x0;
//
//	c_attr = &bmdi->c_attr;
//	if (c_attr->bm_get_tpu_power != NULL) {
//		//mutex_lock(&c_attr->attr_mutex);
//		bm_read_vdd_tpu_current(bmdi, &vdd_tpu_cur);
//		//mutex_unlock(&c_attr->attr_mutex);
//		seq_printf(m, "%d mA\n", vdd_tpu_cur);
//	} else {
//		seq_printf(m, "N/A\n");
//	}
//
//	return 0;
//}
//
//static int bmdrv_tpu_cur_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_tpu_cur_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_tpu_cur_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_tpu_cur_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_tpu_power_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_chip_attr *c_attr;
//	int tpu_power = 0;
//
//	c_attr = &bmdi->c_attr;
//	if (c_attr->bm_get_tpu_power != NULL) {
//		//mutex_lock(&c_attr->attr_mutex);
//		c_attr->bm_get_tpu_power(bmdi, &tpu_power);
//		//mutex_unlock(&c_attr->attr_mutex);
//		seq_printf(m, "%d W\n", tpu_power);
//	} else {
//		seq_printf(m, "N/A\n");
//	}
//	return 0;
//}
//
//static int bmdrv_tpu_power_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_tpu_power_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_tpu_power_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_tpu_power_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_board_power_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_chip_attr *c_attr;
//	int board_power = 0;
//
//	c_attr = &bmdi->c_attr;
//	if (c_attr->bm_get_board_power != NULL) {
//		//mutex_lock(&c_attr->attr_mutex);
//		c_attr->bm_get_board_power(bmdi, &board_power);
//		//mutex_unlock(&c_attr->attr_mutex);
//		seq_printf(m, "%d W\n", board_power);
//	} else {
//		seq_printf(m, "N/A\n");
//	}
//	return 0;
//}
//
//static int bmdrv_board_power_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_board_power_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_board_power_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_board_power_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_chip_temp_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_chip_attr *c_attr;
//	int chip_temp = 0;
//
//	c_attr = &bmdi->c_attr;
//	if (c_attr->bm_get_chip_temp != NULL) {
//		//mutex_lock(&c_attr->attr_mutex);
//		c_attr->bm_get_chip_temp(bmdi, &chip_temp);
//		//mutex_unlock(&c_attr->attr_mutex);
//		seq_printf(m, "%d C\n", chip_temp);
//	} else {
//		seq_printf(m, "N/A\n");
//	}
//	return 0;
//}
//
//static int bmdrv_chip_temp_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_chip_temp_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_chip_temp_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_chip_temp_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_board_temp_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_chip_attr *c_attr;
//	int board_temp = 0;
//
//	c_attr = &bmdi->c_attr;
//	if (c_attr->bm_get_board_temp != NULL) {
//		//mutex_lock(&c_attr->attr_mutex);
//		c_attr->bm_get_board_temp(bmdi, &board_temp);
//		//mutex_unlock(&c_attr->attr_mutex);
//		seq_printf(m, "%d C\n", board_temp);
//	} else {
//		seq_printf(m, "N/A\n");
//	}
//	return 0;
//}
//
//static int bmdrv_board_temp_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_board_temp_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_board_temp_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_board_temp_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_board_sn_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_chip_attr *c_attr;
//	char sn[18] = "";
//
//	c_attr = &bmdi->c_attr;
//	//mutex_lock(&c_attr->attr_mutex);
//	bm_get_sn(bmdi, sn);
//	//mutex_unlock(&c_attr->attr_mutex);
//
//	seq_printf(m, "%s\n", sn);
//	return 0;
//}
//
//static int bmdrv_board_sn_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_board_sn_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_board_sn_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_board_sn_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_mcu_version_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	seq_printf(m, "V%d\n", BM1684_MCU_VERSION(bmdi));
//	return 0;
//}
//
//static int bmdrv_mcu_version_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_mcu_version_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_mcu_version_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_mcu_version_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_board_type_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	char type[10] = "";
//	int board_id = BM1684_BOARD_TYPE(bmdi);
//
//	if (bmdi->cinfo.chip_id != 0x1682) {
//		bm1684_get_board_type_by_id(bmdi, type, board_id);
//		seq_printf(m, "%s\n", type);
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_board_type_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_board_type_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_board_type_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_board_type_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_board_version_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	char version[10] = "";
//	struct bm_device_info *card_bmdi = bmctl_get_card_bmdi(bmdi);
//	int board_version = 0x0;
//	int board_id = 0x0;
//	int pcb_version = 0x0;
//	int bom_version = 0x0;
//
//	if (card_bmdi != NULL)
//		bmdi = card_bmdi;
//
//	board_version = bmdi->misc_info.board_version;
//	board_id = BM1684_BOARD_TYPE(bmdi);
//	pcb_version = (board_version >> 4) & 0xf;
//	bom_version = board_version & 0xf;
//
//	if (bmdi->cinfo.chip_id != 0x1682) {
//		bm1684_get_board_version_by_id(bmdi, version, board_id,
//			pcb_version, bom_version);
//		seq_printf(m, "%s\n", version);
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_board_version_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_board_version_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_board_version_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_board_version_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_bom_version_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_device_info *card_bmdi = bmctl_get_card_bmdi(bmdi);
//	int board_version = 0x0;
//	int bom_version = 0x0;
//
//	if (card_bmdi != NULL)
//		bmdi = card_bmdi;
//
//	board_version = bmdi->misc_info.board_version;
//	bom_version = board_version & 0xf;
//
//	if (bmdi->cinfo.chip_id != 0x1682) {
//		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H ||
//			BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5_P) {
//			seq_printf(m, "V%d\n", bom_version);
//		} else {
//			seq_printf(m, "N/A\n");
//		}
//
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_bom_version_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_bom_version_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_bom_version_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_bom_version_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_pcb_version_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	struct bm_device_info *card_bmdi = bmctl_get_card_bmdi(bmdi);
//	int board_version = 0x0;
//	int pcb_version = 0x0;
//
//	if (card_bmdi != NULL)
//		bmdi = card_bmdi;
//
//	board_version = bmdi->misc_info.board_version;
//	pcb_version = (board_version >> 4) & 0xf;
//
//	if (bmdi->cinfo.chip_id != 0x1682) {
//		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H ||
//			BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5_P) {
//			seq_printf(m, "V%d\n", pcb_version);
//		} else {
//			seq_printf(m, "N/A\n");
//		}
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_pcb_version_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_pcb_version_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_pcb_version_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_pcb_version_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_boot_loader_version_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	if (bmdi->cinfo.chip_id != 0x1682) {
//		if (bmdi->cinfo.boot_loader_version[0][0] == 0 ||
//			bmdi->cinfo.boot_loader_version[1][0] == 0)
//			bm1684_get_bootload_version(bmdi);
//		seq_printf(m, "%s   %s\n", bmdi->cinfo.boot_loader_version[0],
//			bmdi->cinfo.boot_loader_version[1]);
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_boot_loader_version_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_boot_loader_version_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_boot_loader_version_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_boot_loader_version_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_calc_clk(int value)
//{
//	int refdiv = 0;
//	int postdiv = 0;
//	int postdiv2 = 0;
//	int fbdiv = 0;
//
//	refdiv = value & 0x1f;
//	postdiv = (value >> 8) & 0x7;
//	postdiv2 = (value >> 12) & 0x7;
//	fbdiv = (value >> 16) & 0xfff;
//	if (refdiv == 0x0 || postdiv == 0x0
//		|| postdiv2 == 0x0 || fbdiv == 0x0)
//		return -1;
//	return 25*fbdiv/refdiv/(postdiv*postdiv2);
//
//}
//
//static int bmdrv_clk_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	int mpll = 0;
//	int tpll = 0;
//	int vpll = 0;
//	int fpll = 0;
//	int dpll0 = 0;
//	int dpll1 = 0;
//
//	if (bmdi->cinfo.chip_id != 0x1682) {
//		mpll = top_reg_read(bmdi, 0xe8);
//		tpll = top_reg_read(bmdi, 0xec);
//		fpll = top_reg_read(bmdi, 0xf0);
//		vpll = top_reg_read(bmdi, 0xf4);
//		dpll0 = top_reg_read(bmdi, 0xf8);
//		dpll1 = top_reg_read(bmdi, 0xfc);
//		seq_printf(m, "mpll: %d MHz\n", bmdrv_calc_clk(mpll));
//		seq_printf(m, "tpll: %d MHz\n", bmdrv_calc_clk(tpll));
//		seq_printf(m, "fpll: %d MHz\n", bmdrv_calc_clk(fpll));
//		seq_printf(m, "vpll: %d MHz\n", bmdrv_calc_clk(vpll));
//		seq_printf(m, "dpll0: %d MHz\n", bmdrv_calc_clk(dpll0));
//		seq_printf(m, "dpll1: %d MHz\n", bmdrv_calc_clk(dpll1));
//		seq_printf(m, "vpu: %d MHz\n", bmdrv_calc_clk(vpll));
//		seq_printf(m, "jpu: %d MHz\n", bmdrv_calc_clk(vpll));
//		seq_printf(m, "vpp: %d MHz\n", bmdrv_calc_clk(vpll));
//		seq_printf(m, "tpu: %d MHz\n", bmdrv_calc_clk(tpll));
//		seq_printf(m, "ddr: %d MHz\n", 4*bmdrv_calc_clk(dpll0));
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_clk_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_clk_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_clk_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_clk_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_driver_version_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	if (bmdi->cinfo.chip_id != 0x1682) {
//		seq_printf(m, "%d.%d.%d\n", BM_CHIP_VERSION,
//			BM_MAJOR_VERSION, BM_MINOR_VERSION);
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_driver_version_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_driver_version_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_driver_version_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_driver_version_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_versions_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	if (bmdi->cinfo.chip_id != 0x1682) {
//		seq_printf(m, "driver_version: ");
//		bmdrv_driver_version_proc_show(m, v);
//		seq_printf(m, "boot_loader_version: ");
//		bmdrv_boot_loader_version_proc_show(m, v);
//		seq_printf(m, "board_version: ");
//		bmdrv_board_version_proc_show(m, v);
//		seq_printf(m, "board_type: ");
//		bmdrv_board_type_proc_show(m, v);
//		seq_printf(m, "mcu_version: ");
//		bmdrv_mcu_version_proc_show(m, v);
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_versions_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_versions_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_versions_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_versions_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_a53_enable_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	int value = 0x0;
//	int enable_a53_mask = 0x1;
//
//	if (bmdi->cinfo.chip_id != 0x1682) {
//		value = gpio_reg_read(bmdi, 0x50);
//		if ((value & enable_a53_mask) == 0x1) {
//			seq_printf(m, "disable\n");
//		} else {
//			seq_printf(m, "enable\n");
//		}
//		if (bmdi->cinfo.a53_enable == 0x0) {
//			seq_printf(m, "boot_info a53_disable\n");
//		} else {
//			seq_printf(m, "boot_info a53_enable\n");
//		}
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_a53_enable_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_a53_enable_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_a53_enable_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_a53_enable_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_heap_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
// 	struct reserved_mem_info *resmem_info = &bmdi->gmem_info.resmem_info;
//	int i = 0;
//
//	if (bmdi->cinfo.chip_id != 0x1682) {
//		for (i = 0; i < 3; i++) {
//			seq_printf(m, "heap%d, size = 0x%llx, start_addr =0x%llx\n", i,
//				resmem_info->npureserved_size[i], resmem_info->npureserved_addr[i]);
//		}
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_heap_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_heap_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_heap_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_heap_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_boot_mode_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	int value = 0x0;
//	int boot_mode_mask = 0x1 << 6;
//
//	if (bmdi->cinfo.chip_id != 0x1682) {
//		value = top_reg_read(bmdi, 0x4);
//		value = (value & boot_mode_mask) >> 0x6;
//		if (value == 0x0) {
//			seq_printf(m, "rom_boot\n");
//		} else {
//			seq_printf(m, "spi_boot\n");
//		}
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_boot_mode_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_boot_mode_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_boot_mode_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_boot_mode_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_pmu_infos_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//	int vdd_tpu_voltage = 0, vdd_tpu_current = 0;
//	int tpu_power = 0, pmu_tpu_temp = 0;
//	int vdd_tpu_mem_voltage = 0, vdd_tpu_mem_current = 0;
//	int tpu_mem_power = 0, pmu_tpu_mem_temp = 0;
//	int vddc_voltage = 0, vddc_current = 0;
//	int vddc_power = 0, pmu_vddc_temp = 0;
//	struct bm_chip_attr *c_attr = NULL;
//
//	c_attr = &bmdi->c_attr;
//	if (bmdi->cinfo.chip_id != 0x1682) {
//
//		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
//
//			//mutex_lock(&c_attr->attr_mutex);
//			bm_read_vdd_tpu_voltage(bmdi, &vdd_tpu_voltage);
//			bm_read_vdd_tpu_current(bmdi, &vdd_tpu_current);
//			bm_read_vdd_tpu_power(bmdi, &tpu_power);
//			bm_read_vdd_pmu_tpu_temp(bmdi, &pmu_tpu_temp);
//			//mutex_unlock(&c_attr->attr_mutex);
//
//			//mutex_lock(&c_attr->attr_mutex);
//			bm_read_vdd_tpu_mem_voltage(bmdi, &vdd_tpu_mem_voltage);
//			bm_read_vdd_tpu_mem_current(bmdi, &vdd_tpu_mem_current);
//			bm_read_vdd_tpu_mem_power(bmdi, &tpu_mem_power);
//			bm_read_vdd_pmu_tpu_mem_temp(bmdi, &pmu_tpu_mem_temp);
//			//mutex_unlock(&c_attr->attr_mutex);
//
//			//mutex_lock(&c_attr->attr_mutex);
//			bm_read_vddc_voltage(bmdi, &vddc_voltage);
//			bm_read_vddc_current(bmdi, &vddc_current);
//			bm_read_vddc_power(bmdi, &vddc_power);
//			bm_read_vddc_pmu_vddc_temp(bmdi, &pmu_vddc_temp);
//			//mutex_unlock(&c_attr->attr_mutex);
//
//			seq_printf(m, "-------------pmu pxc1331 infos ---------------\n");
//
//			seq_printf(m, "vdd_tpu_voltage     %6dmV\tvdd_tpu_current      %6dmA\n",
//					vdd_tpu_voltage, vdd_tpu_current);
//			seq_printf(m, "vdd_tpu_power        %6dW\tvdd_pmu_tpu_temp      %6dC\n",
//					tpu_power, pmu_tpu_temp);
//			seq_printf(m, "vdd_tpu_mem_voltage %6dmV\tvdd_tpu_mem_current  %6dmA\n",
//					vdd_tpu_mem_voltage, vdd_tpu_mem_current);
//			seq_printf(m, "vdd_tpu_mem_power    %6dW\tvdd_pmu_tpu_mem_temp  %6dC\n",
//					tpu_mem_power, pmu_tpu_mem_temp);
//			seq_printf(m, "vddc_voltage        %6dmV\tvddc_current         %6dmA\n",
//					vddc_voltage, vddc_current);
//			seq_printf(m, "vddc_power           %6dW\tvddc_pmu_vddc_temp    %6dC\n",
//					vddc_power, pmu_vddc_temp);
//		} else {
//			//mutex_lock(&c_attr->attr_mutex);
//			bm_read_vdd_tpu_voltage(bmdi, &vdd_tpu_voltage);
//			bm_read_vdd_tpu_current(bmdi, &vdd_tpu_current);
//			//mutex_unlock(&c_attr->attr_mutex);
//
//			//mutex_lock(&c_attr->attr_mutex);
//			bm_read_vddc_voltage(bmdi, &vddc_voltage);
//			bm_read_vddc_current(bmdi, &vddc_current);
//			//mutex_unlock(&c_attr->attr_mutex);
//
//			seq_printf(m, "-------------68127 infos ---------------\n");
//			seq_printf(m, "vdd_tpu_voltage %6dmV\tvdd_tpu_current %6dmA\n",
//					vdd_tpu_voltage, vdd_tpu_current);
//			seq_printf(m, "vddc_voltage    %6dmV\tvddc_current    %6dmA\n",
//					vddc_voltage, vddc_current);
//		}
//	} else {
//		seq_printf(m, "1682 not support\n");
//	}
//	return 0;
//}
//
//static int bmdrv_pmu_infos_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_pmu_infos_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_pmu_infos_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_pmu_infos_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//#endif
//
//static int bmdrv_cdma_in_counter_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%lld\n", bmdi->profile.cdma_in_counter);
//	return 0;
//}
//
//static int bmdrv_cdma_in_counter_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_cdma_in_counter_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_cdma_in_counter_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_cdma_in_counter_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_cdma_out_counter_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%lld\n", bmdi->profile.cdma_out_counter);
//	return 0;
//}
//
//static int bmdrv_cdma_out_counter_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_cdma_out_counter_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_cdma_out_counter_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_cdma_out_counter_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_cdma_in_time_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%lld us\n", bmdi->profile.cdma_in_time);
//	return 0;
//}
//
//static int bmdrv_cdma_in_time_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_cdma_in_time_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_cdma_in_time_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_cdma_in_time_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_cdma_out_time_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%lld us\n", bmdi->profile.cdma_out_time);
//	return 0;
//}
//
//static int bmdrv_cdma_out_time_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_cdma_out_time_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_cdma_out_time_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_cdma_out_time_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_tpu_process_time_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%lld us\n", bmdi->profile.tpu_process_time);
//	return 0;
//}
//
//static int bmdrv_tpu_process_time_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_tpu_process_time_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_tpu_process_time_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_tpu_process_time_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_sent_api_counter_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%lld\n", bmdi->profile.sent_api_counter);
//	return 0;
//}
//
//static int bmdrv_sent_api_counter_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_sent_api_counter_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_sent_api_counter_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_sent_api_counter_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//static int bmdrv_completed_api_counter_proc_show(struct seq_file *m, void *v)
//{
//	struct bm_device_info *bmdi = m->private;
//
//	seq_printf(m, "%lld\n", bmdi->profile.completed_api_counter);
//	return 0;
//}
//
//static int bmdrv_completed_api_counter_proc_open(struct inode *inode, struct file *file)
//{
//	return single_open(file, bmdrv_completed_api_counter_proc_show, PDE_DATA(inode));
//}
//
//static const struct file_operations bmdrv_completed_api_counter_file_ops = {
//	.owner		= THIS_MODULE,
//	.open		= bmdrv_completed_api_counter_proc_open,
//	.read		= seq_read,
//	.llseek		= seq_lseek,
//	.release	= single_release,
//};
//
//int bmdrv_proc_file_init(struct bm_device_info *bmdi)
//{
//	char name[MAX_NAMELEN];
//
//	sprintf(name, "bmsophon%d", bmdi->dev_index);
//
//	bmdi->proc_dir = proc_mkdir(name, bmsophon_proc_dir);
//	if (!bmdi->proc_dir)
//		return -1;
//
//	proc_create_data("chipid", 0444, bmdi->proc_dir, &bmdrv_chipid_file_ops,
//		(void *)bmdi);
//	proc_create_data("tpuid", 0444, bmdi->proc_dir, &bmdrv_tpuid_file_ops,
//		(void *)bmdi);
//	proc_create_data("mode", 0444, bmdi->proc_dir, &bmdrv_mode_file_ops,
//		(void *)bmdi);
//
//#ifndef SOC_MODE
//	proc_create_data("dbdf", 0444, bmdi->proc_dir, &bmdrv_dbdf_file_ops,
//		(void *)bmdi);
//	proc_create_data("status", 0444, bmdi->proc_dir, &bmdrv_status_file_ops,
//		(void *)bmdi);
//	proc_create_data("tpu_minclk", 0444, bmdi->proc_dir,
//		&bmdrv_tpu_minclk_file_ops, (void *)bmdi);
//	proc_create_data("tpu_maxclk", 0444, bmdi->proc_dir,
//		&bmdrv_tpu_maxclk_file_ops, (void *)bmdi);
//	proc_create_data("maxboardp", 0444, bmdi->proc_dir,
//		&bmdrv_tpu_maxboardp_file_ops, (void *)bmdi);
//	proc_create_data("ecc", 0444, bmdi->proc_dir, &bmdrv_ecc_file_ops,
//		(void *)bmdi);
//	proc_create_data("jpu", 0644, bmdi->proc_dir, &bmdrv_jpu_file_ops,
//		(void *)bmdi);
//	proc_create_data("media", 0644, bmdi->proc_dir, &bmdrv_vpu_file_ops,
//		(void *)bmdi);
//	proc_create_data("dynfreq", 0644, bmdi->proc_dir, &bmdrv_dynfreq_file_ops,
//		(void *)bmdi);
//	proc_create_data("dumpreg", 0644, bmdi->proc_dir, &bmdrv_dumpreg_file_ops,
//		(void *)bmdi);
//	proc_create_data("fan_speed", 0644, bmdi->proc_dir, &bmdrv_fan_speed_file_ops,
//		(void *)bmdi);
//	proc_create_data("pcie_link_speed", 0444, bmdi->proc_dir, &bmdrv_pcie_link_speed_file_ops,
//		(void *)bmdi);
//	proc_create_data("pcie_link_width", 0444, bmdi->proc_dir, &bmdrv_pcie_link_width_file_ops,
//		(void *)bmdi);
//	proc_create_data("pcie_cap_speed", 0444, bmdi->proc_dir, &bmdrv_pcie_cap_speed_file_ops,
//		(void *)bmdi);
//	proc_create_data("pcie_cap_width", 0444, bmdi->proc_dir, &bmdrv_pcie_cap_width_file_ops,
//		(void *)bmdi);
//	proc_create_data("pcie_region", 0444, bmdi->proc_dir, &bmdrv_pcie_region_file_ops,
//		(void *)bmdi);
//	proc_create_data("tpu_power", 0444, bmdi->proc_dir, &bmdrv_tpu_power_file_ops,
//		(void *)bmdi);
//	proc_create_data("tpu_cur", 0444, bmdi->proc_dir, &bmdrv_tpu_cur_file_ops,
//		(void *)bmdi);
//	proc_create_data("tpu_volt", 0444, bmdi->proc_dir, &bmdrv_tpu_volt_file_ops,
//		(void *)bmdi);
//	proc_create_data("board_power", 0444, bmdi->proc_dir, &bmdrv_board_power_file_ops,
//		(void *)bmdi);
//	proc_create_data("chip_temp", 0444, bmdi->proc_dir, &bmdrv_chip_temp_file_ops,
//		(void *)bmdi);
//	proc_create_data("board_temp", 0444, bmdi->proc_dir, &bmdrv_board_temp_file_ops,
//		(void *)bmdi);
//	proc_create_data("sn", 0444, bmdi->proc_dir, &bmdrv_board_sn_file_ops,
//		(void *)bmdi);
//	proc_create_data("mcu_version", 0444, bmdi->proc_dir, &bmdrv_mcu_version_file_ops,
//		(void *)bmdi);
//	proc_create_data("board_type", 0444, bmdi->proc_dir, &bmdrv_board_type_file_ops,
//		(void *)bmdi);
//	proc_create_data("board_version", 0444, bmdi->proc_dir,	&bmdrv_board_version_file_ops,
//		(void *)bmdi);
//	proc_create_data("boot_loader_version", 0444, bmdi->proc_dir, &bmdrv_boot_loader_version_file_ops,
//		(void *)bmdi);
//	proc_create_data("clk", 0444, bmdi->proc_dir, &bmdrv_clk_file_ops,
//		(void *)bmdi);
//	proc_create_data("driver_version", 0444, bmdi->proc_dir, &bmdrv_driver_version_file_ops,
//		(void *)bmdi);
//	proc_create_data("versions", 0444, bmdi->proc_dir, &bmdrv_versions_file_ops,
//		(void *)bmdi);
//	proc_create_data("pcb_version", 0444, bmdi->proc_dir, &bmdrv_pcb_version_file_ops,
//		(void *)bmdi);
//	proc_create_data("bom_version", 0444, bmdi->proc_dir, &bmdrv_bom_version_file_ops,
//		(void *)bmdi);
//	proc_create_data("pmu_infos", 0444, bmdi->proc_dir, &bmdrv_pmu_infos_file_ops,
//		(void *)bmdi);
//	proc_create_data("boot_mode", 0444, bmdi->proc_dir, &bmdrv_boot_mode_file_ops,
//		(void *)bmdi);
//	proc_create_data("a53_enable", 0444, bmdi->proc_dir, &bmdrv_a53_enable_file_ops,
//		(void *)bmdi);
//	proc_create_data("heap", 0444, bmdi->proc_dir, &bmdrv_heap_file_ops,
//		(void *)bmdi);
//#endif
//	proc_create_data("cdma_in_time", 0444, bmdi->proc_dir, &bmdrv_cdma_in_time_file_ops,
//		(void *)bmdi);
//	proc_create_data("cdma_in_counter", 0444, bmdi->proc_dir, &bmdrv_cdma_in_counter_file_ops,
//		(void *)bmdi);
//	proc_create_data("cdma_out_time", 0444, bmdi->proc_dir,	&bmdrv_cdma_out_time_file_ops,
//		(void *)bmdi);
//	proc_create_data("cdma_out_counter", 0444, bmdi->proc_dir, &bmdrv_cdma_out_counter_file_ops,
//		(void *)bmdi);
//	proc_create_data("tpu_process_time", 0444, bmdi->proc_dir, &bmdrv_tpu_process_time_file_ops,
//		(void *)bmdi);
//	proc_create_data("sent_api_counter", 0444, bmdi->proc_dir, &bmdrv_sent_api_counter_file_ops,
//		(void *)bmdi);
//	proc_create_data("completed_api_counter", 0444, bmdi->proc_dir,	&bmdrv_completed_api_counter_file_ops,
//		(void *)bmdi);
//	return 0;
//}
//
//void bmdrv_proc_file_deinit(struct bm_device_info *bmdi)
//{
//	remove_proc_entry("chipid", bmdi->proc_dir);
//	remove_proc_entry("tpuid", bmdi->proc_dir);
//	remove_proc_entry("mode", bmdi->proc_dir);
//
//#ifndef SOC_MODE
//	remove_proc_entry("dbdf", bmdi->proc_dir);
//	remove_proc_entry("status", bmdi->proc_dir);
//	remove_proc_entry("tpu_minclk", bmdi->proc_dir);
//	remove_proc_entry("tpu_maxclk", bmdi->proc_dir);
//	remove_proc_entry("maxboardp", bmdi->proc_dir);
//	remove_proc_entry("ecc", bmdi->proc_dir);
//	remove_proc_entry("jpu", bmdi->proc_dir);
//	remove_proc_entry("media", bmdi->proc_dir);
//	remove_proc_entry("dynfreq", bmdi->proc_dir);
//	remove_proc_entry("fan_speed", bmdi->proc_dir);
//	remove_proc_entry("pcie_link_speed", bmdi->proc_dir);
//	remove_proc_entry("pcie_link_width", bmdi->proc_dir);
//	remove_proc_entry("pcie_cap_speed", bmdi->proc_dir);
//	remove_proc_entry("pcie_cap_width", bmdi->proc_dir);
//	remove_proc_entry("pcie_region", bmdi->proc_dir);
//	remove_proc_entry("tpu_power", bmdi->proc_dir);
//	remove_proc_entry("tpu_cur", bmdi->proc_dir);
//	remove_proc_entry("tpu_volt", bmdi->proc_dir);
//	remove_proc_entry("board_power", bmdi->proc_dir);
//	remove_proc_entry("chip_temp", bmdi->proc_dir);
//	remove_proc_entry("board_temp", bmdi->proc_dir);
//	remove_proc_entry("sn", bmdi->proc_dir);
//	remove_proc_entry("mcu_version", bmdi->proc_dir);
//	remove_proc_entry("board_type", bmdi->proc_dir);
//	remove_proc_entry("boot_loader_version", bmdi->proc_dir);
//	remove_proc_entry("clk", bmdi->proc_dir);
//	remove_proc_entry("driver_version", bmdi->proc_dir);
//	remove_proc_entry("versions", bmdi->proc_dir);
//	remove_proc_entry("pcb_version", bmdi->proc_dir);
//	remove_proc_entry("bom_version", bmdi->proc_dir);
//	remove_proc_entry("pmu_infos", bmdi->proc_dir);
//	remove_proc_entry("boot_mode", bmdi->proc_dir);
//	remove_proc_entry("a53_enable", bmdi->proc_dir);
//	remove_proc_entry("heap", bmdi->proc_dir);
//#endif
//	remove_proc_entry("cdma_in_time", bmdi->proc_dir);
//	remove_proc_entry("cdma_in_counter", bmdi->proc_dir);
//	remove_proc_entry("cdma_out_time", bmdi->proc_dir);
//	remove_proc_entry("cdma_out_counter", bmdi->proc_dir);
//	remove_proc_entry("tpu_process_time", bmdi->proc_dir);
//	remove_proc_entry("sent_api_counter", bmdi->proc_dir);
//	remove_proc_entry("completed_api_counter", bmdi->proc_dir);
//	proc_remove(bmdi->proc_dir);
//}

#undef MAX_NAMELEN

bool bm_arm9fw_log_buffer_empty(struct bm_device_info *bmdi)
{
	int read_index = 0;
	int write_index = 0;

	read_index = gp_reg_read_enh(bmdi, GP_REG_ARM9FW_LOG_RP);
	write_index = gp_reg_read_enh(bmdi, GP_REG_ARM9FW_LOG_WP);
//	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"read_index = 0x%x, write_index = 0x%x\n", read_index, write_index);
	return read_index == write_index;

}

int bm_get_arm9fw_log_from_device(struct bm_device_info *bmdi)
{
	int read_p = gp_reg_read_enh(bmdi, GP_REG_ARM9FW_LOG_RP);
	int write_p = gp_reg_read_enh(bmdi, GP_REG_ARM9FW_LOG_WP);
	int arm9fw_buffer_size = bmdi->monitor_thread_info.log_mem.device_size;
	int host_size = bmdi->monitor_thread_info.log_mem.host_size;
	int size = 0;
	u64 device_paddr = bmdi->monitor_thread_info.log_mem.device_paddr;
	u64 host_paddr = bmdi->monitor_thread_info.log_mem.host_paddr;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	bm_cdma_arg cdma_arg;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"wp = %d, rp = %d, device_paddr = %llx\n", write_p, read_p, device_paddr);
	if (write_p < read_p) {
		size = arm9fw_buffer_size - read_p;

		if (size > host_size) {
			size =  host_size;
			bmdev_construct_cdma_arg(&cdma_arg, device_paddr + read_p,
				 host_paddr & 0xffffffffff, size, CHIP2HOST, FALSE, FALSE);
			if (memcpy_info->bm_cdma_transfer(bmdi, &cdma_arg, TRUE, NULL)) {
//				TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "bm-sophon%d get arm9 log failed\n", bmdi->dev_index);
				return 0;
			}
			read_p = read_p + size;

		} else {
			size = arm9fw_buffer_size - read_p;
			bmdev_construct_cdma_arg(&cdma_arg, device_paddr + read_p,
				host_paddr & 0xffffffffff, size, CHIP2HOST, FALSE, FALSE);
			if (memcpy_info->bm_cdma_transfer(bmdi, &cdma_arg, TRUE, NULL)) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "bm-sophon%d get arm9 log failed\n",
                            bmdi->dev_index);
				return 0;
			}
			read_p = 0;
		}
	} else {
		size = write_p - read_p;
		if (size >= host_size)
			size = host_size;

		bmdev_construct_cdma_arg(&cdma_arg, device_paddr + read_p,
			host_paddr & 0xffffffffff, size, CHIP2HOST, FALSE, FALSE);
		if (memcpy_info->bm_cdma_transfer(bmdi, &cdma_arg, TRUE, NULL)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bm-sophon%d get arm9 log failed\n",
                        bmdi->dev_index);
			return 0;
		}
		read_p = read_p + size;

	}
	gp_reg_write_enh(bmdi, GP_REG_ARM9FW_LOG_RP, read_p);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"size = 0x%x\n", size);
	return size;
}

#define ARM9FW_LOG_HOST_BUFFER_SIZE (1024 * 512)
#define ARM9FW_LOG_DEVICE_BUFFER_SIZE (1024 * 1024 * 4)
#define ARM9FW_LOG_LINE_SIZE 512

void bm_print_arm9fw_log(struct bm_device_info *bmdi, int size)
{
	char str[ARM9FW_LOG_LINE_SIZE] = "";
	int i = 0;
	char *p = bmdi->monitor_thread_info.log_mem.host_vaddr;

	for (i = 0; i < size/ARM9FW_LOG_LINE_SIZE; i++) {
        strncpy_s(str, ARM9FW_LOG_LINE_SIZE, p, ARM9FW_LOG_LINE_SIZE);
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DEVICE,
                    "bm-sophon%d ARM9_LOG: %s",
                    bmdi->dev_index,
                    str);
		p += ARM9FW_LOG_LINE_SIZE;
	}
	memset(bmdi->monitor_thread_info.log_mem.host_vaddr, 0, size);
}

int bm_arm9fw_log_init(struct bm_device_info *bmdi)
{
	int ret = 0;
    NTSTATUS                  status;
    struct bm_arm9fw_log_mem *log_mem = &bmdi->monitor_thread_info.log_mem;
    WDF_DMA_ENABLER_CONFIG    dmaConfig;


    bmdi->monitor_thread_info.log_mem.host_size = ARM9FW_LOG_HOST_BUFFER_SIZE;
    if (bmdi->cinfo.chip_id == 0x1686)
		bmdi->monitor_thread_info.log_mem.device_paddr = bmdi->gmem_info.resmem_info.armreserved_addr + (bmdi->gmem_info.resmem_info.armreserved_size - ARM9FW_LOG_DEVICE_BUFFER_SIZE);
	else
		bmdi->monitor_thread_info.log_mem.device_paddr = bmdi->gmem_info.resmem_info.armfw_addr + (bmdi->gmem_info.resmem_info.armfw_size - ARM9FW_LOG_DEVICE_BUFFER_SIZE);

	bmdi->monitor_thread_info.log_mem.device_size = ARM9FW_LOG_DEVICE_BUFFER_SIZE;

    WDF_DMA_ENABLER_CONFIG_INIT(&dmaConfig, WdfDmaProfilePacket64, ARM9FW_LOG_HOST_BUFFER_SIZE);
    WdfDeviceSetAlignmentRequirement(bmdi->WdfDevice, 1);

    status = WdfDmaEnablerCreate(bmdi->WdfDevice,
                                 &dmaConfig,
                                 WDF_NO_OBJECT_ATTRIBUTES,
                                 &log_mem->log_mem_DmaEnabler);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_CDMA,
                    "arm9fw log buffer WdfDmaEnblerCreate failed: %08X\n",
                    status);
        ret = -1;
    }

    status = WdfCommonBufferCreate(log_mem->log_mem_DmaEnabler,
                                   ARM9FW_LOG_HOST_BUFFER_SIZE,
                                   WDF_NO_OBJECT_ATTRIBUTES,
                                   &log_mem->log_mem_CommonBuffer);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_CDMA,
                    "arm9fw log buffer CommonBuffer failed: %!STATUS!",
                    status);
        ret = -1;
    }

    log_mem->log_mem_vaddr = WdfCommonBufferGetAlignedVirtualAddress(log_mem->log_mem_CommonBuffer);
    log_mem->log_mem_paddr = WdfCommonBufferGetAlignedLogicalAddress(log_mem->log_mem_CommonBuffer);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_CDMA,
                "CommonBuffer 0x%p  (#0x%I64X), length %I64d",
                log_mem->log_mem_vaddr,
                log_mem->log_mem_paddr.QuadPart,
                WdfCommonBufferGetLength(log_mem->log_mem_CommonBuffer));

    bmdi->monitor_thread_info.log_mem.host_paddr = log_mem->log_mem_paddr.QuadPart;
    bmdi->monitor_thread_info.log_mem.host_vaddr = log_mem->log_mem_vaddr;

	memset(bmdi->monitor_thread_info.log_mem.host_vaddr, 0,
			bmdi->monitor_thread_info.log_mem.host_size);

	gp_reg_write_enh(bmdi, GP_REG_ARM9FW_LOG_RP, 0);
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "host size = 0x%x, device_addr = 0x%llx, device size = 0x%x\n",
		bmdi->monitor_thread_info.log_mem.host_size, bmdi->monitor_thread_info.log_mem.device_paddr,
		bmdi->monitor_thread_info.log_mem.device_size);

	return ret;
}

void bm_dump_arm9fw_log(struct bm_device_info *bmdi, int count)
{
	int size = 0;
    u64     start = 0;
    u64     end   = 0;
    int       delt  = 0;

    LARGE_INTEGER CurTime, Freq;
    CurTime = KeQueryPerformanceCounter(&Freq);
    start   = (CurTime.QuadPart * 1000) / Freq.QuadPart;

	bm_npu_utilization_stat(bmdi);
	if(count % 3 == 0) {
		if (!bm_arm9fw_log_buffer_empty(bmdi)) {
			size = bm_get_arm9fw_log_from_device(bmdi);
			bm_print_arm9fw_log(bmdi, size);
            CurTime = KeQueryPerformanceCounter(&Freq);
            end     = (CurTime.QuadPart * 1000) / Freq.QuadPart;
            delt    = (int)(end - start);
            //TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ATTR,"dev_index=%d,bm_dump_arm9fw_log time is %d ms\n",bmdi->dev_index, delt);
            bm_mdelay(((10 - delt) > 0) ? 10 - delt : 0);
		}
	} else if (count % 10 == 0x0) {
#ifndef SOC_MODE
//		if (bm_i2c2_need_recovery(bmdi)) {
//			bm_i2c2_recovey(bmdi);
//			TaceEvents(TRACE_LEVEL_INFORMATION, TRACE_ATTR, "sophon%d recovery i2c2\n", bmdi->dev_index);
//		}
#endif
        CurTime = KeQueryPerformanceCounter(&Freq);
        end     = (CurTime.QuadPart * 1000) / Freq.QuadPart;
        delt    = (int)(end - start);
        //TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ATTR,"dev_index=%d,bm_dump_arm9fw_log time is %d ms\n",bmdi->dev_index, delt);
        bm_mdelay(((10 - delt) > 0) ? 10 - delt : 0);
	} else {
        bm_mdelay(10);
		//TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ATTR, "buffer is empty\n");
	}
}

char *bmdrv_get_error_string(int error){
    UNREFERENCED_PARAMETER(error);
 /*  int er = abs(error);
   switch(er){
        case EPERM:
            return "Operation not permitted";
        case ENOENT:
            return "No such file or directory";
        case ESRCH:
            return "No such process";
        case EINTR:
            return "Interrupted system call";
        case EIO:
            return "I/O error";
        case ENXIO:
            return "No such device or address";
        case E2BIG:
            return "Argument list too long";
        case ENOEXEC:
            return "Exec format error";
        case EBADF:
            return "Bad file number";
        case ECHILD:
            return "No child processes";
        case EAGAIN:
            return "Try again Or Operation would block";
        case ENOMEM:
            return "Out of memory";
        case EACCES:
            return "Permission denied";
        case EFAULT:
            return "Bad address";
        case ENOTBLK:
            return "Block device required";
        case EBUSY:
            return "Device or resource busy";
        case EEXIST:
            return "File exists";
        case EXDEV:
            return "Cross-device link";
        case ENODEV:
            return "No such device";
        case ENOTDIR:
            return "Not a directory";
        case EISDIR:
            return "Is a directory";
        case EINVAL:
            return "Invalid argument";
        case ENFILE:
            return "File table overflow";
        case EMFILE:
            return "Too many open files";
        case ENOTTY:
            return "Not a typewriter";
        case ETXTBSY:
            return "Text file busy";
        case EFBIG:
            return "File too large";
        case ENOSPC:
            return "No space left on device";
        case ESPIPE:
            return "Illegal seek";
        case EROFS:
            return "Read-only file system";
        case EMLINK:
            return "Too many links";
        case EPIPE:
            return "Broken pipe";
        case EDOM:
            return "Math argument out of domain of func";
        case ERANGE:
            return "Math result not representable";

        case EDEADLK:
            return "Resource deadlock would occur";
        case ENAMETOOLONG:
            return "File name too long";
        case ENOLCK:
            return "No record locks available";
        case ENOSYS:
            return "Invalid system call number";
        case ENOTEMPTY:
            return "Directory not empty";
        case ELOOP:
            return "Too many symbolic links encountered";
        case ENOMSG:
            return "No message of desired type";
        case EIDRM:
            return "Identifier removed";
        case ECHRNG:
            return "Channel number out of range";
        case EL2NSYNC:
            return "Level 2 not synchronized";
        case EL3HLT:
            return "Level 3 halted";
        case EL3RST:
            return "Level 3 reset";
        case ELNRNG:
            return "Link number out of range";
        case EUNATCH:
            return "Protocol driver not attached";
        case ENOCSI:
            return "No CSI structure available";
        case EL2HLT:
            return "Level 2 halted";
        case EBADE:
            return "Invalid exchange";
        case EBADR:
            return "Invalid request descriptor";
        case EXFULL:
            return "Exchange full";
        case ENOANO:
            return "No anode";
        case EBADRQC:
            return "Invalid request code";
        case EBADSLT:
            return "Invalid slot";
        case EBFONT:
            return "Bad font file format";
        case ENOSTR:
            return "Device not a stream";
        case ENODATA:
            return "No data available";
        case ETIME:
            return "Timer expired";
        case ENOSR:
            return "Out of streams resources";
        case ENONET:
            return "Machine is not on the network";
        case ENOPKG:
            return "Package not installed";
        case EREMOTE:
            return "Object is remote";
        case ENOLINK:
            return "Link has been severed";
        case EADV:
            return "Advertise error";
        case ESRMNT:
            return "Srmount error";
        case ECOMM:
            return "Communication error on send";
        case EPROTO:
            return "Protocol error";
        case EMULTIHOP:
            return "Multihop attempted";
        case EDOTDOT:
            return "RFS specific error";
        case EBADMSG:
            return "Not a data message";
        case EOVERFLOW:
            return "Value too large for defined data type";
        case ENOTUNIQ:
            return "Name not unique on network";
        case EBADFD:
            return "File descriptor in bad state";
        case EREMCHG:
            return "Remote address changed";
        case ELIBACC:
            return "Can not access a needed shared library";
        case ELIBBAD:
            return "Accessing a corrupted shared library";
        case ELIBSCN:
            return ".lib section in a.out corrupted";
        case ELIBMAX:
            return "Attempting to link in too many shared libraries";
        case ELIBEXEC:
            return "Cannot exec a shared library directly";
        case EILSEQ:
            return "Illegal byte sequence";
        case ERESTART:
            return "Interrupted system call should be restarted";
        case ESTRPIPE:
            return "Streams pipe error";
        case EUSERS:
            return "Too many users";
        case ENOTSOCK:
            return "Socket operation on non-socket";
        case EDESTADDRREQ:
            return "Destination address required";
        case EMSGSIZE:
            return "Message too long";
        case EPROTOTYPE:
            return "Protocol wrong type for socket";
        case ENOPROTOOPT:
            return "Protocol not available";
        case EPROTONOSUPPORT:
            return "Protocol not supported";
        case ESOCKTNOSUPPORT:
            return "Socket type not supported";
        case EOPNOTSUPP:
            return "Operation not supported on transport endpoint";
        case EPFNOSUPPORT:
            return "Protocol family not supported";
        case EAFNOSUPPORT:
            return "Address family not supported by protocol";
        case EADDRINUSE:
            return "Address already in use";
        case EADDRNOTAVAIL:
            return "Cannot assign requested address";
        case ENETDOWN:
            return "Network is down";
        case ENETUNREACH:
            return "Network is unreachable";
        case ENETRESET:
            return "Network dropped connection because of reset";
        case ECONNABORTED:
            return "Software caused connection abort";
        case ECONNRESET:
            return "Connection reset by peer";
        case ENOBUFS:
            return "No buffer space available";
        case EISCONN:
            return "Transport endpoint is already connected";
        case ENOTCONN:
            return "Transport endpoint is not connected";
        case ESHUTDOWN:
            return "Cannot send after transport endpoint shutdown";
        case ETOOMANYREFS:
            return "Too many references: cannot splice";
        case ETIMEDOUT:
            return "Connection timed out";
        case ECONNREFUSED:
            return "Connection refused";
        case EHOSTDOWN:
            return "Host is down";
        case EHOSTUNREACH:
            return "No route to host";
        case EALREADY:
            return "Operation already in progress";
        case EINPROGRESS:
            return "Operation now in progress";
        case ESTALE:
            return "Stale file handle";
        case EUCLEAN:
            return "Structure needs cleaning";
        case ENOTNAM:
            return "Not a XENIX named type file";
        case ENAVAIL:
            return "No XENIX semaphores available";
        case EISNAM:
            return "Is a named type file";
        case EREMOTEIO:
            return "Remote I/O error";
        case EDQUOT:
            return "Quota exceeded";
        case ENOMEDIUM:
            return "No medium found";
        case EMEDIUMTYPE:
            return "Wrong medium type";
        case ECANCELED:
            return "Operation Canceled";
        case ENOKEY:
            return "Required key not available";
        case EKEYREVOKED:
            return "Key has been revoked";
        case EKEYREJECTED:
            return "Key was rejected by service";
        case EOWNERDEAD:
            return "Owner died";
        case ENOTRECOVERABLE:
            return "State not recoverable";
        case ERFKILL:
            return "Operation not possible due to RF-kill";
        case EHWPOISON:
            return "Memory page has hardware error";
    }*/
    return "Unknown Error Dash!";
}