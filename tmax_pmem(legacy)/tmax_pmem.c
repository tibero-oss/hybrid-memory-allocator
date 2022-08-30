/**
 * @brief PMEM을 volatile memory처럼 사용하기 위한 allocator API.
 */

#include <tmax_pmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>

/**
 * @brief pmem 공간을 요청한다. 함수 호출시, 주어진 디렉토리 상에 임시 파일을 생성하고, mmap()을 통해 메모리에 매핑한다.
 *
 * @param dir 파일을 생성할 디렉토리 경로
 * @param size 할당할 공간의 크기
 * @param addr 임시 파일을 매핑할 메모리 주소. NULL이면, 임의로 주소가 정해진다.
 * @param pfile 임시 파일에 대한 정보를 저장할 pmem_file 구조체의 주소.
 * @return void * 할당된 공간의 주소.
 */
void *pmem_malloc(const char *dir, void *addr, size_t size, struct pmem_file **pfile_ptr)
{
    *pfile_ptr = (struct pmem_file *)malloc(sizeof(struct pmem_file));
    if (*pfile_ptr == NULL)
    {
        printf("malloc failed\n");
        goto exit;
    }

    if (pmem_create_tmpfile(dir, pfile_ptr))
    {
        printf("pmem_create_tmpfile failed\n");
        goto exit;
    }

    if (ftruncate((*pfile_ptr)->fd, size)) // 파일의 크기 설정
    {
        printf("ftruncate failed\n");
        goto exit;
    }

    // 파일을 메모리에 매핑한다.
    addr = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_SHARED, (*pfile_ptr)->fd, 0);
    if (addr == MAP_FAILED)
    {
        printf("mmap failed\n");
        goto exit;
    }

    (*pfile_ptr)->current_size = size;

    return addr;

exit:
    if ((*pfile_ptr)->fd != -1)
        (void)close((*pfile_ptr)->fd);
    return NULL;
}

/**
 * @brief 주어진 경로 상에 임시 파일을 생성한다.
 *
 * @param dir 파일을 생성할 디렉토리 경로.
 * @param pfile_ptr pmem_file 구조체의 주소.
 * @return int 성공시 0, 실패시 -1.
 */
int pmem_create_tmpfile(const char *dir, struct pmem_file **pfile_ptr)
{
    static char template[] = "/pmem.XXXXXX";
    int dir_len = strlen(dir);

    // 디렉토리가 존재하는지 체크
    if (access(dir, F_OK))
        return -1;

    if (dir_len > PATH_MAX)
    {
        printf("Could not create temporary file: too long path.");
        return -1;
    }

    char *fullpath = (char *)malloc(dir_len + sizeof(template)); // 파일의 전체 경로 (디렉토리 경로 + 파일명)
    (void)strcpy(fullpath, dir);
    (void)strcat(fullpath, template);

    if (((*pfile_ptr)->fd = mkstemp(fullpath)) < 0) // 임시 파일 생성
    {
        printf("Could not create temporary file");
        goto exit;
    }
    (*pfile_ptr)->fullpath = fullpath;

    return 0;

exit:
    if ((*pfile_ptr)->fd != -1)
        (void)close((*pfile_ptr)->fd);
    (*pfile_ptr)->fd = (int *)-1;
    free(fullpath);
    return -1;
}

/**
 * @brief 파일을 삭제하고 메모리를 해제한다.
 * @param addr 파일에 매핑된 메모리 주소
 * @param pfile 파일 정보가 저장된 pmem_file 구조체의 주소
 *
 * @return int 성공시 0, 실패시 -1.
 */
int pmem_free(void *addr, struct pmem_file **pfile_ptr)
{
    if (munmap(addr, (*pfile_ptr)->current_size) != 0)
    {
        printf("[%s] munmap failed\n", __func__);
        return -1;
    }
    (void)close((*pfile_ptr)->fd);

    // 파일 삭제
    if (unlink((*pfile_ptr)->fullpath) != 0)
    {
        printf("[%s] unlink failed\n", __func__);
        return -1;
    }

    free((*pfile_ptr)->fullpath);
    free(*pfile_ptr);

    return 0;
}

/**
 * @brief 디렉토리 내의 모든 파일을 삭제한다.
 *
 * @param dir 디렉토리 경로
 * @return int 성공시 0, 실패시 -1.
 */
int pmem_cleanup_all(const char *dir)
{
    int err = 0;
    DIR *dirp = opendir(dir);
    if (dirp == NULL)
    {
        err = -1;
        goto exit;
    }
    struct dirent *dp;
    while ((dp = readdir(dirp)) != NULL)
    {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue;
        char *fullpath = (char *)malloc(strlen(dir) + strlen(dp->d_name) + 2);
        (void)strcpy(fullpath, dir);
        (void)strcat(fullpath, "/");
        (void)strcat(fullpath, dp->d_name);
        if (unlink(fullpath) != 0)
        {
            printf("[%s] unlink failed\n", __func__);
            err = -1;
            goto exit;
        }
        free(fullpath);
    }
    closedir(dirp);

    return err;

exit:
    return err;
}