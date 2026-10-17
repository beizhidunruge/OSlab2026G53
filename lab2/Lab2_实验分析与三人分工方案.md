# Lab2（物理内存和页表）实验分析与三人分工方案

> 适用项目：`/home/oslab/Lab/Lab2/code`
> 指导手册：<http://oslab.mobisys.top/lab2026/_book/lab2/lab2.html>（已离线提取为 [lab2_manual_full.md](manual/lab2_manual_full.md)）
> 外部模型协作输入包：[lab2_manual_full.md](manual/lab2_manual_full.md) + [lab2_AI_constraints.md](manual/lab2_AI_constraints.md)
> 本方案**未改动任何源代码**。

---

## 0. 结论速览

| 问题 | 结论 |
|---|---|
| 本次要动手的代码只有几处？ | **只有 2 处标记**：`pmm.c:64`（功能模块一：`page_init`）、`best_fit_pmm.c:6`（功能模块二任务二：Best-Fit）；外加 `pmm.c` 的 `init_pmm_manager()` 里**一行**管理器切换 |
| 自动评分怎么算？ | `make grade` → **20 分**：`Test 1 Page Init` 10 分 + `Test 2 Best-Fit` 10 分 |
| 最容易被忽略的分数点？ | ①`init_pmm_manager()` 不切到 `best_fit_pmm_manager` → Test 2 直接 0 分；②`best_fit_check()` 里有一条断言专门区分 Best-Fit 与 First-Fit（见 §5.3） |
| 最大工程量在哪？ | **Buddy System**（手册任务三，无任何框架、要自带测试），但它**不参与自动评分** |
| 最危险的地方？ | `make grade` 会调用 `tools/inject.py` 做**正则注入**，命名/Include 稍有不慎会让评测脚本直接报错退出（0 分），见 §4.3 |
| 环境就绪吗？ | 就绪。`riscv64-unknown-elf-gcc`、`qemu-system-riscv64`、`riscv64-unknown-elf-gdb` 均已安装 |
| 当前基线能编过吗？ | **不能**。`make` 报 `implicit declaration of function 'page_init'` —— `page_init()` 全项目未定义 |
| 建议分工主轴 | 按**功能模块**切：组长=模块一+整合；成员B=Best-Fit；成员C=Buddy+加分项 |

---

## 1. 现状体检（我实际做了什么检查）

| 检查 | 命令 / 文件 | 结果 |
|---|---|---|
| 项目结构 | `find code -type f` | 33 个文件，`kern/mm/` 下 8 个，无 `kern/test`，无 `lab2.md` |
| 待填标记 | `grep -rn "YOUR CODE"` | 仅 2 处：`kern/mm/pmm.c:64`、`kern/mm/best_fit_pmm.c:6` |
| 基线构建 | `make` | ❌ `kern/mm/pmm.c:77:5: error: implicit declaration of function 'page_init'` |
| 评分机制 | `code/tools/inject.py` + `code/tools/grade.sh` + `code/Makefile:205` | `make grade` 复制到 `/tmp/lab2_grade` → 注入测试 → 跑 QEMU → 正则匹配两个 PASS |
| 工具链 | `command -v` | ✅ `/opt/riscv/bin/riscv64-unknown-elf-gcc`、`/usr/local/bin/qemu-system-riscv64`、`riscv64-unknown-elf-gdb` |
| `KADDR` 可用性 | `kern/mm/pmm.h:67-77` | ⚠️ **整段被注释掉了**，当前不可用 |
| 新文件是否需改 Makefile | `tools/function.mk:5` `listf` = `$(wildcard kern/mm/*)` | ✅ 放进 `kern/mm/` 的新 `.c` 自动参与编译，**不用改 Makefile** |
| `sizeof(struct Page)` | 实编译验证 | **40 字节** → 32768 页的描述符数组 = 1280 KiB |
| 组长/组员信息 | `/home/oslab/Lab/Lab1/report/report.md` | 2414034-丁欣怡、2412604-刘泽民、2412591-贺翰林；模板见 `Lab/实验报告模板.md` |

---

## 2. 实验要做什么：任务清单

| # | 任务 | 类型 | 交付物 | 落点文件 | 自动评分 |
|---|---|---|---|---|---|
| A | **功能模块一 任务一**：实现 `page_init()` | 编程（必做） | 代码 | `kern/mm/pmm.c` | ✅ Test 1（10 分） |
| B | **功能模块二 任务一**：理解 First-Fit + 回答思考题 | 阅读+报告（必做） | 报告文字 | `kern/mm/default_pmm.c`（只读） | ❌ |
| C | **功能模块二 任务二**：实现 Best-Fit 并接入 | 编程（必做） | 代码 + prompt + 思考题 | `kern/mm/best_fit_pmm.c`、`kern/mm/pmm.c`（1 行） | ✅ Test 2（10 分） |
| D | **功能模块二 任务三**：Buddy System，从零生成模块 + 接入 + **自带测试** | 编程（必做，无框架） | 代码 + 2 份 prompt + 测试说明 | 建议新增 `kern/mm/buddy_pmm.{c,h}` | ❌（人工评阅） |
| E | **Challenge**：OS 如何获取可用物理内存范围（思考题） | 选做加分 | 报告文字 | — | ❌ |
| F | **Challenge**：SLUB 任意大小内存分配 + 自带测试 | 选做加分 | 代码 + 2 份 prompt | 建议 `kern/mm/slub.{c,h}` | ❌ |
| G | **分支任务**：用双重 GDB 调试 QEMU 的地址翻译 | 选做/拓展 | 调试报告 | 需编译带 `--enable-debug` 的 QEMU 源码 | ❌ |
| H | **实验报告** | 必做 | `report/report.md`（Markdown） | — | ❌ |
| I | **Git 仓库**（gitee/github），代码+报告 push | 必做 | 仓库链接 | — | ❌ |
| J | 把两处 `YOUR CODE` 替换为**本人学号**、保留 `lab2` 注释 | 必做 | — | 同上 | ❌（提交规范） |

手册原文对“必做/选做”的界定：结尾写明“**Challenges 是选做**”，因此 **D（Buddy）属必做**、**E/F 属加分**。
但自动评分只覆盖 A 与 C —— 这是本次实验最容易“以为做完了其实报告没写”的坑。

---

## 3. 实验原理

### 3.1 为什么需要物理内存管理

物理地址空间是“一个以字节为单位的大数组”，但它不只对应 RAM，还包含外设（MMIO）。
当多个程序共享内存时，直接使用物理地址会冲突，而且要求程序占用连续内存，必然产生**外碎片**。
解决办法是引入“翻译”机制（页表），让每个程序拥有独立地址空间。
翻译的粒度是**页（4 KiB）**——把连续字节合并翻译，避免“一字节一个词条”的荒谬开销。

本次实验只做**第一层**：把物理内存按 4 KiB 页管理起来，为后续虚拟内存管理提供“分配/释放连续 n 页”的能力。

### 3.2 Sv39 三级页表（手册第 8 章）

- 页表项（PTE）8 字节；Sv39 页表项第 53–10 位是 44 位 PPN，第 9–0 位是状态位：`RSW | D | A | G | U | X | W | R | V`。
- `R|W|X` 决定 PTE 类型：`000` = 指向下一级页表；非全 0 即为**叶子**。
- 39 位虚拟地址 = `9 | 9 | 9 | 12`（三级索引 + 页内偏移）。
- **大页技巧**：把一级（最高级）PTE 直接做成叶子 → 映射 **1 GiB**（gigapage）；二级 PTE 做叶子 → **2 MiB**（megapage）。
- 页表基址在 `satp`：`MODE(4) | ASID(16) | PPN(44)`；Sv39 的 `MODE = 8`。
- 一个页恰好放 512 个 PTE（4096/8），这就是 512 的来历。

### 3.3 内核如何进入虚拟地址空间（Lab2 的“页表”部分，**框架已给**）

`tools/kernel.ld` 把 `BASE_ADDRESS` 设为 `0xFFFFFFFFC0200000`，于是内核符号都是高地址，
但 OpenSBI 把内核加载到物理地址 `0x80200000`，且此时 `satp.MODE = Bare`。

`entry.S` 的 `kern_entry` 用一个 **1 GiB 大页**把
`[0xffffffffc0000000, 0xffffffffffffffff]` 映射到 `[0x80000000, 0xc0000000)`：

```
la/lui+sub 算出 boot_page_table_sv39 的物理页号
li t1, 8 << 60        ; Sv39
csrw satp, t0
sfence.vma            ; 刷新 TLB
lui sp, %hi(bootstacktop)
jr  kern_init
```

偏移量 `PHYSICAL_MEMORY_OFFSET = 0xFFFFFFFF40000000` 就是 `KERNBASE(虚拟) - 0x80200000(物理)` 得到的
“虚实映射偏移”。**这部分在 Lab2 骨架里已经写好，不需要学生改**，但它是 §3.4 里所有地址换算的前提。

### 3.4 三个关键全局量与地址换算（`page_init` 的核心）

```
nbase        = DRAM_BASE / PGSIZE = 0x80000000 / 0x1000 = 0x80000      （常量，已给）
npage        = PPN(memory_base + memory_size) = PPN(0x88000000) = 0x88000   （page_init 设置）
va_pa_offset = PHYSICAL_MEMORY_OFFSET                        （page_init 设置）
pages        = 描述符数组首地址（虚拟地址）                    （page_init 设置）

物理地址 pa  ↔  虚拟地址 va :  va = pa + va_pa_offset
Page*        ↔  物理页号     :  page2ppn(page) = page - pages + nbase
Page*        ↔  物理地址     :  page2pa(page)  = page2ppn(page) << 12
物理地址     →  Page*        :  pa2page(pa)    = &pages[PPN(pa) - nbase]   （含 npage 越界 panic）
虚拟地址     →  物理地址      :  PADDR(kva) = kva - va_pa_offset           （要求 kva >= KERNBASE）
```

⚠️ 两个易错点：
1. `pages` 是**虚拟地址**，要与 `end`（也是虚拟地址）比较；但算“描述符数组之后”的物理地址必须过 `PADDR`。
2. `KADDR` 在 `pmm.h` 里**整段被注释**，不能用；需要时用 `pa + va_pa_offset` 自己算。

### 3.5 物理内存里到底谁占了地方（决定空闲区起点）

QEMU `virt` 的 DRAM 默认 `[0x80000000, 0x88000000)`，共 128 MiB。其中：

| 区段 | 占用者 | 能否分配 |
|---|---|---|
| `[0x80000000, 0x80200000)` | OpenSBI 固件 | ❌ |
| `[0x80200000, KernelEnd)` | 内核 text/data/bss + 启动栈 + 引导页表 | ❌ |
| DTB 所在区 | 设备树 blob（只读取，之后理论上可复用） | 本实验不回收 |
| `[KernelEnd 之后的空隙]` | 页对齐产生的空洞 | ❌ |
| `[描述符数组]` | `struct Page[0x8000]` = 1280 KiB | ❌ |
| `[数组之后, 0x88000000)` | 真正的空闲物理页 | ✅ 交给 `init_memmap()` |
| 末尾不足一页 | 尾部碎片 | ❌ |

手册原文对布局的措辞是：OpenSBI 区之后依次是**内核镜像 → 对齐空隙 → `Page` 描述符数组 → 再次按页对齐的空隙 → 最终可分配的连续物理页**。

### 3.6 `pmm_manager` 框架与调用链

C 没有面向对象，ucore 用**函数指针结构体**模拟“类”：

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
```

本次实验的完整调用链：

```
kern_init()                                    [kern/init/init.c]
  └─ dtb_init()                                [kern/driver/dtb.c]  → 存 memory_base / memory_size
  └─ pmm_init()                                [kern/mm/pmm.c]
       ├─ init_pmm_manager()   ← 选管理器（★任务C要在这里切到 best_fit）
       ├─ page_init()          ← ★任务A：建描述符数组 + 注册空闲区
       │    └─ init_memmap() → pmm_manager->init_memmap()
       ├─ check_alloc_page()   → pmm_manager->check()   （跑 basic_check/default_check/best_fit_check）
       └─ 【make grade 注入】test_page_init(); test_best_fit();
```

`page_init()` 与 `pmm_manager` 的分工：`page_init` 负责**发现内存、算清边界、建好描述符**；
`pmm_manager` 负责**具体的分配策略**。二者通过 `init_memmap(base, n)` 交接。

### 3.7 `struct Page` 与空闲链表约定

```c
struct Page {
    int ref;                 // 引用计数
    uint64_t flags;          // bit0 = PG_reserved，bit1 = PG_property
    unsigned int property;   // 空闲块页数，仅“块头”有效
    list_entry_t page_link;  // 链入 free_list
};
```

- `PG_reserved = 1`：内核保留，不可被分配/释放。
- `PG_property = 1`：本页是**空闲块的头页**，`property` 是该块页数。
- 空闲链表只挂**块头**；中间页不挂链、`property` 无意义。
- `le2page(le, page_link)` 用 `to_struct`（即 Linux 的 `container_of`）从链表节点反推 `struct Page*`。

### 3.8 First-Fit vs Best-Fit vs Buddy

| 算法 | 选择准则 | 优点 | 缺点 |
|---|---|---|---|
| **First-Fit**（`default_pmm.c`，已给） | 按地址顺序，第一个 `property >= n` 的块 | 快、实现简单 | 低地址易被切碎，大块需求失败率高 |
| **Best-Fit**（任务 C 要写） | 满足 `property >= n` 的**最小** `property` 块 | 碎片更小 | 需全表扫描；残留极小碎片 |
| **Buddy System**（任务 D 要写） | 块大小恒为 2ⁿ，不足则向上分裂，释放时与“伙伴”合并 | 分配/释放 O(log n)，合并规则确定 | 内部碎片（最多 50%） |

⚠️ **Best-Fit 与 First-Fit 在 ucore 里的可观测差异**：`best_fit_check()` 有一条断言
（见 §5.3 第 4 点）会检查 `alloc_pages(1)` 返回的块是地址更靠后的“更小块”。
如果 Best-Fit 实际按 First-Fit 逻辑写，这条断言会失败 → Test 2 拿不到分。

---

## 4. 评测机制（组内必读，决定了怎么写才不白干）

### 4.1 `make grade` 的完整流程

```makefile
# code/Makefile:205
grade:
	$(MAKE) clean
	$(RM) -rf /tmp/lab2_grade ; $(MKDIR) /tmp/lab2_grade
	tar -C . ... | tar -C /tmp/lab2_grade -xf -      # 复制整个 code/（不改学生原文件）
	python3 tools/inject.py /tmp/lab2_grade         # 注入评测代码
	cd /tmp/lab2_grade && $(SH) tools/grade.sh      # 编译 + QEMU + 正则打分
```

`inject.py` 在临时副本里做 4 件事：

1. 把 `tools/test_lab2.{c,h}` 复制到 `kern/test/`（所以**学生不能自建 `kern/test`**）；
2. 给 Makefile 的 `KINCLUDE`、`KSRCDIR` 追加 `kern/test`（所以 **Makefile 里原本不能出现 `kern/test`**）；
3. 在 `pmm.c` 的 `#include <pmm.h>` 下一行插入 `#include <test_lab2.h>`（所以该行必须**恰好出现一次**）；
4. 在 `pmm_init()` 函数体末尾插入 `test_page_init(); test_best_fit();`。

### 4.2 判分点（`grade.sh`）

| 分值 | 匹配字符串（逐行 `^...$` 精确匹配） |
|---|---|
| 10 | `[Test 1] Page Init: PASS` |
| 10 | `[Test 2] Best-Fit: PASS` |

未满分则 `grade.sh` 退出码为 1。

### 4.3 由注入机制导出的 6 条硬约束（违反任一 → 0 分）

1. **不要创建 `kern/test` 目录**，也不要手动引入 `test_lab2.h`。
2. **`Makefile` 里不能出现字符串 `kern/test`**；原则上**不要改 Makefile**。
3. **`pmm.c` 中 `#include <pmm.h>` 必须恰好一次**，不要重复包含。
4. **`pmm.c` 中只能有一个可被 `\bpmm_init` 匹配的函数**。禁止出现 `xxx_pmm_init(...)` 这类命名
   （正则 `...[A-Za-z0-9_ \t\n\*]*?\bpmm_init[ \t\n]*\(...\)[ \t\n]*\{` 会误命中 → “ambiguous” → 脚本报错）。
5. **不得修改** `basic_check()` / `best_fit_check()` / `default_check()` / `pmm_manager` 结构体定义。
6. **不得把 Buddy 或其它管理器留在 `init_pmm_manager()` 里**——必须切到 `best_fit_pmm_manager`。

> 这些约束的全文见我整理的 [lab2_AI_constraints.md](manual/lab2_AI_constraints.md)，可直接贴给 GPT/Codex。

---

## 5. 如何去做：三条技术路线

### 5.1 任务 A：`page_init()`（`kern/mm/pmm.c`）

**目标**：把 DTB 报告的内存范围，变成“描述符数组 + 空闲页链表”。

**实现步骤（伪代码级）**：

```
1. 读 memory_base / memory_size（get_memory_base/size）
2. 校验：base==0 或 size==0 → cprintf 报错 + return（或 panic）；并输出 physical memory map
3. va_pa_offset = PHYSICAL_MEMORY_OFFSET        ← 必须在任何 PADDR 之前
4. npage = PPN(memory_base + memory_size)
5. pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE)     ← 虚拟地址
6. 对 [nbase, npage) 的每个描述符：flags=0 / ref=0 / SetPageReserved()   ← 全置保留
7. 计算空闲区物理起点：
     pages_end_pa = PADDR((uintptr_t)pages) + (npage - nbase) * sizeof(struct Page)
     free_start_pa = ROUNDUP(pages_end_pa, PGSIZE)
     free_end_pa   = ROUNDDOWN(memory_base + memory_size, PGSIZE)
8. if (free_end_pa > free_start_pa)
       init_memmap(pa2page(free_start_pa), PPN(free_end_pa) - PPN(free_start_pa))
```

**必须满足的 8 条测试断言**：见 [lab2_AI_constraints.md §五](manual/lab2_AI_constraints.md)。
其中最容易踩的三条：
- 检查 1：`va_pa_offset` 必须**先于**任何 `PADDR` 赋值，否则 `PADDR` 会用旧值/0。
- 检查 6/7：`end` 所在页与 `pages` 起始页都必须保持 `PageReserved` —— 关键在第 6 步“**全部先置 reserved**”，
  而 `init_memmap()` 内部会 `assert(PageReserved(p))` 然后把它清零。
- 检查 5：`nr_free_pages() > 0` 且 `<= npage - nbase` —— 空闲区不能算成 0，也不能把保留区算进去。

**预期数值**（默认 128 MiB）：`npage = 0x88000`、`nbase = 0x80000`、描述符数组 1280 KiB。
若把 `npage` 写成 `PPN(KERNTOP)` 或 `KMEMSIZE/PGSIZE` 之类的值，检查 3 会 FAIL。

**验证命令（本任务做完即可首次自测）**：
```bash
cd code && make            # 先确认能编过（-Werror 下无 warning）
make grade                 # 期望：Test 1 PASS（此时 Test 2 仍 FAIL，属正常）
```

### 5.2 任务 C：Best-Fit（`kern/mm/best_fit_pmm.c`）

**做法**：以 `default_pmm.c` 为模板，把“选第一个够用的块”改成“选够用的最小块”。

需要自建的内容（该文件目前只有两个测试函数）：

```
static free_area_t free_area;                 // 名字必须是 free_list / nr_free
#define free_list (free_area.free_list)
#define nr_free   (free_area.nr_free)

best_fit_init()          // list_init(&free_list); nr_free = 0;
best_fit_init_memmap()   // 同 default：清 property/ref、置块头、按地址有序插入、nr_free += n
best_fit_alloc_pages(n)  // ★ 全表扫描，取 property >= n 中最小的块
best_fit_free_pages()    // 同 default：有序插入 + 与前后相邻块合并
best_fit_nr_free_pages() // return nr_free
const struct pmm_manager best_fit_pmm_manager = { ... .check = best_fit_check };
```

**唯一的算法差异点**（也是得分点）：

```c
/* 以 default_alloc_pages 的选择循环为骨架，把 break-on-first 改成取最小 */
struct Page *best = NULL;
for (遍历 free_list) {
    if (p->property >= n && (best == NULL || p->property < best->property))
        best = p;
}
```

**别忘了**：`kern/mm/pmm.c` 的 `init_pmm_manager()` 里把
`pmm_manager = &default_pmm_manager;` 改成 `&best_fit_pmm_manager`。
这一行没改，Test 2 直接 0 分。

**`best_fit_check()` 的关键断言**（决定 Best-Fit 是否“真的 Best”）：
```c
free_pages(p0 + 1, 2);   /* 块A: [p0+1, len=2] */
free_pages(p0 + 4, 1);   /* 块B: [p0+4, len=1] */
assert(alloc_pages(4) == NULL);
assert((p1 = alloc_pages(1)) != NULL);
assert(p0 + 4 == p1);    /* ★ 必须是“最小的够用块”B，First-Fit 会拿到 A 而失败 */
```

**验证命令**：
```bash
cd code && make && make grade     # 期望：Total Score: 20/20
```

### 5.3 任务 D：Buddy System（建议 `kern/mm/buddy_pmm.{c,h}`）

**特点**：无框架、无测试，需自己设计。要点：

1. **数据结构**：按阶（order）维护 `order` 个空闲链表，`free_area[o]` 里的块大小恒为 `2^o` 页。
   或用一个 `free_list` + `property` 表示块大小的 2 的幂（兼容现有 `free_area_t`）。
2. **`alloc_pages(n)`**：求 `order = ceil(log2(n))`；在 `free_area[order]` 找块；
   没有则向更高阶借块并**分裂**（一半留用、一半挂到低阶链表），直到找到或失败。
3. **`free_pages(base, n)`**：把块放回 `free_area[order]`；
   然后**循环与伙伴合并**：`buddy = base XOR (1 << order)`（以“页号偏移”计算），
   若伙伴也在同一阶空闲链表中，则合并并 `order++`，直到无法合并。
4. **注册**：`const struct pmm_manager buddy_pmm_manager = { ... .check = buddy_check };`
   并提供自己的 `buddy_check()` 测试函数（手册要求**第二份 prompt 就是用来生成测试的**）。
5. **接入方式（关键！）**：
   - 因为最终必须让 `best_fit_pmm_manager` 处于活动状态（Test 2 的要求），
     Buddy 的验证要采用「**临时切换法**」：
     ```c
     /* 临时验证 Buddy：验证完必须改回 best_fit_pmm_manager */
     pmm_manager = &buddy_pmm_manager;
     ... buddy 测试 ...
     /* pmm_manager = &best_fit_pmm_manager;  ← 提交前务必恢复 */
     ```
     并在报告/代码注释里说明这是临时验证。
   - 更稳妥的做法：让 `buddy_check()` 通过**直接调用 buddy 的管理函数**测试，
     在 `pmm_init()` 末尾临时调用一次，验证完毕删除该调用。
6. **新文件无需改 Makefile**（`kern/mm/*` 被通配收集）。

**验证**：`make` 编过 → 临时调用 `buddy_check()` 跑一遍 → 恢复 `best_fit` → 再跑 `make grade` 确认仍是 20/20。

### 5.4 加分项 E / F（Challenge）

- **E（思考题，报告题）**：思路要点 —— x86 用 BIOS `INT 0x15, E820` 内存地图；RISC-V/ARM 用设备树（DTB）；
  还有 UEFI 内存描述符表、ACPI、多 ARCH 下的 `multiboot` 内存映射。
  OS 无法提前知道时，必须**由固件/引导协议报告**，或通过**探测**（逐页写读、
  利用异常处理探测可写区间、读内存控制器寄存器）。
- **F（SLUB）**：两层架构 —— 第一层基于页分配器拿 slab（通常 1 页或若干页），
  第二层把 slab 切成对象，按 `kmem_cache`（对象大小分级，如 8/16/32…/4096）管理，
  空闲对象用侵入式链表串起来；释放时对象回链，slab 全空可归还页分配器。
  同样需要**两份 prompt**（生成实现 + 生成测试）。

### 5.5 验证与提交清单

```bash
cd /home/oslab/Lab/Lab2/code
make -j                     # 1. 编译通过（-Werror 无 warning）
make qemu                   # 2. 观察串口输出：memory management: best_fit_pmm_manager
                            #    + physical memory map + check_alloc_page() succeeded!
make grade                  # 3. 自动评分：Total Score: 20/20
grep -rn "YOUR CODE" kern/  # 4. 确认两处标记已替换为本人学号（且 lab2 注释保留）
```

`make qemu` 用 `Ctrl-A X` 退出 QEMU。

---

## 6. 三人分工方案

> 人员沿用 Lab1 的名单：**2414034-丁欣怡、2412604-刘泽民、2412591-贺翰林**（组长）。
> 若实际角色与我假设的不同，按“模块难度 + 谁做整合”调换姓名即可，分工骨架不变。

### 6.1 分工总表

| 成员 | 角色定位 | 负责的代码 | 负责的报告部分 | 主要交付物 |
|---|---|---|---|---|
| **2412591-贺翰林（组长）** | 关键路径 + 整合 | **任务 A：`page_init()`**（`kern/mm/pmm.c`）+ 任务 C 的 1 行管理器切换 | §二实验环境、§三整体逻辑分析、§五测试与验证、§六总结；**全文统稿** | `pmm.c` 实现、`report/report.md` 定稿、Git 仓库 |
| **2414034-丁欣怡** | 分配算法 | **任务 C：Best-Fit**（`kern/mm/best_fit_pmm.c`） | §四 功能模块二-任务二（含 prompt、迭代、改进思考题）；§四 任务一 First-Fit 理解题 | `best_fit_pmm.c` 实现、`report/sections/ding.md` |
| **2412604-刘泽民** | 拓展算法 | **任务 D：Buddy System**（新增 `kern/mm/buddy_pmm.{c,h}` + 自带测试） | §四 功能模块二-任务三（含 2 份 prompt、测试设计说明） | Buddy 模块 + 测试、`report/sections/liu.md` |
| 加分项 | 弹性分配 | F（SLUB）→ 刘泽民（若 Buddy 提前完成）<br>E（思考题，纯报告）→ 丁欣怡（工作量小，优先补）<br>G（GDB 双重调试）→ 全组任一人有余力时做，不阻塞主线 | — | 见 §6.4 |

**为什么这么分？**

1. **`page_init()` 是所有人的关键路径**（不做完谁都测不了），交给组长可以最快推动、随时协调，
   且组长本来就要做整合、需要最先理解全局地址换算。
2. **Best-Fit 与 Buddy 互不依赖**（不同文件、不同 manager），可完全并行。
   两人都不碰 `pmm.c`，从而**避免了唯一的高冲突文件**。
3. **报告量与代码量互补**：丁欣怡代码量中等但报告题多（First-Fit 理解 + Best-Fit 改进 + Challenge E），
   刘泽民代码量最大但报告部分最独立，组长代码量最小但整合成本最高。

### 6.2 文件所有权矩阵（防冲突纪律）

| 文件 | 唯一所有者 | 其他人怎么改 |
|---|---|---|
| `kern/mm/pmm.c` | **贺翰林** | 提“一句话 patch/issue”，由所有者代改 |
| `kern/mm/best_fit_pmm.c` | **丁欣怡** | 只读 |
| `kern/mm/buddy_pmm.c` / `.h`（新增） | **刘泽民** | 只读 |
| `kern/mm/default_pmm.c` | 只读（参考实现） | 任何人不得改 |
| `kern/mm/pmm.h`、`memlayout.h`、`mmu.h`、`list.h` | 只读 | 任何人不得改 |
| `Makefile`、`tools/*` | 只读 | **任何人不得改**（改了会破坏注入/评分） |
| `report/report.md` | **贺翰林**（最终合并） | 组员只写 `report/sections/<name>.md` |
| `report/images/` | 全体 | 各自命名 `lab2-<模块>-<序号>.png` |

> 组长需在开工前把这条纪律写进群公告。Lab2 的冲突面很小，但 `pmm.c` 是唯一雷区
> （第 4.3 节的 6 条硬约束里，4 条与 `pmm.c` 有关）。

### 6.3 时间线与里程碑

| 里程碑 | 内容 | 负责人 | 建议时点 |
|---|---|---|---|
| **M0** | 建好 Git 仓库、把 `实验报告模板.md` 复制为 `report/report.md`、确认基线 `make` 报错 | 组长 | 第 1 天 |
| **M1** | `page_init()` 完成 → `make` 通过、`make grade` 出现 Test 1 PASS | 组长 | 第 1–2 天 |
| **M2** | Best-Fit 完成 + 切换管理器 → `make grade` **20/20** | 丁欣怡（改动由组长落地 `pmm.c`） | 第 2–3 天 |
| **M3** | 各人提交自己报告分节（含 prompt 记录与迭代过程） | 全体 | 第 3–4 天 |
| **M4** | Buddy 模块 + 自带测试完成（临时切换验证后切回 Best-Fit） | 刘泽民 | 第 3–5 天 |
| **M5** | 加分项（E 优先，F/G 视时间） | 丁欣怡 / 刘泽民 | 第 4–5 天 |
| **M6** | 组长统稿：整体逻辑、知识点对比、截图、格式统一 | 组长 | 第 5–6 天 |
| **M7** | 复查清单：`make grade` 20/20、`YOUR CODE`→学号、注释保留、报告与代码同时 push | 组长 | 提交前 1 天 |

> 关键路径是 **M1 → M2**（自动评分的 20 分全部在这里）。M3–M7 是可并行的文本工作。

### 6.4 每人详细任务卡

#### 贺翰林（组长）—— 任务 A + 整合

- [ ] 读手册第 12、14、15 章 + `pmm.c/pmm.h/memlayout.h/dtb.c`
- [ ] 设计 prompt 让 AI 实现 `page_init()`（spec 用 [lab2_AI_constraints.md](manual/lab2_AI_constraints.md) §四.4.1 + §五）
- [ ] 逐条对照 8 项断言自检（尤其 `va_pa_offset` 赋值顺序、全置 reserved、`npage` 取值）
- [ ] `make` 通过 → `make grade` 看到 `[Test 1] Page Init: PASS`
- [ ] 把 `init_pmm_manager()` 切到 `best_fit_pmm_manager`（协同丁欣怡）
- [ ] **整合**：Git 仓库初始化（`.gitignore` 排除 `obj/`、`bin/`、`.qemu.out`）、分支策略、报告骨架
- [ ] 撰写报告 §二、§三、§五、§六，并把各人的 `report/sections/*.md` 合并进 `report/report.md`
- [ ] 最终复查 §5.5 的 4 条提交清单
- [ ] 交付物：`kern/mm/pmm.c`、`report/report.md`、仓库

#### 丁欣怡 —— 任务 C（Best-Fit）+ 思考题 + Challenge E

- [ ] 读手册第 16 章 + 完整读懂 `default_pmm.c` 五个函数（这是任务一“理解 First-Fit”的答案来源）
- [ ] 写报告：First-Fit 如何初始化空闲页 / 分配连续页 / 回收并合并相邻块 + **First-Fit 改进空间**
- [ ] 设计 prompt → 实现 `best_fit_pmm.c`（spec 见 §5.2 与约束清单 §四.4.2、§六）
- [ ] 重点自检：`alloc_pages` 的“最小够用块”选择、`free_pages` 合并逻辑、`best_fit_pmm_manager` 命名与注册
- [ ] 回答 **Best-Fit 改进空间**（如：分离空闲链表 + 位图、按 order 分级减少扫描、引入伙伴/页缓存）
- [ ] Challenge E 思考题（OS 如何获取可用物理内存范围）
- [ ] 交付物：`kern/mm/best_fit_pmm.c`、`report/sections/ding.md`（含最终 prompt + 迭代过程 + bug 与解决）

#### 刘泽民 —— 任务 D（Buddy）+ 加分 F

- [ ] 读手册“功能模块二 任务三”+ 参考资料《伙伴分配器的一个极简实现》
- [ ] **第一份 prompt**：让 AI 参考 ucore 的 `pmm_manager` 框架，从零生成 Buddy 模块并接入
- [ ] **第二份 prompt**：让 AI 针对上面的实现**设计测试函数**（覆盖：分配/释放、分裂、伙伴合并、
      跨阶分配、`n` 非 2 的幂、空闲耗尽、`nr_free` 一致性）
- [ ] 自检测试“为什么能有效检验实现”（报告要写）
- [ ] 用**临时切换法**验证，验证后**必须切回 `best_fit_pmm_manager`**，再跑一次 `make grade` 确认 20/20
- [ ] 报告：§四 任务三 + 两份 prompt + 测试覆盖说明
- [ ] （余力）Challenge F：SLUB（同样两份 prompt + 测试）
- [ ] 交付物：`kern/mm/buddy_pmm.{c,h}`、`report/sections/liu.md`

### 6.5 报告整合流程（组长操作手册）

**Step 1 — 用现成模板搭骨架（0 成本，风格与 Lab1 一致）**

```bash
cp /home/oslab/Lab/实验报告模板.md /home/oslab/Lab/Lab2/report/report.md
mkdir -p /home/oslab/Lab/Lab2/report/sections /home/oslab/Lab/Lab2/report/images
```

**Step 2 — 分工写分节，一人一文件，只在固定标题下写入**

`report/sections/ding.md`、`report/sections/liu.md`、`report/sections/he.md`。
每个分节文件用**统一的两级标题**，组长合并时按标题整段搬运：

```markdown
## 功能模块二 任务二：Best-Fit 连续物理内存分配算法
**负责人：** 2414034-丁欣怡
### 需要实现/修改的函数
### 功能说明
### 最终提示词
### 实现迭代过程            ← 第1次/第2次…，每次写“遇到的问题 + 修改提示词的策略”
### 思考题：Best-Fit 的改进空间
```

**Step 3 — 组长合并与补写**

| 报告章节 | 来源 | 谁写 |
|---|---|---|
| 实验基本信息、小组分工、实验环境 | 组长汇总 | 组长（AI 工具表照实填，参考 Lab1 格式） |
| 一、实验目的 | 手册第 2 章 | 组长 |
| 二、实验环境 | 工具链实测结果 | 组长 |
| 三、实验整体逻辑分析（3.1 主线 / 3.2 逐步实现 / 3.3 核心函数表） | 手册 + 代码调用链 | 组长（这是“整体逻辑”题，必须组长写） |
| 四、实验内容与实现 → 模块一 任务一 + Challenge E | `sections/ding.md` | 丁欣怡 |
| 四 → 模块二 任务一（First-Fit 理解题） | `sections/ding.md` | 丁欣怡 |
| 四 → 模块二 任务二（Best-Fit） | `sections/ding.md` | 丁欣怡 |
| 四 → 模块二 任务三（Buddy）+ F | `sections/liu.md` | 刘泽民 |
| 五、测试与验证（`make qemu` 输出 + `make grade` 20/20 截图） | 组长统一截图 | 组长 |
| 六、实验总结与收获（知识点对照 + OS 原理缺口 + AI 协作经验） | 全组各提供 2 条，组长定稿 | 组长 |

**Step 4 — 定稿前强制对照手册第 18 章的 7 条报告要求**（逐条打勾，缺一不可）：

1. ✅ Markdown 格式、以文本说明为主
2. ✅ 说明对**本章整体逻辑**的理解（按什么顺序、围绕哪些功能逐步实现）
3. ✅ 说明对**每个功能的核心函数/模块**的理解
4. ✅ 说明每个练习的 **spec 怎么写、prompt 怎么写**
5. ✅ 说明 AI 生成代码过程中**出现了什么 bug、怎么解决**
6. ✅ 列出本实验重要知识点 ↔ 对应 OS 原理知识点（含义、关系、差异）
7. ✅ 列出 OS 原理中很重要、但本实验**没有对应上**的知识点
   （可答：进程/线程调度、虚拟内存换页与页面置换算法、文件系统、中断/异常完整处理、
   TLB 刷新与 ASID 管理、页表在进程切换时的切换、缺页异常处理等）

**Step 5 — 提交**

```bash
cd /home/oslab/Lab/Lab2
git add -A && git commit -m "Lab2: 物理内存管理（page_init + Best-Fit + Buddy）+ 实验报告"
git push
```

⚠️ 手册要求：报告放在 **labcodes/lab2 目录下**。当前结构是 `Lab2/code` + `Lab2/report`，
建议在 `code/` 内放报告副本，或把报告目录并入提交仓库结构，并在报告里注明路径。

---

## 7. 与 GPT / Codex 协作的规范

### 7.1 每次对话的输入包（固定三件套）

1. [lab2_manual_full.md](manual/lab2_manual_full.md) —— 手册 Lab2 全文（官方原文，避免模型幻觉）
2. [lab2_AI_constraints.md](manual/lab2_AI_constraints.md) —— 硬约束与接口清单（**这次协作的核心**）
3. 相关源码原文：`pmm.h`、`memlayout.h`、`default_pmm.c`（Best-Fit 时）、`pmm.c`（page_init 时）

### 7.2 提示词结构建议（对接报告里要求的 “spec + prompt”）

```markdown
[ROLE]   你是熟悉 riscv64-ucore 的内核开发者。
[TASK]   在 kern/mm/best_fit_pmm.c 的 "//lab2 function2-2 YOUR CODE" 处实现完整 Best-Fit 管理器。
[CONTEXT]（粘贴 lab2_AI_constraints.md 第四节 4.2、第六节；以及 default_pmm.c 全文）
[CONSTRAINTS]
  - 不得修改 basic_check / best_fit_check
  - 不得改 Makefile，不得新建 kern/test，不得新增第二个可匹配 \bpmm_init 的函数
  - 不使用 KADDR（已被注释）
  - 必须通过 -Werror -std=gnu99 -nostdinc -mcmodel=medany
[ACCEPTANCE]
  - alloc_pages 必须选“满足 property >= n 的最小 property”块
  - best_fit_check() 中 assert(p0 + 4 == p1) 必须成立
[OUTPUT] 只输出该文件需要新增的完整代码段，并附每段的作用与前置/后置条件。
```

### 7.3 纪律

- **保留完整的对话记录**（报告要写“prompt 怎么写的、bug 怎么解决的”）。
- 每人建 `report/sections/<name>.md` 时，**当场记录**每轮 prompt 与报错，不要事后回忆。
- AI 生成的代码**必须逐行读懂**再合入（报告会问“你对核心函数的理解”，且 Assert 失败时得自己定位）。
- 每轮迭代固定格式：`问题 → 现象/报错 → 原因分析 → 提示词怎么改 → 结果`。

---

## 8. 风险清单

| 风险 | 后果 | 对策 |
|---|---|---|
| `pmm.c` 中出现了第二个 `*pmm_init` 函数 | `inject.py` “ambiguous” → **0 分** | 命名避开 `pmm_init` 后缀；提交前 `grep -n "pmm_init" kern/mm/pmm.c` 人工确认 |
| `Makefile` 被误加 `kern/test` | inject 直接报错 → **0 分** | Makefile 标记只读；提交前 `grep -n "kern/test" Makefile` 应为空 |
| 忘记把 `pmm_manager` 切到 Best-Fit | Test 2 = 0 分 | 列入 M2 验收项；`make qemu` 看到 `best_fit_pmm_manager` 才算过 |
| Best-Fit 实际写成 First-Fit | `assert(p0+4 == p1)` 失败 → 20 分只拿 10 | 用 §5.2 的选择循环自检 |
| Buddy 留在 `init_pmm_manager()` 里 | Test 2 = 0 分 | 临时切换法 + 提交前 `grep -n "pmm_manager =" kern/mm/pmm.c` |
| `KADDR` 已被注释却照抄老教程 | 编译失败 | 用 `pa + va_pa_offset` |
| `-Werror` 下有 unused 变量/函数 | 编译失败 | 每次改完立刻 `make` |
| 只做了编程任务、漏了报告要求 | 报告分丢失 | 用 §6.5 Step 4 的 7 条清单逐条打勾 |
| Buddy 工作量超预期 | 拖到 deadline | 设 M4 截止；Buddy 不是自动评分项，优先保证 M1/M2 的 20 分 + 报告完整 |
| 分支任务 G 需编译 QEMU 源码 | 本机**无 qemu-4.1.1 源码**，需下载编译，耗时大 | 明确列为“不阻塞主线的选做”，有余力再做 |
| 无 Git 仓库 | 提交要求未满足 | M0 就建好仓库（当前 `Lab2/` 不是 git 仓库） |

---

## 附：本次产出的文件

| 文件 | 说明 |
|---|---|
| [manual/lab2_manual_full.md](manual/lab2_manual_full.md) | 手册 Lab2 **全部 18 章原文**（HTML→Markdown 无损转换，含代码块与表格），供发给 GPT |
| [manual/lab2_AI_constraints.md](manual/lab2_AI_constraints.md) | 给 AI 的**硬约束与接口清单**（评分机制内幕、8 项断言、API 白名单、禁止修改清单、期望输出） |
| 本文件 | 实验分析 + 原理 + 技术路线 + 三人分工 + 报告整合流程 |
