# Lab2 给 AI（GPT / Codex）的硬约束与接口清单

> 用途：与《lab2_manual_full.md》一起粘贴给 GPT / Codex，作为“规格（spec）”输入。
> 本文件内容全部由**项目内真实源码**（`code/tools/test_lab2.c`、`code/tools/inject.py`、`code/tools/grade.sh`、`code/Makefile`、`code/kern/mm/*`）核对得出，不是猜测。

---

## 一、项目与工具链

| 项 | 值 |
|---|---|
| 内核 | riscv64-ucore（bbl-ucore 血统），Sv39 三级页表 |
| 平台 | QEMU `virt`，`-bios default`，内核加载到物理地址 `0x80200000` |
| 编译 | `riscv64-unknown-elf-gcc`（本机 `/opt/riscv/bin`） |
| 运行 | `qemu-system-riscv64`（本机 `/usr/local/bin`） |
| 构建 | `make`；运行 `make qemu`；评测 `make grade` |
| 编译选项 | `-std=gnu99 -Werror -Wall -O2 -nostdinc -mcmodel=medany -g` |
| 注意 | **`-Werror`**：任何 warning（含“隐式函数声明”“未使用变量”）都会导致构建失败 |

---

## 二、当前骨架状态（基线）

1. `make` 直接失败：
   ```
   kern/mm/pmm.c:77:5: error: implicit declaration of function 'page_init'
   ```
   即 `pmm_init()` 调用了 `page_init()`，但**全项目没有任何地方定义它**。
2. `kern/mm/best_fit_pmm.c` 只提供 `basic_check()` 与 `best_fit_check()` 两个测试函数，
   **不提供任何管理函数，也不提供 `best_fit_pmm_manager` 结构体**；文件第 6 行是
   `//lab2 function2-2 YOUR CODE`。
3. `kern/mm/default_pmm.c` 是**完整可用的 First-Fit 参考实现**，且已通过 `default_pmm_manager` 注册。
4. `kern/mm/pmm.c` 第 64 行是 `//lab2 function1 YOUR CODE`。
5. 全项目仅有两处 `lab2 ... YOUR CODE` 标记（`pmm.c:64`、`best_fit_pmm.c:6`）。

---

## 三、自动评分机制 `make grade`（**最重要，必须逐条遵守**）

`make grade` 的行为链：

```
make grade
 └─ make clean
 └─ 把整个 code/ 复制到 /tmp/lab2_grade        （不修改学生原文件）
 └─ python3 tools/inject.py /tmp/lab2_grade    （注入评测代码）
 └─ cd /tmp/lab2_grade && sh tools/grade.sh    （编译 + QEMU + 正则匹配打分）
```

`inject.py` 对**临时副本**做的 4 件事：

| # | 动作 | 由此产生的硬约束 |
|---|---|---|
| 1 | 把 `tools/test_lab2.c/.h` 复制到 `kern/test/` | 学生**不得**自己创建 `kern/test` 目录 |
| 2 | 在 Makefile 的 `KINCLUDE`、`KSRCDIR` 后追加 `kern/test` | **Makefile 中原本不得出现字符串 `kern/test`**，否则 inject 直接报错退出（0 分） |
| 3 | 在 `pmm.c` 中 `#include <pmm.h>` 那一行的**正下方**插入 `#include <test_lab2.h>` | ①`pmm.c` 中 `#include <pmm.h>` 必须**恰好出现一次**；②`pmm.c` 中原本不得有 `#include <test_lab2.h>` |
| 4 | 在 `pmm_init()` 函数体**末尾**（最后一个 `}` 之前）插入 `test_page_init(); test_best_fit();` | 见下方“函数定位约束” |

### 3.1 函数定位约束（inject.py 用正则 + 花括号配平找函数）

`inject.py` 用如下正则在全文件中查找 `pmm_init`：

```
^[ \t]*(?:static[ \t]+)?[A-Za-z_][A-Za-z0-9_ \t\n\*]*?\bpmm_init[ \t\n]*\([^;{}]*\)[ \t\n]*\{
```

并且要求**匹配次数恰好为 1**，否则报错退出（0 分）。因此：

- `pmm.c` 中只能有**一个**名字里以 `pmm_init` 结尾（`\b` 词边界）的函数定义。
- **禁止**在 `pmm.c` 中新增任何形如 `xxx_pmm_init(...)` 的函数（会被误匹配 → “ambiguous” → 0 分）。
- `page_init()` 不要起类似 `page_pmm_init` 的名字。
- `pmm_init()` 的签名保持 `void pmm_init(void)`，不要改参数/加 `static`。
- 不要在 `pmm.c` 里用奇怪的写法包一层宏去“生成” `pmm_init`。

### 3.2 评测脚本 `grade.sh` 的判分点

`qemuopts="-machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000"`，
抓串口输出后用**精确字符串匹配**：

| 分值 | 标签 | 匹配的字符串（逐行完全匹配 `^...$`） |
|---|---|---|
| 10 | `check page init` | `[Test 1] Page Init: PASS` |
| 10 | `check best fit` | `[Test 2] Best-Fit: PASS` |
| 合计 | 20 分 | 未拿满则 `grade.sh` 退出码 1 |

注意：`FAIL` 分支打印的字符串是 `[Test 1] Page Init: FAIL (原因)`，**不匹配**，得 0 分。
所以必须让两个测试真正走到 PASS 分支。

---

## 四、需要实现的接口

### 4.1 `kern/mm/pmm.c`

```c
static void page_init(void);   /* 需自行定义；由 pmm_init() 调用 */
```

语义（来自手册“功能模块一 任务一”）：

- 根据设备树（DTB）报告的物理内存范围建立 `struct Page` 描述符数组；
- 正确设置全局变量 `va_pa_offset`、`npage`、`pages`；
- 排除 OpenSBI、内核镜像、描述符数组自身占用后，把**完整的空闲页面**交给当前 `pmm_manager`；
- 输出探测到的物理内存范围；
- 设备树信息无效时**终止初始化**；没有可用完整页面时**不得调用 `init_memmap()`**；
- 无参数、无返回值。

**额外硬性要求：** 还必须把 `init_pmm_manager()` 中的

```c
pmm_manager = &default_pmm_manager;
```

切换为

```c
pmm_manager = &best_fit_pmm_manager;
```

否则 `test_best_fit()` 必然 FAIL（它会断言 `pmm_manager == &best_fit_pmm_manager`）。

### 4.2 `kern/mm/best_fit_pmm.c`

需自行设计并实现完整的 Best-Fit 管理器。必须满足：

- 定义 `const struct pmm_manager best_fit_pmm_manager;`（`best_fit_pmm.h` 里已 `extern` 声明，**名字和类型必须完全一致**，链接才通过）。
- 各成员的职责见 `pmm.h` 的 `struct pmm_manager`：`name / init / init_memmap / alloc_pages / free_pages / nr_free_pages / check`。
- `.check` 必须指向本文件的 `best_fit_check`（它内部会调用 `basic_check()`）。
- **不得修改** `basic_check()` 与 `best_fit_check()` 的任何一行（手册明文要求 + 评分依赖它们）。
- 两个测试函数直接使用了标识符 `free_list`、`nr_free`（`free_area_t` 的两部分）与 `PageProperty`、`le2page` 等宏，
  所以 Best-Fit 自己的空闲区状态必须命名为 `free_list` / `nr_free` 才能编译通过。
  （`default_pmm.c` 里这两个是 `static`，不会与 Best-Fit 冲突。）

### 4.3 Buddy System（手册“功能模块二 任务三”，非 Challenge）

- 实验**不提供**任何代码框架和测试函数，需自行从零生成模块 + 测试并接入内核。
- 建议新增 `kern/mm/buddy_pmm.c` + `kern/mm/buddy_pmm.h`。
- **无需修改 Makefile**：`KSRCDIR += kern/mm`，而 `listf` 用 `$(wildcard kern/mm/*)`，
  放在 `kern/mm/` 下的新 `.c` 会被自动收集进内核。
- ⚠️ **最终提交时活动管理器必须是 `best_fit_pmm_manager`**（`test_best_fit()` 的要求）。
  Buddy 只能“临时切换 → 验证 → 切回”，不要把 Buddy 留在 `init_pmm_manager()` 里。

---

## 五、`test_page_init()` 的 8 项断言（Test 1 通过条件）

`test_lab2.c` 中的检查顺序与失败原因（**其中任何一条不满足即 FAIL**）：

| # | 检查 | 失败输出 |
|---|---|---|
| 1 | `va_pa_offset == PHYSICAL_MEMORY_OFFSET`（`0xFFFFFFFF40000000`） | `FAIL (va_pa_offset)` |
| 2 | `get_memory_base() != 0 && get_memory_size() != 0` | `FAIL (dtb mem 0)` |
| 3 | `npage == PPN(memory_base + memory_size)` 且 `npage > nbase` | `FAIL (npage=.. expect=..)` |
| 4 | `pages != NULL`、`(uintptr_t)pages % PGSIZE == 0`、`pages >= ROUNDUP((uintptr_t)end, PGSIZE)` | `FAIL (pages NULL / pages not page-aligned / pages before kernel end)` |
| 5 | `0 < nr_free_pages() <= npage - nbase` | `FAIL (nr_free=..)` |
| 6 | `PPN(PADDR(end)) < npage` 且 `PageReserved(pa2page(PADDR(end)))` | `FAIL (end pa out of range / kernel tail not reserved)` |
| 7 | `PPN(PADDR(pages)) < npage` 且 `PageReserved(pa2page(PADDR(pages)))` | `FAIL (pages pa out of range / pages array not reserved)` |
| 8 | 对 `q = pa2page(PADDR(end))` 有 `pa2page(page2pa(q)) == q` | `FAIL (roundtrip)` |

**要点推导**：检查 6、7 说明描述符数组中**位于 `end` 之后、空闲区之前**的所有页
（含内核镜像尾页与整个描述符数组）都必须是 `PageReserved`。
`init_memmap()` 的实现里第一句就是 `assert(PageReserved(p))`，
所以 `page_init()` 必须**先把所有 `[nbase, npage)` 的描述符置为 reserved**，再调用 `init_memmap()`。

**期望数值（默认 QEMU 128MiB RAM）**：

| 符号 | 值 | 说明 |
|---|---|---|
| `DRAM_BASE` | `0x80000000` | `riscv.h` |
| `PHYSICAL_MEMORY_OFFSET` | `0xFFFFFFFF40000000` | `memlayout.h` |
| `KERNBASE` | `0xFFFFFFFFC0200000` | 内核起始虚拟地址 |
| `nbase` | `0x80000`（524288） | `DRAM_BASE / PGSIZE` |
| `memory_base` / `memory_size` | `0x80000000` / `0x08000000` | DTB 报告 |
| `npage` | `0x88000`（557056） | `PPN(0x88000000)` |
| `npage - nbase` | `0x8000` = 32768 页 | 128 MiB |
| `sizeof(struct Page)` | **40 字节** | 已实测 |
| 描述符数组大小 | `32768 × 40 = 1310720` B = **1280 KiB** | |

---

## 六、`best_fit_check()` / `basic_check()` 的状态约定（Test 2 通过条件）

Test 2 只断言 `pmm_manager == &best_fit_pmm_manager`；
但 `check_alloc_page()` 会调用 `pmm_manager->check()`，即 `best_fit_check()`，
它内部含 `assert()`。因此 Best-Fit 还必须**完整通过**下面这套“空闲链表状态约定”：

1. 遍历 `free_list`：每个块头 `PageProperty(p)` 为真；`Σ p->property == nr_free_pages()`。
2. `basic_check()`：连续 `alloc_page()` 得到的 `p0,p1,p2` 引用计数为 0；
   `list_init` 后 `alloc_page()` 必须返回 `NULL`；`nr_free` 置 0 后分配失败；
   释放 3 页后 `nr_free == 3`；释放 `p0` 后再分配必须**恰好返回 `p0`**（块头复用）；
   最后 `nr_free == 0`。
3. `alloc_pages(5)`：返回非空且 `!PageProperty(p0)`（块头不再是空闲块头）。
4. 关键序列：
   ```c
   free_pages(p0 + 1, 2);        /* 块 [p0+1, len 2] */
   free_pages(p0 + 4, 1);        /* 块 [p0+4, len 1] */
   assert(alloc_pages(4) == NULL);
   assert(PageProperty(p0 + 1) && p0[1].property == 2);
   assert((p1 = alloc_pages(1)) != NULL);
   assert(alloc_pages(2) != NULL);
   assert(p0 + 4 == p1);         /* ★ Best-Fit 必须选“最小的够用块”= len1 的 p0+4 */
   ```
   ★ 这一条把 Best-Fit 与 First-Fit 区分开：First-Fit 会按地址顺序拿到 `p0+1`（len 2），
   断言 `p0+4 == p1` 就会失败。**Best-Fit 的选择准则必须是“满足 `property >= n` 的最小 `property`”。**
5. `free_pages(p0, 5)` 后 `alloc_pages(5)` 成功（相邻块合并正确），随后 `alloc_page() == NULL`。
6. 所有块释放后，`free_list` 中块的 `property` 总和归零（`count == 0 && total == 0`）。

---

## 七、可用的 API 白名单（框架已提供）

| 名称 | 位置 | 用途 |
|---|---|---|
| `get_memory_base()` / `get_memory_size()` | `kern/driver/dtb.h` | DTB 探测到的物理内存起点/大小 |
| `pages`、`npage`、`nbase`、`va_pa_offset` | `pmm.c` / `pmm.h` | 需被 `page_init` 设置或使用 |
| `PADDR(kva)` | `pmm.h` | 虚拟地址 → 物理地址（用 `va_pa_offset`；要求 `kva >= KERNBASE`） |
| `KADDR(pa)` | `pmm.h` | ⚠️ **已被整段注释掉**，当前不可用；如需请用 `pa + va_pa_offset` 自行换算 |
| `page2pa`、`page2ppn`、`pa2page`、`page_ref*`、`set_page_ref` | `pmm.h` | Page 描述符 ↔ 地址/页号 |
| `ROUNDUP` / `ROUNDDOWN` | `defs.h` | 对齐 |
| `PPN(la)` / `PGSIZE` / `PGSHIFT` | `mmu.h` | 页号与页大小 |
| `SetPageReserved` / `ClearPageReserved` / `PageReserved` | `memlayout.h` | 保留位 |
| `SetPageProperty` / `ClearPageProperty` / `PageProperty` | `memlayout.h` | 空闲块头位 |
| `list_init/add/add_before/add_after/del/del_init/empty/next/prev` | `list.h` | 双向链表 |
| `le2page(le, page_link)` | `memlayout.h` | 链表节点 → `struct Page*`（container_of） |
| `list_entry_t`、`free_area_t`、`struct Page` | `memlayout.h` / `list.h` | 数据结构 |
| `cprintf`、`panic`、`assert` | `stdio.h` / `panic.c` / `assert.h` | 输出与断言 |
| `init_memmap(base, n)` | `pmm.c`（`static`） | 把空闲区注册给当前 `pmm_manager` |

`struct Page`：

```c
struct Page {
    int ref;                 // 引用计数
    uint64_t flags;          // PG_reserved(bit0) / PG_property(bit1)
    unsigned int property;   // 空闲块页数（仅块头有效）
    list_entry_t page_link;  // 空闲链表节点
};
```

---

## 八、禁止修改清单

| 文件 / 对象 | 约束 |
|---|---|
| `kern/mm/default_pmm.c` 的 `basic_check` / `default_check` | 不得修改 |
| `kern/mm/best_fit_pmm.c` 的 `basic_check` / `best_fit_check` | **不得修改**（手册明文 + Test 2 依赖） |
| `kern/mm/default_pmm.c`（整体） | 作为 First-Fit 参考，保持原样 |
| `pmm.h` 中 `pmm_manager` 结构体定义与各成员函数指针签名 | 不得修改 |
| `Makefile` | 不得出现 `kern/test`；原则上无需任何修改 |
| `kern/mm/pmm.c` 的 `#include <pmm.h>` | 必须保持恰好 1 处 |
| `page_init` / `pmm_init` 命名 | 不得引入第二个可被 `\bpmm_init` 匹配的函数 |
| 两处 `//lab2 ... YOUR CODE` 注释 | **提交时必须保留注释**，并把 `YOUR CODE` 替换为**本人学号** |

---

## 九、期望串口输出（供对照，非自动判分）

```
DTB Init
HartID: 0
DTB Address: 0x...
Physical Memory from DTB:
  Base: 0x0000000080000000
  Size: 0x0000000008000000 (128 MB)
  End:  0x0000000087ffffff
DTB init completed
(THU.CST) os is loading ...
Special kernel symbols:
  entry  0xffffffffc02000c0 (virtual)
  etext  ...
  edata  ...
  end    0xffffffffc0205058 (virtual)
Kernel executable memory footprint: 20KB
memory management: best_fit_pmm_manager
physical memory map:
  memory: 0x0000000008000000, [0x0000000080000000, 0x0000000087ffffff].
check_alloc_page() succeeded!
satp virtual address: 0xffffffffc0204000
satp physical address: 0x0000000080204000
[Test 1] Page Init: PASS
[Test 2] Best-Fit: PASS
```

说明：
- `memory management: best_fit_pmm_manager` 来自 `init_pmm_manager()` 的 `cprintf`，
  是“已切换到 Best-Fit”的最直接证据。
- `physical memory map:` 那两行**当前源码中不存在**，由 `page_init()` 自己 `cprintf` 输出
  （格式如上，供报告截图与人工核对用，评测脚本不检查）。
- 最后两行 `[Test 1]/[Test 2]` 由 `inject.py` 注入的调用打印，**本地 `make qemu` 看不到**，
  只有 `make grade` 才会出现。

---

## 十、给 AI 的输出要求（建议写进 prompt）

1. 只输出**可直接编译进 `-Werror -std=gnu99 -nostdinc -mcmodel=medany`** 的 C 代码；
   不使用标准库头文件（可用的是 `libs/` 与 `kern/` 下的头文件）。
2. 明确说明每个函数的**前置条件/后置条件**，特别是：
   - `page_init()` 中 `va_pa_offset` 必须在任何 `PADDR` 调用之前赋值；
   - 调用 `init_memmap()` 之前，`[nbase, npage)` 的所有描述符必须已 `SetPageReserved`；
   - `init_memmap()` 的 `n` 必须 > 0。
3. 不得修改任何测试函数、不得改 `Makefile`、不得新增第二个 `*pmm_init` 函数。
4. 不要使用 `KADDR`（已被注释掉）。
5. Best-Fit 的分配准则必须是“**满足 `property >= n` 的最小 `property`**”，
   并在返回 `NULL`/成功时保持 `nr_free` 与链表一致。
6. 输出的代码请自行做一遍“对照第五节 8 项断言与第六节状态约定”的自检。
