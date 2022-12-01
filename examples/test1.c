#include "allocator.h"

void test1()
{
    char *str = tb_malloc(PMEM_SYSTEM_ALLOC, 20);
    strcpy(str, "Hello, World!");
    printf("%s\n", str);
    tb_free(PMEM_SYSTEM_ALLOC, str);
}

void test2() 
{
    char *str1, *str2;

    allocator_t *pga_alloc = region_allocator_new(SYSTEM_ALLOC, false);
    allocator_t *pmem_alloc = region_pallocator_new(PMEM_SYSTEM_ALLOC, false);

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

int main()
{

    init_tballoc();

    test1();
    test2();

    destroy_tballoc();
    return 0;
}
