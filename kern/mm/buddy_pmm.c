#include <pmm.h>
#include <list.h>
#include <buddy_pmm.h>

/*
 * Buddy System physical memory manager.
 *
 * Free memory is tracked as blocks whose size is a power of two pages.
 * buddy_free_area[k] holds the heads of all free blocks of 2^k pages.
 *
 * A block of 2^k pages is always aligned to 2^k pages.  Consequently the
 * block that is the buddy of the block starting at global page index
 * i (i = page - pages) starts at index i ^ 2^k.  All arithmetic here uses
 * that global index rather than a manager-local offset, so pages[] can be
 * fed by several init_memmap() calls at arbitrary positions.  nbase is a
 * multiple of 2^19 and blocks never exceed the DRAM size, therefore XOR on
 * the index gives the same result as XOR on the physical page number.
 *
 * Page conventions (the same layout as default_pmm):
 *   - PageReserved is set by page_init() for every descriptor and cleared
 *     by init_memmap() when the page becomes managed by this manager.
 *   - The head page of a free block has PG_property set and property equal
 *     to the block size in pages.  Other pages of a free block have
 *     property == 0 and PG_property clear.
 *   - The head page of an allocated block has PG_property clear but keeps
 *     property equal to the actual (rounded-up power of two) block size,
 *     so free_pages() can validate the request and learn the real size.
 *     All other pages of an allocated block have property == 0.
 */

/* Hard upper bound on the block order; the platform limit is derived at
 * init time from the physical memory layout. */
#define BUDDY_MAX_ORDER_LIMIT 31

static free_area_t buddy_free_area[BUDDY_MAX_ORDER_LIMIT + 1];
/* Highest order allowed on this platform (floor(log2(total pages))). */
static unsigned int buddy_max_order;
/* Total number of pages currently kept in all free lists. */
static size_t buddy_nr_free;

/* Number of Page descriptors that page_init() created.  npage is the page
 * number of the end of managed memory and nbase is the page number of the
 * first descriptor, so the valid global index range is [0, managed). */
static size_t buddy_managed_pages(void) {
    return npage > nbase ? npage - nbase : 0;
}

/* floor(log2(value)), value >= 1 */
static size_t buddy_order_floor(size_t value) {
    size_t order = 0;
    while (value > 1) {
        value >>= 1;
        order++;
    }
    return order;
}

/* Round n pages up to a power of two.  Returns false when the resulting
 * block would exceed the largest order supported by this platform. */
static bool buddy_order_for_pages(size_t n, size_t *order_store,
                                  size_t *size_store) {
    size_t order = 0;
    while (((size_t)1 << order) < n) {
        if (order >= buddy_max_order) {
            return 0;
        }
        order++;
    }
    *order_store = order;
    *size_store = (size_t)1 << order;
    return 1;
}

/*
 * Insert the free block [page, page + 2^order) and merge it upwards while
 * its buddy is a free block of the same order.  The caller must have
 * already marked page as a free block head (PG_property set, property ==
 * 2^order) with an initialized page_link that is not currently linked.
 * Pages outside the registered range, reserved pages and pages that are
 * not free block heads stop the merge, so blocks never cross those.
 */
static void buddy_insert_free_block(struct Page *page, size_t order) {
    const size_t managed = buddy_managed_pages();
    size_t size = (size_t)1 << order;
    size_t index = (size_t)(page - pages);

    while (order < buddy_max_order) {
        const size_t buddy_index = index ^ size;
        /* The buddy must be a descriptor that actually exists, and the
         * whole merged block must stay inside the descriptor array. */
        if (buddy_index >= managed || size > managed - buddy_index) {
            break;
        }
        struct Page *buddy = pages + buddy_index;
        if (PageReserved(buddy) || !PageProperty(buddy) ||
            buddy->property != (unsigned int)size) {
            break;
        }

        /* Both halves are free: take the buddy out of its list and let
         * the lower-addressed half absorb it. */
        list_del_init(&buddy->page_link);
        buddy_free_area[order].nr_free -= (unsigned int)size;
        ClearPageProperty(buddy);
        buddy->property = 0;

        if (buddy_index < index) {
            /* The buddy is the lower half and becomes the merged head;
             * the old head turns into an ordinary interior page. */
            ClearPageProperty(page);
            page->property = 0;
            index = buddy_index;
            page = buddy;
        }
        order++;
        size <<= 1;
        page->property = (unsigned int)size;
        SetPageProperty(page);
    }

    list_add(&buddy_free_area[order].free_list, &page->page_link);
    /* free_area_t::nr_free counts free pages, so store the block's size. */
    buddy_free_area[order].nr_free += (unsigned int)size;
}

static void buddy_init(void) {
    const size_t phys_pages =
        (size_t)((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE);
    size_t max_order = 0;

    /* floor(log2(phys_pages)), capped by the hard limit */
    if (phys_pages > 0) {
        max_order = buddy_order_floor(phys_pages);
    }
    if (max_order > BUDDY_MAX_ORDER_LIMIT) {
        max_order = BUDDY_MAX_ORDER_LIMIT;
    }
    buddy_max_order = (unsigned int)max_order;

    for (size_t k = 0; k <= BUDDY_MAX_ORDER_LIMIT; k++) {
        list_init(&buddy_free_area[k].free_list);
        buddy_free_area[k].nr_free = 0;
    }
    buddy_nr_free = 0;
}

/*
 * Register [base, base + n) as free memory.  The range is split greedily
 * into naturally aligned power-of-two blocks (largest block first), which
 * covers every page exactly once.  Each block is then inserted and merged
 * with free buddies that were registered by an earlier call.
 */
static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(base != NULL);
    assert(n > 0);

    const size_t managed = buddy_managed_pages();
    assert(managed > 0);
    const size_t first = (size_t)(base - pages);
    assert(first < managed);
    assert(n <= managed - first);

    /* Reject pages that were already handed to the manager (their
     * PageReserved bit has been cleared) and prevent double registration. */
    for (size_t i = 0; i < n; i++) {
        struct Page *p = base + i;
        assert(PageReserved(p));
        assert(!PageProperty(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
        list_init(&p->page_link);
    }
    buddy_nr_free += n;

    size_t index = first;
    size_t remaining = n;
    while (remaining > 0) {
        size_t order = 0;
        size_t block = 1;
        /* Grow while the block still fits and stays naturally aligned. */
        while (order < buddy_max_order && block <= remaining / 2 &&
               (index & (block * 2 - 1)) == 0) {
            block *= 2;
            order++;
        }

        struct Page *head = pages + index;
        head->property = (unsigned int)block;
        SetPageProperty(head);
        buddy_insert_free_block(head, order);

        index += block;
        remaining -= block;
    }
}

static struct Page *buddy_alloc_pages(size_t n) {
    assert(n > 0);

    size_t order, size;
    if (!buddy_order_for_pages(n, &order, &size)) {
        return NULL;
    }

    /* Smallest order whose free list can satisfy the request. */
    size_t found = order;
    while (found <= buddy_max_order &&
           list_empty(&buddy_free_area[found].free_list)) {
        found++;
    }
    if (found > buddy_max_order) {
        return NULL;
    }

    list_entry_t *le = list_next(&buddy_free_area[found].free_list);
    struct Page *page = le2page(le, page_link);
    assert(PageProperty(page));
    list_del_init(&page->page_link);
    buddy_free_area[found].nr_free -= (unsigned int)((size_t)1 << found);

    /* Split the block in half until it has the requested order; the
     * upper half of every split becomes a free block of the lower order. */
    while (found > order) {
        found--;
        struct Page *buddy = page + ((size_t)1 << found);
        buddy->property = (unsigned int)((size_t)1 << found);
        SetPageProperty(buddy);
        list_add(&buddy_free_area[found].free_list, &buddy->page_link);
        buddy_free_area[found].nr_free += (unsigned int)((size_t)1 << found);
    }

    /* Record the real block size on the allocated head and take it out of
     * the free accounting.  Internal pages keep property == 0. */
    page->property = (unsigned int)size;
    ClearPageProperty(page);
    buddy_nr_free -= size;
    return page;
}

static void buddy_free_pages(struct Page *base, size_t n) {
    assert(base != NULL);
    assert(n > 0);

    size_t order, size;
    assert(buddy_order_for_pages(n, &order, &size));

    const size_t managed = buddy_managed_pages();
    assert(managed > 0);
    const size_t index = (size_t)(base - pages);
    assert(index < managed);
    assert(size <= managed - index);
    /* Only the aligned start of an allocated block may be freed. */
    assert((index & (size - 1)) == 0);
    assert(!PageReserved(base));
    assert(!PageProperty(base));
    /* The head remembers the actual block size from alloc_pages(). */
    assert(base->property == (unsigned int)size);

    for (size_t i = 0; i < size; i++) {
        struct Page *p = base + i;
        assert(!PageReserved(p));
        assert(!PageProperty(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
        list_init(&p->page_link);
    }

    /* Mark the whole block free and merge it with free buddies. */
    base->property = (unsigned int)size;
    SetPageProperty(base);
    buddy_nr_free += size;
    buddy_insert_free_block(base, order);
}

static size_t buddy_nr_free_pages(void) {
    return buddy_nr_free;
}

static void buddy_check(void) {
    /* Implemented by a separate testing task. */
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
