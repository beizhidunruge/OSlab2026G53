#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <../sync/sync.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

//lab2 请补充你在lab2中的代码
static void page_init(void) {
    extern char end[];
    const uint64_t mem_base = get_memory_base();
    const uint64_t mem_size = get_memory_size();

    /* nbase and the boot page table describe QEMU virt's fixed DRAM base. */
    if (mem_base != DRAM_BASE || mem_size == 0 ||
        mem_size > (uint64_t)-1 - mem_base) {
        panic("page_init: invalid DTB memory range");
    }
    const uint64_t mem_end = mem_base + mem_size;
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    uintptr_t maxpa = mem_end < PHYSICAL_MEMORY_END ?
                      mem_end : PHYSICAL_MEMORY_END;
    const uintptr_t mapped_end = KERNTOP - va_pa_offset;
    if (maxpa > mapped_end) {
        maxpa = mapped_end;
    }
    maxpa = ROUNDDOWN(maxpa, PGSIZE);
    if (maxpa <= KERNEL_BEGIN_PADDR || (uintptr_t)end < KERNBASE ||
        (uintptr_t)end >= KERNTOP) {
        panic("page_init: RAM does not contain the kernel");
    }

    npage = PPN(maxpa);  /* absolute physical-page-number upper bound */
    const size_t count = npage - nbase;
    const uintptr_t pages_va = ROUNDUP((uintptr_t)end, PGSIZE);
    const uintptr_t pages_pa = PADDR(pages_va);
    /* Subtraction/division checks avoid overflowing an array-end calculation. */
    if (pages_pa > maxpa || count > (maxpa - pages_pa) / sizeof(struct Page)) {
        panic("page_init: RAM too small for Page descriptors");
    }
    const size_t descriptor_bytes = count * sizeof(struct Page);
    const uintptr_t freemem = ROUNDUP(pages_pa + descriptor_bytes, PGSIZE);
    pages = (struct Page *)pages_va;

    for (size_t i = 0; i < count; ++i) {
        struct Page *p = &pages[i];
        p->flags = 0;
        SetPageReserved(p);
        set_page_ref(p, 0);
        p->property = 0;
        list_init(&p->page_link);
    }

    cprintf("physical memory map (DTB): [0x%016lx, 0x%016lx)\n",
            mem_base, mem_end);
    cprintf("managed memory: [0x%016lx, 0x%016lx), npage=0x%lx\n",
            mem_base, maxpa, npage);
    cprintf("Page descriptors: VA=0x%016lx, PA=0x%016lx, count=%lu, bytes=%lu\n",
            pages_va, pages_pa, count, descriptor_bytes);
    cprintf("free memory: [0x%016lx, 0x%016lx), pages=%lu\n",
            freemem, maxpa, freemem < maxpa ? (maxpa - freemem) / PGSIZE : 0);
    /* Never pass an empty range to a manager (First-Fit asserts n > 0). */
    if (freemem < maxpa) {
        init_memmap(pa2page(freemem), (maxpa - freemem) / PGSIZE);
    }
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
