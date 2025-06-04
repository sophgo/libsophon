#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <atomic>

#include "bmlib_ioctl.h"

size_t virtual_to_physical(size_t addr) {
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        printf("open '/proc/self/pagemap' failed!\n");
        return 0;
    }
    size_t pagesize = getpagesize();
    size_t offset = (addr / pagesize) * sizeof(uint64_t);
    if (lseek(fd, offset, SEEK_SET) < 0) {
        printf("lseek() failed!\n");
        close(fd);
        return 0;
    }
    uint64_t info;
    if (read(fd, &info, sizeof(uint64_t)) != sizeof(uint64_t)) {
        printf("read() failed!\n");
        close(fd);
        return 0;
    }
    if ((info & (((uint64_t)1) << 63)) == 0) {
        printf("page is not present!\n");
        close(fd);
        return 0;
    }
    size_t frame = info & ((((uint64_t)1) << 55) - 1);
    size_t phy = frame * pagesize + addr % pagesize;
    close(fd);
    return phy;
}

void print_memory_maps() {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        perror("Failed to open /proc/self/maps");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }

    fclose(fp);
}

void print_physical_address_range(void *mapped_memory, size_t size) {
    size_t page_size = getpagesize();
    size_t start_addr = (size_t)mapped_memory; // Starting address of the mapping
    size_t end_addr = start_addr + size; // Ending address of the mapping

    printf("Physical address mapping range:\n");
    for (size_t addr = start_addr; addr < end_addr; addr += page_size) {
        size_t physical_addr = virtual_to_physical(addr);
        printf("Virtual: %lx -> Physical: %lx\n", addr, physical_addr);
    }
}

#define DEVICE_FILE "/dev/bm-tpu0" // Device file path
#define MAPPED_SIZE 0x30000         // Size of the mapping

int main() {
    int fd = open(DEVICE_FILE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    int ret = ioctl(fd, BMDEV_SET_IOMAP_TPYE, 1);
    printf("ioctl ret: %d\n", ret);
    // Request mmap, passing the starting address and size
    void *mapped_memory = mmap(NULL, MAPPED_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_memory == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return -1;
    }
    printf("mapped_memory: %p\n", mapped_memory);

    // print_memory_maps(); // Print the current process's memory mappings

    // Read the value of this register
    printf("The value of the register is: %x\n", *((int *)mapped_memory));

    volatile int *reg_value = (volatile int *)mapped_memory;
    if (*reg_value != 0x1f) {
        *reg_value = 0x1f;
    } else {
        *reg_value = 0x0;
    }

    // Sleep for 3 seconds
    // usleep(3000000);

    // Use the mapped memory
    printf("-------------------\n");
    printf("after write The value of the register is: %x\n", *((int *)mapped_memory));

    // Unmap
    if (munmap(mapped_memory, MAPPED_SIZE) < 0) {
        perror("munmap failed");
    }
    if (ioctl(fd, BMDEV_READL, 0xC010000) == -1) {
        printf("ioctl failed\n");
    }
    close(fd);

    return 0;
}
