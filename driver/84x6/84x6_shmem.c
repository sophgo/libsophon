#ifndef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": %s:%d: " fmt, __func__, __LINE__
#endif
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/page-flags.h>
#include <linux/mm_types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>

#include "84x6_shmem.h"
#define SHMEM_ALIGN_TO_4KB(addr) PAGE_ALIGN(addr)

#define SHMEM_IS_4KB_ALIGNED(addr) (((phys_addr_t)(addr) & (PAGE_SIZE - 1)) == 0)



struct bmdev_mem_handle {
    unsigned long phys_addr;
    size_t size;
    unsigned int start_block;
    unsigned int block_count;
    bool valid;
};

struct bmtpu_shm_region {
    phys_addr_t base_addr;
    size_t size;
    bool bmdev_initialized;
    unsigned int bmdev_total_blocks;
    unsigned long *bmdev_bitmap;
    struct bmdev_mem_handle bmdev_handles[BM_SHARED_MAX_HANDLES];
    spinlock_t mem_lock;
    struct mutex init_mutex;
    unsigned long handle_bitmap[BM_SHARED_MAX_HANDLES / BITS_PER_LONG + 1];

    void __iomem *shmem_vaddr;
    bool is_mapped;
};


static inline struct bmtpu_shm_region *get_shm_region(struct bm_device_info
                                                      *bmdi)
{
    if (!bmdi || !bmdi->pvShmemInfo) {
        pr_err("BMDev: Invalid bmdi or pvShmemInfo\n");
        return NULL;
    }
    return (struct bmtpu_shm_region *)bmdi->pvShmemInfo;
}

int bmdev_get_shm_addr_from_dts(struct bm_device_info *bmdi)
{
    struct device *dev = bmdi->cinfo.device;
    struct device_node *np = dev->of_node;
    struct device_node *shm_np;
    struct resource res;
    struct bmtpu_shm_region *shm_region;
    phys_addr_t original_addr, aligned_addr;
    int ret = 0;
    size_t size_loss;
    size_t adjusted_size;

    shm_region = get_shm_region(bmdi);
    if (!shm_region) {
        dev_err(dev, "shm_region is NULL\n");
        return -ENODEV;
    }
    shm_np = of_parse_phandle(np, "memory-region", 2);
    if (!shm_np) {
        dev_err(dev, "Failed to get shared memory node\n");
        return -ENODEV;
    }

    ret = of_address_to_resource(shm_np, 0, &res);
    if (ret) {
        dev_err(dev, "Failed to get shared memory resource\n");
        of_node_put(shm_np);
        return ret;
    }
    of_node_put(shm_np);

    original_addr = res.start;

    dev_info(dev, "Original physical address from DTS: 0x%llx\n",
             (u64) original_addr);
    if (!SHMEM_IS_4KB_ALIGNED(original_addr)) {
        dev_warn(dev,
                 "DTS physical address is not 4KB aligned. Adjusting...\n");

        aligned_addr = SHMEM_ALIGN_TO_4KB(original_addr);
        dev_info(dev, "Adjusted physical address to: 0x%llx\n",
                 (u64) aligned_addr);
        if (aligned_addr > original_addr) {
            size_loss = aligned_addr - original_addr;
            adjusted_size = resource_size(&res) - size_loss;
            dev_info(dev,
                     "Address adjustment resulted in a size loss of %zu bytes.\n",
                     size_loss);
            dev_info(dev, "Original size: %llu, Adjusted size: %zu\n",
                     resource_size(&res), adjusted_size);

            if (adjusted_size < BM_SHARED_TOTAL_MEM_SIZE) {
                dev_err(dev,
                        "Adjusted size %zu is less than required %u. Memory region too small or misaligned.\n",
                        adjusted_size, BM_SHARED_TOTAL_MEM_SIZE);
                return -ENOMEM;
            }
            shm_region->size = BM_SHARED_TOTAL_MEM_SIZE;
        } else {
            shm_region->size = BM_SHARED_TOTAL_MEM_SIZE;
        }
    } else {
        aligned_addr = original_addr;
        dev_info(dev, "DTS physical address is properly 4KB aligned.\n");
        shm_region->size = BM_SHARED_TOTAL_MEM_SIZE;
    }

    shm_region->base_addr = aligned_addr;

    if (!SHMEM_IS_4KB_ALIGNED(shm_region->base_addr)) {
        dev_err(dev,
                "CRITICAL: Final base address 0x%llx is still not 4KB aligned!\n",
                (u64) shm_region->base_addr);
        return -EINVAL;
    }

    dev_info(dev,
             "TPU Shared Memory Final: phys=0x%llx, size=%zu (4KB aligned)\n",
             (u64) shm_region->base_addr, shm_region->size);

    return 0;
}

int bmdev_shared_mem_init(struct bm_device_info *bmdi)
{
    int i;
    int ret = 0;
    struct bmtpu_shm_region *shm_region;

    if (!bmdi) {
        pr_err("BMDev: bmdi is NULL\n");
        return -EINVAL;
    }

    bmdi->pvShmemInfo = kzalloc(sizeof(struct bmtpu_shm_region), GFP_KERNEL);
    if (!bmdi->pvShmemInfo) {
        pr_err("BMDev: Failed to allocate shm_region\n");
        return -ENOMEM;
    }

    shm_region = get_shm_region(bmdi);

    spin_lock_init(&shm_region->mem_lock);
    mutex_init(&shm_region->init_mutex);

    mutex_lock(&shm_region->init_mutex);

    if (shm_region->bmdev_initialized) {
        pr_warn("BMDev: Shared memory already initialized\n");
        mutex_unlock(&shm_region->init_mutex);
        kfree(bmdi->pvShmemInfo);
        bmdi->pvShmemInfo = NULL;
        return -EBUSY;
    }

#ifndef SOC_MODE
    /* PCIe: no of_node/DTS and device addr cannot be host-ioremap'ed.
     * The BAR1 inbound window (part 20 -> dev 0x24000000) is set up by
     * bm84x6_map_bar() before io_init(); io_init() already resolved
     * shmem_base_addr into io_bar_vaddr.shmem_bar_vaddr. */
    shm_region->base_addr = bmdi->cinfo.bm_reg->shmem_base_addr;
    shm_region->size = BM_SHARED_TOTAL_MEM_SIZE;
#else
    ret = bmdev_get_shm_addr_from_dts(bmdi);
    if (ret) {
        pr_err("BMDev: Failed to get shared memory address from DTS: %d\n",
               ret);
        mutex_unlock(&shm_region->init_mutex);
        kfree(bmdi->pvShmemInfo);
        bmdi->pvShmemInfo = NULL;
        return ret;
    }
#endif

    pr_info("BMDev shared memory init: physical base 0x%llx, size %zu bytes\n",
            (u64) shm_region->base_addr, shm_region->size);

    shm_region->bmdev_total_blocks = shm_region->size / BM_SHARED_BLOCK_SIZE;
    if (shm_region->bmdev_total_blocks == 0) {
        pr_err("BMDev: No blocks available after size calculation\n");
        mutex_unlock(&shm_region->init_mutex);
        kfree(bmdi->pvShmemInfo);
        bmdi->pvShmemInfo = NULL;
        return -EINVAL;
    }

    shm_region->bmdev_bitmap =
        kcalloc(BITS_TO_LONGS(shm_region->bmdev_total_blocks),
                sizeof(unsigned long), GFP_KERNEL);
    if (!shm_region->bmdev_bitmap) {
        pr_err("BMDev: Failed to allocate bitmap for %u blocks\n",
               shm_region->bmdev_total_blocks);
        mutex_unlock(&shm_region->init_mutex);
        kfree(bmdi->pvShmemInfo);
        bmdi->pvShmemInfo = NULL;
        return -ENOMEM;
    }
#ifndef SOC_MODE
    /* PCIe: vaddr belongs to the BAR1 mapping owned by pci_deinit, not us */
    shm_region->shmem_vaddr =
        bmdi->cinfo.bar_info.io_bar_vaddr.shmem_bar_vaddr;
    if (!shm_region->shmem_vaddr) {
        pr_err("BMDev: shmem BAR1 window not mapped (dev 0x%llx)\n",
               (u64) shm_region->base_addr);
        kfree(shm_region->bmdev_bitmap);
        mutex_unlock(&shm_region->init_mutex);
        kfree(bmdi->pvShmemInfo);
        bmdi->pvShmemInfo = NULL;
        return -ENOMEM;
    }
    shm_region->is_mapped = true;
#else
    //must be none cached
    shm_region->shmem_vaddr = ioremap(shm_region->base_addr, shm_region->size);
    if (!shm_region->shmem_vaddr) {
        pr_err("BMDev: Failed to ioremap shared memory region at 0x%llx\n",
               (u64) shm_region->base_addr);
        kfree(shm_region->bmdev_bitmap);
        mutex_unlock(&shm_region->init_mutex);
        kfree(bmdi->pvShmemInfo);
        bmdi->pvShmemInfo = NULL;
        return -ENOMEM;
    }
    shm_region->is_mapped = true;
#endif
    pr_info("BMDev: Shared memory remapped at virtual address: %p\n",
            shm_region->shmem_vaddr);

    for (i = 0; i < BM_SHARED_MAX_HANDLES; i++) {
        shm_region->bmdev_handles[i].valid = false;
        shm_region->bmdev_handles[i].phys_addr = 0;
        shm_region->bmdev_handles[i].size = 0;
        shm_region->bmdev_handles[i].start_block = 0;
        shm_region->bmdev_handles[i].block_count = 0;
    }

    bitmap_zero(shm_region->handle_bitmap, BM_SHARED_MAX_HANDLES);
    shm_region->bmdev_initialized = true;

    pr_info
        ("BMDev: Shared memory initialized. Total blocks: %u, block size: %u, total size: %zu bytes\n",
         shm_region->bmdev_total_blocks, BM_SHARED_BLOCK_SIZE,
         shm_region->size);

    mutex_unlock(&shm_region->init_mutex);
    return ret;
}


void bmdev_shared_mem_exit(struct bm_device_info *bmdi)
{
    int i, j;
    unsigned long flags;
    int leaked_handles = 0;
    unsigned int block_index;
    struct bmtpu_shm_region *shm_region;

    if (!bmdi) {
        pr_err("BMDev: bmdi is NULL\n");
        return;
    }

    shm_region = get_shm_region(bmdi);
    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return;
    }

    mutex_lock(&shm_region->init_mutex);

    if (!shm_region->bmdev_initialized) {
        pr_warn("BMDev: Shared memory not initialized\n");
        goto out_unlock;
    }

    spin_lock_irqsave(&shm_region->mem_lock, flags);
    for (i = 0; i < BM_SHARED_MAX_HANDLES; i++) {
        if (shm_region->bmdev_handles[i].valid) {
            leaked_handles++;
            pr_warn
                ("BMDev: Handle %d not freed at module exit (phys_addr: 0x%lx, size: %zu)\n",
                 i, shm_region->bmdev_handles[i].phys_addr,
                 shm_region->bmdev_handles[i].size);
            for (j = 0; j < shm_region->bmdev_handles[i].block_count; j++) {
                block_index = shm_region->bmdev_handles[i].start_block + j;
                if (block_index < shm_region->bmdev_total_blocks) {
                    clear_bit(block_index, shm_region->bmdev_bitmap);
                }
            }
        }
    }

    if (shm_region->is_mapped && shm_region->shmem_vaddr) {
#ifdef SOC_MODE
        iounmap(shm_region->shmem_vaddr);
#endif
        /* PCIe: shmem_vaddr is the BAR1 window vaddr, released by pci_deinit */
        shm_region->shmem_vaddr = NULL;
        shm_region->is_mapped = false;
    }

    shm_region->base_addr = 0;
    shm_region->size = 0;
    shm_region->bmdev_total_blocks = 0;

    shm_region->bmdev_initialized = false;
    spin_unlock_irqrestore(&shm_region->mem_lock, flags);

    if (leaked_handles > 0) {
        pr_warn("BMDev: %d handles leaked during module exit\n",
                leaked_handles);
    }

    if (shm_region->bmdev_bitmap) {
        kfree(shm_region->bmdev_bitmap);
        shm_region->bmdev_bitmap = NULL;
    }

    kfree(bmdi->pvShmemInfo);
    bmdi->pvShmemInfo = NULL;

    pr_info("BMDev: Shared memory module exited\n");

  out_unlock:
    mutex_unlock(&shm_region->init_mutex);
}


static int bmdev_alloc_handle_id(struct bm_device_info *bmdi)
{
    unsigned long flags;
    int handle_id;
    struct bmtpu_shm_region *shm_region = get_shm_region(bmdi);

    if (!shm_region)
        return -ENODEV;

    spin_lock_irqsave(&shm_region->mem_lock, flags);
    handle_id =
        find_first_zero_bit(shm_region->handle_bitmap, BM_SHARED_MAX_HANDLES);
    if (handle_id < BM_SHARED_MAX_HANDLES) {
        set_bit(handle_id, shm_region->handle_bitmap);
    } else {
        handle_id = -ENOMEM;
    }
    spin_unlock_irqrestore(&shm_region->mem_lock, flags);

    return handle_id;
}

static void bmdev_free_handle_id(struct bm_device_info *bmdi, int handle_id)
{
    unsigned long flags;
    struct bmtpu_shm_region *shm_region = get_shm_region(bmdi);

    if (!shm_region)
        return;
    if (handle_id < 0 || handle_id >= BM_SHARED_MAX_HANDLES)
        return;

    spin_lock_irqsave(&shm_region->mem_lock, flags);
    clear_bit(handle_id, shm_region->handle_bitmap);
    spin_unlock_irqrestore(&shm_region->mem_lock, flags);
}

static bool bmdev_validate_handle(struct bm_device_info *bmdi, int handle)
{
    struct bmtpu_shm_region *shm_region;
    unsigned long flags;
    bool ret = false;

    if (!bmdi) {
        pr_err("BMDev: bmdi is NULL\n");
        return false;
    }

    shm_region = get_shm_region(bmdi);
    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return false;
    }

    spin_lock_irqsave(&shm_region->mem_lock, flags);

    if (handle < 0 || handle >= BM_SHARED_MAX_HANDLES) {
        pr_debug("BMDev: Handle ID %d out of range\n", handle);
        goto out_unlock;
    }

    if (!shm_region->bmdev_handles[handle].valid) {
        pr_debug("BMDev: Handle ID %d is invalid\n", handle);
        goto out_unlock;
    }

    if (shm_region->bmdev_handles[handle].start_block >=
        shm_region->bmdev_total_blocks) {
        pr_debug("BMDev: Start block %u out of range\n",
                 shm_region->bmdev_handles[handle].start_block);
        goto out_unlock;
    }

    if (shm_region->bmdev_handles[handle].block_count == 0 ||
        shm_region->bmdev_handles[handle].start_block +
        shm_region->bmdev_handles[handle].block_count >
        shm_region->bmdev_total_blocks) {
        pr_debug("BMDev: Block count %u invalid for start block %u\n",
                 shm_region->bmdev_handles[handle].block_count,
                 shm_region->bmdev_handles[handle].start_block);
        goto out_unlock;
    }

    if (shm_region->bmdev_handles[handle].phys_addr < shm_region->base_addr ||
        shm_region->bmdev_handles[handle].phys_addr >=
        shm_region->base_addr + shm_region->size) {
        pr_debug("BMDev: Physical address 0x%lx out of range\n",
                 shm_region->bmdev_handles[handle].phys_addr);
        goto out_unlock;
    }

    if ((shm_region->bmdev_handles[handle].phys_addr) % BM_SHARED_BLOCK_SIZE !=
        0) {
        pr_debug("BMDev: Physical address 0x%lx not aligned to block size %u\n",
                 shm_region->bmdev_handles[handle].phys_addr,
                 BM_SHARED_BLOCK_SIZE);
        goto out_unlock;
    }

    ret = true;

  out_unlock:
    spin_unlock_irqrestore(&shm_region->mem_lock, flags);
    return ret;
}


int bmdev_shared_mem_alloc(struct bm_device_info *bmdi, size_t size)
{
    unsigned long flags;
    unsigned int blocks_needed, start_block = 0;
    int i, j, free_blocks = 0;
    int handle_id = -1;
    struct bmtpu_shm_region *shm_region;

    if (!bmdi) {
        pr_err("BMDev: bmdi is NULL\n");
        return -EINVAL;
    }

    shm_region = get_shm_region(bmdi);
    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return -ENODEV;
    }

    if (size == 0) {
        pr_warn("BMDev: Allocation size is 0\n");
        return -EINVAL;
    }

    if (size > SIZE_MAX - BM_SHARED_BLOCK_SIZE) {
        pr_warn("BMDev: Requested size too large: %zu\n", size);
        return -EINVAL;
    }

    blocks_needed = (size + BM_SHARED_BLOCK_SIZE - 1) / BM_SHARED_BLOCK_SIZE;
    if (blocks_needed == 0) {
        pr_err("BMDev: Calculated blocks needed is 0 for size %zu\n", size);
        return -EINVAL;
    }

    if (blocks_needed > shm_region->bmdev_total_blocks) {
        pr_err
            ("BMDev: Requested size %zu needs %u blocks, but only %u available\n",
             size, blocks_needed, shm_region->bmdev_total_blocks);
        return -ENOMEM;
    }

    spin_lock_irqsave(&shm_region->mem_lock, flags);

    for (i = 0; i <= shm_region->bmdev_total_blocks - blocks_needed; i++) {
        free_blocks = 0;
        for (j = 0; j < blocks_needed; j++) {
            if (!test_bit(i + j, shm_region->bmdev_bitmap)) {
                free_blocks++;
            } else {
                break;
            }
        }
        if (free_blocks == blocks_needed) {
            start_block = i;
            break;
        }
    }

    if (free_blocks != blocks_needed) {
        spin_unlock_irqrestore(&shm_region->mem_lock, flags);
        pr_warn
            ("BMDev: No contiguous free block of size %zu found (needed %u blocks)\n",
             size, blocks_needed);
        return -ENOMEM;
    }

    handle_id =
        find_first_zero_bit(shm_region->handle_bitmap, BM_SHARED_MAX_HANDLES);
    if (handle_id >= BM_SHARED_MAX_HANDLES) {
        spin_unlock_irqrestore(&shm_region->mem_lock, flags);
        pr_warn("BMDev: No available handle ID\n");
        return -ENOMEM;
    }
    set_bit(handle_id, shm_region->handle_bitmap);

    for (j = 0; j < blocks_needed; j++) {
        set_bit(start_block + j, shm_region->bmdev_bitmap);
    }

    shm_region->bmdev_handles[handle_id].phys_addr =
        shm_region->base_addr + (start_block * BM_SHARED_BLOCK_SIZE);
    shm_region->bmdev_handles[handle_id].size =
        blocks_needed * BM_SHARED_BLOCK_SIZE;
    shm_region->bmdev_handles[handle_id].start_block = start_block;
    shm_region->bmdev_handles[handle_id].block_count = blocks_needed;
    shm_region->bmdev_handles[handle_id].valid = true;

    spin_unlock_irqrestore(&shm_region->mem_lock, flags);

    pr_debug("BMDev: Allocated %zu bytes, handle %d, phys_addr 0x%lx\n",
             size, handle_id, shm_region->bmdev_handles[handle_id].phys_addr);
    return handle_id;
}

void bmdev_shared_mem_free(struct bm_device_info *bmdi, int handle)
{
    unsigned long flags;
    int i;
    struct bmtpu_shm_region *shm_region;

    if (!bmdi) {
        pr_err("BMDev: bmdi is NULL\n");
        return;
    }

    shm_region = get_shm_region(bmdi);
    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return;
    }

    if (!bmdev_validate_handle(bmdi, handle)) {
        spin_unlock_irqrestore(&shm_region->mem_lock, flags);
        pr_err("BMDev: Invalid handle ID: %d\n", handle);
        return;
    }

    spin_lock_irqsave(&shm_region->mem_lock, flags);

    for (i = 0; i < shm_region->bmdev_handles[handle].block_count; i++) {
        unsigned int block_index =
            shm_region->bmdev_handles[handle].start_block + i;
        if (block_index < shm_region->bmdev_total_blocks) {
            clear_bit(block_index, shm_region->bmdev_bitmap);
        } else {
            pr_warn("BMDev: Block index %u out of range during free\n",
                    block_index);
        }
    }

    pr_debug("BMDev: Freed handle %d, phys_addr 0x%lx, size %zu\n",
             handle, shm_region->bmdev_handles[handle].phys_addr,
             shm_region->bmdev_handles[handle].size);

    shm_region->bmdev_handles[handle].valid = false;
    shm_region->bmdev_handles[handle].phys_addr = 0;
    shm_region->bmdev_handles[handle].size = 0;
    shm_region->bmdev_handles[handle].start_block = 0;
    shm_region->bmdev_handles[handle].block_count = 0;

    spin_unlock_irqrestore(&shm_region->mem_lock, flags);
    bmdev_free_handle_id(bmdi, handle);
}

unsigned long bmdev_shared_mem_get_phys_addr(struct bm_device_info *bmdi,
                                             int handle)
{
    unsigned long phys_addr;
    unsigned long flags;
    struct bmtpu_shm_region *shm_region;

    if (!bmdi) {
        pr_err("BMDev: bmdi is NULL\n");
        return 0;
    }

    shm_region = get_shm_region(bmdi);
    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return 0;
    }

    if (!bmdev_validate_handle(bmdi, handle)) {
        spin_unlock_irqrestore(&shm_region->mem_lock, flags);
        pr_err("BMDev: Invalid handle ID: %d\n", handle);
        return 0;
    }

    spin_lock_irqsave(&shm_region->mem_lock, flags);
    phys_addr = shm_region->bmdev_handles[handle].phys_addr;
    spin_unlock_irqrestore(&shm_region->mem_lock, flags);

    return phys_addr;
}


unsigned long bm84x6_shmem_get_offset(struct bm_device_info *bmdi,
                                          int handle)
{
    unsigned long offset = 0;
    unsigned long flags;
    struct bmtpu_shm_region *shm_region;

    if (!bmdi) {
        pr_err("BMDev: bmdi is NULL\n");
        return 0;
    }

    shm_region = get_shm_region(bmdi);
    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return 0;
    }

    if (!bmdev_validate_handle(bmdi, handle)) {
        spin_unlock_irqrestore(&shm_region->mem_lock, flags);
        pr_err("BMDev: Invalid handle ID: %d\n", handle);
        return 0;
    }

    spin_lock_irqsave(&shm_region->mem_lock, flags);
    offset =
        shm_region->bmdev_handles[handle].phys_addr - shm_region->base_addr;
    spin_unlock_irqrestore(&shm_region->mem_lock, flags);

    return offset;
}


size_t bmdev_shared_mem_get_size(struct bm_device_info *bmdi, int handle)
{
    size_t size;
    unsigned long flags;
    struct bmtpu_shm_region *shm_region;

    if (!bmdi) {
        pr_err("BMDev: bmdi is NULL\n");
        return 0;
    }

    shm_region = get_shm_region(bmdi);
    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return 0;
    }

    if (!bmdev_validate_handle(bmdi, handle)) {
        spin_unlock_irqrestore(&shm_region->mem_lock, flags);
        pr_err("BMDev: Invalid handle ID: %d\n", handle);
        return 0;
    }

    spin_lock_irqsave(&shm_region->mem_lock, flags);
    size = shm_region->bmdev_handles[handle].size;
    spin_unlock_irqrestore(&shm_region->mem_lock, flags);

    return size;
}

void bmdev_shared_mem_get_info(struct bm_device_info *bmdi, size_t *total,
                               size_t *used, size_t *free_size)
{
    unsigned long flags;
    unsigned int used_blocks = 0;
    int active_handles = 0;
    int i;
    struct bmtpu_shm_region *shm_region;

    if (!bmdi) {
        pr_err("BMDev: bmdi is NULL\n");
        return;
    }

    shm_region = get_shm_region(bmdi);
    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return;
    }

    if (total)
        *total = shm_region->size;

    spin_lock_irqsave(&shm_region->mem_lock, flags);

    for (i = 0; i < shm_region->bmdev_total_blocks; i++) {
        if (test_bit(i, shm_region->bmdev_bitmap)) {
            used_blocks++;
        }
    }

    active_handles =
        bitmap_weight(shm_region->handle_bitmap, BM_SHARED_MAX_HANDLES);
    spin_unlock_irqrestore(&shm_region->mem_lock, flags);

    if (used)
        *used = used_blocks * BM_SHARED_BLOCK_SIZE;
    if (free_size)
        *free_size = shm_region->size - (used_blocks * BM_SHARED_BLOCK_SIZE);

    pr_debug("BMDev: Memory info: used_blocks=%u, active_handles=%d\n",
             used_blocks, active_handles);
}

void bm84x6_shmem_reg_write(struct bm_device_info *bmdi, u32 reg_offset,
                                u32 val)
{
    struct bmtpu_shm_region *shm_region = get_shm_region(bmdi);
    unsigned long offset;

    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return;
    }

    if (!shm_region->is_mapped) {
        pr_err("BMDev: Shared memory not mapped\n");
        return;
    }

    offset = (unsigned long)reg_offset *sizeof(u32);
    if (offset >= shm_region->size) {
        pr_err("BMDev: Offset 0x%lx >= size 0x%zx\n", offset, shm_region->size);
        return;
    }

    iowrite32(val, shm_region->shmem_vaddr + offset);
}


u32 bm84x6_shmem_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    struct bmtpu_shm_region *shm_region = get_shm_region(bmdi);
    unsigned long offset;

    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return 0;
    }

    if (!shm_region->is_mapped) {
        pr_err("BMDev: Shared memory not mapped\n");
        return 0;
    }

    offset = (unsigned long)reg_offset *sizeof(u32);
    if (offset >= shm_region->size) {
        pr_err("BMDev: Register offset 0x%lx out of range (size: 0x%zx)\n",
               offset, shm_region->size);
        return 0;
    }

    return ioread32(shm_region->shmem_vaddr + offset);
}


/* packet_addr: raw value from packet_info->packet_addr (before << 10)
 * phys_out: output physical address (base_addr + (packet_addr << 10))
 * buf: buffer to fill with memory content
 * buf_len: bytes to read (will be rounded down to multiple of 4)
 * Returns: 0 on success, negative on error
 */
int bm84x6_shmem_read_packet(struct bm_device_info *bmdi, u32 packet_addr,
                                 phys_addr_t * phys_out, void *buf,
                                 size_t buf_len)
{
    struct bmtpu_shm_region *shm_region = get_shm_region(bmdi);
    unsigned long offset;
    size_t i;

    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return -ENODEV;
    }
    if (!shm_region->is_mapped) {
        pr_err("BMDev: Shared memory not mapped\n");
        return -ENODEV;
    }

    offset = (unsigned long)packet_addr << 10;
    if (offset >= shm_region->size) {
        pr_err("BMDev: packet_addr 0x%x -> offset 0x%lx >= size 0x%zx\n",
               packet_addr, offset, shm_region->size);
        return -EINVAL;
    }

    if (phys_out)
        *phys_out = shm_region->base_addr + offset;

    if (buf && buf_len > 0) {
        buf_len &= ~(sizeof(u32) - 1);  /* align to 4 bytes */
        for (i = 0; i < buf_len; i += sizeof(u32)) {
            if (offset + i >= shm_region->size)
                break;
            ((u32 *) buf)[i / sizeof(u32)] =
                ioread32(shm_region->shmem_vaddr + offset + i);
        }
    }
    return 0;
}

int bm84x6_shmem_read_packet_at(struct bm_device_info *bmdi,
                                    u32 packet_addr, size_t byte_offset,
                                    void *buf, size_t buf_len)
{
    struct bmtpu_shm_region *shm_region = get_shm_region(bmdi);
    unsigned long offset;
    size_t i;

    if (!shm_region || !shm_region->is_mapped)
        return -ENODEV;

    offset = ((unsigned long)packet_addr << 10) + byte_offset;
    if (offset >= shm_region->size)
        return -EINVAL;

    if (buf && buf_len > 0) {
        buf_len &= ~(sizeof(u32) - 1);
        for (i = 0; i < buf_len && offset + i < shm_region->size;
             i += sizeof(u32))
            ((u32 *) buf)[i / sizeof(u32)] =
                ioread32(shm_region->shmem_vaddr + offset + i);
    }
    return 0;
}

int bmdev_copy_to_shmem(struct bm_device_info *bmdi, unsigned int phys_addr,
                        bm_kapi_header_t * api_header_p, bm_api_t * bm_api_p,
                        bm_kapi_opt_header_t * api_opt_header_p,
                        bool api_from_userspace)
{
    struct bmtpu_shm_region *shm_region = get_shm_region(bmdi);
    int offset, cur_wp, next_wp, idx, msg_buf;
    u32 word_size;
    int ret = 0;
    u32 header_size;

    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return -EINVAL;
    }

    offset = phys_addr - shm_region->base_addr;
    if (offset < 0 || offset > (int)shm_region->size) {
        pr_err("ERROR invalid phys_addr:0x%x, base_addr:0x%llx, size:0x%zx\n",
               phys_addr, (u64) shm_region->base_addr, shm_region->size);
        return -EINVAL;
    }

    cur_wp = offset / sizeof(u32);

    bm84x6_shmem_reg_write(bmdi, cur_wp, api_header_p->api_id);
    next_wp = cur_wp + offsetof(bm_kapi_header_t, api_size) / sizeof(u32);
    bm84x6_shmem_reg_write(bmdi, next_wp, api_header_p->api_size);
    next_wp = cur_wp + offsetof(bm_kapi_header_t, api_handle) / sizeof(u32);
    bm84x6_shmem_reg_write(bmdi, next_wp, (u32) (api_header_p->api_handle));
    next_wp = next_wp + 1;
    bm84x6_shmem_reg_write(bmdi, next_wp,
                               (u32) (api_header_p->api_handle >> 32));
    next_wp = cur_wp + offsetof(bm_kapi_header_t, api_seq) / sizeof(u32);
    bm84x6_shmem_reg_write(bmdi, next_wp, api_header_p->api_seq);
    next_wp = cur_wp + offsetof(bm_kapi_header_t, duration) / sizeof(u32);
    bm84x6_shmem_reg_write(bmdi, next_wp, 0);
    next_wp = cur_wp + offsetof(bm_kapi_header_t, result) / sizeof(u32);
    bm84x6_shmem_reg_write(bmdi, next_wp, 0);

    if (api_opt_header_p != NULL) {
        next_wp = cur_wp + sizeof(bm_kapi_header_t) / sizeof(u32);
        bm84x6_shmem_reg_write(bmdi, next_wp,
                                   (u32) (api_opt_header_p->global_api_seq >>
                                          32));
        next_wp = next_wp + 1;
        bm84x6_shmem_reg_write(bmdi, next_wp,
                                   (u32) (api_opt_header_p->global_api_seq));
        next_wp = next_wp + 1;
        bm84x6_shmem_reg_write(bmdi, next_wp,
                                   (u32) (api_opt_header_p->api_data >> 32));
        next_wp = next_wp + 1;
        bm84x6_shmem_reg_write(bmdi, next_wp,
                                   (u32) (api_opt_header_p->api_data));
        header_size = sizeof(bm_kapi_header_t) + sizeof(bm_kapi_opt_header_t);
    } else {
        header_size = sizeof(bm_kapi_header_t);
    }

    word_size = api_header_p->api_size + header_size / sizeof(u32);

    if (api_header_p->api_id == BM_API_QUIT)
        return 0;

    for (idx = 0; idx < api_header_p->api_size; idx++) {
        next_wp = cur_wp + header_size / sizeof(u32) + idx;
        if (api_from_userspace) {
            ret = get_user(msg_buf, (u32 __user *) (bm_api_p->api_addr) + idx);
            if (ret) {
                pr_err("copy_from_user fail, addr %p\n",
                       (u32 __user *) (bm_api_p->api_addr) + idx);
                return -EFAULT;
            }
        } else {
            msg_buf = *((u32 *) (bm_api_p->api_addr) + idx);
        }
        bm84x6_shmem_reg_write(bmdi, next_wp, msg_buf);
    }

    return 0;
}


int bmdev_shmem_phys_to_file(struct bm_device_info *bmdi, int mem_handle,
                             const char *filename)
{
    struct file *filp = NULL;
    loff_t pos = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0)
    mm_segment_t old_fs;
#endif
    bm_kapi_header_t api_header;
    u32 *data_buffer = NULL;
    u32 total_size_bytes = 0;
    u32 header_size_bytes = sizeof(bm_kapi_header_t);
    u32 idx;
    int retval = -EFAULT;
    ssize_t write_ret;
    unsigned int phys_addr;
    int offset;
    u32 cur_rp;
    u32 value;
    struct bmtpu_shm_region *shm_region;

    if (!bmdi || !filename) {
        pr_err("BMDev: Invalid parameters: bmdi=%p, filename=%p\n", bmdi,
               filename);
        return -EINVAL;
    }

    shm_region = get_shm_region(bmdi);
    if (!shm_region) {
        pr_err("BMDev: shm_region is NULL\n");
        return -ENODEV;
    }

    phys_addr = bmdev_shared_mem_get_phys_addr(bmdi, mem_handle);
    if (phys_addr == 0) {
        pr_err("BMDev: Failed to get physical address for handle %d\n",
               mem_handle);
        return -EINVAL;
    }

    offset = phys_addr - shm_region->base_addr;
    if (offset < 0 || offset >= shm_region->size) {
        pr_err
            ("BMDev: phys_addr 0x%x out of shared memory range (base:0x%llx, size:0x%zx)\n",
             phys_addr, (u64) shm_region->base_addr, shm_region->size);
        return -EINVAL;
    }

    if (phys_addr % sizeof(u32) != 0) {
        pr_err("BMDev: phys_addr 0x%x not aligned to 4 bytes\n", phys_addr);
        return -EINVAL;
    }

    cur_rp = offset / sizeof(u32);

    pr_debug("BMDev: Reading API header from phys_addr 0x%x\n", phys_addr);

    for (idx = 0; idx < header_size_bytes / sizeof(u32); idx++) {
        value = bm84x6_shmem_reg_read(bmdi, cur_rp + idx);
        memcpy((u8 *) & api_header + idx * sizeof(u32), &value, sizeof(u32));
    }

    if (api_header.api_size > shm_region->size - header_size_bytes) {
        pr_err("BMDev: Invalid api_size %u in header\n", api_header.api_size);
        retval = -EINVAL;
        goto out;
    }

    total_size_bytes = header_size_bytes + api_header.api_size;

    if ((offset + total_size_bytes) > shm_region->size) {
        pr_err("BMDev: Data size %u bytes exceeds shared memory boundary\n",
               total_size_bytes);
        retval = -EFAULT;
        goto out;
    }

    data_buffer = kzalloc(total_size_bytes, GFP_KERNEL);
    if (!data_buffer) {
        pr_err("BMDev: Failed to allocate buffer of size %u\n",
               total_size_bytes);
        retval = -ENOMEM;
        goto out;
    }

    for (idx = 0; idx < total_size_bytes / sizeof(u32); idx++) {
        value = bm84x6_shmem_reg_read(bmdi, cur_rp + idx);
        data_buffer[idx] = value;
    }

    pr_debug
        ("BMDev: Successfully read %u bytes from phys_addr 0x%x, api_id=%u, api_size=%u\n",
         total_size_bytes, phys_addr, api_header.api_id, api_header.api_size);

    filp = filp_open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (IS_ERR(filp)) {
        retval = PTR_ERR(filp);
        pr_err("BMDev: Failed to open file %s: %d\n", filename, retval);
        goto cleanup;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0)
    old_fs = get_fs();
    set_fs(KERNEL_DS);
#endif
    write_ret = kernel_write(filp, data_buffer, total_size_bytes, &pos);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0)
    set_fs(old_fs);
#endif

    if (write_ret != total_size_bytes) {
        pr_err("BMDev: Failed to write to file %s: wrote %zd/%u bytes\n",
               filename, write_ret, total_size_bytes);
        retval = (write_ret < 0) ? write_ret : -EIO;
        filp_close(filp, NULL);
        goto cleanup;
    }

    if (filp_close(filp, NULL)) {
        pr_err("BMDev: Warning: failed to close file %s\n", filename);
        retval = -EIO;
        goto cleanup;
    }
    filp = NULL;

    pr_info("BMDev: Successfully wrote %u bytes from phys_addr 0x%x to %s\n",
            total_size_bytes, phys_addr, filename);
    retval = total_size_bytes;

  cleanup:
    if (data_buffer) {
        kfree(data_buffer);
    }
    if (filp && !IS_ERR(filp)) {
        filp_close(filp, NULL);
    }
  out:
    return retval;
}
