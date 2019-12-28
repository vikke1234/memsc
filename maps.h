#ifndef MAPS_H
#define MAPS_H

#include <stdlib.h>
#include <linux/limits.h>

enum file_perms {
    PERM_READ       = 1 << 0,
    PERM_WRITE      = 1 << 1,
    PERM_EXECUTE    = 1 << 2,
    PERM_SHARED     = 1 << 3,
    PERM_PRIVATE    = 1 << 4
};

struct address_range {
    struct address_range    *next;
    void                    *start;
    size_t                  length;
    unsigned char           perms;
    size_t                  offset;
    dev_t                   device;
    ino_t                   inode;
    char                    name[PATH_MAX];
};

struct address_range *get_memory_ranges(pid_t pid);
void free_address_range(address_range *list);
size_t get_address_range_list_size(address_range *list);

#endif /* MAPS_H */
