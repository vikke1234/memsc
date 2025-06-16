#ifndef MAPS_H
#define MAPS_H

#include <vector>
#include <linux/limits.h>
#include <sys/types.h>

enum file_perms {
    PERM_READ       = 1 << 0,
    PERM_WRITE      = 1 << 1,
    PERM_EXECUTE    = 1 << 2,
    PERM_SHARED     = 1 << 3,
    PERM_PRIVATE    = 1 << 4
};

struct address_range {
    /** Start of memory range */
    void                            *start;

    /** Length (in bytes) of the memory range */
    size_t                          length;

    /** Permissions set on the range */
    unsigned char                   perms;

    /**
     * If the region was mapped from a file,
     * it's the offset where the file begins,
     * otherwise 0
     */
    size_t                          offset;

    /**
     * If the region was mapped from a file,
     * this is the major and minor device
     * number where the file lives
     */
    dev_t                           device;

    /**
     * If the region was mapped from a file, this is the file number
     */
    ino_t                           inode;

    /**
     * If the region was mapped from a file,
     * this is the name of the file. This
     * field is blank for anonymous mapped regions.
     * There are also special regions with names
     * like [heap], [stack], or [vdso]. [vdso]
     * stands for virtual dynamic shared object.
     * It's used by system calls to switch to kernel mode.
     */
    char                            name[PATH_MAX];
};

std::vector<address_range> get_memory_ranges(pid_t pid, bool include_exec);
size_t get_address_range_list_size(std::vector<address_range> &ranges, bool include_exec);

#endif /* MAPS_H */
