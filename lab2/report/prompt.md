# 任务二：实现 Best-Fit 连续物理内存分配算法

> 下文相对路径均以 `/home/oslab/Lab/lab2` 为根目录。
> 当前范围：任务二中的 Best-Fit 模块实现。

[PROMPT]

**任务**：在 riscv64-ucore Lab2 中实现完整的 Best-Fit 连续物理内存管理模块。先阅读 `code/kern/mm/default_pmm.c`、`best_fit_pmm.c`、`best_fit_pmm.h`、`pmm.h`、`memlayout.h` 和 `code/libs/list.h`，以实际定义和现有测试为准。

目标文件与职责：仅在 `code/kern/mm/best_fit_pmm.c` 的 `//lab2 function2-2 YOUR CODE` 标记位置补全内部状态、管理函数和 `best_fit_pmm_manager` 注册结构。

**操作要求**：进入实现阶段后，必须修改项目实际文件，不能只展示代码片段。保留既有代码，完整保留 `basic_check()`、`best_fit_check()` 的函数体，不删除、改写、绕过断言，不修改评分脚本。仅修改 `best_fit_pmm.c`，不修改 `pmm.c`、First-Fit、公共结构体、接口、Makefile 或其他任务代码；不实现 `page_init()`、Buddy 或 SLUB。

**输出要求**：完成后说明修改位置、核心设计和各函数职责；记录实际发生的 bug 和修复过程，不虚构运行或测试通过结果。本次不要求启动完整内核、执行评分或整合实验报告。算法复杂度不作硬性限制，优先保证语义正确、兼容现有测试且便于理解。

[RELY]

**1. 框架接口**

`code/kern/mm/pmm.h` 定义的接口如下，排版和注释简化，类型与字段保持一致：

```c
struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)
```

公开包装函数通过当前 `pmm_manager` 调用具体分配器。`best_fit_pmm.h` 已有声明：

```c
extern const struct pmm_manager best_fit_pmm_manager;
```

**2. 页描述符与空闲区状态**

`code/kern/mm/memlayout.h` 中相关结构如下，省略注释：

```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;
```

- 每个 `Page` 对应一个物理页；描述符数组的先后顺序与物理页顺序一致。`base + n` 是后移 `n` 个描述符，对应后移 `n` 个物理页，不能把描述符指针直接当物理地址。
- `PG_reserved` 表示保留页；`PG_property` 表示空闲块首页。可使用已有的 `PageReserved`、`SetPageReserved`、`ClearPageReserved`、`PageProperty`、`SetPageProperty`、`ClearPageProperty` 宏。
- 空闲块以首页进入链表，首页 `property` 记录块的连续页数；非首页的 `property` 不作为有效块长使用。
- `nr_free` 表示空闲页总数，而非链表节点数。`page_ref()`、`set_page_ref()` 和 `page2pa()` 已由框架提供。

**3. 链表与参考代码**

`list.h` 提供带头节点的循环双向链表：`list_init`、`list_empty`、`list_next`、`list_prev`、`list_add`、`list_add_before`、`list_del`。`list_add(pos, node)` 插入到 `pos` 之后；`le2page(le, page_link)` 将链表节点转换为所属 `Page`。可参考 First-Fit 的状态初始化、分裂、回收和合并处理，但分配选择必须改为 Best-Fit。

**4. 现有测试的状态约定**

- `best_fit_pmm.c` 仅提供 `basic_check()`、`best_fit_check()`，它们直接读写 `free_list` 和 `nr_free`。模块必须提供这两个同文件可见、可赋值的名称；可参考 First-Fit 使用模块静态 `free_area_t` 加宏别名。
- 测试会保存原链表头与计数，清空链表后构造局部场景，最后恢复原状态。不要引入无法随这两个状态一起恢复的必要索引或缓存。
- 测试清空链表与清零计数之间，会调用分配函数；此时可能出现“链表为空但计数仍非零”的临时状态，分配必须返回 `NULL`，不能仅凭计数判断成功，也不能在这里断言二者一致。
- Best-Fit 的关键场景：5 页区间中分别释放偏移 1 起的 2 页块、偏移 4 的 1 页块；申请 1 页时必须优先返回后者，不能因前者地址较低而选择前者。
- 测试可能先分配若干子区间，再一次释放合起来的连续已分配区间；不能要求释放范围必须对应单次分配记录。

[GUARANTEE]

在 `best_fit_pmm.c` 中实现以下内部接口。本提示词选定这些名称便于说明，手册本身不强制内部函数命名：

```c
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
```

还必须：

- 定义 `const struct pmm_manager best_fit_pmm_manager`，名称为 `"best_fit_pmm_manager"`，五个管理接口绑定到上述函数，`.check` 绑定现有 `best_fit_check`；根据定义位置添加所需的静态前置声明，不改变测试函数。
- 提供满足测试约定的 `free_list` 和 `nr_free` 状态。
- 可在模块内部新增 `static` 辅助函数、静态变量和宏，不增加无关公开接口或依赖其他动态内存分配器。

[SPECIFICATION]

## 模块状态约束

- 除测试刻意建立的临时状态外，空闲链表各块长度之和应等于 `nr_free`；各块互不重叠，只登记空闲块首页。
- 本提示词采用按物理地址递增组织空闲块的设计，便于释放时寻找相邻块；分配时仍须按块大小选择最优候选，不能把地址顺序误当作 Best-Fit 选择规则。这是本次设计选择，不是手册额外规定。
- 只有物理相邻的块才可以合并；链表位置相邻本身不能作为合并依据。正常释放后应完成可发生的前后合并。
- 分配与释放仅维护页描述符及管理状态，不要求擦除物理页内容或改变页表。按当前实验框架执行，不引入无关并发机制。

## best_fit_init

**Pre-Condition**：首次建立管理器状态，尚未登记可用页。

**Post-Condition**：空闲链表初始化为空，`nr_free == 0`，状态可用于后续登记操作。

## best_fit_init_memmap

**Pre-Condition**：`n > 0`；`[base, base + n)` 对应有效、连续、尚未登记的物理页，当前均处于保留状态；管理器已初始化。

**Post-Condition**：该范围变为可管理空闲页，引用计数重置为 0，清除保留状态，正确设置块首页标记及长度；地址顺序保持正确，空闲页总数恰增加 `n`。

**Case 1**：链表为空时，登记为首个空闲块。

**Case 2**：链表非空时，在正确地址位置登记，不丢失已有块、不产生重叠。若对相邻登记区间进行合并，须同步更新首页标记和长度；登记不跨越保留页或不可用区间。

**Requirements**：对 `n > 0` 和登记页的保留状态采用与参考实现一致的断言检查。

## best_fit_alloc_pages

**Pre-Condition**：`n > 0`，空闲块元数据有效；允许现有测试临时清空链表而尚未同步计数。

**Post-Condition**：成功则返回连续、恰好 `n` 页的首页描述符，分配区间不再属于空闲链表且其中没有空闲块首页标记，`nr_free` 恰减少 `n`；失败则返回 `NULL`，链表、页状态和计数均不变。

**Case 1**：空闲总页数不足，或链表为空，或虽总页数足够但所有单块长度均小于 `n`，返回 `NULL`。

**Case 2**：存在可用块时，选择所有长度不小于 `n` 的块中最小者。若多个块同样最小，本设计选择地址最低者。精确匹配已经达到最优，可提前结束搜索；不能遇到任意足够大的块就停止。

**Case 3**：最优块长度等于 `n`，整块分配，从空闲链表移除并更新状态和计数。

**Case 4**：最优块长度大于 `n`，分配该块前部 `n` 页，余下连续区间继续作为空闲块，正确设置新的首页标记和长度，并保持地址有序。

**Requirements**：断言 `n > 0`；返回区域引用计数符合现有测试的零值约定；不能因空闲总页数足够就返回不连续页面，不能返回超出请求页数的整个大块。

## best_fit_free_pages

**Pre-Condition**：`n > 0`；`[base, base + n)` 是有效、连续、已分配且允许释放的页，不包含保留页，也不与已有空闲块重叠。允许一次释放由多次分配得到的连续区间；调用者负责保证范围有效。

**Post-Condition**：整个范围回到空闲管理状态，释放页的引用计数重置为 0，`nr_free` 恰增加 `n`；最终空闲块不重叠，首页标记和长度正确，地址顺序保持不变，所有可与本次释放区间相接的空闲块完成合并。

**Case 1**：链表为空，或左右均无物理相邻块，将释放范围登记为独立空闲块。

**Case 2**：仅前方或仅后方有物理相邻块，合并为一个连续块，保留合并后最低地址页作为首页，移除失效首页的链表节点和标记。

**Case 3**：前后均有物理相邻块，最终合并为一个完整大块，不因第一次合并改变首页而漏掉另一侧。

**Requirements**：使用与参考实现一致的 `n > 0`、非保留页及非空闲块首页断言；不宣称这些断言能够检测全部非法释放。合并只调整组织结构，不重复增加空闲页计数。

## best_fit_nr_free_pages

**Pre-Condition**：管理器已初始化。

**Post-Condition**：返回当前 `nr_free`，不改变任何状态。

## best_fit_pmm_manager

**Pre-Condition**：Best-Fit 管理函数及现有测试函数可在注册结构中被引用。

**Post-Condition**：注册结构接口完整、类型匹配，管理器名称和各回调绑定符合 `[GUARANTEE]`，可供后续内核接入使用。本次不修改当前管理器选择。
