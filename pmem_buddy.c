#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/mman.h>
#include <linux/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>

#include "pmem_buddy.h"
#include "buddy_alloc.h"

/**
 * @brief Initialize pmem allocator.
 *
 * @param[in] dir
 * @param base_ptr Base address of the allocator. If NULL, it will be allocated.
 * @param max_size Size of the pool.
 * @param size
 * @return pbuddy_alloc_t* Pointer to the allocator.
 */
pbuddy_alloc_t *pbuddy_alloc_init(const char *dir, void *base_ptr, size_t max_size, size_t size)
{
    pbuddy_alloc_t *allocator;
    int fd;
    void *addr;
    static char template[] = "/pmem.XXXXXX";
    int dir_len;
    char *file_fullpath;

    dir_len = strlen(dir);

    // 디렉토리가 존재하는지 체크
    if (access(dir, F_OK))
        return NULL;

    if (dir_len > PATH_MAX)
    {
        printf("Could not create temporary file: too long path.");
        return NULL;
    }

    file_fullpath = (char *)mmap(NULL, dir_len + sizeof(template), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

    if (file_fullpath == NULL)
    {
        printf("mmap failed\n");
        return NULL;
    }
    strcpy(file_fullpath, dir);
    strcat(file_fullpath, template);

    if ((fd = mkstemp(file_fullpath)) < 0) // 임시 파일 생성
    {
        printf("Could not create temporary file");
        goto exit;
    }

    if (ftruncate(fd, max_size)) // 파일의 크기 설정
    {
        printf("ftruncate failed\n");
        goto exit;
    }

    // 파일을 메모리에 매핑한다.
    addr = mmap(base_ptr, max_size, PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE | MAP_SYNC, fd, 0);
    if (addr == MAP_FAILED)
    {
        printf("mmap failed\n");
        goto exit;
    }
    close(fd);
    allocator = buddy_allocator_new(addr, max_size, size, file_fullpath);
    if (allocator == NULL)
    {
        printf("buddy_allocator_new failed\n");
        goto exit;
    }

    return allocator;

exit:
    if (fd != -1)
        (void)close(fd);
    if (file_fullpath != NULL)
        (void)munmap(file_fullpath, dir_len + sizeof(template));
    return NULL;
}

/**
 * @brief 메모리를 unmap하고 파일을 삭제한다.
 *
 * @param alloc_ptr pmem_alloc 구조체의 주소.
 * @return int 성공시 0, 실패시 -1.
 */
int pbuddy_alloc_destroy(pbuddy_alloc_t **alloc_ptr)
{
    if (munmap((void *)((*alloc_ptr)->page_start), (*alloc_ptr)->alloc_size))
    {
        printf("munmap failed\n");
        return -1;
    }
    if (unlink((*alloc_ptr)->file_fullpath))
    {
        printf("unlink failed\n");
        return -1;
    }
    if (munmap((void *)(*alloc_ptr)->file_fullpath, strlen((*alloc_ptr)->file_fullpath) + 1))
    {
        printf("munmap failed\n");
        return -1;
    }
    if (munmap((void *)(*alloc_ptr), (*alloc_ptr)->reserved))
    {
        printf("munmap failed\n");
        return -1;
    }
    return 0;
}

void *pbuddy_malloc(pbuddy_alloc_t *allocator, size_t size)
{
    return buddy_malloc(allocator, size);
}

void pbuddy_free(pbuddy_alloc_t *allocator, void *ptr, size_t size)
{
    buddy_free(allocator, ptr, size);
}