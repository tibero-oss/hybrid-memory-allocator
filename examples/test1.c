#include "allocator.h"

void pmem_system_allocator_example()
{
    allocator_log_on(PMEM_SYSTEM_ALLOC);
    char *str = tb_malloc(PMEM_SYSTEM_ALLOC, 20);
    strcpy(str, "Hello, World!");
    printf("%s\n", str);
    tb_free(PMEM_SYSTEM_ALLOC, str);
    allocator_log_off(PMEM_SYSTEM_ALLOC);
}

void create_allocator() 
{
    char *str1, *str2;

    allocator_t *pga_alloc = region_allocator_new(SYSTEM_ALLOC, false);
    allocator_t *pmem_alloc = region_pallocator_new(PMEM_SYSTEM_ALLOC, false);

    allocator_log_on(pga_alloc);
    allocator_log_on(pmem_alloc);

    str1 = tb_malloc(pga_alloc, 20);
    strcpy(str1, "Hello, World!");
    printf("%s\n", str1);

    str2 = tb_malloc(pmem_alloc, 20);
    memcpy(str2, str1, 20);
    printf("%s\n", str2);

    tb_free(pga_alloc, str1);
    tb_free(pmem_alloc, str2);

    allocator_delete(pga_alloc);
    allocator_delete(pmem_alloc);
}

void alloc_api_example(allocator_t *alloc) 
{
    char *str, *str2;

    allocator_log_on(alloc);

    str = tb_malloc(alloc, 20);
    strcpy(str, "Hello, World!");
    printf("%s\n", str);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));
    tb_free(alloc, str);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));

    str = tb_valloc(alloc, 4096);
    printf("%p\n", str);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));
    tb_free(alloc, str);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));

    str = tb_calloc(alloc, 20);
    printf("%s\n", str);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));
    tb_free(alloc, str);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));

    str = tb_malloc(alloc, 20);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));
    str = tb_realloc(alloc, str, 100);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));
    tb_free(alloc, str);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));

    str = tb_malloc(alloc, 20);
    strcpy(str, "Hello, World!");
    printf("%s\n", str);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));
    str2 = tb_strdup(alloc, str);
    printf("%s\n", str2);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));
    tb_free(alloc, str2);
    
    str2 = tb_strndup(alloc, str, 10);
    printf("%s\n", str2);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));
    tb_free(alloc, str2);
    tb_free(alloc, str);
    printf("total_size:%ld, total_used:%ld\n", get_total_size(alloc), get_total_used(alloc));
}

void alloc_api()
{
    allocator_t *alloc;

    alloc = region_pallocator_new(PMEM_SYSTEM_ALLOC, false);
    alloc_api_example(alloc);
    allocator_delete(alloc);

    alloc = region_allocator_new(SYSTEM_ALLOC, false);
    alloc_api_example(alloc);
    allocator_delete(alloc);
}

void allocator_delete_example() 
{
    void *ptr; 
    allocator_t *alloc_parent, *alloc_child;
    
    alloc_parent = region_pallocator_new(SYSTEM_ALLOC, false);
    allocator_log_on(alloc_parent);
    ptr = tb_malloc(alloc_parent, 1000);
    printf("parent total_size:%ld, parent total_used:%ld\n", get_total_size(alloc_parent), 
            get_total_used(alloc_parent));

    /* create child allocator */
    alloc_child = region_pallocator_new(alloc_parent, false);
    allocator_log_on(alloc_child);
    ptr = tb_malloc(alloc_child, 1000);
    printf("child total_size:%ld, child total_used:%ld\n", get_total_size(alloc_child), 
            get_total_used(alloc_child));
    printf("get_alloc_used_size_including_child: %ld\n", get_alloc_used_size_including_childs(alloc_parent));

    allocator_delete(alloc_parent);
}

int main()
{
    IPARAM(PMEM_DIR) = "/workspace/develop/code_test/pmem_tmp";
    IPARAM(PMEM_MAX_SIZE) = 2L * 1024L * 1024L * 1024L;
    IPARAM(PMEM_ALLOC_SIZE) = 2L * 1024L * 1024L * 1024L;

    tballoc_init();

    pmem_system_allocator_example();
    create_allocator();
    alloc_api();
    allocator_delete_example();

    tballoc_clear();
    return 0;
}