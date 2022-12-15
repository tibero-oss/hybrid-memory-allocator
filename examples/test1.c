#include "allocator.h"
#include "assert.h"
#include "string.h"

void pmem_system_allocator()
{
    char *str = tb_malloc(PMEM_SYSTEM_ALLOC, 20);
    strcpy(str, "Hello, World!");
    assert(strcmp(str, "Hello, World!") == 0);
    tb_free(PMEM_SYSTEM_ALLOC, str);
    assert(get_total_used(PMEM_SYSTEM_ALLOC) == 0);
}

void create_region_allocator() 
{
    char *str1, *str2;

    allocator_t *pga_alloc = region_allocator_new(SYSTEM_ALLOC, false);
    allocator_t *pmem_alloc = region_pallocator_new(PMEM_SYSTEM_ALLOC, false);

    str1 = tb_malloc(pga_alloc, 20);
    strcpy(str1, "Hello, World!");

    str2 = tb_malloc(pmem_alloc, 20);
    memcpy(str2, str1, 20);

    assert(strcmp(str1, str2) == 0);

    tb_free(pga_alloc, str1);
    assert(get_total_used(pga_alloc) == 0);

    tb_free(pmem_alloc, str2);
    assert(get_total_used(pmem_alloc) == 0);

    allocator_delete(pga_alloc);
    allocator_delete(pmem_alloc);
}

void run_all_alloc_api(allocator_t *alloc) 
{
    char *str, *str2;

    str = tb_malloc(alloc, 20);
    strcpy(str, "Hello, World!");
    assert(strcmp(str, "Hello, World!") == 0);
    assert(get_total_used(alloc) == get_chunk_size(20));
    tb_free(alloc, str);
    assert(get_total_used(alloc) == 0);

    str = tb_valloc(alloc, 4096);
    assert((int)((ptrdiff_t)str) % 4096 == 0);
    tb_free(alloc, str);
    assert(get_total_used(alloc) == 0);

    str = tb_calloc(alloc, 5);
    assert(memcmp(str, "\0\0\0\0\0", 5) == 0);
    tb_free(alloc, str);
    assert(get_total_used(alloc) == 0);

    str = tb_malloc(alloc, 20);
    assert(get_total_used(alloc) == get_chunk_size(20));
    str = tb_realloc(alloc, str, 1000);
    assert(get_total_used(alloc) == get_chunk_size(1000));
    tb_free(alloc, str);
    assert(get_total_used(alloc) == 0);

    str = tb_malloc(alloc, 20);
    strcpy(str, "Hello, World!");
    str2 = tb_strdup(alloc, str);
    assert(strcmp(str, str2) == 0);
    assert(get_total_used(alloc) == get_chunk_size(20) + get_chunk_size(strlen(str2) + 1));
    tb_free(alloc, str2);
    
    str2 = tb_strndup(alloc, str, 10);
    assert(strlen(str2) == 10);
    tb_free(alloc, str2);
    tb_free(alloc, str);
    assert(get_total_used(alloc) == 0);
}

void alloc_api()
{
    allocator_t *alloc;

    alloc = region_pallocator_new(PMEM_SYSTEM_ALLOC, false);
    run_all_alloc_api(alloc);
    allocator_delete(alloc);

    alloc = region_allocator_new(SYSTEM_ALLOC, false);
    run_all_alloc_api(alloc);
    allocator_delete(alloc);
}

void allocator_delete_example() 
{
    allocator_t *alloc_parent, *alloc_child;
    
    alloc_parent = region_pallocator_new(SYSTEM_ALLOC, false);
    tb_malloc(alloc_parent, 1000);
    assert(get_total_used(alloc_parent) == get_chunk_size(1000));

    /* create child allocator */
    alloc_child = region_pallocator_new(alloc_parent, false);
    tb_malloc(alloc_child, 1000);
    assert(get_total_used(alloc_child) == get_chunk_size(1000));
    assert(get_alloc_used_size_including_childs(alloc_parent) == get_chunk_size(1000) * 2);

    allocator_delete(alloc_parent);
}
void alloc_fail()
{
    void *ptr;
    IPARAM(PMEM_MAX_SIZE) = 1L * 1024L * 1024L;
    IPARAM(PMEM_ALLOC_SIZE) = 1L * 1024L * 1024L;
    tballoc_init();

    ptr = tb_malloc(PMEM_SYSTEM_ALLOC, 2 * 1024 * 1024);
    assert(ptr == NULL);
    assert(get_total_used(PMEM_SYSTEM_ALLOC) == 0);

    tballoc_clear();
    assert(!PMEM_SYSTEM_ALLOC && !SYSTEM_ALLOC);
}

int main()
{
    IPARAM(PMEM_DIR) = "/workspace/develop/code_test/pmem_tmp";
    IPARAM(PMEM_MAX_SIZE) = 1L * 1024L * 1024L * 1024L;
    IPARAM(PMEM_ALLOC_SIZE) = 1L * 1024L * 1024L * 1024L;

    tballoc_init();

    pmem_system_allocator();
    create_region_allocator();
    alloc_api();
    allocator_delete_example();

    tballoc_clear();

    alloc_fail();
    return 0;
}