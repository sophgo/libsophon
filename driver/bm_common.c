#include "bm_common.h"

char *base_get_chip_id(struct bm_device_info *bmdi)
{
	uint32_t val = 0;
	char *ret = NULL;

	val = (otp_reg_read(bmdi, 0x00002014) & 0x7);
	if (val == 0 || val == 7)
	{
		ret = "SGTPUV8-SOC";
	}
	else
	{
		ret = "SGTPUV8-SOC";
	}

	return ret;
}

/**
 * Convert a user-space virtual address to a physical address.
 * @param addr: The user-space virtual address to convert.
 * @return: The corresponding physical address, or 0 if conversion fails.
 */
unsigned long user_virtual_to_physical(unsigned long addr)
{
    struct mm_struct *mm = current->mm;
    struct page *page = NULL;
    unsigned long phys_addr = 0;
    unsigned long pfn;
    unsigned int offset;
    int ret;


    if (!is_user_address(addr)) {
        pr_err("Address is not in user space\n");
        return 0;
    }


    if (!mm) {
        pr_err("Current process memory descriptor is NULL\n");
        return 0;
    }


    ret = get_user_pages(addr, 1, FOLL_WRITE, &page, NULL);
    if (ret < 1) {
        pr_err("Failed to get user pages: %d\n", ret);
        return 0;
    }


    offset = addr & (PAGE_SIZE - 1);
    pfn = page_to_pfn(page);
    phys_addr = (pfn << PAGE_SHIFT) | offset;

    put_page(page);

    return phys_addr;
}

/**
 * Check if the address is a valid user-space address.
 */
static inline bool is_user_address(unsigned long addr)
{
    return addr < TASK_SIZE;
}

int bmdev_get_phys_addr(struct bm_device_info *bmdi, struct file *file, unsigned long arg){
		int ret=0;
		unsigned long phys_addr;
		struct virAddrToPhyAddr v2p;
		// void *virt_addr;

		// PR_TRACE("test_________________________\n");
		// virt_addr = kmalloc(sizeof(int), GFP_KERNEL);

    // phys_addr = virt_to_phys(virt_addr);
		// PR_TRACE("get_phys_addr1*************%lu\n",phys_addr);
		// phys_addr = get_phys_addr((unsigned long)virt_addr);
		// PR_TRACE("get_phys_addr2*************%lu\n",phys_addr);

		ret=copy_from_user(&v2p, (struct virAddrToPhyAddr __user *)arg, sizeof(struct virAddrToPhyAddr));
		if(ret== -EFAULT){
			PR_TRACE("copy_from_user.........\n");
			return -EFAULT;
		}
		PR_TRACE("yuanlaide addr......%llu\n",v2p.virAddr);
		phys_addr = user_virtual_to_physical(v2p.virAddr);

		v2p.phyAddr = phys_addr;
		
		PR_TRACE("get_phys_addr222......%lu\n",phys_addr);

		ret = copy_to_user((struct virAddrToPhyAddr __user *)arg, &v2p, sizeof(struct virAddrToPhyAddr));
		if(ret== -EFAULT){
			PR_TRACE("put_user.........\n");
			return -EFAULT;
		}
		return ret;
}

int kill_user_thread(long pid_id) {
	struct pid *pid_struct;
	struct task_struct *task;


	pid_struct = find_get_pid(pid_id);
	if (!pid_struct) {
			printk(KERN_INFO "Process ID %ld does not exist.\n", pid_id);
			return -1;
	}


	task = pid_task(pid_struct, PIDTYPE_PID);
	if (!task) {
			printk(KERN_INFO "Process ID %ld does not exist.\n", pid_id);
			put_pid(pid_struct);
			return -1;
	}


	send_sig(SIGTERM, task, 1);
	printk(KERN_INFO "Sent SIGTERM to process ID %ld.\n", pid_id);


	put_pid(pid_struct);
	return 0;
}
