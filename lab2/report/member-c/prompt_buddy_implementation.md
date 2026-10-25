## [PROMPT]

** 一、任务**
请基于现有 ucore 物理内存管理框架，实现 Buddy System（伙伴系统）分配算法模块，并将其接入内核，作为可以替换 First-Fit 的物理内存分配策略。实验框架没有提供 Buddy System 的代码框架和测试函数，因此需要自己实现该模块。

主要文件及职责：
- `kern/mm/buddy_pmm.c`：新建，负责实现 Buddy System 分配器的内部状态管理、初始化、物理页分配、释放及伙伴合并。
- `kern/mm/buddy_pmm.h`：新建，负责声明 Buddy 物理内存管理器。
- `kern/mm/pmm.c`：修改 `init_pmm_manager()`，将物理内存管理策略切换为 `buddy_pmm_manager`。
- `kern/mm/default_pmm.c`：作为已有 First-Fit 实现的参考，用于理解现有接口和 Page 状态管理约定。
- `kern/mm/pmm.h`、`kern/mm/memlayout.h`、`libs/list.h`：提供已有接口、页面描述符及链表操作。

Buddy System 的功能是：把系统中的可用存储空间划分为存储块(Block)来进行管理, 每个存储块的大小必须是2的n次幂(Pow(2, n)), 即1, 2, 4, 8, 16, 32, 64, 128...，分配时选择能容纳请求块大小的最小阶块，当最小阶块没有时尝试从更大块逐级分裂；释放时在释放块后判断同阶伙伴是否空闲，能够合并时逐级向上合并。
例如：
- 申请 1 页，实际占用 1 页。
- 申请 3 页，实际占用 4 页。
- 申请 5 页，实际占用 8 页。
- 当一个 16 页空闲块需要满足 4 页请求时，应能够拆分出 4 页块，并保留其余页面供后续分配。
- 当两个大小相同、属于同一个父块的伙伴都空闲时，应能够合并为更大的空闲块。
采用多阶空闲链表来实现。

** 二、操作要求**
1. 必须在当前项目中实际创建和修改文件，不得仅输出示例代码、伪代码或补丁描述。
2. 修改前先阅读 `pmm.c`、`pmm.h`、`memlayout.h`、`default_pmm.c`、`default_pmm.h`、`mmu.h`、`libs/list.h` 和 `Makefile`，确认函数接口、地址转换、内存初始化顺序及编译机制。
3. 修改范围限于新增的 `buddy_pmm.c`、`buddy_pmm.h`，以及原有的 `pmm.c` 中接入 Buddy 所必需的 include 和管理器选择代码。
4. 可以自行定义 Buddy 专用的静态管理结构、常量、内部状态和辅助函数，但必须遵循已有内核数据结构和接口。
5. 不允许修改 `struct Page`、`struct pmm_manager`、现有内存布局宏、链表库及其他公共接口的定义。
6. 不得修改 `default_pmm.c` 或 `best_fit_pmm.c` 中的已有实现和测试函数。
7. 不得修改 `page_init()` 有关的物理内存探测逻辑、内核地址映射、设备树处理、启动代码。
8. 不得引用本工程中不存在的头文件、函数或运行库。
9. 在功能完成后检查 Makefile 的源码收集规则，确认新建的 Buddy 源文件被编译。只有在实际需要时才允许对 Makefile 做最小修改。
10. 当前源码中，`pmm.c` 的 `//lab2 function1 2414034` 是物理内存初始化任务的实现位置，`best_fit_pmm.c` 也有未完成部分。这些属于其他人的任务，不应为了完成 Buddy 而擅自实现。

如果它们导致完整工程无法编译或运行，应报告具体阻塞原因，区分 Buddy 本身的问题与其他任务尚未完成的问题。

**三、输出要求**

完成后应满足：

1. `buddy_pmm_manager` 已经按照 `struct pmm_manager` 的接口正确注册，内核能够调用 Buddy 的初始化、分配、释放、空闲页统计，检查函数暂时不实现。
2. 在物理内存初始化和其他前置任务已经完成的情况下，Buddy 能正确管理可用物理页。
3. 在条件允许时make qemu运行内核验证。内核能够运行时输出 `memory management: buddy_pmm_manager`。
4. 没有因为新增 Buddy 而修改其他内存管理算法。

最终说明修改了哪些文件、采用了什么内存块管理数据结构、如何识别伙伴、如何完成分裂和合并，以及编译运行结果。

## [RELY]
以下内容是完成本任务所需的最小工程上下文。
** 1. 物理内存管理器接口**
`kern/mm/pmm.h` 中定义：

``` c
struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};
```

`pmm.c` 通过全局 `pmm_manager` 指针调用上述接口。其 `pmm_init()` 流程为：

- `init_pmm_manager()`：选择并初始化具体的分配策略；
- `page_init()`：完成物理内存探测、保留页排除及空闲页登记；
- `check_alloc_page()`：调用当前管理器的 `check()`。

Buddy 必须完整实现上述接口，不能更改接口签名或绕过 `pmm_manager`。

** 2. 物理页描述符**

`kern/mm/memlayout.h` 中定义：

```
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};
```

相关标记：

- `PG_reserved`：表示该物理页被内核保留，不能作为空闲页分配。
- `PG_property`：表示该 `Page` 是一个空闲连续物理块的头页。
- `SetPageReserved`、`ClearPageReserved`、`PageReserved`。
- `SetPageProperty`、`ClearPageProperty`、`PageProperty`。

相关字段约定：

- `ref` 表示页面引用计数。
- `property` 在 First-Fit 中用于保存空闲块大小；Buddy 可以在保持原结构不变的前提下定义适合自身的使用约定。
- `page_link` 可用于把空闲块头页接入双向链表。

** 3. 地址转换**

`kern/mm/pmm.h` 提供：

- `pages`：物理页描述符数组起始地址；
- `npage`：可管理物理内存的物理页号上界；
- `nbase`：DRAM 起始物理页号；
- `page2ppn(page)`：将描述符转换为物理页号；
- `page2pa(page)`：转换为物理地址；
- `pa2page(pa)`：将物理地址转换为描述符。

当前代码的映射关系为：

``` c
page2ppn(page) = page - pages + nbase
```

因此，`pages[0]`对应物理页号`nbase = 0x80000`，可以使用在pages数组中的位置即数组下标来代替物理页号的使用。

**4. 页面大小和空闲链表接口**
`kern/mm/mmu.h` 提供：
``` c
#define PGSIZE 4096
#define PGSHIFT 12
#define PPN(la) (((uintptr_t)(la)) >> PGSHIFT)
```
Buddy 以物理页为最小管理单位，而不是直接以字节管理内存。

`libs/list.h` 提供：

- `list_entry_t`
- `list_init`
- `list_add`
- `list_add_before`
- `list_del`
- `list_del_init`
- `list_empty`
- `list_next`
- `list_prev`

`memlayout.h` 提供 `le2page(le, member)`， 可以将链表节点转换回 `struct Page *`。
另外有
``` c
\\kern/mm/memlayout.h
#define PHYSICAL_MEMORY_END         0x88000000
\\libs/riscv.h
#define DRAM_BASE          0x80000000
```
如果选择多阶空闲链表实现 Buddy，可利用这些接口。

`default_pmm.c` 展示了 `Page` 描述符初始化、空闲块头页标记、链表操作和空闲页计数的工程语义，可以参考其接口使用方式。。

**5. Buddy System 的伙伴关系**

伙伴块必须同时满足：
1. 两个块大小相同，均为 2 的整数次幂。
2. 两个块是同一个更高阶块分裂产生的左右子块。
3. 两个块都处于空闲状态。
4. 合并后的整个范围都属于已注册且可管理的物理页面。

对于大小为 `2^k` 页的块，其起始物理页号必须是 `2^k` 的整数倍，当满足该对齐条件时，其伙伴起始页号可以通过起始页号与 `2^k` 异或得到。但计算出的地址还必须经过范围和状态检查。

## [GUARANTEE]

需要实现以下主要接口：

``` c
static void buddy_init(void);

static void buddy_init_memmap(struct Page *base, size_t n);

static struct Page *buddy_alloc_pages(size_t n);

static void buddy_free_pages(struct Page *base, size_t n);

static size_t buddy_nr_free_pages(void);

static void buddy_check(void);
```

在 `kern/mm/buddy_pmm.c` 中定义：

``` c
const struct pmm_manager buddy_pmm_manager;
```

在 `kern/mm/buddy_pmm.h` 中声明：

``` c
extern const struct pmm_manager buddy_pmm_manager;
```

在 `kern/mm/pmm.c` 的 `init_pmm_manager()` ，使其选用该管理器，并保留原本的初始化及名称输出行为。

允许自行添加辅助函数、模块内部宏定义和必要的静态变量，但不能擅自修改公共接口。
新增内部辅助函数应尽量声明为 `static`，不得无故扩大对外接口。
本次暂不实现的 `buddy_check()`，由后续独立测试任务实现。

## [SPECIFICATION]

### 1. buddy_init()
**Pre-Condition**
- 内核已经选择 Buddy 作为管理器。
- 此时尚未通过 Buddy 登记可用物理页。

**Post-Condition**
- 所有k对应的空闲链表初始化完毕，处于空状态。
- 当前空闲页计数置为 0。

**Case 1：正常初始化**
初始化所需的空闲链表和统计状态，供 `init_memmap()` 注册可用页面。

**Requirements**
根据可用物理内存上限来确定最大k。
此函数只初始化 Buddy 的管理状态。

### 2. buddy_init_memmap(struct Page *base, size_t n)
**Pre-Condition**
- Buddy 内部状态已经初始化。
- `base` 指向一个此前未注册的、连续可用的物理页描述符区间`base`，`[base, base + n)` 代表已经由上层确认可用、尚未登记的连续物理页。
- `n > 0`。
- 输入范围不包括内核镜像、OpenSBI、页描述符数组或其他保留区域。

**Post-Condition**
- 全部 `n` 个物理页被登记为空闲页。
- 该区间被变为一个或多个互不重叠、大小为 2 的幂、按自身大小对齐的空闲块。
- 所有被登记的页`flag`除`PG_property`为0，引用计数为 0。
- 每个空闲块头页具有正确的 `PG_property` 状态及 `property` 值。
- 每个空闲块进入其对应 k 的空闲管理结构。
- 空闲页总数增加 `n`，登记过程中不会丢失页面或重复统计页面。

**Case 1：输入只有一个物理页**
当 `n = 1` 时，应登记一个 k = 0 的空闲块，空闲页数增加 1。

**Case 2：区间长度不是 2 的幂**
必须能够将区间划分为多个 2 的幂大小的合法块，而不是丢弃多余页面。
例如输入包含 13 个连续页。不得丢弃多余页面，应拆为若干合法的二次幂块，最终登记页数之和必须为 13。

**Case 3：传入起始位置未按大块对齐**
必须根据物理页号处理对齐。
例如可用区间的物理页号范围为 `[3,16)` 时，可形成大小为 1、4、8 页的合法块，分别为 `[3,4)`、`[4,8)`、`[8,16)`。

**Case 4：多次登记可用区间**
多次调用可登记不同的、不重叠的可用物理页区间。新的登记不能破坏已存在的空闲块。
对于物理位置相邻、大小相同且满足 Buddy 关系的完整空闲块，允许并应能够最终合并。不能越过保留页或未登记区间合并。

**Case 5: 非法输入**
- `n <= 0`
- 有页不为保留页

**Requirements**
- 不允许注册输入区间以外的页。
- 对重叠或重复登记的非法输入应进行检查，不能把同一页重复加入空闲链表。
- 必须使用 base - pages 取得全局下标，而不能把每次登记区间的起点当作新的下标 0

### 3. buddy_alloc_pages(size_t n)

**Pre-Condition**
- Buddy 已完成初始化及必要的物理页登记。
- `n > 0`。
- 各阶空闲管理结构保持一致。

**Post-Condition**
成功时：
- 返回一个有效的 `struct Page *`，指向实际分配块的起始页。
- 返回块包含至少 `n` 个连续物理页，实际块大小是能够容纳 `n` 的最小合法二次幂， `property`为实际块大小，不记录实际申请的块大小，按自身大小对齐。
- 被分配的块不再作为空闲块留在任何k链表中。
- 空闲页总数减少实际分配块的页数，返回块中的首页应从空闲状态转为已分配状态。

失败时返回 `NULL`，且原分配器状态保持不变。

**Case 1：n为非 2 的幂**
应向上取整为能够容纳请求的最小 2 的幂页数。例如 `n=3` 实际保留 4 页，`n=5` 实际保留 8 页。多出的页面属于当前分配块，不能再拆分分配。空闲页计数应按照实际分配块的容量减少。

**Case 2：存在恰好匹配的空闲块**
例如请求 4 页且存在可用的 4 页伙伴块时，可以直接分配该块。
维护头页标记、分配信息及空闲页计数。

**Case 3：只有更大的空闲块**
- 寻找能够满足请求的更高阶且阶数最小的空闲块，并逐级对半拆分，每次拆分后两个子块大小相等，均为父块的一半；
- 未分配出去的伙伴子块继续保持可用，应纳入相应 k 的空闲管理结构，已被拆分的父块不得继续作为一个完整空闲块出现。例如存在 16 页块而申请 4 页时，应能够保留未分配的 12 页供后续使用，但是已经被拆分成4，8。
- 维护头页标记、分配信息及空闲页计数。

**Case 4：没有可满足请求的连续 Buddy 块**
当不存在足够大小的合法空闲块时返回 `NULL`。
这不仅包括总空闲页数不足，也包括虽然总空闲页数足够，但不存在满足请求大小的连续且可合并空闲块。
失败后空闲块结构、页面状态和空闲页统计必须保持不变。

**Case 5：异常参数**

- `n == 0`：使用 `assert` 拒绝非法调用。
- 当 `n` 过大，无法安全地计算向上取整后的 2 的幂大小或者超过 Buddy 所支持的最大块容量时，应返回 `NULL`

**Requirements**
成功返回块的起始物理页号必须满足实际块容量的对齐要求。
每个成功分配的块只有空闲块大小，不记录实际申请的页数。

### 4. buddy_free_pages(struct Page *base, size_t n)

**Pre-Condition**
- `base` 是此前一次成功分配的起始页，对应的分配块尚未被释放。
- `n > 0`，为该次分配的原始请求页数。
- 调用的请求n向上取 2 的幂后，必须与该块记录的实际分配大小一致。
- 不允许从块内部任意位置开始释放，也不允许对为空闲块重复释放。

**Post-Condition**
- 整个原始分配块变为空闲块。
- 若同阶伙伴块空闲，则能够逐级合并。被合并的两个子块不再作为独立块存在，起始物理页号满足对齐要求。
- 维护头页标记、分配信息及空闲页计数，空闲页只增加本次归还的实际页数。

**Case 1：伙伴不存在、未登记或不空闲**
只释放当前块，不进行合并。

**Case 2：两个同阶伙伴均为空闲**
两个块应合并为一个大小翻倍的空闲块。
若合并后的块仍有符合条件的空闲伙伴，应继续向上合并。直到遇到不可合并的伙伴、管理范围边界或最大k。

**Case 3：相邻块并非同阶伙伴**
即使它们物理连续，也不能错误合并。

**Case 4：非 2 的幂请求释放**
n向上取 2 的最小幂。例如 `alloc_pages(3)` 实际占用 4 页，应允许使用 `free_pages(base, 3)` 释放 4 页。

不能只归还 3 页，遗漏的 1 页也不能变成不可管理的内存。

**Case 5：非法释放**
以下操作非法，应通断言或检查拒绝：
- `base == NULL`或者起始页号不是实际分配块大小的整数倍；
- `n == 0`；
- 释放已空闲的块；
- 从已分配块内部开始释放；
- 释放页数与记录的块大小不匹配；
- 释放保留页或未登记页。

### 5. buddy_nr_free_pages()
**Pre-Condition**
Buddy 已经初始化。

**Post-Condition**
直接返回当前真正处于空闲状态的物理页总数。。

**Requirements**
空闲块计数已在对块的操作中更新了。

### 6. buddy_check()

**Pre-Condition**
管理器已初始化且已有合法可用页区间。

**Post-Condition**
进行基本分配、释放及计数检查，检查结束后所有测试用页面均已归还。

**Requirements**
暂不进行实现。
