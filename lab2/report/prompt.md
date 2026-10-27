# Lab2 提示词与规格汇编

代码路径说明：最终工程以 `lab2/code/` 为源码根目录，从本文位置可通过 [`../code/`](../code/) 访问。Best-Fit 提示词以 `lab2/` 为相对路径根目录，其中 `code/` 指源码目录；刘泽民原分支将源码直接放在 `lab2/`，其原提示词中的 `kern/mm/` 以当时源码根目录为准，对应最终工程的 `code/kern/mm/`。下文保留各提示词原有的功能范围。

| 负责人 | 材料 |
| --- | --- |
| 丁欣怡（2414034） | [初始化和调试规格、实际协作记录](#initialization-prompts) |
| 贺翰林（2412591） | [Best-Fit 模块提示词](#best-fit-prompt) |
| 刘泽民（2412604） | [Buddy 实现提示词](#buddy-implementation-prompt) |
| 刘泽民（2412604） | [Buddy 测试提示词](#buddy-test-prompt) |

最终报告见 [report.md](report.md)。Best-Fit 的后续接入和最终编译开关属于集成操作，不倒写进原始模块提示词。

<a id="initialization-prompts"></a>

## 一、丁欣怡：初始化和调试规格及协作记录

````markdown
## Prompt 1：物理内存探测与 Page 描述符初始化

[PROMPT]

**任务**：在 `lab2/code/kern/mm/pmm.c` 中实现或核对 `static void page_init(void)`，完成 DTB 内存范围检查、物理页描述符布局、已占用页保留和完整空闲页登记。代码标记保留为 `//lab2 function1 2414034`。

**操作要求**：必须在项目中修改实际文件，而不是仅展示代码片段。先阅读当前实现；若已满足规格，不要重复改写。只修改初始化相关内容，保留已有 `alloc_pages`、`free_pages` 等接口、组员的管理器选择和条件编译逻辑。不得修改 First-Fit、Best-Fit 的原有检查函数，不得调整 Page 布局来规避问题。可新增本模块内部的 `static` 辅助函数，但不得增加无关功能。

**输出要求**：依据下列上下文和规格完成必要修改，给出修改位置、布局计算、实际构建和运行结果，以及发现的问题和处理过程。编写独立的初始化验证命令或脚本，保留真实日志；不能将预计结果写成实测结果。报告材料单独保存，不默认上传。

[RELY]

工程使用 RV64、Sv39 和 4 KiB 物理页。启动链为 `kern_entry → kern_init → dtb_init → pmm_init`；`pmm_init` 先初始化当前分配器，再调用 `page_init`，随后进行分配器自检。

相关真实定义分别来自 `mmu.h`、`memlayout.h` 和 `riscv.h`，下列摘录仅省略原注释：

```c
#define PGSIZE                  4096
#define PGSHIFT                 12
#define PPN(la)                 (((uintptr_t)(la)) >> PGSHIFT)
#define DRAM_BASE               0x80000000
#define KERNBASE                0xFFFFFFFFC0200000
#define KMEMSIZE                0x7E00000
#define KERNTOP                 (KERNBASE + KMEMSIZE)
#define PHYSICAL_MEMORY_END     0x88000000
#define PHYSICAL_MEMORY_OFFSET  0xFFFFFFFF40000000
#define KERNEL_BEGIN_PADDR      0x80200000

struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;
```

`nbase` 在 `pmm.c` 定义为 `DRAM_BASE / PGSIZE`。`npage` 表示可管理物理范围的绝对页号上界，描述符数量为 `npage - nbase`，不能将两者混同。描述符数组本身也占用 RAM，必须按实际 `sizeof(struct Page)` 计算，不能把某次构建的结构体大小硬编码进去。

已有接口与宏如下，均为依赖，不要求重新实现：

```c
// dtb.h：获取 dtb_init 已解析并保存的内存信息
uint64_t get_memory_base(void);
uint64_t get_memory_size(void);

// pmm.c：调用当前 pmm_manager 的 init_memmap
static void init_memmap(struct Page *base, size_t n);

// pmm.h：已有内联函数
static inline struct Page *pa2page(uintptr_t pa);
static inline uintptr_t page2pa(struct Page *page);
static inline void set_page_ref(struct Page *page, int val);

// 链接脚本提供的内核末端符号
extern char end[];
```

可使用工程已有的 `ROUNDUP`、`ROUNDDOWN`、`PADDR`、`SetPageReserved`、`PageReserved`、`list_init`、`panic` 和 `cprintf`，不要自行改名或重复定义。

地址关系：`page2pa(p) = (p - pages + nbase) << PGSHIFT`；`pa2page(pa)` 对应 `&pages[PPN(pa)-nbase]`。当前 `pa2page` 仅显式检查页号上界，调用者必须保证传入地址不低于管理起点。`PADDR` 的偏移计算依赖已正确设置的 `va_pa_offset`。

本框架从固定 DRAM 起点描述物理内存；DTB 解析器只提供一个内存区间。多段 RAM、设备树保留区的通用解析不在本次编码范围内。

[GUARANTEE]

必须实现或核对的接口：

```c
static void page_init(void);
```

该函数需要正确建立 `va_pa_offset`、`npage`、`pages` 和物理页初始状态，并通过已有 `init_memmap` 接口登记可分配区间。DTB getter、地址转换接口和分配器接口保持不变。

同时交付与该函数相关的独立验证脚本或操作步骤、真实日志和地址布局说明。验证不得修改课程检查函数，也不得将 B/C 的分配器实现纳入本任务。

[SPECIFICATION]

### page_init(void)

**Pre-Condition**：

- 内核已进入能访问高地址映射的 C 执行环境，链接器能够提供有效的 `end` 符号。
- `dtb_init` 已运行，getter 可被调用；它们返回的数据仍须验证，不得预设解析一定成功。
- 当前物理内存管理器已经执行初始化，但尚未登记本次空闲区；启动阶段为单核顺序执行。

**Post-Condition**：

- 正常返回时，虚实地址偏移与启动映射一致；可管理范围不超出 DTB 报告范围、框架物理上界及对应的内核映射范围，并仅包含完整页面。
- `npage` 是管理范围末端的绝对物理页号；`pages` 位于内核末端之后的页对齐地址，描述符数量正确，整个数组能够放入可管理 RAM。
- 全部物理页先获得确定的保留状态和元数据；只有确认为完整空闲页的区间才交给分配器。固件、内核、描述符数组及对齐空隙对应的页不能进入空闲链表。
- 输出实际探测范围、管理范围、数组位置与大小、空闲范围及页数，能够据此复核布局。

**Case 1：范围有效且存在完整空闲页。** 检查并裁剪内存范围，建立描述符数组，在数组占用范围之后登记剩余完整页。初始登记的页数与区间长度一致，已占用区域保持保留状态。

**Case 2：输入范围不受支持或不合法。** 对零长度、非固定 DRAM 起点、起点与长度求和溢出等情况，进入明确的错误处理路径；不得据此设置可分配区间或写入越界的描述符。

**Case 3：内存不能容纳内核或完整描述符数组。** 在写入描述符数组之前发现容量不足并报告错误；不得先执行可能溢出的乘加运算，再使用已失真的结果作边界判断。

**Case 4：可以容纳描述符，但没有剩余完整空闲页。** 正确建立描述符和保留状态，保持分配器空闲页数为零，不调用要求正页数的 `init_memmap`。该情况只验收初始化，不要求依赖分配内存的框架自检继续成功。

**Case 5：RAM 超过框架上界或末端不是页边界。** 仅管理允许范围内的完整页，不能登记被裁剪区域或末尾不足一页的字节。

**Requirements**：

- 先确认范围和容量有效，再写描述符；先设置正确的地址转换状态，再调用依赖它的转换宏。
- 数组大小使用实际类型布局，保留区不能被 `init_memmap` 误释放。初始化不依赖分配器先给自己分配描述符所需内存。
- 独立副本中使用已有 First-Fit 验证基础初始化；集成工程保留原来的 Best-Fit/Buddy 配置。每次切换构建配置应清理或隔离构建产物。
- 验证至少覆盖 64/128/256 MiB、无效范围、加法溢出、内核或数组容量不足、零空闲页、尾页对齐；检查整个保留区，而不只查看一次成功输出。
- 记录当前构建的实际布局。内核变大后，描述符起点和空闲页数可以变化，不能以历史固定地址作为通用成功条件。

## Prompt 2：分页开启后的内核启动兼容修复

[PROMPT]

**任务**：定位 `lab2/code/kern/init/entry.S` 开启分页后不能进入 `kern_init` 的问题，并在有实际证据时作最小修复。目标是在现有高地址启动映射下正确完成重定位，不更改内存分配算法。

**操作要求**：先通过真实 GDB 记录确定故障发生位置，不能仅凭串口停止输出判定 `page_init` 出错。需要修改时必须修改实际文件，保留 hartid、DTB 地址保存、启动栈和页表内容。保持学号 `2414034` 的说明。当前文件若已满足下述规格，只验证，不重复添加跳转或异常入口。

**输出要求**：提供修复前后的 PC、satp、stvec 等证据，说明修改影响及平台假设，实际验证能进入 C 初始化。将启动问题与初始化算法问题分别记录。

[RELY]

- 原始执行入口的物理地址为 `0x80200000`，内核链接到高虚拟地址。固定偏移为 `0xFFFFFFFF40000000`。
- `entry.S` 提供 `kern_entry`、`boot_page_table_sv39`、`bootstacktop`、`boot_hartid` 和 `boot_dtb`；C 入口为 `kern_init`。
- 根页表前 511 项为零，第 511 项为 `(0x80000 << 10) | 0xcf`，是映射 1 GiB 的根级叶子，未给启动低地址建立恒等映射。
- satp 使用模式字段与根页表物理页号；stvec 是 S 模式异常入口。若采用临时异常向量处理低地址取指页故障，须确认平台将该异常委托给 S 模式。
- 当前 main 已有 `paging_enabled` 高地址标签、临时 stvec 和显式高地址跳转；这也是需要保留和核对的现有行为。

[GUARANTEE]

必须核对或修复的汇编入口为 `kern_entry`；高地址恢复位置使用现有 `paging_enabled` 标签。它们是汇编符号，不虚构 C 函数签名。

完成从物理入口到高地址执行、有效栈设置和进入 `kern_init` 的连接。不得改变 `kern_init` 的 C 接口、根页表映射策略和其他成员的分配器代码。

[SPECIFICATION]

### kern_entry → paging_enabled → kern_init

**Pre-Condition**：固件已经加载内核并进入入口，传入 hartid 与 DTB 物理地址；启动页表在 RAM 中可用且满足对齐要求，目标虚拟地址属于有效映射。

**Post-Condition**：内核在高地址空间继续执行，启动栈有效，参数保存未损坏，能够进入 `kern_init`。临时异常向量只能在执行位置已安全转移后清理，且原页表布局不变。

**Case 1：开启分页后发生低地址取指页故障。** 在平台已委托该异常的前提下，通过有效的高地址临时异常入口完成转移，避免向未映射的异常入口继续取指。

**Case 2：模拟器继续执行已翻译的低地址指令块。** 即使未立即触发上述页故障，也必须显式转移到有效高地址后再清理临时入口，不能依赖某个模拟器恰好在特定位置结束指令块。

**Case 3：断点没有到达目标位置。** 根据实际寄存器、异常原因和源码继续定位，区分固件跳转、异常委托、页表及调试器控制问题；不能通过直接改 PC 或绕过失败指令制造“正常启动”的证明。

**Requirements**：异常入口地址正确对齐；必要的 TLB 同步顺序合理；用实际环境验证，并记录 QEMU/固件版本。不能把仅适用于一次构建的指令地址写入通用启动逻辑。

## Prompt 3：真实访存、Sv39 页表遍历及软件 TLB 调试

[PROMPT]

**任务**：完成成员 A 的 QEMU/MMU/TLB 调试，观察一条真实内核访存的地址翻译，并比较 Bare 与 Sv39 的路径。分析启动页表为何可以在根级完成翻译，解释 QEMU 软件 TLB 与真实硬件 TLB 的逻辑区别。

**操作要求**：在 `lab2/_local` 内准备独立、带调试信息的 QEMU 4.1.1，不安装覆盖系统 QEMU。必须在项目中实际创建或更新复现脚本与调试记录，而非只给教程。复现工具可放在 `lab2/tools/member_a`，记录放在 `lab2/report/member-a`；已有文件应检查后复用。原则上只读 QEMU 的 MMU 实现，不通过改动翻译算法获得预期输出。

**输出要求**：提供版本、构建和运行命令、断点及条件、真实客户机指令、关键变量、源码单步日志及解释。记录实际遇到的问题和 AI 帮助解决的过程；区分实测现象、源码分析与未验证假设。

[RELY]

- 客户机为本实验 RV64 内核；宿主 GDB 调试 QEMU 进程，客户机 `gdb-multiarch` 或可用的 RISC-V GDB 调试内核。两者控制的层次不同。
- 可通过实际反汇编选择入口保存 hartid 的存储指令作为 Bare 样例，选择 `kern_init` 清零 BSS 时的存储指令作为 Sv39 样例。必须核对当次构建的有效地址，不能直接照抄旧版地址或固定单步次数。
- 客户机可读取 `pc`、通用寄存器、`satp` 及内核符号；符号包括 `kern_entry`、`kern_init`、`memset`、`boot_page_table_sv39` 和 `edata`。
- QEMU 4.1.1 的相关位置包括 `target/riscv/cpu_helper.c` 的 `get_physical_address`、`riscv_cpu_tlb_fill`，以及 `accel/tcg/cputlb.c` 的 `store_helper`、`tlb_fill`、`tlb_set_page_with_attrs`、`get_page_addr_code`。这些名称是需要查阅的已有实现，不是要求重新实现的接口。
- 该版本可观察 `addr`、`access_type`、`mmu_idx`、`env->satp`、页表遍历变量，以及 TLB 项的 `addr_read`、`addr_write`、`addr_code`、`addend`；以实际停下的栈帧和类型定义为准。
- 当前启动页表第 511 项是有效根级叶子。Sv39 的最大层数是三层，但本次根级命中不应被描述为实际遍历三张表。

[GUARANTEE]

本任务不新增或修改内核公开函数接口，需要交付以下相互关联的调试产物：

1. 独立 QEMU 的构建和启动入口，以及可重复执行的客户机、宿主调试脚本。
2. Bare 与 Sv39 两组真实访存记录，包含访问类型、模式、有效地址、关键调用栈和返回结果。
3. 页表翻译单步证据、TLB 填充与查询行为证据，以及完整 VA→PA 计算说明。
4. 对应 Markdown 调试章节和实际问题记录。不能将脚本中的成功字符串或预计地址代替真实执行证据。

[SPECIFICATION]

### 独立调试环境

**Pre-Condition**：具备可构建的内核、QEMU 源码及所需工具；当前系统 QEMU 的位置和版本能够确认。

**Post-Condition**：独立二进制包含调试符号，能够运行本次内核，使用与版本匹配的固件和启动参数；系统原有 QEMU 保持原样，命令可在文档中复现。

**Case 1：依赖齐全。** 构建并确认符号、版本和运行入口，再进行源码断点调试。

**Case 2：缺少依赖、版本不匹配或启动失败。** 保存实际错误并先修复环境；未运行成功的阶段不得写成通过。

### 真实访存与地址翻译

**Pre-Condition**：已从本次内核符号和反汇编确认访问指令及有效地址，能够区分指令自身的 PC 与数据地址。

**Post-Condition**：调试记录能把真实内核访问与 QEMU 中的地址翻译调用联系起来，明确物理页基址、页内偏移和完整物理地址的关系。

**Case 1：Bare。** 选择可与分页后比较的实际访问，记录特权级和 satp，观察直接地址路径；不能把 M 模式特权绕过简单等同于 S 模式 Bare。

**Case 2：Sv39。** 从 satp 得到根页表物理地址，单步观察规范地址检查、索引计算、PTE 读取、叶子判断、权限和大页对齐检查，以及物理页号组成。记录实际循环次数，并区分返回状态与输出地址。

**Case 3：断点未命中或目标地址变化。** 检查当前符号、有效地址、访问类型、特权模式及缓存状态，修正观测方法；不得为了命中而人工调用翻译函数或把旧地址写入结果。

**Requirements**：条件断点应减少无关固件访问和调试器读取的干扰。可采用双 GDB 配合，也可先确认真实目标再单独运行宿主条件断点；如分次采集，报告中明确说明。所有数值必须能从当前日志复核。

### 软件 TLB 与对比分析

**Pre-Condition**：已掌握目标 QEMU 版本的 TLB 相关源码，并获取真实访存的上下文。

**Post-Condition**：说明主表查询、必要时的 victim cache 查询、未命中填充及命中后的行为，解释软件缓存如何同时服务客户机翻译和宿主内存定位。

**Case 1：查询未命中。** 记录实际进入的慢路径及后续填充，说明 TLB 条目的标签、权限、映射范围和地址偏移信息，不能声称每次主表未命中都必然遍历页表。

**Case 2：查询命中。** 记录匹配的地址与标签，并单步证明跳过 refill 分支。若采用后续取指作为命中样例，明确标注为取指证据，不能写成目标数据存储的再次命中。

**Case 3：Bare 中也发生软件 TLB 填充。** 结合实际日志说明此时不进行 Sv39 页表遍历，但仍需要将客户机物理内存映射到宿主存储；不要以“未开启分页”为由否认模拟器缓存的存在。

**Requirements**：区分内核固定虚实偏移与 QEMU TLB 的宿主地址 `addend`；说明 TCG 快路径可能直接生成为宿主机器码，所以并非每次内存访问都进入 C helper。硬件 TLB 的具体容量、相联方式或替换策略不能从该软件实现推断。

````

<a id="best-fit-prompt"></a>

## 二、贺翰林：Best-Fit 模块提示词

````markdown
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
````

<a id="buddy-implementation-prompt"></a>

## 三、刘泽民：Buddy 实现提示词

````markdown
## [PROMPT]
参考当前 ucore 框架，从零实现 Buddy System 物理页分配模块并接入内核。
保留标准的多阶空闲链表，以设计直观、容易解释为目标，不为压缩行数增加复杂结构。
新增 buddy_pmm.c、buddy_pmm.h，并在 pmm.c 中完成必要的管理器选择。
若这些文件和接入已经存在，则在原有位置修改。
不要修改 page_init()、公共数据结构、First-Fit、Best-Fit 或它们的测试。
本阶段实现分配算法，buddy_check() 暂留空，后续单独实现测试。

## [RELY]
先阅读 pmm.c、pmm.h、memlayout.h、mmu.h、libs/list.h、default_pmm.c 和 Makefile。
pmm_manager 提供 init、init_memmap、alloc_pages、free_pages、nr_free_pages、check。
pages 描述从物理页号 nbase 开始的页面，描述符数量为 npage-nbase。
使用 page-pages 的全局下标判断对齐及伙伴关系。
当前平台 nbase 按所有支持的块大小对齐，管理上限128 MiB，页大小4 KiB。
page_init() 在初始化管理器后登记一次连续可用区间，输入页带 PG_reserved。

## [GUARANTEE]
实现以下静态函数并定义 buddy_pmm_manager：
buddy_init(void)
buddy_init_memmap(struct Page *base, size_t n)
buddy_alloc_pages(size_t n)
buddy_free_pages(struct Page *base, size_t n)
buddy_nr_free_pages(void)
头文件声明 extern const struct pmm_manager buddy_pmm_manager。
各阶只维护链表，只保留一个总空闲页计数；正常最高阶15。
保留可临时调整的最高阶变量，供后续16页受控测试使用。
函数头概括实现思路，函数体只注释关键阶段，不逐句复述代码。

## [SPECIFICATION]
1. 状态约定：空闲块头 PG_property=1、property=实际块页数；已分配块头
   PG_property=0，但保留实际大小；块内其他页的 property=0。
2. buddy_init：清空所有阶链表、总计数和测试遗留状态，不访问尚未登记的物理页。
3. buddy_init_memmap：检查范围及保留页状态，初始化页信息；将输入区间划分为
   大小为二次幂、起点按自身大小对齐的块，完整登记 n 页，不丢弃尾部页面。
   根据当前单次连续区间的调用方式实现，不增加跨次登记自动合并的要求。
4. buddy_alloc_pages：n>0；取整到最小可容纳阶，从该阶向上选第一条非空链表；
   取下块后逐级分裂，保留前半块、挂回后半块；记录实际大小并扣除实际页数。
   没有块或请求过大时返回 NULL，且不改变状态；超大输入不能造成移位溢出。
5. buddy_free_pages：n>0；取整后校验范围、对齐、未释放状态及实际块大小。
   清理原块并增加本次实际归还页数，以 index XOR size 寻找同阶空闲伙伴。
   可合并时摘下伙伴、清除旧块头、保留低地址块头并继续向上合并。
   伙伴越界、保留、已分配或大小不同则停止；合并不重复增加计数。
6. buddy_nr_free_pages：返回总空闲页数。
7. 使用现有工程工具链编译，确认内核选择 buddy_pmm_manager。
````

<a id="buddy-test-prompt"></a>

## 四、刘泽民：Buddy 测试提示词

````markdown
## [PROMPT]
针对现有 Buddy 分配器实现 buddy_check() 和必要的静态测试辅助函数。
参考 First-Fit、Best-Fit 的 check 保存和恢复真实状态，但按 Buddy 的完整块释放约定设计案例。
核心行为都要有明确检查点；同一性质适当验证，不增加重复参数枚举或测试框架。
仅修改 buddy_pmm.c 中的测试部分及必要的 check 绑定，不修改分配、释放算法。

## [RELY]
先阅读 buddy_pmm.c、pmm.h、memlayout.h、libs/list.h 和已有 check。
free_list[k] 管理 2^k 页块，nr_free 是总空闲页数，max_order 是当前最高阶。
已分配块头 property 保存实际大小，PG_property 清除；释放时按取整大小核对。
分裂保留前半块、后半块挂链，因此受控布局下可确定返回地址。
内核通过 pmm_manager->check() 调用 buddy_check()。

## [GUARANTEE]
使用 assert 检查结果，正常模式测试结束后完整恢复真实分配器状态。
可以实现 buddy_basic_check(base)、buddy_check_block(page, order)、buddy_prepare_memmap(base)。
统一复用从真实分配器借出的16页区域，保存各阶链表头、计数和最高阶。
清空测试链表并将最高阶临时设为4，避免区域向外合并。
恢复时先放回真实状态，再归还16页；仅在最后统一遍历全局链表。
函数头写实现思路，函数体按场景写关键操作与检验目的，详略适当。

## [SPECIFICATION]
1. 完整初始化：将测试区准备为保留页，登记 base 开始的16页。
   检查只有第4阶有完整块，低阶为空，总数16。
2. 基础分配：申请3页得到 base 处4页块，剩余 base+4处4页、base+8处8页，总数12。
   再申请1页应返回 base+4，证明选择最小可用阶；释放后再申请4页，应精确取得原余块。
   最后按原请求3页释放第一个块，再释放第二个4页块。
3. 综合场景：申请四个4页块 A/B/C/D，检查地址依次相差4页、不重叠、总数0。
   申请1页失败。按 B、C、A、D 的顺序释放：
   - B、C释放后仍是两个第2阶块；申请8页、16页均失败，块和总数8保持不变。
   - A释放后A+B为8页，C仍为4页，旧头B清零。
   - D释放后C+D先合并，再与A+B合并为16页；低阶为空，旧头B/C/D清零，总数16。
4. 请求上界：申请17页和 (size_t)-1 页返回 NULL；随后仍能取得完整16页块，总数0。
5. 初始化边界：重新准备保留页，只登记 base+3 开始的13页。
   检查 base+3处1页、base+4处4页、base+8处8页，总数13，前3页仍保留。
   依次申请1、4、8页，检查对应地址；再申请应失败，所有链表为空。
6. 还原自有16页元数据。正常模式恢复真实状态后归还整块，最后一次全局遍历检查
   各阶标志、大小、对齐、范围和总数，总空闲页数应恢复到整个测试开始之前。
7. 独立非法释放：BUDDY_ILLEGAL_FREE_TEST 默认0，只保留模式1和3。
   在16页区域起点申请4页：模式1按2页释放，应触发实际大小不匹配断言；
   模式3从p+1按4页释放，应触发对齐断言且不能先越界。
   错误调用返回后立即 panic("TEST FAILED: ...")。
8. 重新编译并分别启动正常模式和两种非法模式，核对具体断言表达式和位置。
````
