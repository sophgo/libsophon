#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include "bm_common.h"
#include "bm_uapi.h"
#include "bm_thread.h"
#include "bm_fw.h"
#include "bm_ctl.h"
#include "bm_drv.h"
#include "bm_card.h"
#include "bm_pcie.h"
#include "bm1684_card.h"
#include "bm1684_clkrst.h"
#include "bm1684_base64.h"
#include "bm_timer.h"
#ifndef SOC_MODE
#include "spi.h"
#include "i2c.h"
#include "vpp/vpp_platform.h"
#include "bm1684_jpu.h"
#include "bm1684_irq.h"
#include "vpu/vpu.h"
#include "console.h"
#include "bm_napi.h"
#include "bm_pt.h"
#include "efuse.h"
#endif

extern dev_t bm_devno_base;
extern dev_t bm_ctl_devno_base;
extern int bmdrv_reset_bmcpu(struct bm_device_info *bmdi);

static int bmdev_open(struct inode *inode, struct file *file)
{
	struct bm_device_info *bmdi;
	pid_t open_pid;
	struct bm_handle_info *h_info;
	struct bm_thread_info *thd_info = NULL;

	PR_TRACE("bmdev_open\n");

	bmdi = container_of(inode->i_cdev, struct bm_device_info, cdev);

	if (bmdi == NULL)
		return -ENODEV;

#ifndef SOC_MODE
	if (atomic_read(&bmdi->dev_recovery) != 0) {
		pr_info("bmsophon%d, is recoverying, should open the device after recoverying\n", bmdi->dev_index);
		return -EBUSY;
	}
#endif

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	bmdi->dev_refcount++;
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);
	open_pid = current->pid;

	h_info = kmalloc(sizeof(struct bm_handle_info), GFP_KERNEL);
	if (!h_info) {
		mutex_lock(&bmdi->gmem_info.gmem_mutex);
		bmdi->dev_refcount--;
		mutex_unlock(&bmdi->gmem_info.gmem_mutex);
		return -ENOMEM;
	}

	hash_init(h_info->api_htable);
	h_info->file = file;
	h_info->open_pid = open_pid;
	h_info->gmem_used = 0ULL;
	h_info->h_send_api_seq = 0ULL;
	h_info->h_cpl_api_seq = 0ULL;
	init_waitqueue_head(&h_info->h_msg_done);
	mutex_init(&h_info->h_api_seq_mutex);

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	thd_info = bmdrv_create_thread_info(h_info, open_pid);
	if (!thd_info) {
		kfree(h_info);
		bmdi->dev_refcount--;
		mutex_unlock(&bmdi->gmem_info.gmem_mutex);
		return -ENOMEM;
	}
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	list_add(&h_info->list, &bmdi->handle_list);
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	file->private_data = bmdi;
#ifndef SOC_MODE
	if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
		bm_vpu_open(inode, file);
		bm_jpu_open(inode, file);
	}
#endif

#ifdef USE_RUNTIME_PM
	pm_runtime_get_sync(bmdi->cinfo.device);
#endif
	return 0;
}

static ssize_t bmdev_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
#ifndef SOC_MODE
	return bm_vpu_read(filp, buf, len, ppos);
#else
	return -1;
#endif
}

static ssize_t bmdev_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
#ifndef SOC_MODE
	return bm_vpu_write(filp, buf, len, ppos);
#else
	return -1;
#endif
}

static int bmdev_fasync(int fd, struct file *filp, int mode)
{
#ifndef SOC_MODE
	return bm_vpu_fasync(fd, filp, mode);
#else
	return -1;
#endif
}

static int bmdev_close(struct inode *inode, struct file *file)
{
	struct bm_device_info *bmdi = file->private_data;
	struct bm_handle_info *h_info, *h_node;
	int handle_num = 0;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bmdrv: file list is not found!\n");
		return -EINVAL;
	}

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	list_for_each_entry(h_node, &bmdi->handle_list, list) {
		if (h_node->open_pid == h_info->open_pid) {
			handle_num++;
		}
	}
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	if (handle_num == 1)
		bmdrv_api_clear_lib(bmdi, file);

#ifndef SOC_MODE
	if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
		bm_vpu_release(inode, file);
		bm_jpu_release(inode, file);
	}
#endif
	/* invalidate pending APIs in msgfifo */
	bmdev_invalidate_pending_apis(bmdi, h_info);

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	bmdrv_delete_thread_info(h_info);
	list_del(&h_info->list);
	kfree(h_info);
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	file->private_data = NULL;

#ifdef USE_RUNTIME_PM
	pm_runtime_put_sync(bmdi->cinfo.device);
#endif
	PR_TRACE("bmdev_close\n");
	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	bmdi->dev_refcount--;
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);
	return 0;
}

static long bm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)file->private_data;
	int ret = 0;

	if (bmdi->status_over_temp || bmdi->status_pcie) {
		pr_err("bmsophon %d the temperature is too high, bypass send ioctl cmd to chip\n", bmdi->dev_index);
		if (cmd != BMDEV_GET_STATUS) {
			return -1;
		}
	}

	switch (cmd) {
#ifndef SOC_MODE
	case BMDEV_TRIGGER_VPP:
		//pr_info("begin process trigger_vpp in bm_ioctl.\n");
		ret = trigger_vpp(bmdi, arg);
		//pr_info("process trigger_vpp in bm_ioctl complete.ret=%d\n", ret);
		break;
	case BMDEV_TRIGGER_SPACC:
		//pr_info("begin process trigger_spacc in bm_ioctl.\n");
		ret = trigger_spacc(bmdi, arg);
		//pr_info("process trigger_spacc in bm_ioctl complete.ret=%d\n", ret);
		break;
	case BMDEV_SECKEY_VALID:
		ret = bmdev_is_seckey_valid(bmdi, arg);
		break;
	case BMDEV_EFUSE_WRITE:
		{
			struct bm_efuse_param temp;
			unsigned int *buffer;
			int i = 0;

			mutex_lock(&bmdi->efuse_mutex);
			ret = copy_from_user(&temp, (void *)arg, sizeof(struct bm_efuse_param));
			if (ret != 0) {
				pr_err("BMDEV_EFUSE_WRITE copy_from_user wrong,ret= %d\n", ret);
				mutex_unlock(&bmdi->efuse_mutex);
				return -EFAULT;
			}
			buffer = kzalloc(temp.datalen *4, GFP_KERNEL);
			if (buffer == NULL) {
				ret = -EFAULT;
				pr_info("BMDEV_EFUSE_WRITE buffer is NULL, dev_index  %d\n", bmdi->dev_index);
				mutex_unlock(&bmdi->efuse_mutex);
				return ret;
			}
			ret = copy_from_user(buffer, temp.data, temp.datalen*4);
			if (ret != 0) {
				pr_err("BMDEV_EFUSE_WRITE copy_from_user wrong2,ret= %d\n", ret);
				kfree(buffer);
				mutex_unlock(&bmdi->efuse_mutex);
				return -EFAULT;
			}

			for(i = 0; i < temp.datalen; i++){
				bmdrv_efuse_write(bmdi, temp.addr +i , buffer[i]);
				//pr_info("wirte datalen = %d, addr = %d, data = %x \n" , temp.datalen, temp.addr +i, buffer[i]);
			}

			kfree(buffer);
			mutex_unlock(&bmdi->efuse_mutex);
			break;

		}
	case BMDEV_EFUSE_READ:
		{
			struct bm_efuse_param temp;
			unsigned int *buffer;
			int i = 0;

			mutex_lock(&bmdi->efuse_mutex);
			ret = copy_from_user(&temp, (void *)arg, sizeof(struct bm_efuse_param));
			if (ret != 0) {
				pr_err("BMDEV_EFUSE_READ copy_from_user wrong,ret= %d\n", ret);
				mutex_unlock(&bmdi->efuse_mutex);
				return -EFAULT;
			}

			buffer = kzalloc(temp.datalen *4, GFP_KERNEL);
			if (buffer == NULL) {
				ret = -EFAULT;
				pr_info("BMDEV_EFUSE_READ buffer is NULL, dev_index  %d\n", bmdi->dev_index);
				mutex_unlock(&bmdi->efuse_mutex);
				return ret;
			}

			for(i = 0; i < temp.datalen; i++){
				buffer[i]=bmdrv_efuse_read(bmdi, temp.addr+i);
				//pr_info("read datalen = %d, addr = %d, data = %x \n" , temp.datalen, temp.addr +i, buffer[i]);
			}

			ret = copy_to_user(temp.data, buffer, temp.datalen*4);
			if (ret != 0) {
				pr_err("BMDEV_EFUSE_READ copy_from_user wrong2,ret= %d\n", ret);
				kfree(buffer);
				mutex_unlock(&bmdi->efuse_mutex);
				return -EFAULT;
			}

			kfree(buffer);
			mutex_unlock(&bmdi->efuse_mutex);
			break;
		}
	case BMDEV_TRIGGER_BMCPU:
		{
			unsigned char buffer_r[4] = {0};
			unsigned char buffer_w[4] = {0};
			int delay = 0;
			u32 val;

			if (bmdi->cinfo.chip_id == 0x1686) {
				val = top_reg_read(bmdi, 0x214);
				val |= 1;
				top_reg_write(bmdi, 0x214, val);
				break;
			}

			if (bmdi->misc_info.a53_enable == 1) {
				ret = copy_from_user(&delay, (void *)arg, sizeof(int));
				if (ret != 0) {
					pr_err("BMDEV_TRIGGER_BMCPU copy_from_user wrong,the number of bytes not copied %d\n", ret);
					return -EFAULT;
				}

				mutex_lock(&bmdi->c_attr.attr_mutex);
				bmdev_memcpy_d2s_internal(bmdi, (void *)buffer_r, 0x10000FF4,sizeof(buffer_r));
				buffer_w[0] = buffer_r[0]|(1<<0);
				ret = bmdev_memcpy_s2d_internal(bmdi, 0x10000FF4, (void *)buffer_w, sizeof(buffer_w));
				msleep(delay);
				mutex_unlock(&bmdi->c_attr.attr_mutex);
				bmdev_memcpy_s2d_internal(bmdi, 0x10000FF4, (void *)buffer_r, sizeof(buffer_r));

				bmdi->status_reset = A53_RESET_STATUS_FALSE;
				break;
			} else {
				ret = -1;
				break;
			}
		}

	case BMDEV_SETUP_VETH:
		if (bmdi->eth_state == true) {
			ret = -EEXIST;
			pr_info("bmsophon%d veth has been started!\n", bmdi->dev_index);
			pr_info("no need to reinit the veth%d\n", bmdi->dev_index);
			break;
		}
		ret = bmdrv_veth_init(bmdi, bmdi->cinfo.pcidev);
		if (ret == 0)
			bmdi->eth_state = true;
		else
			bmdi->eth_state = false;
		break;

	case BMDEV_RMDRV_VETH:
	{
		if (bmdi->eth_state == true) {
			bmdi->eth_state = false;
			bmdrv_veth_deinit(bmdi, bmdi->cinfo.pcidev);
		} else {
			ret = -EEXIST;
			pr_info("bmsophon%d veth do not exit!\n", bmdi->dev_index);
			pr_info("no need to remove the veth%d\n", bmdi->dev_index);
		}
		break;
	}

	case BMDEV_SET_IP:
	{
		u32 ip;

		ret = copy_from_user(&ip, (void *)arg, sizeof(u32));
		bm_write32(bmdi, VETH_SHM_START_ADDR_1684X + VETH_IPADDRESS_REG, ip);
		break;
	};

	case BMDEV_SET_GATE:
	{
		u32 gate;

		ret = copy_from_user(&gate, (void *)arg, sizeof(u32));
		bm_write32(bmdi, VETH_SHM_START_ADDR_1684X + VETH_GATE_ADDRESS_REG, gate);
		break;
	};

	case BMDEV_GET_SMI_ATTR:
	{
		ret = bmdev_ioctl_get_attr(bmdi, (void *)arg);
		break;
	}

	case BMDEV_SET_FW_MODE:
	{
		typedef enum {
			FW_PCIE_MODE,
			FW_SOC_MODE,
			FW_MIX_MODE
		} bm_arm9_fw_mode;
		bm_arm9_fw_mode mode;

		ret = copy_from_user(&mode, (void *)arg, sizeof(bm_arm9_fw_mode));
		pr_info("set arm9 fw mode %d\n", mode);
		if (ret != 0)
		{
			pr_err("BMDEV_SET_FW_MODE copy_from_user wrong, ret is %d\n", ret);
			return -EFAULT;
		}
		gp_reg_write_enh(bmdi, GP_REG_ARM9_FW_MODE, mode);
		break;
	};

	case BMDEV_GET_VETH_STATE:
	{
		unsigned int value;
		if (bmdi->cinfo.chip_id == 0x1684)
			value = bm_read32(bmdi, VETH_SHM_START_ADDR_1684 + VETH_A53_STATE_REG);
		else if (bmdi->cinfo.chip_id == 0x1686)
			value = bm_read32(bmdi, VETH_SHM_START_ADDR_1684X + VETH_A53_STATE_REG);
		ret = copy_to_user((unsigned int __user *)arg, &value, sizeof(value));
		break;
	};

	case BMDEV_COMM_READ:
	{
		int cnt;
		struct sgcpu_comm_data data;

		mutex_lock(&bmdi->comm.data_mutex);
		ret = copy_from_user(&data, (struct sgcpu_comm_data __user *)arg, sizeof(struct sgcpu_comm_data));
		if (ret != 0) {
			pr_err("read data copy from user error!\n");
			mutex_unlock(&bmdi->comm.data_mutex);
			return -22;
		}

		cnt = sg_comm_recv(bmdi, data.data, data.len);
		ret = copy_to_user((struct sgcpu_comm_data __user *)arg, &data, sizeof(struct sgcpu_comm_data));
		mutex_unlock(&bmdi->comm.data_mutex);
		if (ret == 0)
			return cnt;

		return -1;
	}

	case BMDEV_COMM_WRITE:
	{
		int cnt;
		struct sgcpu_comm_data data;

		mutex_lock(&bmdi->comm.data_mutex);
		ret = copy_from_user(&data, (struct sgcpu_comm_data __user *)arg, sizeof(struct sgcpu_comm_data));
		if (ret == 0) {
			cnt = sg_comm_send(bmdi, data.data, data.len);
			mutex_unlock(&bmdi->comm.data_mutex);
			return cnt;
		}
		mutex_unlock(&bmdi->comm.data_mutex);
		return -1;
	}

	case BMDEV_COMM_READ_MSG:
	{
		int cnt;
		struct sgcpu_comm_data data;

		mutex_lock(&bmdi->comm.msg_mutex);
		ret = copy_from_user(&data, (struct sgcpu_comm_data __user *)arg, sizeof(struct sgcpu_comm_data));
		if (ret != 0) {
			mutex_unlock(&bmdi->comm.msg_mutex);
			pr_err("read msg copy from user error!\n");
			return -22;
		}
		cnt = sg_comm_msg_recv(bmdi, data.data, data.len);
		ret = copy_to_user((struct sgcpu_comm_data __user *)arg, &data, sizeof(struct sgcpu_comm_data));
		mutex_unlock(&bmdi->comm.msg_mutex);
		if (ret == 0)
			return cnt;

		return -1;
	}

	case BMDEV_COMM_WRITE_MSG:
	{
		int cnt;
		struct sgcpu_comm_data data;

		mutex_lock(&bmdi->comm.msg_mutex);
		ret = copy_from_user(&data, (struct sgcpu_comm_data __user *)arg, sizeof(struct sgcpu_comm_data));
		if (ret == 0) {
			mutex_unlock(&bmdi->comm.msg_mutex);
			cnt = sg_comm_msg_send(bmdi, data.data, data.len);
			return cnt;
		}
		mutex_unlock(&bmdi->comm.msg_mutex);
		return -1;
	}

	case BMDEV_COMM_CONNECT_STATE:
		ret = copy_to_user((int *)arg, &bmdi->comm.buffer_ready, sizeof(int));
		break;

	case BMDEV_COMM_SET_CARDID:
	{
		int id;

		ret = copy_from_user(&id, (int *)arg, sizeof(int));
		if (ret == 0) {
			bm_write32(bmdi, COMM_SHM_START_ADDR + COMM_SHM_CARDID_OFFSET, id);
		} else {
			pr_err("copy failed when set card id\n");
		}
		break;
	}
	case BMDEV_SET_BMCPU_STATUS:
	{
		typedef enum {
			BMCPU_IDLE    = 0,
			BMCPU_RUNNING = 1,
			BMCPU_FAULT   = 2
		} bm_cpu_status_t;
		bm_cpu_status_t status;
		ret = copy_from_user(&status, (void *)arg, sizeof(bm_cpu_status_t));
		if (ret != 0){
			pr_err("BMDEV_SET_BMCPU_STATUS copy_from_user wrong, ret is %d\n", ret);
			return -EFAULT;
		}

		mutex_lock(&bmdi->c_attr.attr_mutex);
		bmdi->status_bmcpu = status;
		mutex_unlock(&bmdi->c_attr.attr_mutex);
		break;
	};

	case BMDEV_GET_BMCPU_STATUS:
	{
		typedef enum {
			BMCPU_IDLE    = 0,
			BMCPU_RUNNING = 1,
			BMCPU_FAULT   = 2
		} bm_cpu_status_t;
		bm_cpu_status_t status;
		mutex_lock(&bmdi->c_attr.attr_mutex);
		status = bmdi->status_bmcpu;
		mutex_unlock(&bmdi->c_attr.attr_mutex);

		ret = copy_to_user((void __user *)arg, &status, sizeof(bm_cpu_status_t));
		if (ret != 0) {
			pr_err("BMDEV_GET_BMCPU_STATUS copy_to_user wrong, ret is %d\n", ret);
			return -EFAULT;
		}
		break;
	}

	case BMDEV_FORCE_RESET_A53:
		//TODO: use reset replace force reset
		// sg_comm_clear_queue(bmdi);
		// bm_write32(bmdi, 0x207F440, 0);
		// if (bmdi->comm.ring_buffer) {
		// 	pt_uninit(bmdi->comm.ring_buffer);
		// 	bmdi->comm.ring_buffer = NULL;
		// }

		ret = bmdrv_reset_bmcpu(bmdi);
		break;

	case BMDEV_I2C_READ_SLAVE:
		ret = bm_i2c_read_slave(bmdi, arg);
		break;

	case BMDEV_I2C_WRITE_SLAVE:
		ret = bm_i2c_write_slave(bmdi, arg);
		break;

	case BMDEV_GET_HEAP_STAT_BYTE:
		ret = bmdrv_gmem_ioctl_get_heap_stat_byte_by_id(bmdi, file, arg);
		break;

	case BMDEV_GET_HEAP_NUM:
		ret = bmdrv_gmem_ioctl_get_heap_num(bmdi, arg);
		break;

	case BMDEV_I2C_SMBUS_ACCESS:
		ret = bm_i2c_smbus_access(bmdi, arg);
		break;
#endif
	case BMDEV_MEMCPY:
		ret = bmdev_memcpy(bmdi, file, arg);
		break;

	case BMDEV_ALLOC_GMEM:
		ret = bmdrv_gmem_ioctl_alloc_mem(bmdi, file, arg);
		break;

	case BMDEV_ALLOC_GMEM_ION:
		ret = bmdrv_gmem_ioctl_alloc_mem_ion(bmdi, file, arg);
		break;

	case BMDEV_FREE_GMEM:
		ret = bmdrv_gmem_ioctl_free_mem(bmdi, file, arg);
		break;

	case BMDEV_TOTAL_GMEM:
		ret = put_user(bmdrv_gmem_total_size(bmdi), (u64 __user *)arg);
		break;

	case BMDEV_AVAIL_GMEM:
		ret = put_user(bmdrv_gmem_avail_size(bmdi), (u64 __user *)arg);
		break;

	case BMDEV_SEND_API:
		if (bmdi->status_sync_api == 0) {
			ret = bmdrv_send_api(bmdi, file, arg, false);
		} else {
			pr_err("bm-sophon%d: tpu hang\n",bmdi->dev_index);
			ret = -EBUSY;
		}
		break;

	case BMDEV_SEND_API_EXT:
		if (bmdi->status_sync_api == 0)
			ret = bmdrv_send_api(bmdi, file, arg, true);
		else {
			pr_err("bm-sophon%d: tpu hang\n",bmdi->dev_index);
			ret = -EBUSY;
		}
		break;

	case BMDEV_QUERY_API_RESULT:
		if (bmdi->status_sync_api == 0)
			ret = bmdrv_query_api(bmdi, file, arg);
		else {
			pr_err("bm-sophon%d: tpu hang\n",bmdi->dev_index);
			ret = -EBUSY;
		}
		break;

	case BMDEV_THREAD_SYNC_API:
		if (bmdi->status_sync_api == 0) {
			ret = bmdrv_thread_sync_api(bmdi, file);
			bmdi->status_sync_api = ret;
		} else {
			pr_err("bm-sophon%d: tpu hang\n",bmdi->dev_index);
			ret = -EBUSY;
		}
		break;

	case BMDEV_HANDLE_SYNC_API:
		if (bmdi->status_sync_api == 0) {
			ret = bmdrv_handle_sync_api(bmdi, file);
			bmdi->status_sync_api = ret;
		} else {
			pr_err("bm-sophon%d: tpu hang\n",bmdi->dev_index);
			  ret = -EBUSY;
		}
		break;

	case BMDEV_DEVICE_SYNC_API:
		if (bmdi->status_sync_api == 0) {
			ret = bmdrv_device_sync_api(bmdi);
			bmdi->status_sync_api = ret;
		} else {
			pr_err("bm-sophon%d: tpu hang\n",bmdi->dev_index);
			ret = -EBUSY;
		}
		break;

	case BMDEV_REQUEST_ARM_RESERVED:
		ret = put_user(bmdi->gmem_info.resmem_info.armreserved_addr, (unsigned long __user *)arg);
		break;

	case BMDEV_RELEASE_ARM_RESERVED:
		break;

	case BMDEV_UPDATE_FW_A9:
		{
			bm_fw_desc fw;

			if (copy_from_user(&fw, (bm_fw_desc __user *)arg, sizeof(bm_fw_desc)))
				return -EFAULT;

#ifndef SOC_MODE
#if SYNC_API_INT_MODE == 1
			if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686))
				bm1684_pcie_msi_irq_enable(bmdi->cinfo.pcidev, bmdi);
#endif
#endif
			ret = bmdrv_fw_load(bmdi, file, &fw);
			break;
		}
#ifndef SOC_MODE
	case BMDEV_PROGRAM_A53:
		{
			struct bin_buffer bin_buf;
			u8 *kernel_bin_addr;
			int pkt_num, pkt_len, last_len, loop;

			if (copy_from_user(&bin_buf, (struct bin_buffer __user *)arg,
						sizeof(struct bin_buffer)))
				return -EFAULT;

			pkt_len = 1024 * 1024 * sizeof(char);
			kernel_bin_addr = kmalloc(pkt_len, GFP_KERNEL);
			if (!kernel_bin_addr)
				return -ENOMEM;

			bm_spi_init(bmdi);
			pkt_num = bin_buf.size / pkt_len;
			last_len = bin_buf.size % pkt_len;

			for (loop = 0; loop < pkt_num; loop++) {
				if (copy_from_user(kernel_bin_addr, (u8 __user *)bin_buf.buf + pkt_len * loop, pkt_len)) {
					kfree(kernel_bin_addr);
					return -EFAULT;
				}

				ret = bm_spi_flash_program(bmdi, kernel_bin_addr, bin_buf.target_addr + pkt_len * loop, pkt_len);
				if (ret != 0) {
					pr_err("bmdev spi flash program failed!\n");
					kfree(kernel_bin_addr);
					return ret;
				}
			}
			if (last_len != 0) {
				if (copy_from_user(kernel_bin_addr, (u8 __user *)bin_buf.buf + pkt_len * loop, last_len)) {
					kfree(kernel_bin_addr);
					return -EFAULT;
				}

				ret = bm_spi_flash_program(bmdi, kernel_bin_addr, bin_buf.target_addr + pkt_len * pkt_num, last_len);
				if (ret != 0) {
					pr_err("bmdev spi flash program failed!\n");
					kfree(kernel_bin_addr);
					return ret;
				}
			}

			bm_spi_enable_dmmr(bmdi);
			kfree(kernel_bin_addr);
			break;
		}

	case BMDEV_PROGRAM_MCU:
		{
			struct bin_buffer bin_buf;
			u8 *kernel_bin_addr;
			u8 *align_128_kernel_bin_addr;
			u32 size = 0;
			u8 *read_bin_addr;
			struct console_ctx ctx;
			ctx.uart.bmdi= bmdi;
			ctx.uart.uart_index = 0x2;

			if (copy_from_user(&bin_buf, (struct bin_buffer __user *)arg,
						sizeof(struct bin_buffer)))
				return -EFAULT;
			kernel_bin_addr = kmalloc(bin_buf.size, GFP_KERNEL);
			if (!kernel_bin_addr)
				return -ENOMEM;
			if (copy_from_user(kernel_bin_addr, (u8 __user *)bin_buf.buf,
						bin_buf.size)) {
				kfree(kernel_bin_addr);
				return -EFAULT;
			}

			if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
				(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
				(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
				if (bmdi->bmcd->sc5p_mcu_bmdi != NULL && bmdi->bmcd != NULL)
					ctx.uart.bmdi= bmdi->bmcd->sc5p_mcu_bmdi;
				if (bin_buf.size % 128 != 0)
					size = ((bin_buf.size / 128) + 1) *128;
				else
					size = bin_buf.size;

				align_128_kernel_bin_addr = kzalloc(size, GFP_KERNEL);
				if (!align_128_kernel_bin_addr) {
					kfree(kernel_bin_addr);
					return -ENOMEM;
				}

				memcpy(align_128_kernel_bin_addr, kernel_bin_addr, bin_buf.size);
				mutex_lock(&ctx.uart.bmdi->c_attr.attr_mutex);
				ret = console_cmd_download(&ctx, bin_buf.target_addr, align_128_kernel_bin_addr, size);
				mutex_unlock(&ctx.uart.bmdi->c_attr.attr_mutex);
				if (ret) {
					kfree(kernel_bin_addr);
					kfree(align_128_kernel_bin_addr);
					break;
				}

				msleep(1500);
				pr_info("mcu program offset 0x%x size 0x%x complete\n", bin_buf.target_addr,
					bin_buf.size);
				kfree(kernel_bin_addr);
				kfree(align_128_kernel_bin_addr);
				break;
			}

			ret = bm_mcu_program(bmdi, kernel_bin_addr, bin_buf.size, bin_buf.target_addr);
			if (ret) {
				kfree(kernel_bin_addr);
				break;
			}
			pr_info("mcu program offset 0x%x size 0x%x complete\n", bin_buf.target_addr,
					bin_buf.size);
			msleep(1500);

			read_bin_addr = kmalloc(bin_buf.size, GFP_KERNEL);
			if (!read_bin_addr) {
				kfree(kernel_bin_addr);
				return -ENOMEM;
			}
			ret = bm_mcu_read(bmdi, read_bin_addr, bin_buf.size, bin_buf.target_addr);
			if (!ret) {
				ret = memcmp(kernel_bin_addr, read_bin_addr, bin_buf.size);
				if (ret)
					pr_err("read after program mcu and check failed!\n");
				else
					pr_info("read after program mcu and check succeeds.\n");
			}
			kfree(kernel_bin_addr);
			kfree(read_bin_addr);
			break;
		}

	case BMDEV_CHECKSUM_MCU:
		{
			struct bin_buffer bin_buf;
			unsigned char cksum[16];

			if (copy_from_user(&bin_buf, (struct bin_buffer __user *)arg,
						sizeof(struct bin_buffer)))
				return -EFAULT;

			if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
				(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
				(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
				pr_err("bmsophon %d, sc5p not support mcu sheck sum\n", bmdi->dev_index);
				return -ENOSYS;
			}

			ret = bm_mcu_checksum(bmdi, bin_buf.target_addr, bin_buf.size, cksum);
			if (ret)
				return -EFAULT;
			if (copy_to_user((u8 __user *)bin_buf.buf, cksum, sizeof(cksum)))
				return -EFAULT;
			break;
		}

	case BMDEV_GET_BOOT_INFO:
		{
			struct bm_boot_info boot_info;

			if (bm_spi_flash_get_boot_info(bmdi, &boot_info))
				return -EFAULT;

			if (copy_to_user((struct bm_boot_info __user *)arg, &boot_info,
						sizeof(struct bm_boot_info)))
				return -EFAULT;

			break;
		}

	case BMDEV_UPDATE_BOOT_INFO:
		{
			struct bm_boot_info boot_info;

			if (copy_from_user(&boot_info, (struct bm_boot_info __user *)arg,
						sizeof(struct bm_boot_info)))
				return -EFAULT;

			if (bm_spi_flash_update_boot_info(bmdi, &boot_info))
				return -EFAULT;

			break;
		}

	case BMDEV_SET_REG:
		{
			struct bm_reg reg;

			if (copy_from_user(&reg, (struct bm_reg __user *)arg,
						sizeof(struct bm_reg)))
				return -EFAULT;

			if (bm_set_reg(bmdi, &reg))
				return -EFAULT;
			break;
		}

	case BMDEV_GET_REG:
		{
			struct bm_reg reg;

			if (copy_from_user(&reg, (struct bm_reg __user *)arg,
						sizeof(struct bm_reg)))
				return -EFAULT;

			if (bm_get_reg(bmdi, &reg))
				return -EFAULT;

			if (copy_to_user((struct bm_reg __user *)arg, &reg,
						sizeof(struct bm_reg)))
				return -EFAULT;

			break;
		}
	case BMDEV_GET_CORRECTN:
		ret = bm_get_correct_num(bmdi, arg);
		break;
	case BMDEV_GET_12V_ATX:
		ret = bm_get_12v_atx(bmdi, arg);
		break;
	case BMDEV_GET_SN:
		ret = bm_get_device_sn(bmdi, arg);
		break;
	case BMDEV_GET_TPU_MINCLK:
		ret = put_user(bmdi->boot_info.tpu_min_clk,(u32 __user *)arg);
		break;
	case BMDEV_GET_TPU_MAXCLK:
		ret = put_user(bmdi->boot_info.tpu_max_clk,(u32 __user *)arg);
		break;
	case BMDEV_SN:
		ret = bm_burning_info_sn(bmdi, arg);
		break;
	case BMDEV_MAC0:
		ret = bm_burning_info_mac(bmdi, 0, arg);
		break;
	case BMDEV_MAC1:
		ret = bm_burning_info_mac(bmdi, 1, arg);
		break;
	case BMDEV_BOARD_TYPE:
		ret = bm_burning_info_board_type(bmdi, arg);
		break;
#endif
	case BMDEV_GET_STATUS:
		ret = put_user(bmdi->status,(int __user *)arg);
		break;

	case BMDEV_GET_DRIVER_VERSION:
	{
		ret = put_user(BM_DRIVER_VERSION, (int __user *)arg);
		break;
	}

        case BMDEV_GET_BOARD_TYPE:
	{
		char board_name[25];
#ifdef SOC_MODE
 		switch(bmdi->cinfo.chip_id){
		case 0x1684:
			snprintf(board_name, 20, "1684-SOC");
			break;
		case 0x1686:
			snprintf(board_name, 20, "1684X-SOC");
			break;
		}
		ret = copy_to_user((char __user *)arg, board_name, sizeof(board_name));
#else
		char board_type[20];
		bm1684_get_board_type_by_id(bmdi, board_type, BM1684_BOARD_TYPE(bmdi));
		switch(bmdi->cinfo.chip_id){
		case 0x1684:
			snprintf(board_name, 25, "1684-%s", board_type);
			break;
		case 0x1686:
			snprintf(board_name, 25, "1684X-%s", board_type);
			break;
		}
		ret = copy_to_user((char __user *)arg, board_name, sizeof(board_name));
#endif
		break;
	}

	case BMDEV_GET_BOARDT:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		if (c_attr->bm_get_board_temp != NULL)
		  ret = put_user(c_attr->board_temp, (u32 __user *)arg);
		else
		  return -EFAULT;
		break;
	}

	case BMDEV_GET_CHIPT:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		if (c_attr->bm_get_chip_temp != NULL)
		  ret = put_user(c_attr->chip_temp, (u32 __user *)arg);
		else
		  return -EFAULT;
		break;
	}

	case BMDEV_GET_TPU_P:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		if (c_attr->bm_get_tpu_power != NULL) {
		  long power = c_attr->vdd_tpu_volt * c_attr->vdd_tpu_curr;
		  ret = put_user(power, (long __user *)arg);
		}
		else
		  return -EFAULT;
		break;
	}

	case BMDEV_GET_TPU_V:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		if (c_attr->bm_get_tpu_power != NULL) {
#ifndef SOC_MODE
		  ret = put_user(c_attr->vdd_tpu_volt, (u32 __user *)arg);
#endif
		}
		else
		  ret = put_user(ATTR_NOTSUPPORTED_VALUE, (u32 __user *)arg);;
		break;
	}

	case BMDEV_GET_CARD_ID:
	{
		struct bm_card *bmcd = bmdi->bmcd;
		if (put_user(bmcd->card_index, (u32 __user *)arg)) {
			return -EFAULT;
		}
		break;
	}

	case BMDEV_GET_DYNFREQ_STATUS:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		mutex_lock(&c_attr->attr_mutex);
		ret = copy_to_user((int __user *)arg, &bmdi->enable_dyn_freq, sizeof(int));
		mutex_unlock(&c_attr->attr_mutex);
		break;
	}

	case BMDEV_CHANGE_DYNFREQ_STATUS:
		{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		mutex_lock(&c_attr->attr_mutex);
		if (copy_from_user(&bmdi->enable_dyn_freq, (unsigned int __user *)arg, sizeof(int)));
		mutex_unlock(&c_attr->attr_mutex);
		break;
		}

	case BMDEV_ENABLE_PERF_MONITOR:
		{
			struct bm_perf_monitor perf_monitor;

			if (copy_from_user(&perf_monitor, (struct bm_perf_monitor __user *)arg,
						sizeof(struct bm_perf_monitor)))
				return -EFAULT;

			if (bmdev_enable_perf_monitor(bmdi, &perf_monitor))
				return -EFAULT;

			break;
		}

	case BMDEV_DISABLE_PERF_MONITOR:
		{
			struct bm_perf_monitor perf_monitor;

			if (copy_from_user(&perf_monitor, (struct bm_perf_monitor __user *)arg,
						sizeof(struct bm_perf_monitor)))
				return -EFAULT;

			if (bmdev_disable_perf_monitor(bmdi, &perf_monitor))
				return -EFAULT;

			break;
		}

	case BMDEV_GET_DEVICE_TIME:
		{
			unsigned long time_us = 0;

			time_us = bmdev_timer_get_time_us(bmdi);

			ret = copy_to_user((unsigned long __user *)arg,
					&time_us, sizeof(unsigned long));
			break;
		}

	case BMDEV_GET_PROFILE:
		{
			struct bm_thread_info *thd_info;
			pid_t api_pid;
			struct bm_handle_info *h_info;

			if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
				pr_err("bmdrv: file list is not found!\n");
				return -EINVAL;
			}

			mutex_lock(&bmdi->gmem_info.gmem_mutex);
			api_pid = current->pid;
			thd_info = bmdrv_find_thread_info(h_info, api_pid);

			if (!thd_info)
				ret = -EFAULT;
			else
				ret = copy_to_user((unsigned long __user *)arg,
						&thd_info->profile, sizeof(bm_profile_t));
			mutex_unlock(&bmdi->gmem_info.gmem_mutex);
			break;
		}
	case BMDEV_GET_VERSION:
		{
			ret = copy_to_user(((struct bootloader_version __user *)arg)->bl1_version, bmdi->cinfo.version.bl1_version, BL1_VERSION_SIZE);
			ret |= copy_to_user(((struct bootloader_version __user *)arg)->bl2_version, bmdi->cinfo.version.bl2_version, BL2_VERSION_SIZE);
			ret |= copy_to_user(((struct bootloader_version __user *)arg)->bl31_version, bmdi->cinfo.version.bl31_version, BL31_VERSION_SIZE);
			ret |= copy_to_user(((struct bootloader_version __user *)arg)->uboot_version, bmdi->cinfo.version.uboot_version, UBOOT_VERSION_SIZE);
			break;
		}
	case BMDEV_LOADED_LIB:
		{
			loaded_lib_t loaded_lib;
			struct bmcpu_lib *lib_temp, *lib_next, *lib_info = bmdi->lib_dyn_info;;

			ret = copy_from_user(&loaded_lib, (loaded_lib_t __user *)arg, sizeof(loaded_lib_t));
			if (ret) {
				pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
				return ret;
			}
			loaded_lib.loaded = 0;

			mutex_lock(&lib_info->bmcpu_lib_mutex);
			list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
				if(!memcmp(lib_temp->md5, loaded_lib.md5, 16)) {
					loaded_lib.loaded = 1;
					break;
				}
			}
			mutex_unlock(&lib_info->bmcpu_lib_mutex);
			ret = copy_to_user((loaded_lib_t __user *)arg, &loaded_lib, sizeof(loaded_lib_t));
			break;
		}
	case BMDEV_GET_DEV_STAT:
		{
			bm_dev_stat_t stat;
			struct bm_chip_attr *c_attr;

			c_attr = &bmdi->c_attr;
			stat.mem_total = (int)(bmdrv_gmem_total_size(bmdi)/1024/1024);
			stat.mem_used = stat.mem_total - (int)(bmdrv_gmem_avail_size(bmdi)/1024/1024);
			stat.tpu_util = c_attr->bm_get_npu_util(bmdi);
			bmdrv_heap_mem_used(bmdi, &stat);
			ret = copy_to_user((unsigned long __user *)arg, &stat, sizeof(bm_dev_stat_t));
			break;
		}

	case BMDEV_TRACE_ENABLE:
		ret = bmdev_trace_enable(bmdi, file);
		break;

	case BMDEV_TRACE_DISABLE:
		ret = bmdev_trace_disable(bmdi, file);
		break;

	case BMDEV_TRACEITEM_NUMBER:
		ret = bmdev_traceitem_number(bmdi, file, arg);
		break;

	case BMDEV_TRACE_DUMP:
		ret = bmdev_trace_dump_one(bmdi, file, arg);
		break;

	case BMDEV_TRACE_DUMP_ALL:
		ret = bmdev_trace_dump_all(bmdi, file, arg);
		break;

	case BMDEV_GET_MISC_INFO:
		ret = copy_to_user((unsigned long __user *)arg,	&bmdi->misc_info,
				sizeof(struct bm_misc_info));
		break;

        case BMDEV_GET_TPUC:
                {
                       struct bm_chip_attr *c_attr = &bmdi->c_attr;

                       if (c_attr->bm_get_tpu_power != NULL) {
                         ret = copy_to_user((u32 __user *)arg, &c_attr->vdd_tpu_curr, sizeof(u32));
                       } else {
                         return -EFAULT;
                       }
                       break;
                }

        case BMDEV_GET_MAXP:
                {
                       struct bm_boot_info *boot_info = &bmdi->boot_info;

                       ret = copy_to_user((unsigned int __user *)arg, &boot_info->max_board_power, sizeof(unsigned int));
                       break;
                }

        case BMDEV_GET_BOARDP:
                {
                       struct bm_device_info *c_bmdi;
                       struct bm_chip_attr *c_attr;

                       if (bmdi->bmcd != NULL) {
                         if (bmdi->bmcd->card_bmdi[0] != NULL) {
                           c_bmdi = bmdi->bmcd->card_bmdi[0];
                           c_attr = &c_bmdi->c_attr;
                         } else {
                           return -EFAULT;
                         }
                       } else {
                         return -EFAULT;
                       }

                       if(c_attr->bm_get_board_power != NULL) {
                         ret = copy_to_user((u32 __user *)arg, &c_attr->board_power, sizeof(u32));
                       } else {
                         return -EFAULT;
                       }
                       break;
                }

        case BMDEV_GET_FAN:
                {
                       struct bm_chip_attr *c_attr = &bmdi->c_attr;

                       if (c_attr->bm_get_fan_speed != NULL) {
                         ret = copy_to_user((u16 __user *)arg, &c_attr->fan_speed, sizeof(u16));
                       } else {
                         return -EFAULT;
                       }
                       break;
                }

	case BMDEV_SET_TPU_DIVIDER:
		if (bmdi->misc_info.pcie_soc_mode == BM_DRV_SOC_MODE)
			ret = -EPERM;
		else {
			mutex_lock(&bmdi->clk_reset_mutex);
			ret = bmdev_clk_ioctl_set_tpu_divider(bmdi, arg);
			mutex_unlock(&bmdi->clk_reset_mutex);
		}
		break;

	case BMDEV_SET_TPU_FREQ:
		mutex_lock(&bmdi->clk_reset_mutex);
		ret = bmdev_clk_ioctl_set_tpu_freq(bmdi, arg);
		mutex_unlock(&bmdi->clk_reset_mutex);
		break;

	case BMDEV_GET_TPU_FREQ:
		mutex_lock(&bmdi->clk_reset_mutex);
		ret = bmdev_clk_ioctl_get_tpu_freq(bmdi, arg);
		mutex_unlock(&bmdi->clk_reset_mutex);
		break;

	case BMDEV_SET_MODULE_RESET:
		if (bmdi->misc_info.pcie_soc_mode == BM_DRV_SOC_MODE)
			ret = -EPERM;
		else {
			mutex_lock(&bmdi->clk_reset_mutex);
			ret = bmdev_clk_ioctl_set_module_reset(bmdi, arg);
			mutex_unlock(&bmdi->clk_reset_mutex);
		}
		break;

#ifndef SOC_MODE
	case BMDEV_BASE64_PREPARE:
		{
			struct ce_base  test_base;

			switch (bmdi->cinfo.chip_id) {
			case 0x1682:
				pr_info("bm1682 not supported!\n");
				break;
			case 0x1684:
			case 0x1686:
				ret = copy_from_user(&test_base, (struct ce_base *)arg,
						sizeof(struct ce_base));
				if (ret) {
					pr_err("s2d failed\n");
					return -EFAULT;
				}
				base64_prepare(bmdi, test_base);
				break;
			}
			break;
		}

	case BMDEV_BASE64_START:
		{
			switch (bmdi->cinfo.chip_id) {
			case 0x1682:
				pr_info("bm1682 not supported!\n");
				break;
			case 0x1684:
			case 0x1686:
				ret = 0;
				base64_start(bmdi);
				if (ret)
					return -EFAULT;
				pr_info("setting ready\n");
				break;
			}
			break;
		}

	case BMDEV_BASE64_CODEC:
		{
			struct ce_base test_base;

			switch (bmdi->cinfo.chip_id) {
			case 0x1682:
				pr_info("bm1682 not supported!\n");
				break;
			case 0x1684:
			case 0x1686:
				ret = copy_from_user(&test_base, (struct ce_base *)arg,
						sizeof(struct ce_base));
				if (ret) {
					pr_err("s2d failed\n");
					return -EFAULT;
				}
				mutex_lock(&bmdi->spaccdrvctx.spacc_mutex);
				base64_prepare(bmdi, test_base);
				base64_start(bmdi);
				mutex_unlock(&bmdi->spaccdrvctx.spacc_mutex);
				if (ret)
					return -EFAULT;
				break;
			}
			break;
		}

#endif

#ifdef SOC_MODE
	case BMDEV_INVALIDATE_GMEM:
		{
			u64 arg64;
			u32 arg32l, arg32h;

			if (get_user(arg64, (u64 __user *)arg)) {
				dev_dbg(bmdi->dev, "cmd 0x%x get user failed\n", cmd);
				return -EINVAL;
			}
			arg32l = (u32)arg64;
			arg32h = (u32)((arg64 >> 32) & 0xffffffff);
			bmdrv_gmem_invalidate(bmdi, ((unsigned long)arg32h)<<6, arg32l);
			break;
		}

	case BMDEV_FLUSH_GMEM:
		{
			u64 arg64;
			u32 arg32l, arg32h;

			if (get_user(arg64, (u64 __user *)arg)) {
				dev_dbg(bmdi->dev, "cmd 0x%x get user failed\n", cmd);
				return -EINVAL;
			}

			arg32l = (u32)arg64;
			arg32h = (u32)((arg64 >> 32) & 0xffffffff);
			bmdrv_gmem_flush(bmdi, ((unsigned long)arg32h)<<6, arg32l);
			break;
		}
	case BMDEV_GMEM_ADDR:
		{
			struct bm_gmem_addr addr;

			if (copy_from_user(&addr, (struct bm_gmem_addr __user *)arg,
						sizeof(struct bm_gmem_addr)))
				return -EFAULT;
			if (bmdrv_gmem_vir_to_phy(bmdi, &addr))
				return -EFAULT;

			if (copy_to_user((struct bm_gmem_addr __user *)arg, &addr,
						sizeof(struct bm_gmem_addr)))
				return -EFAULT;

			break;
		}
#endif

	default:
		dev_err(bmdi->dev, "*************Invalid ioctl parameter************\n");
		return -EINVAL;
	}

	return ret;
}

static long bmdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)file->private_data;
	int ret = 0;

	if ((_IOC_TYPE(cmd)) == BMDEV_IOCTL_MAGIC)
		ret = bm_ioctl(file, cmd, arg);
#ifndef SOC_MODE
	else if ((_IOC_TYPE(cmd)) == VDI_IOCTL_MAGIC)
		ret = bm_vpu_ioctl(file, cmd, arg);
	else if ((_IOC_TYPE(cmd)) == JDI_IOCTL_MAGIC)
		ret = bm_jpu_ioctl(file, cmd, arg);
#endif
	else {
		dev_dbg(bmdi->dev, "Unknown cmd 0x%x\n", cmd);
		return -EINVAL;
	}
	return ret;
}

static int bmdev_ctl_open(struct inode *inode, struct file *file)
{
	struct bm_ctrl_info *bmci;

	bmci = container_of(inode->i_cdev, struct bm_ctrl_info, cdev);
	file->private_data = bmci;
	return 0;
}

static int bmdev_ctl_close(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long bmdev_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bm_ctrl_info *bmci = file->private_data;
	int ret = 0;

	switch (cmd) {
	case BMCTL_GET_DEV_CNT:
		ret = put_user(bmci->dev_count, (int __user *)arg);
		break;

	case BMCTL_GET_SMI_ATTR:
		ret = bmctl_ioctl_get_attr(bmci, arg);
		break;

	case BMCTL_GET_PROC_GMEM:
		ret = bmctl_ioctl_get_proc_gmem(bmci, arg);
		break;

	case BMCTL_SET_LED:
#ifndef SOC_MODE
		ret = bmctl_ioctl_set_led(bmci, arg);
#endif
		break;

	case BMCTL_SET_ECC:
#ifndef SOC_MODE
		ret = bmctl_ioctl_set_ecc(bmci, arg);
#endif
		break;

		/* test i2c1 slave */
	case BMCTL_TEST_I2C1:
		//	ret = bmctl_test_i2c1(bmci, arg);
		break;

	case BMCTL_DEV_RECOVERY:
		ret = -EPERM;
#ifndef SOC_MODE
		ret = bmctl_ioctl_recovery(bmci, arg);
#endif
		break;

	case BMCTL_GET_DRIVER_VERSION:
		ret = put_user(BM_DRIVER_VERSION, (int __user *)arg);
		break;

        case BMCTL_GET_CARD_NUM:
        {
		int card_num = 0;
		card_num = bm_get_card_num_from_system();
		ret = put_user(card_num, (int __user *)arg);
		break;
	}

	case BMCTL_GET_CARD_INFO:
	{
		struct bm_card bmcd;
		if (copy_from_user(&bmcd, (struct bm_card __user *)arg, sizeof(struct bm_card))) {
			return -EINVAL;
		}
		ret = bm_get_card_info(&bmcd);
		if (ret != 0) {
			return -ENODEV;
		}
		ret = copy_to_user((struct bm_card __user *)arg, &bmcd, sizeof(struct bm_card));
		break;
	}

	default:
		pr_err("*************Invalid ioctl parameter************\n");
		return -EINVAL;
	}
	return ret;
}

static const struct file_operations bmdev_fops = {
	.open = bmdev_open,
	.read = bmdev_read,
	.write = bmdev_write,
	.fasync = bmdev_fasync,
	.release = bmdev_close,
	.unlocked_ioctl = bmdev_ioctl,
	.mmap = bmdev_mmap,
	.owner = THIS_MODULE,
};

static const struct file_operations bmdev_ctl_fops = {
	.open = bmdev_ctl_open,
	.release = bmdev_ctl_close,
	.unlocked_ioctl = bmdev_ctl_ioctl,
	.owner = THIS_MODULE,
};

int bmdev_register_device(struct bm_device_info *bmdi)
{
	bmdi->devno = MKDEV(MAJOR(bm_devno_base), MINOR(bm_devno_base) + bmdi->dev_index);
	bmdi->dev = device_create(bmdrv_class_get(), bmdi->parent, bmdi->devno, NULL,
			"%s%d", BM_CDEV_NAME, bmdi->dev_index);

	cdev_init(&bmdi->cdev, &bmdev_fops);

	bmdi->cdev.owner = THIS_MODULE;
	cdev_add(&bmdi->cdev, bmdi->devno, 1);

	dev_set_drvdata(bmdi->dev, bmdi);
	dev_dbg(bmdi->dev, "%s\n", __func__);
	return 0;
}

int bmdev_unregister_device(struct bm_device_info *bmdi)
{
	dev_dbg(bmdi->dev, "%s\n", __func__);
	cdev_del(&bmdi->cdev);
	device_destroy(bmdrv_class_get(), bmdi->devno);
	return 0;
}

int bmdev_ctl_register_device(struct bm_ctrl_info *bmci)
{
	bmci->devno = MKDEV(MAJOR(bm_ctl_devno_base), MINOR(bm_ctl_devno_base));
	bmci->dev = device_create(bmdrv_class_get(), NULL, bmci->devno, NULL,
			"%s", BMDEV_CTL_NAME);
	cdev_init(&bmci->cdev, &bmdev_ctl_fops);
	bmci->cdev.owner = THIS_MODULE;
	cdev_add(&bmci->cdev, bmci->devno, 1);

	dev_set_drvdata(bmci->dev, bmci);

	return 0;
}

int bmdev_ctl_unregister_device(struct bm_ctrl_info *bmci)
{
	cdev_del(&bmci->cdev);
	device_destroy(bmdrv_class_get(), bmci->devno);
	return 0;
}
