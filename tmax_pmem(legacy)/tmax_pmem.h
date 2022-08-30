#include <stddef.h>
#include <stdio.h>

struct pmem_file
{
    int fd;              // file descriptor
    size_t current_size; // current size of the file
    char *fullpath;      // full path of the file
};

void *pmem_malloc(const char *dir, void *addr, size_t size, struct pmem_file **pfile_ptr);
int pmem_create_tmpfile(const char *dir, struct pmem_file **pfile_ptr);
int pmem_free(void *addr, struct pmem_file **pfile_ptr);
int pmem_cleanup_all(const char *dir);