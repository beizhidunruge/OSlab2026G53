# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | lab4: 进程管理 |
| **小组成员** | 2412591-贺翰林、2412604-刘泽民、2414034-丁欣怡 |
| **完成日期** | YYYY-MM-DD |

### 小组分工

练习如何分工？

| 成员 | 负责的练习/模块 |
|------|----------------|
| 2412604-刘泽民 | 功能模块一：虚拟内存页操作功能，Challenge 2 |
| 2414034-丁欣怡 | 功能模块二：进程创建及初始化功能 |
| 2412591-贺翰林 | 功能模块三：进程调度，Challenge 1 |

实验报告如何分工？

各自完成所负责的模块部分的报告，贺翰林负责最终整合报告。

---

## 一、实验目的

本实验的主要目的是：

1. 了解虚拟内存管理的基本结构，掌握虚拟内存的组织与管理方式
2. 了解内核线程创建/执行的管理过程
3. 了解内核线程的切换和基本调度过程

---

## 二、实验环境

你们使用的 AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|------|------------|---------|------|
| 2412604-刘泽民 | [例如：Cline（VS Code插件） / Claude Code（终端 Agent） / DeepSeek Harness等] | [例如：Claude Sonnet 4.5 / GPT-5.5 / DeepSeek-V4-Flash等] | [可选：特殊配置等] |
| 2414034-丁欣怡 | | | |
| 2412591-贺翰林 | DeepSeek Harness | DeepSeek-V4.1-Flash | |

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

本章围绕“进程管理”展开，核心问题是：内核已经具备物理内存管理和基础页表能力之后，如何让多个执行实体（内核线程）轮流使用 CPU。

整个实验沿着“地址空间 → 执行实体 → 调度切换”三层逐步推进：

1. **地址空间层**：引入虚拟内存描述结构，管理线程/进程的虚拟地址空间，并补齐页表映射的建立与撤销（`page_insert`/`page_remove`），让“地址”真正可用；
2. **执行实体层**：设计进程控制块 `proc_struct`，把当前执行现场包装成第 0 个内核线程 `idleproc`，再通过 `kernel_thread/do_fork/copy_thread` 创建第 1 个内核线程 `initproc`，为每个线程准备独立的内核栈、trapframe 和首次运行上下文；
3. **调度切换层**：用 `schedule` 从就绪队列中选择下一个线程，用 `proc_run` 更新 `current` 和页表，用 `switch_to` 完成寄存器现场的保存与恢复；`cpu_idle` 作为调度点，让 `idleproc` 主动把 CPU 交给 `initproc`。

整条启动链可以概括为：

```text
kern_init → pmm_init → vmm_init → proc_init → cpu_idle
          → schedule → proc_run → switch_to → initproc 首次运行
```

其中前半段（`pmm_init/vmm_init`）负责把地址空间准备好，中间（`proc_init`）负责把线程对象准备好，后半段（`cpu_idle/schedule/proc_run/switch_to`）负责让第一个可运行线程真正跑起来。三者缺一不可：没有页表就无法切换地址空间，没有 PCB 就没有调度对象，没有调度器则线程只能停留在“已创建”状态。

### 3.2 功能的逐步实现

本章按以下顺序实现，顺序由依赖关系决定：

1. **先实现功能模块一：虚拟内存页操作**（`page_insert/page_remove`）。
   原因：`pmm_init` 内部的 `check_pgdir()`/`check_boot_pgdir()` 会立即调用这两个函数，页表测试过不了，内核连初始化都无法完成；同时它们也是后续线程地址空间和 `vmm` 的基础。关键是把“页表项内容、物理页引用计数、TLB 状态”三者保持同步。

2. **接着实现功能模块二：进程创建及初始化**（`alloc_proc/proc_init/kernel_thread/do_fork/copy_thread/copy_mm`）。
   原因：调度的对象是 PCB。`proc_init` 把当前执行现场包装成 `idleproc`，再创建 `initproc`；`kernel_thread/copy_thread` 负责把函数入口、参数、内核栈和 trapframe 组织成“第一次被切换时能正确运行”的现场。没有这一层，`schedule` 没有东西可选。

3. **最后实现功能模块三：进程调度**（`cpu_idle/schedule/proc_run/switch_to`）。
   原因：它依赖前两层提供的页表、PCB、内核栈和 `context`。`cpu_idle` 是 `kern_init` 的最后一个调用，只有前面全部就绪，`idleproc` 才能通过 `schedule → proc_run → switch_to` 把 CPU 交给 `initproc`。`switch_to` 只做寄存器保存/恢复，`proc_run` 负责 `current` 与 `satp`，`schedule` 负责选择策略，三者分工明确。

4. **最后完成 Challenge 的思考题**（开关中断与分页模式）。
   原因：这些内容是对上述实现的原理性总结；例如 `local_intr_save/restore` 正是 `schedule/proc_run` 临界区的实现基础，放在代码完成之后分析更具体。

---

## 四、实验内容与实现

### 功能模块一：虚拟内存页操作

**负责人：** 2412604-刘泽民

### 功能模块：模块二——进程创建及初始化

**负责人：** 2414034-丁欣怡

#### 模块功能描述

**需要实现/修改的函数：**

```c
static struct proc_struct *alloc_proc(void);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
void proc_init(void);
```

**功能说明：**

本模块负责把函数入口和参数转换为具有 PCB、独立内核栈、首次执行现场和 PID 的可运行内核线程。它承接模块一提供的内存管理能力，为模块三的调度与上下文切换提供线程对象；当前阶段共享内核地址空间，不复制用户地址空间。

1. `alloc_proc` 使用 `kmalloc` 分配并清零 PCB，设置未初始化状态、未分配的 PID 和内核页表物理地址，防止后续流程使用未初始化字段。
2. `copy_mm` 检查当前线程的 `mm == NULL`，保持子线程 `mm == NULL`，保留后续地址空间管理的接口。
3. `copy_thread` 将临时 Trap frame 复制到子线程自己的内核栈顶部，设置子线程返回值和栈指针，并将 PCB 内嵌的 `context.ra/sp` 设置为首次恢复入口和现场地址。
4. `kernel_thread` 在临时现场中设置 `s0=fn`、`s1=arg`、`epc=kernel_thread_entry`，配置 SPP、SPIE 和 SIE，通过 `do_fork` 创建线程。
5. `do_fork` 依次分配 PCB 和内核栈、准备地址空间与现场，再在保存原中断状态的临界区中分配唯一 PID、登记、更新计数并唤醒。失败时逆序释放已取得的资源，避免留下半成品线程。
6. `proc_init` 初始化全局管理结构，将当前执行流包装成复用 `bootstack` 的 PID 0 线程 idleproc，再创建 PID 1 的 initproc。idleproc 设置重调度请求，使后续调度能够选中 initproc。

与模块三的关键连接是首次运行链：`switch_to → forkret → forkrets → __trapret → sret → kernel_thread_entry → fn(arg)`。本模块负责预先构造现场，模块三与前置 Lab3 恢复代码负责将这些字段恢复到处理器中。

规格详见 [模块二 spec](spec-module2.md)。spec 围绕资源所有权、状态转换和首次恢复约定编写，避免只列函数名称。prompt 提供实际 RV64 字段和已有接口，明确页表使用物理地址、临时现场必须复制、失败逆序释放资源，并把 A/C 的修改区域排除在外。

#### 最终提示词

以下为当前实现采用的提示词版本，完整运行验收仍待 A/C 模块整合；同步记录于 prompt.md。

````markdown
[PROMPT]

**任务**：实现 lab4/code/kern/process/proc.c 中 function2 的六个函数，创建共享内核地址空间的 idle/init 内核线程。

**操作要求**：修改实际文件，仅补全 function2，标记改为 2414034；保留 function3 和既有函数、评分代码。使用独立分支。同步记录规格和真实验证结果。

**输出要求**：按工程真实 RV64 接口实现，禁止套用 x86/TSS/CR3 或旧字段；不扩展用户地址空间复制、退出回收、抢占调度。不把未运行写成测试通过。

[RELY]

proc.h 定义 PCB，含 state/pid/runs/kstack/need_resched/parent/mm/context/tf/pgdir/flags/name/list_link/hash_link。context 是 PCB 内嵌的 ra、sp、s0-s11，tf 指向内核栈上的 trapframe。页表物理地址使用 boot_pgdir_pa。依赖 kmalloc/kfree、setup_kstack/put_kstack、get_pid/hash_proc/find_proc、wakeup_proc、list_init/list_add、local_intr_save/restore、bootstack、KSTACKSIZE、CLONE_VM、MAX_PROCESS 和已有错误码。

forkret 调用 forkrets(current->tf)，后者进入 __trapret；kernel_thread_entry 从 s1 取参数到 a0，通过 s0 调用函数。set_proc_name 固定复制 PROC_NAME_LEN 字节，传入补零的固定长度名称数组。

[GUARANTEE]

```c
static struct proc_struct *alloc_proc(void);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
void proc_init(void);
```

不得更改公开接口；内部辅助函数应保持 static。

[SPECIFICATION]

1. alloc_proc：内存失败返回 NULL；成功清零 PCB，设置 UNINIT、pid=-1、pgdir=boot_pgdir_pa 并初始化链表节点，不登记、不唤醒。
2. copy_mm：前提 current 有效且 mm=NULL，断言该约束，子线程 mm=NULL，返回零；不取得用户地址空间资源。
3. copy_thread：前提子线程已分配内核栈且 tf 有效；在 kstack+KSTACKSIZE-sizeof(trapframe) 放置现场副本，禁止保存局部模板的指针；a0=0；恢复 sp 使用非零 esp，否则为 tf 地址；context.ra=forkret，context.sp=tf。
4. do_fork：前提 current/tf 有效；检查进程上限，依次分配 PCB、设置 parent、分配栈、copy_mm、copy_thread；登记临界区保存并恢复原中断状态，复查上限，分配唯一 PID，挂入哈希与链表，增加计数，最后唤醒。成功返回 PID；上限返回 -E_NO_FREE_PROC，内存不足返回 -E_NO_MEM；失败逆序释放已占用资源，不留下全局登记。
5. kernel_thread：清零临时 tf，s0=fn、s1=arg、epc=kernel_thread_entry；status 基于当前 sstatus 设置 SPP/SPIE，清除 SIE；调用 do_fork(clone_flags|CLONE_VM,0,&tf)，传播返回值。
6. proc_init：启动时只调用一次；初始化全局链表和哈希表。idle 使用 bootstack、PID0、RUNNABLE、need_resched=1，parent/mm/tf=NULL，runs/flags/context 初值为零；设 current=idle 并计数，但 idle 不加入普通链表和哈希。通过 kernel_thread(init_main,"Hello world!!",0) 创建 PID1，find_proc 后命名 init；创建失败 panic。

验收应分清模块编译和整机运行；整合后使用原测试验证 Process Create，并与 C 检查完整首次恢复链，不修改测试判据。
````

#### 实现迭代过程

本模块目前完成了 1 次实现迭代。以下记录对应 2026-11-16 的实际实现与验证；完整运行验收尚未完成。

---

##### 第一次迭代

**遇到的问题：**

- 全工程 `make` 到达链接阶段后，出现 `page_insert`、`page_remove`、`cpu_idle` 等未定义引用。原因是 A/C 对应接口尚未补齐，不能据此将整机运行问题归因于模块二，也不能认定模块二已经通过运行测试。
- 构建出现文件修改时间在未来的提示和 `Clock skew detected` 警告，需要强制重编译确认模块对象确实由本次源码生成。
- 阅读既有 `set_proc_name` 时发现其固定复制 `PROC_NAME_LEN` 字节，直接传入较短的字符串字面量存在越界读取风险。这是实现前的接口检查发现，不是实际运行中已经发生的故障。

**问题解决策略：**

- 在提示词中明确函数归属和验收边界，保留原有测试，待 A/C 交付后整合，不以空实现绕过缺失接口。
- 执行 `make -B obj/kern/process/proc.o`，强制重新编译模块二。命令实际输出编译动作并以状态码 0 结束；时间偏差警告仍存在，未将其描述为已消除。
- 在 `[RELY]` 中明确名称接口的固定读取长度，并在实现中使用补零的固定大小名称数组，不修改既有辅助函数。

**最终结果：**

经过 1 次实现迭代，本模块当前结果如下：

- 已完成六个函数、登记临界区、资源失败回滚及学号标记。
- 模块对象 `proc.o` 编译通过，强制重编译通过；`git diff --check` 通过。
- 全工程链接尚未通过，等待 A/C 接口整合。
- 尚未执行成功的 QEMU 运行或自动评分，未取得 Process Create 10/10 或整机 30/30 的结果。
- 分配失败路径已进行代码审查，尚未进行故障注入验证；首次恢复链仍需与 C 联调，运行行为是否符合预期以整合后的实测为准。

**关键改进点总结（如有）：**

1. 在规格中明确 `pgdir` 是物理地址、`context` 内嵌于 PCB、Trap frame 位于子线程栈，避免混淆对象位置和地址含义。
2. 将“完成资源与现场准备后才能登记、唤醒”和“失败逆序回收”写成明确约束，并在登记临界区复查进程数上限。
3. 将首次恢复需要的入口、函数、参数与状态位统一约定，便于 B/C 按接口整合。
4. 在报告中区分模块编译、整机链接和运行评分，保留未完成项，不编造后续迭代或测试成功记录。

---

### 功能模块三：进程调度

**负责人：** 2412591-贺翰林

#### 模块功能描述

**需要实现/修改的函数：**

```c
// kern/process/switch.S
void switch_to(struct context *from, struct context *to);

// kern/process/proc.c
void proc_run(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

// kern/schedule/sched.c
void schedule(void);
```

**功能说明：**

功能模块三负责把前两个模块准备好的“线程对象”真正交给 CPU 执行，是 Lab4 从“能创建线程”走到“能并发运行线程”的最后一步。它包含四个层次：

1. `switch_to`：最底层的上下文切换，只负责把当前线程的 `ra/sp/s0-s11` 保存到 `from->context`，再从 `to->context` 恢复同一组寄存器并 `ret`。它不参与调度决策，也不修改 `current`。
2. `proc_run`：一次具体切换的执行者。它更新 `current`，把 `satp` 指向目标线程的页表根物理地址并刷新 TLB，然后调用 `switch_to`；整个过程在关中断临界区内完成。
3. `schedule`：调度决策者。它清掉 `current->need_resched`，在 `proc_list` 中按轮转规则找到一个 `PROC_RUNNABLE` 的线程；找不到时回退 `idleproc`；最后通过 `proc_run` 完成切换。
4. `cpu_idle`：空闲线程的执行主体。启动阶段 `proc_init` 把 `idleproc->need_resched` 置 1，`cpu_idle` 检测到后调用 `schedule`，把 CPU 第一次交给 `initproc`；之后它持续作为“没有就绪线程时的保底执行流”。

模块的主要场景包括：

- **启动时的第一次切换**：`idleproc` → `initproc`。这是本实验最关键的场景：`initproc` 的 `context.ra` 由模块二设为 `forkret`，`context.sp` 指向它自己的 trapframe；`switch_to` 恢复后经 `forkret → forkrets → __trapret → sret → kernel_thread_entry` 首次运行 `init_main`。
- **运行中的轮转**：测试线程和 `initproc` 反复把 `need_resched` 置 1 并调用 `schedule`，验证轮转选择能回到每个可运行线程。
- **无就绪线程**：一圈扫描不到 `PROC_RUNNABLE` 时回到 `idleproc`，保证 CPU 不会因为找不到线程而崩溃。

与其他模块的交互：

- 依赖模块一维护的页表：`satp` 写入的是页表根**物理地址**，内核线程共享 `boot_pgdir_pa`；
- 依赖模块二准备好的 PCB、内核栈、`context`、`tf` 和 `PROC_RUNNABLE` 状态；
- 依赖 `sync.h` 的 `local_intr_save/local_intr_restore` 保证“改 `current` + 换页表 + 切换现场”的原子性；
- 依赖 `trapentry.S` 的 `forkrets/__trapret` 完成新线程的首次运行。

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown
[PROMPT]
**任务**：修改 kern/process/switch.S、kern/process/proc.c、kern/schedule/sched.c，
实现 Lab4 功能模块三“进程调度”：switch_to、proc_run、schedule、cpu_idle。
switch.S 只放 switch_to 汇编；proc.c 在 `//LAB4 function3 YOUR CODE` 处补
proc_run 与 cpu_idle；sched.c 在同名标记处补 schedule。

**操作要求**：必须在项目中修改真实文件，不要只输出代码片段；保留现有代码、
注释和 `//LAB4 function3` 标记，只在标记处补全；不要改动功能模块一/二的函数
（page_insert/page_remove/alloc_proc/proc_init/kernel_thread/do_fork/
copy_thread/copy_mm），也不要改 Makefile、trap.c、pmm.c。
禁止使用 libs/riscv.h 中的 lsatp()（它是 Sv32 遗留，MODE=1，在 Sv39 下会出错）。

**输出要求**：直接改文件；以 [RELY] 中的工程真实定义为准；不要引入无关功能、
日志或测试钩子。

[RELY]
// struct context（kern/process/proc.h）
struct context {
    uintptr_t ra, sp, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
};  // 偏移依次为 0,8,16,...,104

// struct proc_struct 关键字段：state/pid/runs/kstack/need_resched/
// context/tf/pgdir(页表根物理地址)
// 全局：proc_list、idleproc、initproc、current；状态 PROC_RUNNABLE
// list.h：list_entry_t、list_next、le2proc
// sync.h：local_intr_save(x)/local_intr_restore(x)，x 为 bool，
//         保存调用前的 sstatus.SIE，必要时关中断
// riscv.h：read_csr/write_csr、SATP_MODE_SV39=8、RISCV_PGSHIFT=12
// pmm.c enable_paging() 的正确写法：
//   write_csr(satp, 0x8000000000000000 | (boot_pgdir_pa >> RISCV_PGSHIFT));
// pmm.h：boot_pgdir_pa、flush_tlb()（sfence.vma）
// 首次运行链：copy_thread 置 context.ra=forkret、context.sp=tf；
//   forkret → forkrets(a0=tf) → __trapret → sret → kernel_thread_entry

[GUARANTEE]
void switch_to(struct context *from, struct context *to);  // switch.S
void proc_run(struct proc_struct *proc);                   // proc.c
void schedule(void);                                       // sched.c
void cpu_idle(void);                                       // proc.c, noreturn

[SPECIFICATION]
## switch_to
**Pre-Condition**：from/to 指向有效 context；a0=from，a1=to；调用者已关中断。
**Post-Condition**：from 的 ra/sp/s0-s11 被写入；从 to 恢复同组寄存器；
ret 后目标现场继续执行。
**Case 1**：正常切换，14 个字段一一对应，不保存 a/t 寄存器。
**Requirements**：先集中 sd 到 from，再集中 ld 从 to，最后 ret。

## proc_run
**Pre-Condition**：proc 非空；proc->pgdir 是 Sv39 根页表物理地址
（内核线程为 boot_pgdir_pa）。
**Post-Condition**：current==proc；satp=SV39|PPN(pgdir)；TLB 已刷新；
switch_to 完成切换；本函数回到原线程时按保存值恢复中断。
**Case 1**：proc==current，直接返回，不做任何切换。
**Case 2**：关中断→记录 prev→current=proc→写 satp→flush_tlb→
switch_to(&prev->context,&proc->context)→恢复中断。
**Requirements**：整个过程在关中断临界区；satp 常量必须用 64 位。

## schedule
**Pre-Condition**：current 非空；可在任意内核上下文调用，中断状态任意。
**Post-Condition**：current->need_resched=0；选出 next；next->runs++；
next!=current 时 proc_run(next)。
**Case 1**：current==idleproc，从 proc_list 头开始环形查找。
**Case 2**：current!=idleproc，从 current->list_link 之后环形查找。
**Case 3**：一圈都没有 PROC_RUNNABLE，next=idleproc 保底。
**Requirements**：选择与切换在关中断临界区；只选 PROC_RUNNABLE；
循环必须能终止；idleproc 只作保底，若它被链入 proc_list 则跳过它作普通候选。

## cpu_idle
**Pre-Condition**：current==idleproc，且 idleproc->need_resched=1。
**Post-Condition**：永不返回；need_resched 为真时调用 schedule，否则空转。
**Requirements**：函数不得返回；不要每次循环无条件重设 need_resched。
````

#### 实现迭代过程

本模块的实现经历了 3 次迭代，过程如下：

##### 第一次迭代

**目标：** 用四段式提示词让 AI 在三个文件的标记处补全四个函数。

**遇到的问题：**

- 问题 1：`satp` 的写法没有在初版提示词中固定。RISC-V 的 `satp` 在 Sv39 下 MODE 字段是 4 位，若照抄 `libs/riscv.h` 的 `lsatp()`（它写的是 Sv32 的 `SATP32_MODE`，MODE=1），在 RV64 上会写入保留模式；若写成 `SATP_MODE_SV39 << 60`，`SATP_MODE_SV39` 是 `int`，左移 60 位属于未定义行为，`-O2` 下可能得到 0。两种情况都会让第一次切换后取指/访存 page fault。
- 问题 2：`switch.S` 只描述“保存/恢复上下文”，没有给字段偏移。`struct context` 的顺序是 `ra, sp, s0…s11`，不是按寄存器编号排列；若按编号顺序保存，`sp/ra` 会错位，切换后直接崩溃。

**问题解决策略：**

- 在 `[RELY]` 中直接给出 `struct context` 的字段顺序与偏移（0,8,…,104），并给出 `pmm.c::enable_paging()` 的正确写法作为唯一参照，明确写出“禁止使用 `lsatp()`”；
- 在 `[SPECIFICATION]` 的 `Requirements` 中规定 `switch_to` 必须“先集中 `sd` 到 from，再集中 `ld` 从 to，最后 `ret`”，`proc_run` 的 satp 常量必须用 64 位；
- 生成后用 `riscv64-unknown-elf-objdump -d obj/kern/process/switch.o` 核对，确认 14 条 `sd` + 14 条 `ld` 的偏移正好是 0…104。

**最终结果：** `switch.S`、`proc_run` 编译通过，反汇编偏移与 `struct context` 完全对应。

---

##### 第二次迭代

**遇到的问题：**

- 问题 1：`schedule` 的经典写法会把 `idleproc` 当成普通 `PROC_RUNNABLE` 候选。若模块二的 `proc_init` 把 `idleproc` 链入 `proc_list`，轮转一圈后会选中它；而 `idleproc` 的 `need_resched` 早在第一次让出 CPU 时就被清 0，它回到 `cpu_idle` 后会一直空转、不再让出 CPU。`test_lab4_schedule` 由 `initproc` 连续调用 8 次 `schedule`，一旦 CPU 被 `idleproc` 占住，测试就永远走不到 `assert`，Schedule 项超时。
- 问题 2：`proc_run` 默认 `proc->pgdir` 一定是有效物理地址；如果模块二的 `alloc_proc` 漏设（0）或误设成虚拟地址，`satp` 会指向错误页表。

**问题解决策略：**

- 在 `schedule` 的扫描条件中加入 `next != idleproc`，让 `idleproc` 只作为“一圈都找不到可运行线程”时的兜底；
- 在 `proc_run` 中对 `next->pgdir == 0` 回退 `boot_pgdir_pa`（内核线程共享启动页表），并在注释中说明 `pgdir` 必须是物理地址；
- 为了验证第一种情况，专门在临时测试副本里把 `idleproc` 加入 `proc_list` 做压力测试，`make grade` 仍然 30/30，说明该防御有效。

**最终结果：** 无论模块二是否把 `idleproc` 链入进程表，调度都不会死锁；`pgdir` 为 0 时也不会把 satp 写成非法值。

---

##### 第三次迭代

**遇到的问题：**

- 模块一/二尚未合入真实工程，直接 `make` 会在 `page_insert/page_remove/proc_init` 处链接失败，无法直接跑 `make grade`。

**问题解决策略：**

- 先分别编译本模块的三个目标文件：`make obj/kern/process/switch.o obj/kern/process/proc.o obj/kern/schedule/sched.o`，确认语法、汇编、头文件引用无误；
- 再在一个临时副本中按练习页的接口约定补上模块一/二的参考实现，运行 `make grade`，得到 `check PMM: OK`、`check Process Create: OK`、`check Schedule: OK`、`Total Score: 30/30`；启动输出也出现 `this initproc, pid = 1, name = "init"` 等预期行；
- 最终集成测试等小组把模块一/二合入后，在真实工程中重新执行 `make grade` 并截图（见第五节）。

**最终结果：**

经过 3 次迭代，该模块最终：

- 编译通过（三个目标文件均可单独编译）
- 在模块一/二按接口就绪的测试副本上，三个评分点全部通过（30/30）
- 功能符合预期：`initproc` 能从 `idleproc` 手里接管 CPU，轮转调度可回到每个可运行线程

**关键改进点总结：**

1. 在 `[RELY]` 中固定 `satp` 的 Sv39 写法、`struct context` 布局和字段偏移，消除了“Sv32 模式 / 32 位移位 / 汇编偏移”三类高危错误；
2. 在 `[SPECIFICATION]` 中把“`idleproc` 只作保底”和“`pgdir` 是物理地址”写成明确的系统约束，避免 AI 只按经典算法生成而忽略本实验的测试流程；
3. 验证手段从“编译通过”升级为“反汇编核对 + 压力测试 + `make grade`”，并把跨模块契约（`pgdir`、`tf/context`、`idleproc` 链表）单独列出，降低联调成本。

---

### Challenge 1：local_intr_save/local_intr_restore 如何实现开关中断

**负责人：** 2412591-贺翰林

#### 1. 代码位置

`local_intr_save/local_intr_restore` 定义在 `kern/sync/sync.h`，底层依赖 `kern/driver/intr.c` 的 `intr_enable/intr_disable` 和 `libs/riscv.h` 的 CSR 读写宏：

```c
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}
static inline void __intr_restore(bool flag) {
    if (flag) intr_enable();
}
#define local_intr_save(x) do { x = __intr_save(); } while (0)
#define local_intr_restore(x) __intr_restore(x);
```

```c
void intr_enable(void)  { set_csr(sstatus, SSTATUS_SIE); }
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
```

#### 2. 执行过程

`local_intr_save(flag)` 展开为 `flag = __intr_save()`：

1. `read_csr(sstatus)` 读出当前 hart 的 S 态状态寄存器；
2. 检查 `SSTATUS_SIE`（bit 1）：为 1 说明中断当前是开的；
3. 若开着，调用 `intr_disable()`（`clear_csr` 清 SIE），并返回 1；
4. 若本来就关着，不改变状态，返回 0。

因此 `flag` 记录的不是“是否成功关中断”，而是“**调用前中断是否开着**”。

`local_intr_restore(flag)` 展开为 `__intr_restore(flag)`：只有 `flag == 1` 时才 `intr_enable()`（`set_csr` 置 SIE）；`flag == 0` 时保持关中断。

#### 3. 为什么必须“保存旧状态”，而不是直接关/开

关键是**可嵌套**和**调用上下文无关**。例如：

```c
local_intr_save(f1);        // 原来 SIE=1：f1=1，关中断
    local_intr_save(f2);    // 现在 SIE=0：f2=0，不再关
    /* 内层临界区 */
    local_intr_restore(f2); // f2=0：不开中断
/* 外层临界区 */
local_intr_restore(f1);     // f1=1：才重新开中断
```

如果内层 `restore` 无条件 `intr_enable()`，就会在外层临界区中间把中断打开，破坏外层的原子性。反过来，如果 `save` 不保存旧状态，只是“关中断”，那么在内核已经关中断的上下文（如 trap 处理、早期启动）里调用后，`restore` 会错误地把中断打开。

`save/restore` 的语义是“**把中断状态恢复到调用前的样子**”，因此对调用者是透明的：调用者不需要知道自己进来时中断是开还是关。

#### 4. 与 RISC-V 硬件行为的关系

- 进入 trap 时，硬件自动做 `SPIE ← SIE`、`SIE ← 0`；`sret` 时 `SIE ← SPIE`。因此“保存/恢复 SIE”与 trap 的硬件语义是一致的。
- `read_csr(sstatus)` 和随后的 `clear_csr` 不是一条原子指令，但在单 hart 上仍然是安全的：如果在这两条指令之间来了中断，硬件进入 trap 时已经把 SIE 清 0，随后的 clear 只是再清一次；`flag` 仍正确反映调用前的状态，恢复时也能正确开中断。
- `set_csr/clear_csr` 分别编译为 `csrrs/csrrc`，是单条 CSR 读改写指令；每个 hart 的 `sstatus` 是私有的。

#### 5. 宏写法与局限

- `local_intr_save` 用 `do { ... } while (0)` 包住，保证宏在语法上等价于一条语句，能安全地放在 `if/else` 或无花括号的循环体中，并要求调用者写分号。
- `local_intr_restore(x)` 宏末尾自带分号，正常写 `local_intr_restore(flag);` 会变成两个分号（无害）；但若写成 `if (c) local_intr_restore(x); else ...`，多出的空语句会让 `else` 悬空。当前代码的用法都带花括号，因此没有实际问题。
- 局限：只控制 S 态中断（SIE），不控制 M 态；本实验是单核，没有考虑 SMP 锁和内存屏障；临界区应尽量短。
- 在本实验中，`pmm.c` 的 `alloc_pages/free_pages` 以及模块三的 `schedule/proc_run` 都用这对宏保护临界区，保证“选线程 + 改 `current` + 换 `satp` + `switch_to`”不会被中断打断。

---

## 五、测试与验证

<!-- 根据该实验的具体情况，提供完整的测试运行截图，应包含：
- 编译及运行成功的输出（make qemu），可能有多个
- 测试通过的结果（make grade），只有一个
-->

**测试截图：**

![测试结果截图](./images/test_result.png)

---

## 六、实验总结与收获

### 对操作系统的理解

#### 1. 本实验知识点与 OS 原理的对应

| 本实验知识点 | 对应 OS 原理 | 含义、关系与差异 |
|---|---|---|
| `proc_struct` 进程控制块 | PCB / TCB | 本实验用同一个结构描述内核线程，只保留状态、pid、内核栈、上下文、页表等必要字段；真实 OS 的 PCB 还要管理文件、信号、凭证、地址空间等，进程与线程的描述也常分开。 |
| `context` + `switch_to` | 上下文切换 | 本实验只保存 `ra/sp/s0-s11` 等被调用者保存寄存器，调用者保存寄存器交给编译器处理；OS 的切换还要处理浮点/SIMD、TLS、页表等，开销更大。 |
| `schedule` + `proc_list` | 调度器 / 就绪队列 | 本实验是单核、FIFO/轮转，且只在 `cpu_idle` 或显式调用时调度；OS 通常由时钟中断驱动抢占，还要处理优先级、时间片和多核负载均衡。 |
| `idleproc` + `cpu_idle` | 空闲进程 / idle task | 本实验用一个 `idleproc` 作为无就绪线程时的保底执行流；Linux 则是每个 CPU 一个 idle 任务，并与 cpuidle 节能策略配合。 |
| `local_intr_save/restore` | 临界区 / 关中断 | 本实验通过保存 `sstatus.SIE` 支持嵌套关中断，单核即可；OS 还要配合自旋锁、信号量、抢占计数和内存屏障。 |
| `satp` + Sv39 页表 | 页表基址 / 地址空间切换 | 本实验的内核线程共享 `boot_pgdir_pa`，切换 satp 实际仍指向同一页表；OS 中每个进程有独立页表，切换 satp 才真正切换地址空间。 |
| `trapframe` + `forkret/forkrets` | 陷阱帧 / 首次返回 | 本实验用 `sret` 回到 S 态的 `kernel_thread_entry`；用户进程则需要构造 U 态现场，从系统调用或异常返回用户态。 |

#### 2. OS 原理中重要、但本实验未覆盖的知识点

- **抢占式调度**：本实验没有时钟驱动的 `need_resched`，调度点是显式的，未涉及时间片、优先级和实时调度。
- **SMP 与并发同步**：实验是单核、只靠关中断保护临界区，没有自旋锁、原子操作、内存屏障和 per-CPU 数据。
- **用户态/内核态隔离与系统调用**：本实验全部是共享内核地址空间的内核线程，用户进程和系统调用要到后续实验才涉及。
- **进程退出与资源回收**：`do_exit` 仍是 `panic`，没有僵尸态、父进程等待和资源回收的完整闭环。
- **缺页异常、按需分页与页面置换/swap**：实验只建立了静态映射，没有真正的缺页处理和换入换出。
- **文件系统、IPC、信号**：这些进程资源与管理机制在本实验中尚未出现。

### AI 协作开发的经验

- 提示词要带**真实上下文**：结构体布局、宏定义、已有函数和调用链必须从工程里抄，不能让 AI 凭记忆写；否则很容易生成 x86/旧版 uCore 风格的代码（如 `lsatp()`、32 位的 `SATP_MODE_SV39 << 60`）。
- 用 `[SPECIFICATION]` 把**边界和禁止项**写清楚，例如“`idleproc` 只作保底”“`pgdir` 是物理地址”“只改标记处”，比“实现调度功能”这类笼统描述有效得多。
- 生成结果必须**人工审查并验证**：编译、`objdump` 核对 `switch_to` 偏移、gdb 断点、`make grade`。AI 生成的代码“看起来对”不等于能跑，跨模块契约（`pgdir`、`tf/context`、`idleproc` 链表）尤其要人来判断。
- 把每次失败的**现象、根因、提示词改动**记录下来，既能快速定位问题，也直接成为报告的“实现迭代过程”素材；与组员提前对齐接口，比事后联调省时间。

---
