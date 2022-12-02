#include "allocator.h"

void test1()
{
    allocator_log_on(PMEM_SYSTEM_ALLOC);
    char *str = tb_malloc(PMEM_SYSTEM_ALLOC, 20);
    strcpy(str, "Hello, World!");
    printf("%s\n", str);
    tb_free(PMEM_SYSTEM_ALLOC, str);
    allocator_log_off(PMEM_SYSTEM_ALLOC);
}

void test2() 
{
    char *str1, *str2;

    allocator_t *pga_alloc = region_allocator_new(SYSTEM_ALLOC, false);
    allocator_t *pmem_alloc = region_pallocator_new(PMEM_SYSTEM_ALLOC, false);

    allocator_log_on(pga_alloc);
    allocator_log_on(pmem_alloc);

    str1 = tb_malloc(pga_alloc, 20);
    strcpy(str1, "Hello, World!");
    printf("%s\n", str1);

    str2 = tb_calloc(pmem_alloc, 20);
    memcpy(str2, str1, 20);
    printf("%s\n", str2);

    tb_free(pga_alloc, str1);
    tb_free(pmem_alloc, str2);

    allocator_delete(pga_alloc);
    allocator_delete(pmem_alloc);
}

void test3() 
{
    char *str1, *str2;

    allocator_t *alloc = region_pallocator_new(SYSTEM_ALLOC, false);

    allocator_log_on(alloc);

    str1 = tb_malloc(alloc, 20);
    strcpy(str1, "Hello, World!");
    printf("%s\n", str1);

    str2 = tb_calloc(alloc, 20);
    memcpy(str2, str1, 20);
    printf("%s\n", str2);

    allocator_cleanup(alloc);

    str1 = tb_malloc(alloc, 20);
    strcpy(str1, "Hello, World2!");
    printf("%s\n", str1);

    allocator_delete(alloc);
}

void test4() 
{
    char *str1;

    allocator_t *alloc = region_pallocator_new(SYSTEM_ALLOC, false);

    allocator_log_on(alloc);

    str1 = tb_valloc(alloc, 4096);
    printf("%p\n", str1);

    allocator_delete(alloc);
}

int main()
{
    tballoc_init();

    test1();
    test2();
    test3();
    test4();

    tballoc_clear();
    return 0;
}