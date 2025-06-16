#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/sysmacros.h>

#include "maps.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

std::vector<address_range> get_memory_ranges(pid_t pid, bool include_exec) {
    auto list = std::make_unique<address_range>();

    char filename[256];
    sprintf(filename, "/proc/%d/maps", pid);
    FILE *f = nullptr;
    f =  fopen(filename, "r");
    if(f == nullptr) {
        fprintf(stderr, "Error opening file %s: %s\n", filename, strerror(errno));
        return {};
    }
    char *line = nullptr;
    size_t n;

    std::vector<address_range> ranges{};

    while (getline(&line, &n, f) > 0) {
        address_range current{};

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

        dev_t device = makedev(dev_major, dev_minor);
        current.start = (void *)start;
        current.length = end - start;
        current.offset = offset;
        current.device = device;
        current.inode = inode;

        if (name_end > name_start) {
            memcpy(current.name, line + name_start, (size_t) (name_end - name_start));
        }
        current.name[name_end-name_start] = '\0';

        if(strchr(perms, 'r')) {
            current.perms |= PERM_READ;
        }
        if(strchr(perms, 'w')) {
            current.perms |= PERM_WRITE;
        }
        if(strchr(perms, 'p')) {
            current.perms |= PERM_PRIVATE;
        }
        if(strchr(perms, 's')) {
            current.perms |= PERM_SHARED;
        }
        if(strchr(perms, 'x')) {
            current.perms |= PERM_EXECUTE;
        }
        if ((include_exec || !(current.perms & PERM_EXECUTE)) &&
            std::strncmp(current.name, "[vvar]", 4096) &&
            std::strncmp(current.name, "[vvar_vclock]", 4096)) {
            ranges.push_back(current);
        }
    }

    free(line);
    fclose(f);
    return ranges;
}

size_t get_address_range_list_size(std::vector<address_range> &ranges, bool include_exec) {
    size_t n = 0;
    for(address_range& current : ranges) {
        if ((!(current.perms & PERM_EXECUTE) || include_exec) && current.perms & PERM_READ) {
            n += current.length;
        }
    }
    return n;
}

