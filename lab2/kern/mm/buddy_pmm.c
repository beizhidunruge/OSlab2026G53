//lab2 function2-3 2412604
#include <pmm.h>
#include <list.h>
#include <buddy_pmm.h>

/* 第 k 阶链表管理 2^k 页块；本实验128 MiB内存上限对应最高阶15。 */
#define BUDDY_MAX_ORDER 15
static list_entry_t free_list[BUDDY_MAX_ORDER + 1];
static size_t nr_free;  // 空闲页总数，分裂与合并不重复计数。
static unsigned int max_order;  // 正常为15。

/* 空闲块头设置 PG_property，并记录实际页数；已分配块头清除此标志，
 * 但保留大小供释放校验；块内其他页的 property 为0。
 * 伙伴计算使用 page-pages 的全局下标，描述符数量为 npage-nbase。
 * 本平台 nbase 按所有支持的块大小对齐，因此也满足物理页号的对齐要求。 */

/* 计算容纳 n 页的最小阶；超出最高阶时返回 max_order+1，避免移位溢出。 */
static unsigned int buddy_order(size_t n) {
    unsigned int order = 0;
    while (order < max_order && ((size_t)1 << order) < n) {
        order++;
    }
    return ((size_t)1 << order) < n ? max_order + 1 : order;
}

/* 设置空闲块头并挂入对应阶链表，空闲页计数由调用者维护。 */
static void buddy_add_block(struct Page *page, unsigned int order) {
    page->property = (unsigned int)((size_t)1 << order);
    SetPageProperty(page);
    list_add(&free_list[order], &page->page_link);
}

/* 初始化各阶链表和计数，后续由 init_memmap() 登记实际可用页面。 */
static void buddy_init(void) {
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&free_list[order]);
    }
    max_order = BUDDY_MAX_ORDER;
    nr_free = 0;
}

/* 按全局页下标的对齐和剩余长度，将连续区间划分为尽可能大的二次幂块。
 * 完整登记所有输入页 */
static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(base != NULL && n > 0);
    const size_t managed = npage - nbase;
    size_t index = (size_t)(base - pages);
    assert(index < managed && n <= managed - index);

    /* 确认页面尚未登记，清理页状态和链表节点。 */
    for (size_t i = 0; i < n; i++) {
        struct Page *page = base + i;
        assert(PageReserved(page));
        page->flags = page->property = 0;
        set_page_ref(page, 0);
        list_init(&page->page_link);
    }
    nr_free += n;

    /* 从低地址开始，逐块选取满足对齐和剩余长度的最大块。 */
    while (n > 0) {
        unsigned int order = 0;
        while (order < max_order) {
            size_t next_size = (size_t)1 << (order + 1);
            if (next_size > n || (index & (next_size - 1)) != 0) {
                break;
            }
            order++;
        }
        size_t size = (size_t)1 << order;
        buddy_add_block(pages + index, order);
        index += size;
        n -= size;
    }
}

/* 将请求取整后，从最小可用阶取块并逐级分裂，多余的后半块留在空闲链表。
 * 已分配块头记录实际大小，供释放时还原整个分配块。 */
static struct Page *buddy_alloc_pages(size_t n) {
    assert(n > 0);
    unsigned int order = buddy_order(n);
    unsigned int found = order;
    /* 从目标阶向上查找；先判断阶数，再访问链表，避免超大请求越界。 */
    while (found <= max_order && list_empty(&free_list[found])) {
        found++;
    }
    if (found > max_order) {
        return NULL;
    }

    struct Page *page = le2page(list_next(&free_list[found]), page_link);
    list_del_init(&page->page_link);
    ClearPageProperty(page);

    /* 逐级对半分裂：保留前半块，后半块加入低一阶链表。 */
    while (found > order) {
        found--;
        buddy_add_block(page + ((size_t)1 << found), found);
    }
    /* 记录取整后的实际大小，并按实际页数记账。 */
    page->property = (unsigned int)((size_t)1 << order);
    nr_free -= page->property;
    return page;
}

/* 按取整大小校验完整分配块，释放后用异或查找同阶空闲伙伴并向上合并。
 * 合并保留低地址块头，空闲页数只增加本次归还的实际大小。 */
static void buddy_free_pages(struct Page *base, size_t n) {
    assert(base != NULL && n > 0);
    unsigned int order = buddy_order(n);
    assert(order <= max_order);
    size_t size = (size_t)1 << order;
    const size_t managed = npage - nbase;
    size_t index = (size_t)(base - pages);
    assert(index < managed && size <= managed - index);
    /* 校验起点对齐、分配状态和实际大小，拒绝内部地址、错误大小及重复释放。 */
    assert((index & (size - 1)) == 0);
    assert(!PageReserved(base) && !PageProperty(base));
    assert(base->property == size);

    /* 清理原块元数据并归还本次页数，最终只设置合并后的块头。 */
    for (size_t i = 0; i < size; i++) {
        struct Page *page = base + i;
        assert(!PageReserved(page) && !PageProperty(page));
        page->flags = page->property = 0;
        set_page_ref(page, 0);
        list_init(&page->page_link);
    }
    nr_free += size;

    /* 同阶伙伴空闲时摘下合并；遇到边界或状态、大小不匹配时停止。 */
    while (order < max_order) {
        size_t buddy_index = index ^ size;
        if (buddy_index >= managed || size > managed - buddy_index) {
            break;
        }
        struct Page *buddy = pages + buddy_index;
        if (PageReserved(buddy) || !PageProperty(buddy) ||
            buddy->property != size) {
            break;
        }
        /* 清除旧伙伴头，选择低地址一半作为下一轮候选块。 */
        list_del_init(&buddy->page_link);
        ClearPageProperty(buddy);
        buddy->property = 0;
        if (buddy_index < index) {
            base = buddy;
            index = buddy_index;
        }
        order++;
        size <<= 1;
    }
    buddy_add_block(base, order);
}

/* 直接返回维护好的空闲页总数。 */
static size_t buddy_nr_free_pages(void) {
    return nr_free;
}

/* 非法释放测试：默认模式0关闭，模式1检查错误大小，模式3检查起点未对齐。
 * 通过 DEFS=-DBUDDY_ILLEGAL_FREE_TEST=1 或3重新编译，每次启动验证一种。
 * 必须触发对应断言；其他 panic、超时或失败兜底均不算通过。 */
#ifndef BUDDY_ILLEGAL_FREE_TEST
#define BUDDY_ILLEGAL_FREE_TEST 0
#endif
#if BUDDY_ILLEGAL_FREE_TEST != 0 && BUDDY_ILLEGAL_FREE_TEST != 1 && \
    BUDDY_ILLEGAL_FREE_TEST != 3
#error "BUDDY_ILLEGAL_FREE_TEST must be 0, 1 or 3"
#endif

#define BUDDY_TEST_ORDER 4
#define BUDDY_TEST_PAGES ((size_t)1 << BUDDY_TEST_ORDER)

/* 局部核对空闲块的标志、大小及其是否实际挂在对应阶链表中。 */
static void buddy_check_block(struct Page *page, unsigned int order) {
    assert(PageProperty(page));
    assert(page->property == ((size_t)1 << order));
    list_entry_t *head = &free_list[order];
    list_entry_t *le = list_next(head);
    while (le != head && le != &page->page_link) {
        le = list_next(le);
    }
    assert(le == &page->page_link);
}

/* 将已借出的16页模拟为尚未登记的保留页，供初始化用例使用。 */
static void buddy_prepare_memmap(struct Page *base) {
    for (size_t i = 0; i < BUDDY_TEST_PAGES; i++) {
        base[i].flags = base[i].property = 0;
        SetPageReserved(base + i);
        set_page_ref(base + i, 0);
        list_init(&base[i].page_link);
    }
}

/* 在同一16页区间内，通过连续申请和释放验证取整、分裂余块及精确匹配。
 * 结束后恢复完整16页块，交给后续综合测试。 */
static void buddy_basic_check(struct Page *base) {
    /* 申请3页应实际占4页，余块为4、8页。
     * 检查块头状态、余块挂链及实际扣减的页数。 */
    struct Page *a = alloc_pages(3);
    assert(a == base && a->property == 4 && !PageProperty(a));
    assert(nr_free_pages() == 12);
    buddy_check_block(base + 4, 2);
    buddy_check_block(base + 8, 3);

    /* 再申请1页应优先分裂4页余块；释放后恢复该余块，供下一步精确匹配。 */
    struct Page *small = alloc_page();
    assert(small == base + 4 && small->property == 1);
    assert(page_ref(small) == 0);
    free_page(small);

    /* 申请4页直接复用刚恢复的余块，最后按原始请求大小释放两块。 */
    struct Page *b = alloc_pages(4);
    assert(b == base + 4 && b->property == 4);
    free_pages(a, 3);  // 按3页请求归还实际占用的4页。
    free_pages(b, 4);
}

/* 借出16页隔离真实状态，依次检查初始化、分配、合并和失败边界。
 * 正常模式最终恢复真实状态；非法模式单独验证目标断言会拒绝错误调用。 */
static void buddy_check(void) {
    /* 借出16页作为受控区域，记录真实空闲总量。 */
    const size_t free_before = nr_free_pages();
    struct Page *base = alloc_pages(BUDDY_TEST_PAGES);
    assert(base != NULL);

    /* 保存并清空链表头，真实空闲节点留在原位，结束时恢复链表头即可接回。
     * 临时将最高阶设为4，阻止测试区向外合并。 */
    list_entry_t saved_list[BUDDY_MAX_ORDER + 1];
    const size_t saved_nr_free = nr_free;
    const unsigned int saved_max_order = max_order;
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        saved_list[order] = free_list[order];
        list_init(&free_list[order]);
    }
    nr_free = 0;
    max_order = BUDDY_TEST_ORDER;

    /* 完整16页初始化：应只有一个第4阶块，再执行基础分配测试。 */
    buddy_prepare_memmap(base);
    buddy_init_memmap(base, BUDDY_TEST_PAGES);
    buddy_check_block(base, BUDDY_TEST_ORDER);
    assert(nr_free_pages() == BUDDY_TEST_PAGES);
    for (unsigned int order = 0; order < BUDDY_TEST_ORDER; order++) {
        assert(list_empty(&free_list[order]));
    }
    buddy_basic_check(base);

    /* 分配 A、B、C、D 四个4页块，检查互不重叠及耗尽后分配失败。 */
    struct Page *block[4];
    for (size_t i = 0; i < 4; i++) {
        block[i] = alloc_pages(4);
        assert(block[i] == base + 4 * i);
    }
    assert(nr_free_pages() == 0);
    assert(alloc_page() == NULL);

    /* 释放B、C：相邻但非伙伴，应保持两个4页块。
     * 申请8页、16页分别验证碎片与容量不足，再检查失败未改变布局和计数。 */
    free_pages(block[1], 4);
    free_pages(block[2], 4);
    assert(alloc_pages(8) == NULL);
    assert(alloc_pages(16) == NULL);
    assert(nr_free_pages() == 8);
    buddy_check_block(block[1], 2);
    buddy_check_block(block[2], 2);
    unsigned int count = 0;
    for (list_entry_t *le = list_next(&free_list[2]);
         le != &free_list[2]; le = list_next(le)) {
        assert(++count <= 2);
    }
    assert(count == 2);
    assert(list_empty(&free_list[3]) && list_empty(&free_list[4]));

    /* 释放A：A+B合并为8页，遇到仅4页的C停止；旧块头B应清零。 */
    free_pages(block[0], 4);
    buddy_check_block(base, 3);
    buddy_check_block(block[2], 2);
    assert(!PageProperty(block[1]) && block[1]->property == 0);

    /* 释放D：先C+D合并，再与A+B合并成16页，检查递归合并和低阶节点清理。
     * 结合释放A的路径，覆盖向两个方向寻找伙伴。 */
    free_pages(block[3], 4);
    buddy_check_block(base, 4);
    assert(nr_free_pages() == BUDDY_TEST_PAGES);
    for (unsigned int order = 0; order < BUDDY_TEST_ORDER; order++) {
        assert(list_empty(&free_list[order]));
    }
    for (size_t i = 1; i < 4; i++) {
        assert(!PageProperty(block[i]) && block[i]->property == 0);
    }
    /* 大请求失败后仍应能申请完整16页，验证失败不改状态且合并后的大块可用。 */
    assert(alloc_pages(17) == NULL);
    assert(alloc_pages((size_t)-1) == NULL);
    assert(alloc_pages(BUDDY_TEST_PAGES) == base);
    assert(nr_free_pages() == 0);

    /* 只登记偏移 [3,16) 的13页，应形成1、4、8页块，前3页保持保留状态。
     * 检查未对齐起点、非二次幂长度及登记范围。 */
    buddy_prepare_memmap(base);
    buddy_init_memmap(base + 3, 13);
    assert(nr_free_pages() == 13);
    buddy_check_block(base + 3, 0);
    buddy_check_block(base + 4, 2);
    buddy_check_block(base + 8, 3);
    for (size_t i = 0; i < 3; i++) {
        assert(PageReserved(base + i) && !PageProperty(base + i));
    }
    /* 取走预期的三个块，确认初始化结果可用，全部取走后没有残留节点。 */
    assert(alloc_page() == base + 3);
    assert(alloc_pages(4) == base + 4);
    assert(alloc_pages(8) == base + 8);
    assert(nr_free_pages() == 0 && alloc_page() == NULL);
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        assert(list_empty(&free_list[order]));
    }

    /* 整个16页始终归测试所有，清除临时布局，还原为最初借入的已分配块。 */
    for (size_t i = 0; i < BUDDY_TEST_PAGES; i++) {
        base[i].flags = base[i].property = 0;
    }
    base->property = BUDDY_TEST_PAGES;

#if BUDDY_ILLEGAL_FREE_TEST != 0
    /* 在已知16页区间内申请4页，分别验证大小不匹配和起点未对齐断言。
     * 错误调用若返回，则执行 TEST FAILED；此兜底表示测试失败。 */
    free_pages(base, BUDDY_TEST_PAGES);
    struct Page *p = alloc_pages(4);
    assert(p == base && p->property == 4);
#if BUDDY_ILLEGAL_FREE_TEST == 1
    free_pages(p, 2);
    panic("TEST FAILED: mismatched release size was accepted");
#else
    free_pages(p + 1, 4);  // 范围合法，应触发对齐断言。
    panic("TEST FAILED: unaligned release was accepted");
#endif
#endif

    /* 先恢复真实链表、计数和最高阶，再归还16页，使其能与外部伙伴正常合并。 */
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        free_list[order] = saved_list[order];
    }
    nr_free = saved_nr_free;
    max_order = saved_max_order;
    free_pages(base, BUDDY_TEST_PAGES);

    /* 退出前统一核对各阶块头、大小、对齐和范围，累计页数应等于接口计数。
     * 与测试前总数比较，确认测试页全部归还、真实状态恢复。 */
    size_t total = 0, blocks = 0;
    const size_t managed = npage - nbase;
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        size_t size = (size_t)1 << order;
        for (list_entry_t *le = list_next(&free_list[order]);
             le != &free_list[order]; le = list_next(le)) {
            struct Page *page = le2page(le, page_link);
            size_t index = (size_t)(page - pages);
            assert(++blocks <= managed);
            assert(PageProperty(page) && !PageReserved(page));
            assert(page->property == size && page2ppn(page) % size == 0);
            assert(index < managed && size <= managed - index);
            total += size;
        }
    }
    assert(total == nr_free_pages() && nr_free_pages() == free_before);
}

/* 通过统一的 pmm_manager 接口接入内核。 */
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
