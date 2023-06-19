#include <stdio.h>
#include <stdlib.h>
#ifdef VIDEO_MMU_RAND_PTE
#include <time.h>
#endif /* VIDEO_MMU_RAND_PTE */
#include <malloc.h>
#include <stdint.h>
// #include "cache.h"
//#include "sysdma.h"

#include "vdi_mmu.h"

/*****************************************************************************/
/* MACRO AND DEFINE                                                          */
/*****************************************************************************/
typedef struct vpu_iommu_entry vpu_iommu_entry_t;
#define REG(x)      (*(volatile unsigned int *)(x))

#if   (defined VIDEO_MMU_PAGE_4K)
    #define WMMU_PCR_PS         WMMU_PCR_4K
#elif (defined VIDEO_MMU_PAGE_32K )
    #define WMMU_PCR_PS         WMMU_PCR_32K
#elif (defined VIDEO_MMU_PAGE_64K )
    #define WMMU_PCR_PS         WMMU_PCR_64K
#elif (defined VIDEO_MMU_PAGE_128K )
    #define WMMU_PCR_PS         WMMU_PCR_128K
#else
    #error "!!YOU MUST DEFINE PAGE SIZE MACRO FIRST!!"
#endif

/*****************************************************************************/
/* STATIC VARIABLE                                                           */
/*****************************************************************************/
static vpu_iommu_pt      s_vpu_system_mmu[8];

/*****************************************************************************/
/* STATIC FUNCTION                                                           */
/*****************************************************************************/
extern void *memset(void *dst, int val, size_t count);
extern void inv_dcache_range(unsigned long start, unsigned long size);
static void wmmu_write_register(int system_id, unsigned int addr, unsigned int data)
{
    unsigned int wmmu_base;
	unsigned int *reg_addr;
         if(0==system_id) wmmu_base =  WMMU_SYSTEM00_BASE;
    else if(1==system_id) wmmu_base =  WMMU_SYSTEM01_BASE;
    else if(2==system_id) wmmu_base =  WMMU_SYSTEM10_BASE;
    else if(3==system_id) wmmu_base =  WMMU_SYSTEM11_BASE;
    else if(4==system_id) wmmu_base =  WMMU_ENCODER_BASE;
    else {printf("invaid system id"); exit(1);}
    
	reg_addr = (unsigned int *)(addr + (unsigned long)wmmu_base);
	*(volatile unsigned int *)reg_addr = data;
    flush_dcache_range((long unsigned int)reg_addr, sizeof(unsigned int));
}

static unsigned int wmmu_read_register(int system_id, unsigned int addr)
{
    unsigned int wmmu_base;
	unsigned int *reg_addr;
         if(0==system_id) wmmu_base =  WMMU_SYSTEM00_BASE;
    else if(1==system_id) wmmu_base =  WMMU_SYSTEM01_BASE;
    else if(2==system_id) wmmu_base =  WMMU_SYSTEM10_BASE;
    else if(3==system_id) wmmu_base =  WMMU_SYSTEM11_BASE;
    else if(4==system_id) wmmu_base =  WMMU_ENCODER_BASE;
    else {printf("invaid system id"); exit(1);}

	reg_addr = (unsigned int *)(addr + (unsigned long)wmmu_base);
    inv_dcache_range((long unsigned int)reg_addr, sizeof(unsigned int));
	return *(volatile unsigned int *)reg_addr;
}

#ifdef VIDEO_MMU_REMAP
static void wmmu_ramap_pt(int system_id)
{
    unsigned long pt_size   = (VDI_DRAM_DATA_SIZE/VDI_PAGE_SIZE*4);
    if(pt_size > VDI_LRAM_PHYSICAL_SIZE) pt_size = VDI_LRAM_PHYSICAL_SIZE;
    
    /* the position of page talbe in local ram. */
    unsigned long damp_addr = (system_id&0x2) ? VDI_LRAM_PHYSICAL_BASE1 : VDI_LRAM_PHYSICAL_BASE0;
    damp_addr += pt_size;
    wmmu_write_register(system_id, WMMU_DAMP,   (unsigned int)(damp_addr & 0xFFFFFFFF));
    wmmu_write_register(system_id, WMMU_DAMP+4, (unsigned int)((damp_addr >> 32) & 0xFFFFFFFF));
    printf("[JF::DEBUG] WMMU DAMP = %p\n", (unsigned long *)damp_addr);
    
    unsigned long dama_addr = (system_id&0x2) ? VDI_DRAM_PT_BASE1 : VDI_DRAM_PT_BASE0;
    dama_addr += pt_size;
    wmmu_write_register(system_id, WMMU_DAMA,   (unsigned int)(damp_addr & 0xFFFFFFFF));
    wmmu_write_register(system_id, WMMU_DAMA+4, (unsigned int)((damp_addr >> 32) & 0xFFFFFFFF));
    printf("[JF::DEBUG] WMMU DAMA = %p\n", (unsigned long *)dama_addr);
    
    return;
}

static unsigned int wmmu_dma_pt( int system_id )
{
#if (defined VIDEO_SYSTEM0)
    #define MEM_SRC_BLOCK   VDI_DRAM_PT_BASE0
    #define MEM_DST_BLOCK   VDI_LRAM_PHYSICAL_BASE0
#elif (defined VIDEO_SYSTEM1 )
    #define MEM_SRC_BLOCK   VDI_DRAM_PT_BASE1    
    #define MEM_DST_BLOCK   VDI_LRAM_PHYSICAL_BASE1
#else
    #error "!!YOU MUST DEFINE CHANNEL0 or CHANNEL1 FIRST!!"
#endif
    #define MEM_BLOCK_SIZE  (VDI_DRAM_DATA_SIZE/VDI_PAGE_SIZE*4)
	int ret = 0;
	struct dma_chan *chan = NULL;
    int dma_size = MEM_BLOCK_SIZE;
    if(dma_size > VDI_LRAM_PHYSICAL_SIZE) dma_size = VDI_LRAM_PHYSICAL_SIZE;    
    struct slave_config mem2mem_cfg = {
      .direction        = DMA_MEM_TO_MEM,
      .src_addr         = MEM_SRC_BLOCK,
      .dst_addr         = MEM_DST_BLOCK,
      .data_width       = SLAVE_BUSWIDTH_4_BYTES,
      .src_addr_width   = SLAVE_BUSWIDTH_4_BYTES,
      .dst_addr_width   = SLAVE_BUSWIDTH_4_BYTES,
      .src_msize        = DW_DMA_MSIZE_32,
      .dst_msize        = DW_DMA_MSIZE_32,
      .device_fc        = DW_DMA_FC_D_M2M,
      .src_master       = DMA0,
      .dst_master       = DMA0,
      .len              = dma_size/4
    };
    
    printf("[VJF::DEBUG] Initial %d System Local RAM\n", system_id);
    
	chan = malloc(sizeof(*chan));
	if(!chan) 
    {
		printf("chan mem alloc failed \n");
        return 1;
    }

	chan->chan_id   = 0; // channel 0
	chan->config    = &mem2mem_cfg;
	chan->direction = DMA_MEM_TO_MEM;

	ret = device_alloc_chan(chan);
	if(ret < 0)
		return ret;

	ret = device_prep_dma_slave(chan);
	if(ret < 0)
		return ret;

	ret = device_issue_pending(chan);
	if(ret < 0)
		return ret;
	printf("after device_issue_pending : 0x50001d08 = 0x%x, 0x50001d0c = 0x%x\n",
           REG(0x50001d08), REG(0x50001d0c));
    
	// ret = check_dst_memory((void *)chan->config->src_addr,
	//	(void *)chan->config->dst_addr, chan->config->len);
        
    if( chan ) {
        free( chan );
        chan = NULL;
    }
	if(ret) {
		return ret;
	}
	return 0;    
}
#endif /* VIDEO_MMU_REMAP */

static void build_vpu_iommu(int system_id)
{
    int i                   = 0;
    unsigned long pt_addr   = (system_id&0x2) ? VDI_DRAM_PT_BASE1 : VDI_DRAM_PT_BASE0;
    unsigned long mem_addr  = (system_id&0x2) ? VDI_DRAM_DATA_BASE1 : VDI_DRAM_DATA_BASE0;
    unsigned int  mem_size  = VDI_DRAM_DATA_SIZE;
    unsigned int  page_size = VDI_PAGE_SIZE;
    unsigned int  entry_num = 0;

    vpu_iommu_pt *vpu_iommu_pt = &(s_vpu_system_mmu[system_id]);
    memset(vpu_iommu_pt, 0, sizeof(vpu_iommu_pt));

    entry_num                = mem_size / page_size;
    vpu_iommu_pt->alloc_ptr  = pt_addr;
    vpu_iommu_pt->entry_num  = entry_num;
    vpu_iommu_pt->enabled    = 0;
    vpu_iommu_pt->entry_base = (vpu_iommu_entry_t *)pt_addr;
    memset(vpu_iommu_pt->entry_base, 0, VDI_DRAM_PT_SIZE);
    for(i = 0; i < entry_num; i++)
    {
        unsigned int phy_addr = (unsigned int)(((mem_addr & VDI_DRAM_DATA_WIDTH) >> VDI_PAGE_SHIFT) & VDI_PAGE_MASK);
        vpu_iommu_pt->entry_base[i].paddr = phy_addr;
        vpu_iommu_pt->entry_base[i].dma_task_end = 1;
        vpu_iommu_pt->entry_base[i].valid = 1;
        mem_addr += VDI_PAGE_SIZE;
    }

#ifdef VIDEO_MMU_RAND_PTE
    for(i=entry_num-1; i>=0; --i)
    {
        int j = rand() % (i+1);
        struct vpu_iommu_entry temp = vpu_iommu_pt->entry_base[i];
        vpu_iommu_pt->entry_base[i] = vpu_iommu_pt->entry_base[j];
        vpu_iommu_pt->entry_base[j] = temp;
    }    
#endif /* VIDEO_MMU_RAND_PTE */

#ifdef VIDEO_MMU_REMAP
    /* trans page table to local ram */
    wmmu_dma_pt(system_id);

    /* set local ram range and remap address */
    wmmu_ramap_pt(system_id);
#endif /* VIDEO_MMU_REMAP */

    vpu_iommu_pt->enabled   = 1;
   
    return;
}

static int wmmu_enable_channel(int system_id, int channel )
{
    printf("[WMMU] Enable WMMU System %d, channel %d ..........\n", system_id, channel);
    
    /* setting page table address at sdram, same as ptr */
    unsigned long wmmu_mem = (system_id&0x2)? VDI_DRAM_PT_BASE1 : VDI_DRAM_PT_BASE0;  
    unsigned long reg_offset = 0x100 * channel;
    wmmu_write_register(system_id, WMMU_TTBR+reg_offset,   (unsigned int)(wmmu_mem & 0xFFFFFFFF));
    wmmu_write_register(system_id, WMMU_TTBR+reg_offset+4, (unsigned int)((wmmu_mem >> 32) & 0xFFFFFFFF));

    /* virtual address range */
    wmmu_mem = (system_id&0x2) ? VDI_DRAM_MVA_BASE1 : VDI_DRAM_MVA_BASE0;
    
    wmmu_mem += VDI_DRAM_DATA_SIZE-1;
    wmmu_write_register(system_id, WMMU_TTER+reg_offset,   (unsigned int)(wmmu_mem & 0xFFFFFFFF));
    wmmu_write_register(system_id, WMMU_TTER+reg_offset+4, (unsigned int)((wmmu_mem >> 32) & 0xFFFFFFFF));

    /* config page size */
    wmmu_write_register(system_id, WMMU_PCR+reg_offset, WMMU_PCR_PS);
    printf("JPEG MMU PAGE SIZE = %d\n", WMMU_PCR_PS);

    /* WMMU enable, disable remap and interrupt function */
#ifdef VIDEO_MMU_REMAP
    wmmu_write_register(system_id, WMMU_SCR+reg_offset, WMMU_SCR_ENABLE | WMMU_SCR_REMAP_ENABLE);
#else
    wmmu_write_register(system_id, WMMU_SCR+reg_offset, WMMU_SCR_ENABLE);
#endif /* VIDEO_MMU_REMAP */

    return 0;
}

static int wmmu_enable(int system_id)
{
    printf("[WMMU] Enable WMMU System %d ..........\n", system_id);

    // clear page table in WMMU cache
    wmmu_write_register(system_id, WMMU_IR, 1);

    //check cache status for page table
    while ( 0x1!=wmmu_read_register(system_id, WMMU_IS))
    {
        printf("there is page table in WMMU cache ..., need invalid\n");
    }

#ifdef VIDEO_MULT_CHAN
    wmmu_enable_channel(system_id, 1);
    wmmu_write_register(system_id, WMMU_GSCR, 1);
#else
    wmmu_enable_channel(system_id, 0);
#endif /* VIDEO_MULT_CHAN */

    return 0;
}

static int wmmu_disable( int system_id )
{
    printf("Disable WMMU.......\n");

    //clear interrupt
    wmmu_write_register(system_id, WMMU_ICR, (1 << 0));

    //clear page table in WMMU cache
    wmmu_write_register(system_id, WMMU_IR, (1 << 0));

    //check cache status for page table
    while (( wmmu_read_register(system_id, WMMU_IS) & 0x1) != 0x1)
    {
        printf("there is page table in WMMU cache...need invalid\n");
    }

#ifdef VIDEO_MULT_CHAN
    wmmu_write_register(system_id, WMMU_GSCR, 0);
#endif /* VIDEO_MULT_CHAN */

    /* disable interrupt */
    wmmu_write_register(system_id, WMMU_SCR, WMMU_SCR_INTERRUPT_MASK);

    return 0;
}

static int wmmu_invalidate( int system_id )
{
    printf("Invalid WMMU.......\n");

    //wmmu is invalide
    if ((wmmu_read_register(system_id, WMMU_IS) & 0x1) == 0x1)
        return 0;

    /* must make sure jpu or vpu is idle */

    // check cache status for page table
    while (( wmmu_read_register(system_id, WMMU_IS) & 0x1) != 0x1)
    {
        printf("there is page table in WMMU cache...need invalid\n");

        // clear page table in WMMU cache
        wmmu_write_register(system_id, WMMU_IR, (1 << 0));
    }

    return 0;
}

void init_vpu_iommu(int system_id)
{
    wmmu_disable(system_id);
    vpu_iommu_pt * system_mmu = &s_vpu_system_mmu[system_id];
    memset(system_mmu, 0, sizeof(vpu_iommu_pt));
    
#ifndef VIDEO_MMU
    return;
#endif

    build_vpu_iommu( system_id);

    /* enable mmu channel operator */
    wmmu_enable(system_id);
}

void uninit_vpu_iommu( int system_id )
{
    wmmu_disable(system_id);
    wmmu_invalidate(system_id);

    vpu_iommu_pt * system_mmu = &(s_vpu_system_mmu[system_id]);
    memset(system_mmu, 0, sizeof(vpu_iommu_pt));
}

unsigned int wmmu_irq( int system_id, int channel )
{
    printf("WMMU system %d, channel %d irq.......\n", system_id, channel);

    unsigned long reg_offset = 0x100 * channel;
    unsigned int status = wmmu_read_register(system_id, WMMU_FSR+reg_offset);//fault status register
    if( status & WMMU_FSR_WRITE_FAULT)
    {
        if (status & WMMU_FSR_WRITE_FAULT)
            printf("WMMU write page invalid fault: 0x%x, addr-hi 0x%x,addr-low 0x%x\n",
            status,
            wmmu_read_register(system_id, WMMU_FARW+reg_offset+4),
            wmmu_read_register(system_id, WMMU_FARW+reg_offset));
        else
            printf("WMMU write page cross the border fault: 0x%x, addr-hi 0x%x, addr-low 0x%x\n",
            status,
            wmmu_read_register(system_id, WMMU_FARW+reg_offset+4),
            wmmu_read_register(system_id, WMMU_FARW+reg_offset));
    }
    if( status & WMMU_FSR_READ_FAULT)
    {
        if (status & WMMU_FSR_READ_FAULT)
            printf("WMMU read page invalid fault: 0x%x, addr-hi 0x%x, addr-low 0x%x\n",
            status,
            wmmu_read_register(system_id, WMMU_FARR+reg_offset+4),
            wmmu_read_register(system_id, WMMU_FARR+reg_offset));
        else
            printf("WMMU read page cross the border fault: 0x%x, addr-hi 0x%x, addr-low 0x%x\n",
            status,
            wmmu_read_register(system_id, WMMU_FARR+reg_offset+4),
            wmmu_read_register(system_id, WMMU_FARR+reg_offset));
    }

    // clear interrupt
    wmmu_write_register(system_id, WMMU_ICR+reg_offset, (1 << 0));
    return 0;
}

#if (defined VIDEO_MMU)
unsigned long get_mmu_phyaddr(int system_id, unsigned long addr)
{
    vpu_iommu_pt *vpu_iommu_pt = &(s_vpu_system_mmu[system_id]);
    
    unsigned int  page_size = VDI_PAGE_SIZE;
    unsigned int pte_index   = addr / page_size;    
    unsigned long physic_addr= vpu_iommu_pt->entry_base[pte_index].paddr;

    physic_addr = ((physic_addr & VDI_PAGE_MASK) << VDI_PAGE_SHIFT) & VDI_DRAM_DATA_WIDTH;

    return physic_addr;
}
#endif /* (defined VIDEO_MMU) && (defined VIDEO_MMU_RAND_PTE) */
