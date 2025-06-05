#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/sysmacros.h>

#include "maps.h"

#include <cstdint>
#include <memory>

std::unique_ptr<address_range> get_memory_ranges(pid_t pid) {
    auto list = std::make_unique<address_range>();

    char filename[256];
    sprintf(filename, "/proc/%d/maps", pid);
    FILE *f = nullptr;
    f =  fopen(filename, "r");
    if(f == nullptr) {
        fprintf(stderr, "Error opening file %s: %s\n", filename, strerror(errno));
        return nullptr;
    }
    char *line = nullptr;
    size_t n;

    size_t list_len = 0;
    address_range *current = list.get();
    address_range *prev = nullptr;
    while (getline(&line, &n, f) > 0) {
        uintptr_t start, end, offset;
        unsigned int dev_major, dev_minor;
        char    perms[8];
        ino_t   inode;
        int name_start = 0, name_end = 0;

        if(sscanf(line, "%lx-%lx %4s %lx %x:%x %lu %n%*[^\n]%n",
               &start, &end, perms, &offset, &dev_major, &dev_minor,
               &inode, &name_start, &name_end) < 7) {
            break;
        }
        if (name_end <= name_start) {
            name_end = name_start = 0;
        }

        if(current == nullptr) {
            break;
        }

        dev_t device = makedev(dev_major, dev_minor);
        current->start = (void *)start;
        current->length = end - start;
        current->offset = offset;
        current->device = device;
        current->inode = (ino_t) inode;

        if (name_end > name_start) {
            memcpy(current->name, line + name_start, (size_t) (name_end - name_start));
        }
        current->name[name_end-name_start] = '\0';

        if(strchr(perms, 'r')) {
            current->perms |= PERM_READ;
        }
        if(strchr(perms, 'w')) {
            current->perms |= PERM_WRITE;
        }
        if(strchr(perms, 'p')) {
            current->perms |= PERM_PRIVATE;
        }
        if(strchr(perms, 's')) {
            current->perms |= PERM_SHARED;
        }
        if(strchr(perms, 'x')) {
            current->perms |= PERM_EXECUTE;
        }
        current->next = std::make_unique<address_range>();
        prev = current;
        current = current->next.get();
        list_len++;
    }
    if (prev != nullptr) {
        // Remove last entry which was allocated unnecessarely, TODO: find out if there's a better way.
        prev->next = nullptr;
    }
    free(line);
    fclose(f);
    return list;
}

size_t get_address_range_list_size(address_range *list) {
    size_t n = 0;
    address_range *current = list;
    while(current != nullptr) {
        current = current->next.get();
        n++;
    }
    return n;
}

