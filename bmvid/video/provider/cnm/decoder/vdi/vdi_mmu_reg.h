
#ifndef _VDI_MMU_REG_H_
#define _VDI_MMU_REG_H_

/* wmmu system 0 and 1 address */
#define WMMU_SYSTEM00_BASE   (0x50113000)
#define WMMU_SYSTEM01_BASE   (0x50114000)
#define WMMU_SYSTEM10_BASE   (0x50123000)
#define WMMU_SYSTEM11_BASE   (0x50124000)
#define WMMU_ENCODER_BASE    (0x50136000)

#define WMMU_FARR           (0x00)          // read fault VA
#define WMMU_FARW           (0x08)          // write fault VA
#define WMMU_FSR            (0x10)          // fault reason
#define WMMU_ICR            (0x18)          // write 1 to clear interrupt
#define WMMU_IR             (0x20)          // write 1 to clear prefetched page entries
#define WMMU_IS             (0x28)          // 1: all prefetched page entires are cleared; 0: prefetched entries remain
#define WMMU_TTBR           (0x30)          // page table start address
#define WMMU_TTER           (0x38)          // max VA for translation
#define WMMU_SCR            (0x40)          // control
#define WMMU_PCR            (0x48)          // page configuration register: 0: 4K, 1: 32K, 2: 64K, 3: 128K; default: 0
#define WMMU_DAMP           (0x50)          // WMMU descriptor addr remap patten,size 512bit
#define WMMU_DAMA           (0x58)          // WMMU descriptor addr remap addr,size512bit
#define WMMU_GSCR           (0xFC)          // WMMU global system control register

#define WMMU_FSR_WRITE_FAULT_TYPE (1 << 3) // 1: invalid page entry; 0: cross boundary (input VA too large)
#define WMMU_FSR_WRITE_FAULT      (1 << 2)
#define WMMU_FSR_READ_FAULT_TYPE  (1 << 1) // 1: invalid page entry; 0: cross boundary (input VA too large)
#define WMMU_FSR_READ_FAULT       (1 << 0)

#define WMMU_SCR_ENABLE           (1 << 0) // 1: enable address translation; 0: PA == VA
#define WMMU_SCR_INTERRUPT_MASK   (1 << 1) // 1: mask interrupts(disable), 0: enable interrupt. default value 1
#define WMMU_SCR_REMAP_ENABLE     (1 << 2) // 1: enable remap; 0: disable remap

#define WMMU_PCR_4K         (0)
#define WMMU_PCR_32K        (1)
#define WMMU_PCR_64K        (2)
#define WMMU_PCR_128K       (3)

// #define WMMU_TASK_ALIGNMENT       (16) // every DMA task is aligned to 16 page entries, must be power of 2
// #define WMMU_ADDR_BIT_NUM         (40)

#endif /* _VDI_MMU_REG_H_ */