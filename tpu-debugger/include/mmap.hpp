#pragma once

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

namespace tpu_debugger {

class DevMem {
private:
    int fd;
    volatile uint8_t *mapped_memory;
    size_t map_size;
    off_t map_offset;
    bool valid_;

public:
    DevMem() : fd(-1), mapped_memory(nullptr), map_size(0), map_offset(0), valid_(false) {}

    ~DevMem() { close(); }

    // 禁止拷贝
    DevMem(const DevMem&) = delete;
    DevMem& operator=(const DevMem&) = delete;

    // 允许移动
    DevMem(DevMem&& other) noexcept
        : fd(other.fd), mapped_memory(other.mapped_memory),
          map_size(other.map_size), map_offset(other.map_offset),
          valid_(other.valid_) {
        other.fd = -1;
        other.mapped_memory = nullptr;
        other.valid_ = false;
    }

    DevMem& operator=(DevMem&& other) noexcept {
        if (this != &other) {
            close();
            fd = other.fd;
            mapped_memory = other.mapped_memory;
            map_size = other.map_size;
            map_offset = other.map_offset;
            valid_ = other.valid_;
            other.fd = -1;
            other.mapped_memory = nullptr;
            other.valid_ = false;
        }
        return *this;
    }

    // 打开 "/dev/mem"
    bool open() {
        if (fd >= 0) return true;  // 已经打开

        fd = ::open("/dev/mem", O_RDWR | O_SYNC);
        if (fd < 0) {
            std::cerr << "Failed to open /dev/mem: " << strerror(errno) << std::endl;
            valid_ = false;
            return false;
        } else {
            std::cout << "successed to open /dev/mem !" << std::endl;
        }
        valid_ = true;
        return true;
    }

    // 检查是否有效
    bool isValid() const { return valid_ && fd >= 0; }

    bool mapMemory(uintptr_t phys_addr, size_t size) {
        if (!isValid()) {
            if (!open()) return false;
        }

        // 如果已经映射了相同的区域，直接返回成功
        if (mapped_memory != nullptr &&
            map_offset == static_cast<off_t>(phys_addr) &&
            map_size >= size) {
            return true;
        }

        // 取消之前的映射
        if (mapped_memory != nullptr) {
            munmap(const_cast<uint8_t *>(mapped_memory), map_size);
            mapped_memory = nullptr;
        }

        // offset of page-size
        off_t page_offset = phys_addr % sysconf(_SC_PAGESIZE);
        map_offset = phys_addr - page_offset;
        map_size = size + page_offset;

        // map the memory
        mapped_memory = static_cast<volatile uint8_t *>(mmap(
            nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_offset));

        if (mapped_memory == MAP_FAILED) {
            std::cerr << "Failed to mmap: " << strerror(errno) << std::endl;
            mapped_memory = nullptr;
            return false;
        } else {
            std::cout << "Successed to mmap !" << std::endl;
        }

        return true;
    }

    // read register value
    template <typename T>
    T read(uintptr_t phys_addr) {
        if (!mapped_memory) {
            throw std::runtime_error("Memory not mapped");
        }

        off_t offset = phys_addr - map_offset;
        if (offset + sizeof(T) > map_size) {
            throw std::out_of_range("Address out of mapped range");
        }

        volatile T *ptr = reinterpret_cast<volatile T *>(mapped_memory + offset);
        return *ptr;
    }

    // read multiple 32-bit words
    std::vector<uint32_t> readWords(uintptr_t phys_addr, uint32_t num_words) {
        std::vector<uint32_t> result;
        result.reserve(num_words);
        for (uint32_t i = 0; i < num_words; ++i) {
            result.push_back(read<uint32_t>(phys_addr + i * 4));
        }
        return result;
    }

    // write register value
    template <typename T>
    void write(uintptr_t phys_addr, T value) {
        if (!mapped_memory) {
            throw std::runtime_error("Memory not mapped");
        }

        off_t offset = phys_addr - map_offset;
        if (offset + sizeof(T) > map_size) {
            throw std::out_of_range("Address out of mapped range");
        }

        volatile T *ptr = reinterpret_cast<volatile T *>(mapped_memory + offset);
        *ptr = value;
    }

    // print data
    void hexdump(uintptr_t start_addr, size_t size) {
        if (!mapped_memory) {
            throw std::runtime_error("Memory not mapped");
        }

        std::cout << std::hex << std::setfill('0');

        for (size_t i = 0; i < size; i += 16) {
            // show address
            std::cout << std::setw(8) << (start_addr + i) << ": ";

            // show hex
            for (size_t j = 0; j < 16; ++j) {
                if (i + j < size) {
                    uint8_t val = read<uint8_t>(start_addr + i + j);
                    std::cout << std::setw(2) << (int)val << " ";
                } else {
                    std::cout << "   ";
                }

                if (j == 7)
                    std::cout << " ";
            }

            std::cout << " |";

            // show ASCII
            for (size_t j = 0; j < 16 && i + j < size; ++j) {
                uint8_t val = read<uint8_t>(start_addr + i + j);
                std::cout << (std::isprint(val) ? (char)val : '.');
            }

            std::cout << "|" << std::endl;
        }

        std::cout << std::dec;
    }

    // close the device
    void close() {
        if (mapped_memory) {
            munmap(const_cast<uint8_t *>(mapped_memory), map_size);
            mapped_memory = nullptr;
        }
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
        valid_ = false;
    }
};

class FileMemory {
private:
    std::vector<uint8_t> data_;
    bool valid_;
    uint64_t base_addr_;  // Base address for memory mapping

public:
    FileMemory() : valid_(false), base_addr_(0) {}

    ~FileMemory() { close(); }

    FileMemory(const FileMemory&) = delete;
    FileMemory& operator=(const FileMemory&) = delete;

    FileMemory(FileMemory&& other) noexcept
        : data_(std::move(other.data_)), valid_(other.valid_), base_addr_(other.base_addr_) {
        other.valid_ = false;
        other.base_addr_ = 0;
    }

    FileMemory& operator=(FileMemory&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            valid_ = other.valid_;
            base_addr_ = other.base_addr_;
            other.valid_ = false;
            other.base_addr_ = 0;
        }
        return *this;
    }

    bool open(const std::string& filepath, uint64_t base_addr = 0) {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filepath << std::endl;
            valid_ = false;
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        data_.resize(size);
        if (!file.read(reinterpret_cast<char*>(data_.data()), size)) {
            std::cerr << "Failed to read file: " << filepath << std::endl;
            valid_ = false;
            return false;
        }

        base_addr_ = base_addr;
        valid_ = true;
        return true;
    }

    bool isValid() const { return valid_; }
    uint64_t getBaseAddr() const { return base_addr_; }

    template <typename T>
    T read(uintptr_t addr) {
        // Map absolute address to file offset
        if (addr < base_addr_) {
            throw std::out_of_range("Address below base address");
        }
        uintptr_t offset = addr - base_addr_;
        if (offset + sizeof(T) > data_.size()) {
            throw std::out_of_range("Address out of file range");
        }
        T value;
        std::memcpy(&value, data_.data() + offset, sizeof(T));
        return value;
    }

    std::vector<uint32_t> readWords(uintptr_t addr, uint32_t num_words) {
        std::vector<uint32_t> result;
        result.reserve(num_words);
        for (uint32_t i = 0; i < num_words; ++i) {
            result.push_back(read<uint32_t>(addr + i * 4));
        }
        return result;
    }

    size_t size() const { return data_.size(); }

    void close() {
        data_.clear();
        valid_ = false;
        base_addr_ = 0;
    }
};

} // namespace tpu_debugger
