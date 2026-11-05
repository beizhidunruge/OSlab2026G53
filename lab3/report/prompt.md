# Lab3 提示词与规格汇编

代码路径以仓库根目录为准，源码位于 `lab3/code/`。实验报告见 [report.md](report.md)。
模块一原始提示词中的路径以源码根目录为准，对应当前 `lab3/code/`。

| 负责人 | 材料 |
| --- | --- |
| 刘泽民（2412604） | [Trap 上下文恢复与入口模块 Prompt 和 Spec](#module1-prompt) |
| 丁欣怡（2414034） | [中断与异常处理模块 Prompt 和 Spec](#module2-prompt) |

<a id="module1-prompt"></a>

## 功能模块一：Trap 上下文恢复与 Trap 入口功能

**负责人：** 2412604-刘泽民

**说明：** 原分支源码根目录为 `lab3/`，本地对应 `lab3/code/`；下列原始提示词中的 `kern/` 路径均相对于源码根目录。

````markdown
# 功能模块 1：Trap 上下文恢复与入口实现

[PROMPT]

**任务**：修改 `kern/trap/trapentry.S`，补全 `RESTORE_ALL` 和 `__alltraps` 中的待实现代码，实现功能模块一：Trap 上下文恢复与 Trap 入口功能，使 Trap 处理程序能够正确调用 C 函数，并在处理结束后恢复执行现场。

**操作要求**：直接修改实际文件，即 `kern/trap/trapentry.S`中的`#lab3 function1 YOUR CODE`下，仅修改指定的待实现部分，其他已有代码不进行修改。

[RELY]

1. **SAVE_ALL**

已有的上下文保存宏，将通用寄存器及相关 CSR 保存到当前栈上的 `struct trapframe`。

2. **struct trapframe：**

``` c
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};

void trap(struct trapframe *tf);
```
   - `gpr` 按照 x0～x31 的顺序保存通用寄存器对应的值。
   - `status`、`epc`、`badvaddr`、`cause` 分别对应 `sstatus`、`sepc`、异常附加信息和 `scause`。
   - `trapframe`占据36个`uintptr_t`的空间，总大小为 288 字节。

3. **trap(struct trapframe *tf)**：

   C 语言 Trap 处理函数，接收当前 trapframe 的指针。定义于 `kern/trap/trap.c`，负责中断和异常的分发与处理。

4. **__trapret：**
   已有的 Trap 返回入口，通过 `RESTORE_ALL` 恢复上下文，并使用 `sret` 完成 Trap 返回 。

5. **REGBYTES`、`LOAD`、`STORE：**
   `riscv.h`已有的汇编宏。

[GUARANTEE]

在 `kern/trap/trapentry.S` 中实现：

1. `RESTORE_ALL`：与已有的 `SAVE_ALL` 对应，恢复 trapframe 中保存的通用寄存器、`sstatus`、`sepc` 和原始栈指针。

2. `__alltraps`：在 `SAVE_ALL` 后，将当前 trapframe 指针传给 `trap()`，函数返回后进入 `__trapret`。

[SPECIFICATION]

**1. RESTORE_ALL**

- Pre-Condition：`SAVE_ALL` 已完成上下文保存，当前 `sp` 指向 trapframe。
- Post-Condition：恢复通用寄存器、`sstatus`、`sepc` 和原始栈指针，使程序能够通过 `sret` 返回正确的地址继续执行。
- Requirements：
  - 寄存器恢复顺序及偏移必须与 `SAVE_ALL` 的保存顺序和 `trapframe` 的布局对应。
  - 注意 `SAVE_ALL` 对 `x2(sp)` 的特殊处理，恢复时不能破坏后续访问 trapframe 所需的栈指针。
  - `sepc` 和 `sstatus` 应使用 trapframe 中的当前值，以保留 `trap()` 中可能进行的修改。

**2. __alltraps**

- Pre-Condition：Trap 已进入 `__alltraps`，能够通过已有的 `SAVE_ALL` 保存上下文。
- Post-Condition：将保存后的 trapframe 作为 `struct trapframe *` 传递给 `trap()`，在 `trap()` 返回后进入 `__trapret`，完成上下文恢复并通过 `sret` 返回原执行流。
- Requirements：
  - 通过 `a0` 传递 trapframe 指针。
  - 调用 `trap()` 时保持栈对齐，返回后 `sp` 应与调用前一致。
  - `trap()` 返回后进入已有的 `__trapret`。

请先阅读相关源码，按照已有的寄存器布局和汇编约定完成实现，并使用make qemu检查结果。由于`kern/trap/trap.c`中还有代码未实现，可能运行不了。
````

<a id="module2-prompt"></a>

## 功能模块二：中断与异常处理

**负责人：** 2414034-丁欣怡

````markdown
## [PROMPT]

**任务：** 在 `lab3/code/kern/trap/trap.c` 中实现功能模块二：中断与异常处理。接收入口保存的 Trap frame，根据原因处理事件，必要时调整异常返回位置，与已有的上下文恢复流程衔接。

**操作要求：**

1. 先阅读相关源码，以实际接口、宏和数据结构为准；必须修改实际文件，在 `lab3 function2` 标记处补全实现。
2. 实现 `interrupt_handler()` 和 `exception_handler()`，保持签名和外部可见性不变。可以增加必要的头文件、模块内部常量、静态状态和静态辅助函数。
3. 不修改 `struct trapframe`、`trap_dispatch()`、`trap()`、`print_ticks()`、`print_trapframe()` 及模块一的汇编代码，不修改官方测试和判分条件。
4. 注释说明函数职责及原因码、计数边界、指令长度等关键约束。不增加系统调用、缺页恢复或设备驱动等无关功能。
5. 若构建或运行依赖尚未完成，应指出具体限制，不把编译通过等同于功能通过。

**输出要求：** 说明修改位置、实现依据、实际验证结果及未解决的问题；未执行的测试不得写成通过。

## [RELY]

相关文件（以下相对于 `lab3/code`）：

- `kern/trap/trap.c`、`trap.h`、`trapentry.S`。
- `kern/driver/clock.c`、`clock.h`。
- `libs/riscv.h`、`libs/sbi.h`。
- `tools/grade.sh`、`tools/test_lab3.c`、`tools/test_lab3_context.S`。

已有调用关系：`__alltraps → SAVE_ALL → trap(tf) → trap_dispatch(tf) → 对应处理函数 → RESTORE_ALL → sret`。`trap_dispatch()` 已按原因最高位区分中断与异常；模块二不负责保存和恢复通用寄存器，也不直接执行 `sret`。

`struct trapframe` 中的 `cause` 保存原因，`epc` 保存恢复执行位置，`gpr`、`status` 等字段用于恢复现场。RV64 下 `uintptr_t` 为 64 位无符号类型。具体布局以头文件为准。

已有定义和接口：

```c
/* clock.h 中的计数器及事件接口 */
extern volatile size_t ticks;
void clock_set_next_event(void);

/* sbi.h 中的关机接口 */
void sbi_shutdown(void);
```

可使用 `IRQ_S_TIMER` 等中断原因宏、`CAUSE_ILLEGAL_INSTRUCTION`、`CAUSE_BREAKPOINT`、`cprintf()`、`print_ticks()` 和 `print_trapframe(tf)`。`TICK_NUM` 当前为 100，`clock_init()` 初始化 `ticks`。

**关机阈值约定：** 初始框架未定义手册提及的 `SHUTDOWN_AFTER`，手册也未给出其数值。当前实现采用 `SHUTDOWN_AFTER = 10`，表示输出 10 次统计信息后关机，即正常从零计数时达到 1000 ticks。这是本次实现约定，不是已确认的课程要求；若课程补充数值，应同步调整代码和本文。

本次指令长度处理范围为实验使用的 16/32 位编码，不要求支持更长编码。官方非法指令测试使用 `.word 0x0000007b`，长度为 4 字节。断点需考虑压缩和普通两种编码。

## [GUARANTEE]

必须实现：

```c
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);
```

实现应完成原因识别、持续时钟、计数和关机、异常地址更新及兜底诊断。正常返回时必须保留与本次处理无关的现场，供模块一恢复执行。

## [SPECIFICATION]

### 1. interrupt_handler(struct trapframe *tf)

**Pre-Condition：**

- `tf` 指向有效、可读取的 Trap frame。
- 正常中断原因含最高位中断标志；也允许测试直接传入未知原因码。
- 真实时钟路径中，时钟模块已经初始化；本次启动的计数和统计状态从零开始。

**Post-Condition：**

- 按去除最高中断标志后的原因码完成处理。
- 正常返回后，Trap frame 的所有字段保持不变，包括 `epc`、`cause`、通用寄存器及状态。
- 只有 S 模式时钟中断更新时钟计数与统计状态。

**Case 1：S 模式时钟中断。** 原因为 `IRQ_S_TIMER` 时，安排下一次事件，使时钟能够持续触发；`ticks` 相比进入时恰好增加 1。更新后的计数为 `TICK_NUM` 的整数倍时，调用已有 `print_ticks()` 一次；其他时刻不打印统计信息。每次统计输出使累计输出次数增加 1，第 10 次输出完成后调用 `sbi_shutdown()`。

**Case 2：其他已知中断。** 按框架中的类型区分并打印提示，不改变时钟计数、统计状态或 Trap frame。S 模式软件中断输出包含 `Supervisor software interrupt`，不附加本实验未要求的设备处理。

**Case 3：未知中断。** 输出去除标志后的原因码，再调用 `print_trapframe(tf)`，保留原始 `tf->cause`。测试原因码 15 的输出包含 `Unknown interrupt: cause 15`。不改变时钟计数、统计状态和 Trap frame。当前实现的原因码提示按 `int` 输出，完整原始原因仍由 Trap frame 诊断保留；对超出 `int` 范围原因码的无损提示不是本版已验证能力。

**Requirements：** 原因码提取按寄存器位宽进行无符号运算；不能只计数而遗漏下次事件设置；不推进 `epc`，不在处理函数中主动开启全局中断。正常关机验证不启用会让 `print_ticks()` 提前 panic 的 `DEBUG_GRADE`。

### 2. exception_handler(struct trapframe *tf)

**Pre-Condition：**

- `tf` 指向有效、可读写的 Trap frame，`cause` 表示异常原因。
- 对要求恢复的非法指令和断点异常，`epc` 指向可读取的内核指令，属于当前实验的 16/32 位编码范围。

**Post-Condition：**

- 完成异常分类及对应输出。
- 非法指令或断点的 `epc` 恰好推进到该指令之后；其他异常保持原地址。
- 除上述 `epc` 更新外，其他 Trap frame 字段、`ticks` 和时钟统计状态保持不变。

**Case 1：非法指令。** 原因为 `CAUSE_ILLEGAL_INSTRUCTION` 时，输出 `Illegal instruction at` 及原异常地址，然后按实际指令长度更新 `epc`。对 `.word 0x0000007b`，地址增加 4，避免再次执行同一非法指令。

**Case 2：断点。** 原因为 `CAUSE_BREAKPOINT` 时，输出 `Breakpoint at` 及原断点地址；对 2 字节压缩断点增加 2，对 4 字节普通断点增加 4。返回位置是下一条指令，不重复触发断点，也不跳过下一条指令。

**Case 3：其他异常。** 调用 `print_trapframe(tf)`，不修改 `epc` 或其他字段。此分支只诊断，不保证实际故障恢复。

**Requirements：** 输出原异常地址，不对所有异常使用固定增量，也不把断点都当作压缩指令。读取编码时考虑 2 字节对齐，不无条件把 `badvaddr` 当作指令编码。修改保存于 Trap frame 的返回位置，由汇编恢复流程完成返回；不在处理函数中直接操作返回流程。

### 3. 验证要求

先检查修改范围并编译。模块一及启动路径就绪后运行官方评分，验证寄存器和断点返回、非法指令返回与诊断、持续时钟至 `ticks=200`。

补充测试覆盖显式的 16/32 位断点、99→100 和 199→200 的计数边界、非时钟事件不改变计数、中断不改变 `epc`，以及第 10 次输出后的正常关机。区分正常关机与外部超时终止。

补充测试在独立副本或可移除入口中进行，不污染正式启动流程。分别报告编译、独立逻辑测试和集成验证，不将预期结果记为实测结果。
````
