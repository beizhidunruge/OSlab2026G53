#include <pmm.h>
#include <list.h>
#include <string.h>
#include <best_fit_pmm.h>
#include <stdio.h>

//lab2 请补充你在lab2中的代码
static free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

/* 初始化分配器：建立空闲块链表，并将空闲页总数清零。 */
static void
best_fit_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

/*
 * 将已设置首页标记和块长的空闲块按地址递增插入链表，
 * 再与物理相邻的前后空闲块合并，清除失效首页的标记和块长。
 * 空闲页计数由调用者更新，本函数只调整链表和块的组织方式。
 */
static void
best_fit_insert_free_block(struct Page *base) {
    list_entry_t *le = list_next(&free_list);
    while (le != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (base < p) {
            break;
        }
        le = list_next(le);
    }
    list_add_before(le, &(base->page_link));

    le = list_prev(&(base->page_link));
    if (le != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            base->property = 0;
            list_del(&(base->page_link));
            base = p;
        }
    }

    // 向前合并可能改变块首页，因此需要基于新的首页重新取得后继。
    le = list_next(&(base->page_link));
    if (le != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            p->property = 0;
            list_del(&(p->page_link));
        }
    }
}

/*
 * 将 base 起始的连续 n 个保留页登记为可分配页：清除原状态，
 * 重置引用计数，设置空闲块首页及长度，增加空闲页总数后插入并合并。
 */
static void
best_fit_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    best_fit_insert_free_block(base);
}

/*
 * 选择能容纳 n 页的最小空闲块；长度相同时选择地址最低者，
 * 精确匹配时可提前结束搜索。分配块的前 n 页，将剩余部分留在链表中，
 * 更新首页标记和空闲页计数，返回已分配区间的首页描述符。
 * 若总页数不足或不存在足够大的连续块，则返回 NULL，保持状态不变。
 */
static struct Page *
best_fit_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }

    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        // 链表按地址递增，仅在块更小时替换候选，保证同长度时选择最低地址。
        if (p->property >= n &&
            (page == NULL || p->property < page->property)) {
            page = p;
            if (p->property == n) {
                break;
            }
        }
    }
    if (page == NULL) {
        return NULL;
    }

    list_entry_t *prev = list_prev(&(page->page_link));
    list_del(&(page->page_link));
    if (page->property > n) {
        struct Page *remainder = page + n;
        remainder->property = page->property - n;
        SetPageProperty(remainder);
        list_add(prev, &(remainder->page_link));
    }
    ClearPageProperty(page);
    page->property = 0;
    nr_free -= n;
    return page;
}

/*
 * 释放 base 起始的连续 n 个已分配页：检查页面状态并重置元数据，
 * 建立空闲块、增加空闲页总数，再按地址插入并合并相邻空闲块。
 * 调用者须保证释放范围有效，且不包含保留页或已经空闲的页面。
 */
static void
best_fit_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    best_fit_insert_free_block(base);
}

/* 返回当前空闲页总数，不改变分配器状态。 */
static size_t
best_fit_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the best fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
best_fit_check(void) {
    int score = 0 ,sumscore = 6;
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    // * - - * -
    free_pages(p0 + 1, 2);
    free_pages(p0 + 4, 1);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 1) && p0[1].property == 2);
    // * - - * *
    assert((p1 = alloc_pages(1)) != NULL);
    assert(alloc_pages(2) != NULL);      // best fit feature
    assert(p0 + 4 == p1);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    p2 = p0 + 1;
    free_pages(p0, 5);
    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
}


