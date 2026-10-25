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

/*
 * ---------------------------------------------------------------------------
 * Buddy allocator self-test.
 *
 * The helpers below only read or drive the public alloc/free interface plus
 * the manager's own static free lists.  Tests that need an exactly known
 * block layout (split, merge, allocation failure) first take one aligned
 * 2^BUDDY_ARENA_ORDER page block away from the real allocator, save the real
 * free lists, install a private free-list state describing only that block,
 * and restore everything before returning the block.  While the private
 * state is installed the arena's own buddy just outside it is temporarily
 * marked reserved, so the controlled blocks can never merge with pages that
 * belong to the real allocator.
 * ---------------------------------------------------------------------------
 */

#define BUDDY_ARENA_ORDER 4
#define BUDDY_ARENA_PAGES ((size_t)1 << BUDDY_ARENA_ORDER)

/* Optional destructive tests for invalid releases.  These requests must
 * trip an assertion and halt the kernel, so only one case can run per boot
 * and they are disabled by default.  Override with
 * -DBUDDY_ILLEGAL_FREE_TEST=N (e.g. make DEFS=-DBUDDY_ILLEGAL_FREE_TEST=1):
 *   0 - disabled (default)
 *   1 - round_up(n) is smaller than the allocated block size
 *   2 - round_up(n) is larger than the allocated block size
 *   3 - the released block does not start at an aligned page
 */
#ifndef BUDDY_ILLEGAL_FREE_TEST
#define BUDDY_ILLEGAL_FREE_TEST 0
#endif

/* Discard every free block and reset the free-page counter.  Only used when
 * the real free-list state has been saved elsewhere. */
static void buddy_reset_free_lists(void) {
    for (size_t order = 0; order <= BUDDY_MAX_ORDER_LIMIT; order++) {
        list_init(&buddy_free_area[order].free_list);
        buddy_free_area[order].nr_free = 0;
    }
    buddy_nr_free = 0;
}

/* Number of blocks currently linked in the free list of the given order. */
static size_t buddy_list_count(size_t order) {
    list_entry_t *list = &buddy_free_area[order].free_list;
    list_entry_t *le = list;
    size_t count = 0;

    while ((le = list_next(le)) != list) {
        /* A corrupted or cyclic list must trip an assertion, not hang. */
        assert(++count <= buddy_managed_pages());
    }
    return count;
}

/* Sum of the block sizes (in pages) linked in the given order's list. */
static size_t buddy_list_pages(size_t order) {
    list_entry_t *list = &buddy_free_area[order].free_list;
    list_entry_t *le = list;
    size_t sum = 0;
    size_t guard = 0;

    while ((le = list_next(le)) != list) {
        struct Page *head = le2page(le, page_link);
        assert(++guard <= buddy_managed_pages());
        sum += head->property;
    }
    return sum;
}

/* Is page currently the head of a free block in the given order's list? */
static bool buddy_list_has(size_t order, struct Page *page) {
    list_entry_t *list = &buddy_free_area[order].free_list;
    list_entry_t *le = list;
    size_t guard = 0;

    while ((le = list_next(le)) != list) {
        assert(++guard <= buddy_managed_pages());
        if (le2page(le, page_link) == page) {
            return 1;
        }
    }
    return 0;
}

/* Is page registered in any free list? */
static bool buddy_any_list_has(struct Page *page) {
    for (size_t order = 0; order <= BUDDY_MAX_ORDER_LIMIT; order++) {
        if (buddy_list_has(order, page)) {
            return 1;
        }
    }
    return 0;
}

/* Independent oracle for the expected block size: the smallest power of two
 * that is at least n.  Deliberately not the manager's own helper, so a bug in
 * that helper cannot hide itself. */
static size_t buddy_test_round_up(size_t n) {
    assert(n > 0);
    size_t size = 1;
    while (size < n) {
        size <<= 1;
    }
    return size;
}

/* Check the invariants of a block just returned by alloc_pages(request). */
static void buddy_check_allocated(struct Page *base, size_t request) {
    assert(base != NULL);
    const size_t size = buddy_test_round_up(request);

    const size_t managed = buddy_managed_pages();
    const size_t index = (size_t)(base - pages);
    const size_t ppn = (size_t)page2ppn(base);

    /* Head of an allocated block: PG_property clear, property holds the
     * actual block size, the block fits in managed memory and starts at a
     * position aligned to its own size. */
    assert(!PageReserved(base));
    assert(!PageProperty(base));
    assert(base->property == (unsigned int)size);
    assert(index < managed);
    assert(size <= managed - index);
    assert((index & (size - 1)) == 0);
    assert(ppn >= nbase);
    assert(ppn + size <= npage);
    assert(ppn % size == 0);

    /* The block must not be visible in the free lists any more. */
    assert(!buddy_any_list_has(base));

    for (size_t i = 0; i < size; i++) {
        struct Page *page = base + i;
        assert(!PageReserved(page));
        assert(!PageProperty(page));
        assert(page_ref(page) == 0);
        if (i != 0) {
            /* Only the head keeps the block size. */
            assert(page->property == 0);
        }
    }
}

/* Validate every order list and the global free-page accounting. */
static void buddy_check_free_lists(void) {
    const size_t managed = buddy_managed_pages();
    size_t total = 0;

    for (size_t order = 0; order <= BUDDY_MAX_ORDER_LIMIT; order++) {
        list_entry_t *list = &buddy_free_area[order].free_list;
        const size_t size = (size_t)1 << order;
        size_t sum = 0;
        size_t blocks = 0;
        list_entry_t *le = list;

        while ((le = list_next(le)) != list) {
            assert(++blocks <= managed);
            struct Page *head = le2page(le, page_link);
            const size_t index = (size_t)(head - pages);

            /* A block of order k must be a 2^k page, 2^k aligned free head
             * whose whole range belongs to registered, non-reserved RAM. */
            assert(PageProperty(head));
            assert(head->property == (unsigned int)size);
            assert(index < managed);
            assert(size <= managed - index);
            assert((index & (size - 1)) == 0);
            assert((size_t)page2ppn(head) >= nbase);
            assert((size_t)page2ppn(head) + size <= npage);
            assert(!PageReserved(head));
            assert(page_ref(head) == 0);

            for (size_t i = 0; i < size; i++) {
                struct Page *page = head + i;
                assert(!PageReserved(page));
                if (i != 0) {
                    assert(!PageProperty(page));
                    assert(page->property == 0);
                    assert(page_ref(page) == 0);
                }
            }

            sum += size;
        }

        /* free_area_t::nr_free counts pages, so it must match the walk. */
        assert(buddy_free_area[order].nr_free == (unsigned int)sum);
        total += sum;
    }

    assert(total == buddy_nr_free);
    assert(nr_free_pages() == buddy_nr_free);

    /* No page may belong to two free blocks, whether the two blocks sit in
     * the same order list or in different ones.  This also catches a head
     * duplicated in a list. */
    for (size_t order1 = 0; order1 <= buddy_max_order; order1++) {
        const size_t size1 = (size_t)1 << order1;
        list_entry_t *list1 = &buddy_free_area[order1].free_list;

        for (list_entry_t *le1 = list_next(list1); le1 != list1;
             le1 = list_next(le1)) {
            struct Page *head1 = le2page(le1, page_link);
            const size_t index1 = (size_t)(head1 - pages);

            for (size_t order2 = 0; order2 <= buddy_max_order; order2++) {
                const size_t size2 = (size_t)1 << order2;
                list_entry_t *list2 = &buddy_free_area[order2].free_list;

                for (list_entry_t *le2 = list_next(list2); le2 != list2;
                     le2 = list_next(le2)) {
                    /* Skip only the block compared with itself; the same
                     * head showing up under a different order is an error. */
                    if (order1 == order2 && le1 == le2) {
                        continue;
                    }
                    struct Page *head2 = le2page(le2, page_link);
                    const size_t index2 = (size_t)(head2 - pages);
                    assert(index1 + size1 <= index2 ||
                           index2 + size2 <= index1);
                }
            }
        }
    }
}

/* Allocate and release three single pages, one at a time. */
static void buddy_basic_check(void) {
    const size_t free_before = nr_free_pages();
    assert(free_before >= 3);

    struct Page *p0 = alloc_page();
    struct Page *p1 = alloc_page();
    struct Page *p2 = alloc_page();
    assert(p0 != NULL);
    assert(p1 != NULL);
    assert(p2 != NULL);
    assert(p0 != p1 && p0 != p2 && p1 != p2);
    buddy_check_allocated(p0, 1);
    buddy_check_allocated(p1, 1);
    buddy_check_allocated(p2, 1);
    assert(nr_free_pages() == free_before - 3);

    /* Free only the middle page: the other two must stay allocated and must
     * not be linked into any free list. */
    free_page(p1);
    assert(nr_free_pages() == free_before - 2);
    assert(!PageProperty(p0) && p0->property == 1);
    assert(!PageProperty(p2) && p2->property == 1);
    assert(!buddy_any_list_has(p0));
    assert(!buddy_any_list_has(p2));
    buddy_check_free_lists();

    /* The freshly freed page (or another block) can satisfy one more
     * request, but it must not be p0 or p2. */
    struct Page *p3 = alloc_page();
    assert(p3 != NULL);
    assert(p3 != p0 && p3 != p2);
    buddy_check_allocated(p3, 1);
    assert(nr_free_pages() == free_before - 3);
    buddy_check_free_lists();

    free_page(p0);
    assert(nr_free_pages() == free_before - 2);
    free_page(p2);
    assert(nr_free_pages() == free_before - 1);
    free_page(p3);
    assert(nr_free_pages() == free_before);
    buddy_check_free_lists();
}

/* Requests whose size is not a power of two must be rounded up. */
static void buddy_sizes_check(void) {
    static const size_t requests[] = {2, 3, 5};
    static const size_t expected[] = {2, 4, 8};

    for (size_t i = 0; i < sizeof(requests) / sizeof(requests[0]); i++) {
        const size_t free_before = nr_free_pages();
        struct Page *page = alloc_pages(requests[i]);
        assert(page != NULL);
        buddy_check_allocated(page, requests[i]);
        assert(page->property == (unsigned int)expected[i]);
        assert(nr_free_pages() == free_before - expected[i]);

        free_pages(page, requests[i]);
        assert(nr_free_pages() == free_before);
        buddy_check_free_lists();
    }

    /* free_pages() only compares the rounded release argument with the
     * block head's property, so any n that rounds to the same power of two
     * must release the whole block correctly.  Allocate with one n and
     * release with a different one from the same bucket. */
    static const size_t same_bucket[][2] = {
        {3, 4}, {4, 3}, {5, 6}, {6, 5}, {5, 7}, {7, 5}, {5, 8}, {8, 7},
    };

    for (size_t i = 0;
         i < sizeof(same_bucket) / sizeof(same_bucket[0]); i++) {
        const size_t request = same_bucket[i][0];
        const size_t release = same_bucket[i][1];
        const size_t size = buddy_test_round_up(request);

        assert(request != release);
        assert(size == buddy_test_round_up(release));

        const size_t free_before = nr_free_pages();
        struct Page *page = alloc_pages(request);
        assert(page != NULL);
        buddy_check_allocated(page, request);
        assert(page->property == (unsigned int)size);
        assert(nr_free_pages() == free_before - size);

        free_pages(page, release);
        assert(nr_free_pages() == free_before);
        buddy_check_free_lists();
    }

    /* Hold four blocks of different real sizes at the same time and make
     * sure their page ranges never intersect. */
    static const size_t mixed_request[] = {1, 2, 3, 5};
    struct Page *blocks[4];
    size_t sizes[4];
    const size_t free_before = nr_free_pages();
    size_t taken = 0;

    for (size_t i = 0; i < 4; i++) {
        const size_t size = buddy_test_round_up(mixed_request[i]);
        blocks[i] = alloc_pages(mixed_request[i]);
        assert(blocks[i] != NULL);
        buddy_check_allocated(blocks[i], mixed_request[i]);
        sizes[i] = size;
        taken += size;
        assert(nr_free_pages() == free_before - taken);
    }

    for (size_t i = 0; i < 4; i++) {
        for (size_t j = i + 1; j < 4; j++) {
            const size_t start_i = (size_t)(blocks[i] - pages);
            const size_t start_j = (size_t)(blocks[j] - pages);
            assert(start_i + sizes[i] <= start_j ||
                   start_j + sizes[j] <= start_i);
        }
    }

    /* Release out of allocation order; every free adds exactly its real
     * block size, and the blocks still allocated stay out of the lists. */
    static const size_t release_order[] = {2, 0, 3, 1};
    bool freed[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < 4; i++) {
        const size_t slot = release_order[i];
        assert(!freed[slot]);
        free_pages(blocks[slot], mixed_request[slot]);
        freed[slot] = 1;
        taken -= sizes[slot];
        assert(nr_free_pages() == free_before - taken);

        for (size_t j = 0; j < 4; j++) {
            if (!freed[j]) {
                assert(!PageProperty(blocks[j]));
                assert(blocks[j]->property == (unsigned int)sizes[j]);
                assert(!buddy_any_list_has(blocks[j]));
            }
        }
    }

    assert(nr_free_pages() == free_before);
    buddy_check_free_lists();
}

/* Several rounds of mixed allocations released in a different order. */
static void buddy_repeat_check(void) {
    static const size_t rounds = 3;
    static const size_t requests[][4] = {
        {1, 3, 2, 5},
        {5, 2, 1, 3},
        {3, 1, 5, 2},
    };
    static const size_t release_order[][4] = {
        {1, 3, 0, 2},
        {2, 0, 3, 1},
        {3, 1, 0, 2},
    };

    for (size_t round = 0; round < rounds; round++) {
        const size_t free_before = nr_free_pages();
        struct Page *blocks[4];
        size_t sizes[4];
        bool freed[4] = {0, 0, 0, 0};
        size_t held = 0;

        for (size_t i = 0; i < 4; i++) {
            const size_t size = buddy_test_round_up(requests[round][i]);
            blocks[i] = alloc_pages(requests[round][i]);
            assert(blocks[i] != NULL);
            buddy_check_allocated(blocks[i], requests[round][i]);
            sizes[i] = size;
            held += size;
            assert(nr_free_pages() == free_before - held);
        }

        for (size_t i = 0; i < 4; i++) {
            const size_t slot = release_order[round][i];
            assert(!freed[slot]);
            free_pages(blocks[slot], requests[round][slot]);
            freed[slot] = 1;
            held -= sizes[slot];
            assert(nr_free_pages() == free_before - held);

            for (size_t j = 0; j < 4; j++) {
                if (!freed[j]) {
                    assert(!PageProperty(blocks[j]));
                    assert(blocks[j]->property == (unsigned int)sizes[j]);
                    assert(!buddy_any_list_has(blocks[j]));
                }
            }
        }

        assert(nr_free_pages() == free_before);
        buddy_check_free_lists();
    }
}

/* Bookkeeping needed to install and remove a controlled free-list state. */
struct buddy_controlled_state {
    struct Page *arena;      /* 2^BUDDY_ARENA_ORDER page block owned by us */
    struct Page *guard;      /* arena's buddy just outside, if it exists */
    uint64_t guard_flags;    /* its flags/property before the test */
    unsigned int guard_property;
    free_area_t saved_area[BUDDY_MAX_ORDER_LIMIT + 1];
    size_t saved_nr_free;
    size_t real_free_before;
};

/* Turn the whole arena into one free block of the highest order.  Any
 * previously installed controlled state is discarded first. */
static void buddy_controlled_clear(struct Page *arena) {
    buddy_reset_free_lists();
    for (size_t i = 0; i < BUDDY_ARENA_PAGES; i++) {
        struct Page *page = arena + i;
        page->flags = 0;
        page->property = 0;
        set_page_ref(page, 0);
        list_init(&page->page_link);
    }
}

static void buddy_controlled_make_full_free(struct Page *arena) {
    buddy_controlled_clear(arena);
    /* free_pages() checks the head against the size being released. */
    arena->property = (unsigned int)BUDDY_ARENA_PAGES;
    buddy_free_pages(arena, BUDDY_ARENA_PAGES);

    assert(nr_free_pages() == BUDDY_ARENA_PAGES);
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 1);
    assert(buddy_list_has(BUDDY_ARENA_ORDER, arena));
    assert(arena->property == (unsigned int)BUDDY_ARENA_PAGES);
    assert(PageProperty(arena));
    buddy_check_free_lists();
}

/* Take an aligned arena from the real allocator and replace the real free
 * lists with a private state containing the arena as one 16-page block. */
static void buddy_controlled_begin(struct buddy_controlled_state *state) {
    assert(buddy_max_order >= BUDDY_ARENA_ORDER);

    state->real_free_before = nr_free_pages();
    assert(state->real_free_before >= BUDDY_ARENA_PAGES);

    struct Page *arena = alloc_pages(BUDDY_ARENA_PAGES);
    assert(arena != NULL);
    const size_t arena_index = (size_t)(arena - pages);
    assert((arena_index & (BUDDY_ARENA_PAGES - 1)) == 0);
    state->arena = arena;

    /* Remember the real allocator state; it is put back in _end(). */
    for (size_t order = 0; order <= BUDDY_MAX_ORDER_LIMIT; order++) {
        state->saved_area[order] = buddy_free_area[order];
    }
    state->saved_nr_free = buddy_nr_free;

    /* The buddy of the whole arena lies just outside it.  If it exists,
     * mark it reserved while the controlled state is installed so the
     * arena can never merge with pages that still belong to the real
     * allocator.  Its exact flags/property are restored in _end(). */
    state->guard = NULL;
    state->guard_flags = 0;
    state->guard_property = 0;
    const size_t guard_index = arena_index ^ BUDDY_ARENA_PAGES;
    if (guard_index < buddy_managed_pages()) {
        struct Page *guard = pages + guard_index;
        state->guard = guard;
        state->guard_flags = guard->flags;
        state->guard_property = guard->property;
        SetPageReserved(guard);
    }

    buddy_controlled_make_full_free(arena);
}

/* Restore the real free lists and give the arena back to the allocator. */
static void buddy_controlled_end(struct buddy_controlled_state *state) {
    struct Page *arena = state->arena;

    /* Drop the controlled lists and leave the arena in the shape expected
     * of a freshly allocated block. */
    buddy_controlled_clear(arena);
    arena->property = (unsigned int)BUDDY_ARENA_PAGES;

    /* Re-enable the real buddy outside the arena. */
    if (state->guard != NULL) {
        state->guard->flags = state->guard_flags;
        state->guard->property = state->guard_property;
    }

    /* Put back the real free lists exactly as they were after the arena
     * allocation.  The real pages' links were never modified, so their
     * back-pointers to the (restored) list heads are still valid. */
    for (size_t order = 0; order <= BUDDY_MAX_ORDER_LIMIT; order++) {
        buddy_free_area[order] = state->saved_area[order];
    }
    buddy_nr_free = state->saved_nr_free;

    buddy_free_pages(arena, BUDDY_ARENA_PAGES);
    assert(nr_free_pages() == state->real_free_before);
    buddy_check_free_lists();
}

/* Higher-order split: alloc_pages(4) out of an isolated 16-page block. */
static void buddy_split_check(struct Page *arena) {
    const size_t total = BUDDY_ARENA_PAGES;

    buddy_controlled_make_full_free(arena);

    /* The allocation really has to split: only order 4 is populated. */
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 1);
    assert(buddy_list_has(BUDDY_ARENA_ORDER, arena));
    assert(buddy_list_count(3) == 0);
    assert(buddy_list_count(2) == 0);

    struct Page *page = alloc_pages(4);
    assert(page != NULL);
    assert(page == arena);
    assert(page->property == 4);
    assert(!PageProperty(page));
    assert(nr_free_pages() == total - 4);

    /* The 16-page block is gone; the untouched halves must be the 8-page
     * block at arena+8 and the 4-page block at arena+4. */
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 0);
    assert(buddy_list_count(3) == 1);
    assert(buddy_list_pages(3) == 8);
    assert(buddy_list_has(3, arena + 8));
    assert((arena + 8)->property == 8);
    assert(PageProperty(arena + 8));
    assert(buddy_list_count(2) == 1);
    assert(buddy_list_pages(2) == 4);
    assert(buddy_list_has(2, arena + 4));
    assert((arena + 4)->property == 4);
    assert(PageProperty(arena + 4));
    /* The allocated block is not in any free list, and the free blocks do
     * not overlap it. */
    assert(!buddy_any_list_has(page));
    assert(buddy_list_pages(3) + buddy_list_pages(2) == total - 4);
    buddy_check_free_lists();

    /* The split remainders can satisfy a smaller request. */
    struct Page *small = alloc_pages(2);
    assert(small != NULL);
    assert(small == arena + 4);
    assert(small->property == 2);
    assert(!PageProperty(small));
    assert(nr_free_pages() == total - 6);
    /* The selected 4-page block was split once: the upper 2-page half is
     * parked in the order-1 list, leaving order 2 empty. */
    assert(buddy_list_count(2) == 0);
    assert(buddy_list_count(1) == 1);
    assert(buddy_list_has(1, arena + 6));
    assert((arena + 6)->property == 2);
    assert(PageProperty(arena + 6));
    assert(buddy_list_count(3) == 1);
    assert(buddy_list_has(3, arena + 8));
    buddy_check_free_lists();

    /* Freeing it merges back with its 2-page buddy but stops at the
     * still-allocated 4-page block at arena. */
    free_pages(small, 2);
    assert(nr_free_pages() == total - 4);
    assert(buddy_list_count(2) == 1);
    assert(buddy_list_has(2, arena + 4));
    assert((arena + 4)->property == 4);
    assert(PageProperty(arena + 4));
    buddy_check_free_lists();

    /* Returning the allocated 4-page block rebuilds the whole 16-page
     * block (the arena guard stops it from growing further). */
    free_pages(page, 4);
    assert(nr_free_pages() == total);
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 1);
    assert(buddy_list_has(BUDDY_ARENA_ORDER, arena));
    assert(arena->property == total);
    assert(PageProperty(arena));
    assert(buddy_list_count(2) == 0);
    assert(buddy_list_count(3) == 0);
    buddy_check_free_lists();
}

/* Buddy coalescing, including the recursive step. */
static void buddy_merge_check(struct Page *arena) {
    const size_t total = BUDDY_ARENA_PAGES;

    buddy_controlled_make_full_free(arena);

    struct Page *a = alloc_pages(4);
    struct Page *b = alloc_pages(4);
    assert(a != NULL && b != NULL);
    assert(a == arena);
    assert(b == arena + 4);
    assert(nr_free_pages() == total - 8);
    buddy_check_free_lists();

    /* Case 1: b is still allocated, so freeing a cannot create an 8-page
     * free block even though an 8-page block sits at arena+8. */
    free_pages(a, 4);
    assert(nr_free_pages() == total - 4);
    assert(buddy_list_count(2) == 1);
    assert(buddy_list_has(2, arena));
    assert(buddy_list_count(3) == 1);
    assert(buddy_list_has(3, arena + 8));
    assert(!buddy_list_has(3, arena));
    assert(!PageProperty(b));
    buddy_check_free_lists();

    /* Occupy arena+8 so the next merge can be observed at order 3. */
    struct Page *c = alloc_pages(8);
    assert(c != NULL);
    assert(c == arena + 8);
    assert(c->property == 8);
    assert(!PageProperty(c));
    assert(nr_free_pages() == total - 12);
    assert(buddy_list_count(3) == 0);
    assert(buddy_list_count(2) == 1);
    assert(buddy_list_has(2, arena));
    buddy_check_free_lists();

    /* Case 2: both 4-page buddies are now free: they must coalesce into
     * one 8-page block.  Only 4 pages are added to the free count. */
    free_pages(b, 4);
    assert(nr_free_pages() == total - 8);
    assert(buddy_list_count(2) == 0);
    assert(buddy_list_count(3) == 1);
    assert(buddy_list_pages(3) == 8);
    assert(buddy_list_has(3, arena));
    assert(arena->property == 8);
    assert(PageProperty(arena));
    assert(!buddy_list_has(2, arena));
    assert(!buddy_list_has(2, arena + 4));
    buddy_check_free_lists();

    /* Case 3: the 8-page block at arena+8 is free too, so the new 8-page
     * block must keep merging upwards into a 16-page block. */
    free_pages(c, 8);
    assert(nr_free_pages() == total);
    assert(buddy_list_count(3) == 0);
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 1);
    assert(buddy_list_has(BUDDY_ARENA_ORDER, arena));
    assert(arena->property == total);
    assert(PageProperty(arena));
    buddy_check_free_lists();
}

/* Blocks that are not buddies must never coalesce. */
static void buddy_no_merge_check(struct Page *arena) {
    const size_t total = BUDDY_ARENA_PAGES;

    buddy_controlled_make_full_free(arena);

    struct Page *a = alloc_pages(4);
    struct Page *b = alloc_pages(4);
    struct Page *c = alloc_pages(4);
    struct Page *d = alloc_pages(4);
    assert(a != NULL && b != NULL && c != NULL && d != NULL);
    assert(a == arena);
    assert(b == arena + 4);
    assert(c == arena + 8);
    assert(d == arena + 12);
    assert(nr_free_pages() == 0);
    buddy_check_free_lists();

    /* a and c are both 4 pages but they are not buddies (they do not share
     * a parent), so they must stay separate 4-page free blocks. */
    free_pages(a, 4);
    free_pages(c, 4);
    assert(nr_free_pages() == 8);
    assert(buddy_list_count(2) == 2);
    assert(buddy_list_pages(2) == 8);
    assert(buddy_list_has(2, arena));
    assert(buddy_list_has(2, arena + 8));
    assert(buddy_list_count(3) == 0);
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 0);
    buddy_check_free_lists();

    /* Freeing b merges a+b into an 8-page block at arena.  The adjacent
     * 4-page block at arena+8 has a different size and must not be
     * absorbed, so no 16-page block may appear. */
    free_pages(b, 4);
    assert(nr_free_pages() == 12);
    assert(buddy_list_count(3) == 1);
    assert(buddy_list_pages(3) == 8);
    assert(buddy_list_has(3, arena));
    assert(arena->property == 8);
    assert(PageProperty(arena));
    assert(buddy_list_count(2) == 1);
    assert(buddy_list_has(2, arena + 8));
    assert((arena + 8)->property == 4);
    assert(PageProperty(arena + 8));
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 0);
    buddy_check_free_lists();

    /* Only now can the two 8-page buddies (arena and arena+8) coalesce. */
    free_pages(d, 4);
    assert(nr_free_pages() == total);
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 1);
    assert(buddy_list_has(BUDDY_ARENA_ORDER, arena));
    assert(arena->property == total);
    assert(PageProperty(arena));
    assert(buddy_list_count(2) == 0);
    assert(buddy_list_count(3) == 0);
    buddy_check_free_lists();

    /* Equal-size blocks can also be physically adjacent without being
     * buddies: for order 2 the buddy of [arena+4, arena+8) is arena, and
     * the buddy of [arena+8, arena+12) is arena+12.  With a and d still
     * allocated, freeing b and c must leave two separate 4-page blocks
     * even though they touch. */
    buddy_controlled_make_full_free(arena);
    a = alloc_pages(4);
    b = alloc_pages(4);
    c = alloc_pages(4);
    d = alloc_pages(4);
    assert(a == arena);
    assert(b == arena + 4);
    assert(c == arena + 8);
    assert(d == arena + 12);
    assert(b + 4 == c); /* the two candidates are physically adjacent */
    assert(nr_free_pages() == 0);

    free_pages(b, 4);
    free_pages(c, 4);
    assert(nr_free_pages() == 8);
    assert(buddy_list_count(2) == 2);
    assert(buddy_list_pages(2) == 8);
    assert(buddy_list_has(2, arena + 4));
    assert(buddy_list_has(2, arena + 8));
    assert(buddy_list_count(3) == 0);
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 0);
    buddy_check_free_lists();

    /* Giving back a may only form the legitimate 8-page block with its
     * real buddy b; the adjacent block c stays outside it. */
    free_pages(a, 4);
    assert(nr_free_pages() == 12);
    assert(buddy_list_count(3) == 1);
    assert(buddy_list_has(3, arena));
    assert(arena->property == 8);
    assert(PageProperty(arena));
    assert(buddy_list_count(2) == 1);
    assert(buddy_list_has(2, arena + 8));
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 0);
    buddy_check_free_lists();

    /* d is c's buddy, so this release completes both merges up to 16. */
    free_pages(d, 4);
    assert(nr_free_pages() == total);
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 1);
    assert(buddy_list_has(BUDDY_ARENA_ORDER, arena));
    assert(arena->property == total);
    assert(PageProperty(arena));
    assert(buddy_list_count(2) == 0);
    assert(buddy_list_count(3) == 0);
    buddy_check_free_lists();
}

/* Allocation failures must leave the free lists untouched. */
static void buddy_failure_check(struct Page *arena) {
    const size_t total = BUDDY_ARENA_PAGES;

    /* Case 1: ask for more pages than the controlled arena can provide. */
    buddy_controlled_make_full_free(arena);
    assert(nr_free_pages() == total);

    struct Page *denied = alloc_pages(total + 1);
    assert(denied == NULL);
    assert(nr_free_pages() == total);
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 1);
    assert(buddy_list_has(BUDDY_ARENA_ORDER, arena));
    assert(arena->property == total);
    assert(PageProperty(arena));
    buddy_check_free_lists();

    /* Case 2: enough pages in total, but no block big enough to split.
     * Build eight isolated 2-page blocks by hand so nothing coalesces. */
    buddy_controlled_clear(arena);
    for (size_t i = 0; i < BUDDY_ARENA_PAGES / 2; i++) {
        struct Page *head = arena + 2 * i;
        head->property = 2;
        SetPageProperty(head);
        list_add(&buddy_free_area[1].free_list, &head->page_link);
        buddy_free_area[1].nr_free += 2;
    }
    buddy_nr_free = total;

    assert(nr_free_pages() == total);
    assert(buddy_list_count(1) == 8);
    assert(buddy_list_pages(1) == total);
    /* Confirm there is no higher-order block that could satisfy 4 pages. */
    for (size_t order = 2; order <= BUDDY_MAX_ORDER_LIMIT; order++) {
        assert(buddy_list_count(order) == 0);
    }
    buddy_check_free_lists();

    /* Take one 2-page block so we can also verify that an already
     * allocated block is not resurrected by a failed allocation. */
    struct Page *held = alloc_pages(2);
    assert(held != NULL);
    assert(held->property == 2);
    assert(!PageProperty(held));
    assert(nr_free_pages() == total - 2);
    assert(buddy_list_count(1) == 7);
    assert(buddy_list_pages(1) == total - 2);
    assert(!buddy_any_list_has(held));
    for (size_t i = 0; i < BUDDY_ARENA_PAGES / 2; i++) {
        struct Page *head = arena + 2 * i;
        assert(buddy_list_has(1, head) == (head != held));
    }
    buddy_check_free_lists();

    denied = alloc_pages(4);
    assert(denied == NULL);
    assert(nr_free_pages() == total - 2);
    assert(buddy_list_count(1) == 7);
    assert(buddy_list_pages(1) == total - 2);
    for (size_t order = 2; order <= BUDDY_MAX_ORDER_LIMIT; order++) {
        assert(buddy_list_count(order) == 0);
    }
    for (size_t i = 0; i < BUDDY_ARENA_PAGES / 2; i++) {
        struct Page *head = arena + 2 * i;
        assert(buddy_list_has(1, head) == (head != held));
        if (head != held) {
            assert(head->property == 2);
            assert(PageProperty(head));
        }
    }
    /* The block that was already allocated stays allocated. */
    assert(!PageProperty(held));
    assert(held->property == 2);
    assert(!buddy_any_list_has(held));
    buddy_check_free_lists();

    /* Return the held block and rebuild the full arena. */
    free_pages(held, 2);
    assert(nr_free_pages() == total);
    buddy_check_free_lists();
    buddy_controlled_make_full_free(arena);
}

/* Register ranges through buddy_init_memmap() and verify the resulting
 * blocks.  The arena pages are temporarily marked reserved (the state a
 * freshly discovered memory range is in) before each registration. */
static void buddy_init_memmap_check(struct Page *arena) {
    const size_t total = BUDDY_ARENA_PAGES;

    /* Case 1: the length 12 is not a power of two and arena+4 is aligned
     * only to 4 pages, not to a larger block.  The greedy split must turn
     * [arena+4, arena+16) into a 4-page block at arena+4 followed by an
     * 8-page block at arena+8, and must not grow into the unregistered
     * pages below arena+4. */
    buddy_controlled_clear(arena);
    for (size_t i = 0; i < total; i++) {
        SetPageReserved(arena + i);
    }
    assert(nr_free_pages() == 0);

    buddy_init_memmap(arena + 4, 12);
    assert(nr_free_pages() == 12);
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 0);
    assert(buddy_list_count(3) == 1);
    assert(buddy_list_pages(3) == 8);
    assert(buddy_list_has(3, arena + 8));
    assert((arena + 8)->property == 8);
    assert(PageProperty(arena + 8));
    assert(buddy_list_count(2) == 1);
    assert(buddy_list_pages(2) == 4);
    assert(buddy_list_has(2, arena + 4));
    assert((arena + 4)->property == 4);
    assert(PageProperty(arena + 4));
    assert(buddy_list_pages(2) + buddy_list_pages(3) == 12);
    for (size_t i = 0; i < 4; i++) {
        assert(PageReserved(arena + i));
        assert(!PageProperty(arena + i));
    }
    buddy_check_free_lists();

    /* Case 2: adjacent ranges registered by separate calls must coalesce
     * as if they had been registered together.  The upper 4 pages are
     * registered first so both merge directions are exercised: first a
     * higher-addressed buddy is absorbed, then a lower-addressed buddy
     * becomes the merged head. */
    buddy_controlled_clear(arena);
    for (size_t i = 0; i < total; i++) {
        SetPageReserved(arena + i);
    }

    /* The upper half on its own: no free buddy, so it stays alone. */
    buddy_init_memmap(arena + 4, 4);
    assert(nr_free_pages() == 4);
    assert(buddy_list_count(2) == 1);
    assert(buddy_list_has(2, arena + 4));
    buddy_check_free_lists();

    /* Registering the lower half: its upper buddy arena+4 is already free,
     * so the new head absorbs it. */
    buddy_init_memmap(arena, 4);
    assert(nr_free_pages() == 8);
    assert(buddy_list_count(2) == 0);
    assert(buddy_list_count(3) == 1);
    assert(buddy_list_has(3, arena));
    assert(arena->property == 8);
    assert(PageProperty(arena));
    buddy_check_free_lists();

    /* The lower 8-page buddy at arena is already free, so it becomes the
     * merged head and the two form the complete 16-page range. */
    buddy_init_memmap(arena + 8, 8);
    assert(nr_free_pages() == total);
    assert(buddy_list_count(3) == 0);
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 1);
    assert(buddy_list_has(BUDDY_ARENA_ORDER, arena));
    assert(arena->property == total);
    assert(PageProperty(arena));
    buddy_check_free_lists();

    /* Case 3: separate calls registering ranges with a gap between them
     * must not merge across the unregistered pages. */
    buddy_controlled_clear(arena);
    for (size_t i = 0; i < total; i++) {
        SetPageReserved(arena + i);
    }

    buddy_init_memmap(arena, 4);
    buddy_init_memmap(arena + 8, 4);
    assert(nr_free_pages() == 8);
    assert(buddy_list_count(2) == 2);
    assert(buddy_list_pages(2) == 8);
    assert(buddy_list_has(2, arena));
    assert(buddy_list_has(2, arena + 8));
    assert(buddy_list_count(3) == 0);
    assert(buddy_list_count(BUDDY_ARENA_ORDER) == 0);
    /* The gap [arena+4, arena+8) stays reserved and outside both blocks. */
    for (size_t i = 4; i < 8; i++) {
        assert(PageReserved(arena + i));
        assert(!PageProperty(arena + i));
        assert(!buddy_any_list_has(arena + i));
    }
    /* Eight pages are free, but not as one contiguous 8-page block. */
    struct Page *denied = alloc_pages(8);
    assert(denied == NULL);
    assert(nr_free_pages() == 8);
    buddy_check_free_lists();
}

static void buddy_controlled_check(void) {
    struct buddy_controlled_state state;

    buddy_controlled_begin(&state);
    buddy_init_memmap_check(state.arena);
    buddy_split_check(state.arena);
    buddy_merge_check(state.arena);
    buddy_no_merge_check(state.arena);
    buddy_failure_check(state.arena);
    buddy_controlled_end(&state);
}

#if BUDDY_ILLEGAL_FREE_TEST != 0
/* Invalid releases: free_pages() must reject each of them with an assert.
 * Because the resulting panic stops the kernel, this runs only when a case
 * is explicitly selected with BUDDY_ILLEGAL_FREE_TEST. */
static void buddy_illegal_free_check(void) {
    struct buddy_controlled_state state;

    buddy_controlled_begin(&state);
    buddy_controlled_make_full_free(state.arena);

    /* A 4-page block taken from the 16-page aligned arena starts at arena,
     * so its start is aligned for any larger release as well; that way
     * cases 1 and 2 reach the size check instead of the alignment check. */
    struct Page *page = alloc_pages(4);
    assert(page != NULL);
    assert(page == state.arena);
    assert(page->property == 4);

#if BUDDY_ILLEGAL_FREE_TEST == 1
    /* round_up(2) = 2 pages is smaller than the 4-page block. */
    free_pages(page, 2);
    panic("free_pages accepted a release smaller than the block");
#elif BUDDY_ILLEGAL_FREE_TEST == 2
    /* round_up(5) = 8 pages is larger than the 4-page block. */
    free_pages(page, 5);
    panic("free_pages accepted a release larger than the block");
#elif BUDDY_ILLEGAL_FREE_TEST == 3
    /* arena+1 is not the aligned start of the allocated 4-page block. */
    free_pages(page + 1, 4);
    panic("free_pages accepted an unaligned block start");
#else
#error "BUDDY_ILLEGAL_FREE_TEST must be 0, 1, 2 or 3"
#endif
}
#endif /* BUDDY_ILLEGAL_FREE_TEST != 0 */

static void buddy_check(void) {
    const size_t free_before = nr_free_pages();
    assert(free_before > 0);

    buddy_check_free_lists();

    buddy_basic_check();
    buddy_sizes_check();
    buddy_repeat_check();

    buddy_check_free_lists();

    buddy_controlled_check();

    /* Every page taken during the test was returned and the order lists
     * are still consistent. */
    assert(nr_free_pages() == free_before);
    buddy_check_free_lists();

#if BUDDY_ILLEGAL_FREE_TEST != 0
    /* Deliberately invalid releases; expected to panic here. */
    buddy_illegal_free_check();
#endif
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
