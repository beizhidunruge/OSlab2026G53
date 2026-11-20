# Lab4 提示词记录

## 功能模块二：进程创建及初始化

**负责人：** 2414034-丁欣怡

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

### 1. alloc_proc

**Pre-Condition**：内核动态内存分配器可用，启动页表物理地址 `boot_pgdir_pa` 已初始化。

**Post-Condition**：返回安全初始化但尚未登记的 PCB，或者在分配失败时返回 `NULL`，不改变进程管理全局状态。

**Case 1**：分配成功时，状态为 `PROC_UNINIT`，PID 为 -1，`pgdir` 为 `boot_pgdir_pa`；运行次数、重调度标记、内核栈地址、标志、context 和名称清零，parent/mm/tf 为空，链表节点初始化。此时线程不可被调度。

**Case 2**：分配失败时返回 `NULL`，不增加进程计数、不加入链表或哈希表。

**Requirements**：不在此函数分配内核栈、分配正式 PID 或唤醒线程。

### 2. copy_mm

**Pre-Condition**：`current` 和目标 PCB 有效，当前创建的是内核线程，`current->mm == NULL`。

**Post-Condition**：目标线程的 `mm` 保持 `NULL`，共享启动内核页表，不取得用户地址空间资源，返回 0。

**Requirements**：使用断言检查当前线程没有用户地址空间的约束；保留 `clone_flags` 参数和函数签名供后续实验扩展，不实现用户地址空间复制。

### 3. copy_thread

**Pre-Condition**：目标 PCB 已拥有大小为 `KSTACKSIZE` 的独立内核栈，输入 `tf` 指向完整可读的 Trap frame；非零 `esp` 是调用者提供的有效恢复栈地址。

**Post-Condition**：目标线程拥有自己的 Trap frame 副本和可供首次调度恢复的 context，输入现场不被修改。

**Case 1**：`esp == 0` 时，将 Trap frame 放在内核栈顶减去其结构体大小的位置，复制输入现场，将子线程返回寄存器 `a0` 设为 0，恢复栈指针设为该 Trap frame 地址。

**Case 2**：`esp != 0` 时，Trap frame 仍在子线程自己的内核栈中，但其恢复栈指针使用传入的 `esp`；其他现场构造规则与 Case 1 相同。

**Requirements**：`context.ra` 指向 `forkret`，`context.sp` 指向子线程的 Trap frame；满足 RV64 栈对齐要求。context 内嵌于 PCB，不能把临时模板地址作为子线程长期使用的现场地址。

### 4. do_fork

**Pre-Condition**：进程管理链表和哈希表已初始化，`current`、输入 `tf` 有效，当前线程满足共享内核地址空间的约束，内存分配接口可用。

**Post-Condition**：成功返回唯一的正 PID，新增一个现场完整的可运行子线程；失败返回负错误码，释放本次已分配资源，不留下全局登记或错误的进程计数。

**Case 1**：资源充足且未达到进程数上限时，为子线程准备 PCB、独立内核栈、地址空间约定和首次执行现场，父线程为 `current`；登记唯一 PID，加入哈希表及进程链表，计数增加一次，最后唤醒为 `PROC_RUNNABLE`，返回子线程 PID。

**Case 2**：创建开始时或登记前复查发现进程数已达 `MAX_PROCESS`，返回 `-E_NO_FREE_PROC`；若已取得资源，释放这些资源，不登记线程。

**Case 3**：PCB 或内核栈分配失败，返回 `-E_NO_MEM`，只释放已经成功取得的资源；不得释放尚未分配的内核栈。

**Case 4**：地址空间准备接口返回错误时，传播该错误码，逆序释放已分配的内核栈和 PCB。当前 `copy_mm` 不取得额外资源；后续扩展时必须同步扩展回滚责任。

**Requirements**：资源和现场准备完成后才能发布线程。PID 分配、链表及哈希登记、计数更新和唤醒在保存原中断状态的临界区完成，退出时恢复原状态；不得无条件开中断。该约束针对当前单 hart 实验，不宣称提供多核互斥。

### 5. kernel_thread

**Pre-Condition**：`fn` 是有效的内核线程入口，参数 `arg` 在函数使用期间有效，当前线程与内存管理环境满足 `do_fork` 的前置条件。

**Post-Condition**：成功时返回新线程 PID，线程首次执行时能够进入 `fn(arg)`；失败时原样返回创建流程的负错误码。

**Case 1**：构造清零的临时 Trap frame，`s0` 保存函数入口，`s1` 保存参数，`epc` 指向 `kernel_thread_entry`；通过共享地址空间的创建路径完成线程创建，使用 `clone_flags | CLONE_VM` 和零恢复栈参数。

**Case 2**：创建流程失败时直接传播错误，不留下指向局部 Trap frame 的持久引用。

**Requirements**：初始 status 基于当前 sstatus，设置 SPP、SPIE 并清除 SIE，使首次 sret 返回 S 模式并按约定恢复中断使能；本函数构造现场，不直接调用线程入口代替调度。

### 6. proc_init

**Pre-Condition**：在内核启动初始化阶段调用一次；物理内存、动态分配器、启动页表和 `bootstack` 可用，尚未对外提供线程调度。

**Post-Condition**：管理链表及哈希表可用，`current == idleproc`，idle 和 init 均已正确初始化；启动创建失败时明确 panic，不继续使用无效对象。

**Case 1**：初始化成功时，idle 的 PID 为 0、状态为 `PROC_RUNNABLE`、栈为 `bootstack`、`need_resched=1`，parent/mm/tf 为空，runs/flags/context 保持零初值。idle 计入进程数但不加入普通链表及哈希表。创建以 `init_main` 为入口、以 `"Hello world!!"` 为参数的 PID 1 线程，令其父线程为 idle，并设置名称 `"init"`。

**Case 2**：idle PCB 分配失败，或 init 创建失败时 panic；成功返回 PID 后应能查到对应的 init PCB，并检查其 PID 为 1。

**Requirements**：idle 复用已有执行流，不额外分配其运行栈；名称接口使用补零的足够长度数组。保持 `forkret → forkrets → __trapret → kernel_thread_entry` 首次执行约定，不在初始化阶段直接执行 `init_main`。

验收应分清模块编译和整机运行；整合后使用原测试验证 Process Create，并与 C 检查完整首次恢复链，不修改测试判据。

---

## 功能模块三：进程调度

**负责人：** 2412591-贺翰林

[PROMPT]

**任务**：修改 `kern/process/switch.S`、`kern/process/proc.c`、`kern/schedule/sched.c`，
实现 Lab4 功能模块三的 `switch_to`、`proc_run`、`schedule`、`cpu_idle`。
保留现有代码和 `//LAB4 function3` 标记，只在对应位置补全，不修改模块一/二、
Makefile、trap.c 或 pmm.c；禁止使用 Sv32 遗留的 `lsatp()`。

[RELY]

`struct context` 的字段顺序为 `ra, sp, s0-s11`，偏移为 0、8、16…104。
`proc_struct` 提供 `state`、`runs`、`need_resched`、`context`、`pgdir` 等字段；
`pgdir` 是页表根物理地址。全局对象包括 `proc_list`、`idleproc`、`current`，
状态为 `PROC_RUNNABLE`。使用 `list_next`/`le2proc` 遍历链表，
`local_intr_save`/`local_intr_restore` 保护临界区，`flush_tlb()` 刷新 TLB。
Sv39 的 satp 写法为 `0x8000000000000000ULL | (pgdir >> RISCV_PGSHIFT)`。

[GUARANTEE]

```c
void switch_to(struct context *from, struct context *to);
void proc_run(struct proc_struct *proc);
void schedule(void);
void cpu_idle(void);
```

[SPECIFICATION]

1. `switch_to` 集中保存 `from` 的 ra/sp/s0-s11，再恢复 `to` 的同组寄存器并
   `ret`，不保存 a/t 寄存器。
2. `proc_run` 对当前进程直接返回；否则在关中断状态下记录旧进程、更新
   `current`、写入 Sv39 satp、刷新 TLB、调用 `switch_to`，最后恢复原中断状态。
3. `schedule` 清除 `current->need_resched`，在 `proc_list` 中从正确的后继位置
   环形查找可运行进程，跳过 `idleproc` 作为普通候选；找不到时以 idle 为保底，
   增加目标进程的 `runs`，必要时调用 `proc_run`。
4. `cpu_idle` 不返回；仅在 `need_resched` 为真时调用 `schedule`，否则继续空转。
