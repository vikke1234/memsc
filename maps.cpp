#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/sysmacros.h>

#include "maps.h"

struct address_range *get_memory_ranges(pid_t pid) {
    address_range *list = nullptr;
    char filename[64];
    sprintf(filename, "/proc/%d/maps", pid);
    FILE *f = NULL;
    f =  fopen(filename, "r");
    if(f == NULL) {
        fprintf(stderr, "Error get_memory_ranges: %s\n", strerror(errno));
        return NULL;
    }
    char *line = NULL;
    size_t n;

    size_t list_len = 0;

    while (getline(&line, &n, f) > 0) {
        address_range *current = nullptr;
        unsigned long start, end, offset;
        unsigned int dev_major, dev_minor;
        char    perms[8];
        ino_t   inode;
        int name_start = 0, name_end = 0;

        if(sscanf(line, "%lx-%lx %4s %lx %u:%u %lu %n%*[^\n]%n",
               &start, &end, perms, &offset, &dev_major, &dev_minor,
               &inode, &name_start, &name_end) < 7) {
            fclose(f);
            free(line);

            return NULL;
        }
        if (name_end <= name_start) {
            name_end = name_start = 0;
        }

        current = (address_range*)malloc(sizeof(address_range));
        if(current == NULL) {
            fclose(f);
            free(line);
            return NULL;
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
            current->perms |= PERM_SHARED;
        }

        current->next = list;
        list = current;
        list_len++;
    }
    fclose(f);
    return list;
}

void free_address_range(address_range *list) {
    while (list != nullptr) {
        address_range *curr = list;
        list = list->next;
        curr->next = nullptr;
        curr->length = 0;
        curr->perms = 0;
        curr->name[0] = '\0';
        delete curr;
    }
}

size_t get_address_range_list_size(address_range *list) {
    size_t n = 0;
    address_range *current = list;
    while(current != NULL) {
        current = current->next;
        n++;
    }
    return n;
}

