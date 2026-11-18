# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab X: [实验名称] |
| **小组成员** | 学号1-姓名1、学号2-姓名2、学号3-姓名3 |
| **完成日期** | YYYY-MM-DD |

### 小组分工

练习如何分工？

| 成员 | 负责的练习/模块 |
|------|----------------|
| 学号1-姓名1 | [练习或模块名称] |
| 学号2-姓名2 | [练习或模块名称] |
| 学号3-姓名3 | [练习或模块名称] |

实验报告如何分工？

---

## 一、实验目的

<!-- 说明本次实验的主要目标和预期成果 -->

本实验的主要目的是：

1. [目的1：例如，理解用户态进程如何通过系统调用进入内核]
2. [目的2：例如，掌握进程创建、执行、退出的完整生命周期]
3. [目的3：例如，学习使用 AI 辅助开发操作系统核心功能]

---

## 二、实验环境

你们使用的 AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|------|------------|---------|------|
| 学号1-姓名1 | [例如：Cline（VS Code插件） / Claude Code（终端 Agent） / DeepSeek Harness等] | [例如：Claude Sonnet 4.5 / GPT-5.5 / DeepSeek-V4-Flash等] | [可选：特殊配置等] |
| 学号2-姓名2 | | | |
| 学号3-姓名3 | | | |

**说明：**
- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面
- **底层模型**：指该工具使用的大语言模型及版本

---

## 三、实验整体逻辑分析

### 2.1 本章节的逻辑主线

[请用自己的话描述本章实验的核心主线，例如：
- 本章围绕哪个核心主题展开？（例如：用户进程管理、虚拟内存、文件系统等）
- 整个实验解决什么问题？（例如：如何让用户程序运行在操作系统之上）]

### 2.2 功能的逐步实现

[说明本章实验是按照什么顺序逐步实现各个功能的，以及为什么要按这个顺序，例如：

1. 首先实现**功能A** → 为什么首先实现这个？（例如：建立系统调用接口是后续所有用户态功能的基础）
2. 接着再完善**功能B** → 为什么接着实现这个？（例如：有了系统调用后，需要能够加载和启动第一个用户进程）
3. 然后完成**功能C** → 为什么然后实现这个？（例如：有了第一个进程后，需要实现进程复制功能）
4. 最后补充了**功能D** → 为什么最后实现这个？（例如：最后完善进程退出和清理机制，形成完整闭环）]

---

## 四、实验内容与实现

<!-- 
说明：本部分按照 exercises 文档中的练习顺序组织
每个功能模块或者练习中可能包含多项任务，请根据实际情况调整
-->

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

### 练习：[练习标题]

**负责人：** [学号-姓名]

[按照练习的具体要求进行解答]

---

### Challenge：[Challenge标题]

**负责人：** [学号-姓名]

[按照Challenge的具体要求进行解答]

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

1. 列出你们认为本实验中重要的知识点，以及与对应的 OS 原理中的知识点，并简要说明二者的含义、关系和差异
2. 列出你们认为 OS 原理中很重要、但在本实验中没有对应上的知识点

### AI 协作开发的经验

[写出你们在与 AI 协作过程中实际收获的经验或者感悟]

---

