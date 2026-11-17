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