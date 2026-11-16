# Lab4 提示词记录

## 功能模块二：进程创建及初始化（2414034）

本提示词归纳本轮已采用的实现约束；不是额外一次独立模型调用记录。A/C 的提示词由对应成员补充。

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
