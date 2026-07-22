#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include "tradecore/common/assert.hpp"

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#if __has_include(<numaif.h>)
#define TRADECORE_HAS_NUMA
#include <numaif.h>
#endif
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace tradecore::memory {

class HugePageAllocator {
public:
    static void* allocate(std::size_t size, int numa_node = -1) {
#ifdef __linux__
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;
        
        // Use huge pages if requested size is >= 2MB
        if (size >= 2 * 1024 * 1024) {
            flags |= MAP_HUGETLB | MAP_POPULATE;
        } else {
            flags |= MAP_POPULATE; // Still prefault
        }

        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, flags, -1, 0);
        if (ptr == MAP_FAILED) {
            throw std::bad_alloc();
        }

#ifdef TRADECORE_HAS_NUMA
        if (numa_node >= 0) {
            unsigned long nodemask = (1UL << numa_node);
            if (mbind(ptr, size, MPOL_BIND, &nodemask, sizeof(nodemask) * 8, 0) != 0) {
                // Ignore failure gracefully if NUMA is not fully supported on this host
                // but in production, we might want to log this.
            }
        }
#else
        (void)numa_node;
#endif

        return ptr;

#elif defined(_WIN32)
        (void)numa_node; // NUMA API not directly mapped here yet
        
        DWORD alloc_type = MEM_COMMIT | MEM_RESERVE;
        
        // Attempt to use large pages if requested size is >= 2MB
        if (size >= 2 * 1024 * 1024) {
            // Adjust token privileges required for large pages
            HANDLE hToken;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
                TOKEN_PRIVILEGES tp;
                if (LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &tp.Privileges[0].Luid)) {
                    tp.PrivilegeCount = 1;
                    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                    AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
                }
                CloseHandle(hToken);
            }
            
            alloc_type |= MEM_LARGE_PAGES;
        }

        void* ptr = VirtualAlloc(NULL, size, alloc_type, PAGE_READWRITE);
        
        // Fallback to normal pages if large pages failed
        if (!ptr && (alloc_type & MEM_LARGE_PAGES)) {
            ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        }
        
        if (!ptr) {
            throw std::bad_alloc();
        }
        
        return ptr;
#else
        (void)numa_node;
        // Generic POSIX fallback
        void* ptr = nullptr;
        if (posix_memalign(&ptr, 4096, size) != 0) {
            throw std::bad_alloc();
        }
        return ptr;
#endif
    }

    static void deallocate(void* ptr, std::size_t size) noexcept {
        if (!ptr) return;
        
#ifdef __linux__
        munmap(ptr, size);
#elif defined(_WIN32)
        (void)size;
        VirtualFree(ptr, 0, MEM_RELEASE);
#else
        (void)size;
        free(ptr);
#endif
    }
};

} // namespace tradecore::memory
