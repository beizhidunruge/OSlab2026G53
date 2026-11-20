# Lab4 提示词记录
## 功能模块一：虚拟内存页操作功能

**负责人：** 2412604-刘泽民

[PROMPT]

**任务**：修改 `kern/mm/pmm.c`，实现虚拟内存页操作功能，包括建立、更新和撤销虚拟地址到物理页的映射，保证页表项、物理页引用计数和 TLB 状态一致。

**操作要求**：必须修改项目实际文件，在 `//LAB4 function1 YOUR CODE` 处补全功能，保留其他现有代码及测试。复用已有接口，不引入无关功能。

**输出要求**：根据以下规格完成实现。代码应逻辑清晰、结构简洁，避免重复逻辑、不必要的辅助函数和冗余注释，同时完整处理异常情况。完成后编译并验证相关测试，说明实际结果。

[RELY]

现有工程提供以下数据结构、宏和接口：

- **数据结构**：`pde_t`、`pte_t`、`struct Page`（`ref` 为引用计数）、`pages`、`boot_pgdir_va`。
- **页表操作**：`get_pte(pgdir, la, create)` 定位页表项，`create=1` 时按需分配中间级页表页，失败返回 `NULL`；`pte_create(ppn, perm)` 构造包含 `PTE_V` 的页表项；`pte2page()`、`pde2page()`、`page2ppn()` 用于物理页转换。
- **物理页管理**：`page_ref()`、`page_ref_inc()`、`page_ref_dec()`、`set_page_ref()`、`alloc_page()`、`free_page()`。
- **地址与权限**：`PDX1()`、`PDX0()`、`PTX()`、`PDE_ADDR()`、`KADDR()`、`PGSIZE`、`PTE_V`、`PTE_U`、`PTE_W`、`PTE_R`、`PTE_X`。
- **TLB 操作**：`tlb_invalidate()`、`flush_tlb()`。
- **错误码**：`E_NO_MEM`。

注意：现有 `get_pte()` 可能在成功挂接第一级页表页后，因下一级分配失败而返回 `NULL`，此时已创建的页表页不会自动回收。

[GUARANTEE]

在 `kern/mm/pmm.c` 中实现：

```
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);(kern/mm/pmm.c)
void page_remove(pde_t *pgdir, uintptr_t la);(kern/mm/pmm.c)
```

保持函数签名不变。必要时可添加模块内部的 `static` 辅助函数，不修改公开接口。

[SPECIFICATION]

### 1. page_insert()

**Pre-Condition**

- `pgdir` 和 `page` 均有效，`la` 不要求页对齐。
- 目标虚拟地址可能未映射，也可能已映射到相同或不同的物理页。

**Post-Condition**

- 成功返回 `0`，目标页表项映射到 `page`，包含 `PTE_V` 和 `perm` 指定的权限。
- 物理页引用计数与映射关系一致，映射或权限变化后对应 TLB 项失效。
- 页表分配失败返回 `-E_NO_MEM`，原有映射与引用计数不变，不留下半完成映射或资源泄漏。

**Case 1：原地址未映射**

按需建立中间级页表，创建目标映射，目标物理页引用计数增加一次。

**Case 2：原地址已映射到同一物理页**

更新权限，保持引用计数和空闲物理页数量不变，不得释放该物理页。

**Case 3：原地址已映射到其他物理页**

替换旧映射，新物理页引用计数增加一次，旧物理页引用计数减少一次；旧页引用计数归零时释放。

**Case 4：页表分配失败**

返回 `-E_NO_MEM`。若本次调用已创建部分中间级页表页，应撤销新增的页表项并释放对应页表页，不影响原有页表结构和映射，原来的页表项应保持一致。

**Requirements**

- 判断有效映射时通过页表项指针和`PTE_V`判断。
- 同页重复插入不得重复增加引用计数或错误释放物理页。
- 新权限以 `perm` 为准，不保留旧权限；映射或权限更新后使对应 TLB 项失效。

### 2. page_remove()

**Pre-Condition**

- `pgdir` 有效，`la` 不要求页对齐，目标地址可能存在或不存在映射。

**Post-Condition**

- 有效映射被撤销后，页表项清零，物理页引用计数正确减少，对应 TLB 项失效。
- 无有效映射时直接返回，不改变页表和引用计数。

**Case 1：存在有效映射**

清除页表项，原物理页引用计数减少一次，归零时释放物理页，并使对应 TLB 项失效。

**Case 2：不存在有效映射**

直接返回，不分配页表页、不修改引用计数，也不释放物理页。重复删除应保持相同结果。

**Requirements**

- 查找页表项时不得创建缺失的中间级页表。
- 仅对有效映射使用 `pte2page()`。
- 不回收中间级页表页，避免破坏其他映射及现有测试依赖的页表结构。

### 3. 验证要求

不得修改测试代码

---

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

1. alloc_proc：内存失败返回 NULL；成功清零 PCB，设置 UNINIT、pid=-1、pgdir=boot_pgdir_pa 并初始化链表节点，不登记、不唤醒。
2. copy_mm：前提 current 有效且 mm=NULL，断言该约束，子线程 mm=NULL，返回零；不取得用户地址空间资源。
3. copy_thread：前提子线程已分配内核栈且 tf 有效；在 kstack+KSTACKSIZE-sizeof(trapframe) 放置现场副本，禁止保存局部模板的指针；a0=0；恢复 sp 使用非零 esp，否则为 tf 地址；context.ra=forkret，context.sp=tf。
4. do_fork：前提 current/tf 有效；检查进程上限，依次分配 PCB、设置 parent、分配栈、copy_mm、copy_thread；登记临界区保存并恢复原中断状态，复查上限，分配唯一 PID，挂入哈希与链表，增加计数，最后唤醒。成功返回 PID；上限返回 -E_NO_FREE_PROC，内存不足返回 -E_NO_MEM；失败逆序释放已占用资源，不留下全局登记。
5. kernel_thread：清零临时 tf，s0=fn、s1=arg、epc=kernel_thread_entry；status 基于当前 sstatus 设置 SPP/SPIE，清除 SIE；调用 do_fork(clone_flags|CLONE_VM,0,&tf)，传播返回值。
6. proc_init：启动时只调用一次；初始化全局链表和哈希表。idle 使用 bootstack、PID0、RUNNABLE、need_resched=1，parent/mm/tf=NULL，runs/flags/context 初值为零；设 current=idle 并计数，但 idle 不加入普通链表和哈希。通过 kernel_thread(init_main,"Hello world!!",0) 创建 PID1，find_proc 后命名 init；创建失败 panic。

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
