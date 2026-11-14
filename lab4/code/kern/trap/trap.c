#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <stdio.h>
#include <trap.h>
#include <vmm.h>

#define TICK_NUM 100

static void print_ticks()
{
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S
 */
void idt_init(void)
{
    extern void __alltraps(void);
    /* Set sscratch register to 0, indicating to exception vector that we are
     * presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
    /* Allow kernel to access user memory */
    set_csr(sstatus, SSTATUS_SUM);
}

void print_trapframe(struct trapframe *tf)
{
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr)
{
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

extern struct mm_struct *check_mm_struct;

//lab3 请补充你在lab3中的代码
/* 返回 epc 处指令的长度（字节）。
 * RISC-V 指令长度由最低两比特区分：bit[1:0]==0b11 为 32 位指令，
 * 其余为 16 位压缩指令。这里读取半字而非整字，以便指令仅 2 字节对齐时
 * 也能安全读取，且本实验只需覆盖 16/32 位编码，不做更长编码的扩展。 */
static int insn_length(uintptr_t epc) {
    uint16_t insn = *(const uint16_t *)epc;
    return ((insn & 0x3) == 0x3) ? 4 : 2;
}

/* interrupt_handler - 处理中断。
 * 依据去除最高中断标志位后的原因码分派；仅 S 模式时钟中断更新计数与
 * 统计状态，其余已知中断只输出提示，未知中断输出原因码并打印 Trap frame。 */
void interrupt_handler(struct trapframe *tf) {
    // 提取中断原因码：清除 scause 最高位的中断标志。
    // 使用 uintptr_t 位宽做无符号移位，避免窄整数掩码或有符号运算出错。
    uintptr_t cause = (tf->cause << 1) >> 1;

    switch (cause) {
    case IRQ_S_TIMER:
        // S 模式时钟中断：先重新安排下一次时钟事件，保证中断持续触发，
        // 再累加计数，避免只计数而遗漏下一次事件设置。
        clock_set_next_event();
        ticks++;
        // 计数到达 TICK_NUM 的整数倍时输出一次统计信息。
        if (ticks % TICK_NUM == 0) {
            print_ticks();
            // // 累计统计输出次数；达到约定的 SHUTDOWN_AFTER 时关机。
            // if (++print_count == SHUTDOWN_AFTER) {
            //     sbi_shutdown();
            // }
        }
        break;
    case IRQ_S_SOFT:
        cprintf("Supervisor software interrupt\n");
        break;
    case IRQ_U_SOFT:
        cprintf("User software interrupt\n");
        break;
    case IRQ_H_SOFT:
        cprintf("Hypervisor software interrupt\n");
        break;
    case IRQ_M_SOFT:
        cprintf("Machine software interrupt\n");
        break;
    case IRQ_U_TIMER:
        cprintf("User timer interrupt\n");
        break;
    case IRQ_H_TIMER:
        cprintf("Hypervisor timer interrupt\n");
        break;
    case IRQ_M_TIMER:
        cprintf("Machine timer interrupt\n");
        break;
    case IRQ_U_EXT:
        cprintf("User external interrupt\n");
        break;
    case IRQ_S_EXT:
        cprintf("Supervisor external interrupt\n");
        break;
    case IRQ_H_EXT:
        cprintf("Hypervisor external interrupt\n");
        break;
    case IRQ_M_EXT:
        cprintf("Machine external interrupt\n");
        break;
    case IRQ_COP:
        cprintf("Coprocessor interrupt\n");
        break;
    case IRQ_HOST:
        cprintf("Host interrupt\n");
        break;
    default:
        // 未知中断：输出原因码并打印完整诊断信息；保留原始 tf->cause，
        // 不改变时钟计数与 Trap frame。
        cprintf("Unknown interrupt: cause %d\n", (int)cause);
        print_trapframe(tf);
        break;
    }
}

/* exception_handler - 处理异常。
 * 对非法指令与断点异常，按指令实际长度推进 tf->epc 以越过故障指令；
 * 其余异常仅诊断输出，不保证可恢复，也不擅自跳过故障指令。 */
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
    case CAUSE_ILLEGAL_INSTRUCTION: {
        // 先保存原异常地址用于诊断，再按实际指令长度推进返回地址。
        uintptr_t epc = tf->epc;
        cprintf("Illegal instruction at 0x%lx\n", (unsigned long)epc);
        tf->epc = epc + insn_length(epc);
        break;
    }
    case CAUSE_BREAKPOINT: {
        // 断点可能为 16 位压缩指令或 32 位普通指令，按编码区分长度。
        uintptr_t epc = tf->epc;
        cprintf("Breakpoint at 0x%lx\n", (unsigned long)epc);
        tf->epc = epc + insn_length(epc);
        break;
    }
    default:
        // 其他异常：仅打印完整 Trap frame，不推进 epc、不修改其他字段。
        print_trapframe(tf);
        break;
    }
}


/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf)
{
    // dispatch based on what type of trap occurred
    if ((intptr_t)tf->cause < 0)
    {
        // interrupts
        interrupt_handler(tf);
    }
    else
    {
        // exceptions
        exception_handler(tf);
    }
}
