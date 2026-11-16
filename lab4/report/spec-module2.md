# 模块二规格与协作接口

负责人：2414034 丁欣怡。范围：proc.c 的 function2；不实现用户空间复制、退出回收或调度策略。

## 前提与数据约定

在单 hart 内核中，物理页分配、kmalloc 和 boot_pgdir_pa 已初始化；do_fork 的 current 有效且 mm 为 NULL，输入 tf 指向可读现场。proc_init 只在启动时调用一次。模块一和前置 Lab2 由 A 负责，调度和前置 Lab3 由 C 负责。

PCB 内嵌 context；trapframe 位于独立内核栈顶部。pgdir 保存 boot_pgdir_pa（物理地址），mm 为 NULL。idle 不加入普通进程链表/哈希表，但计入 nr_process。

## 六函数规格

| 接口 | 前置条件 | 成功后置条件 | 失败行为 |
|---|---|---|---|
| alloc_proc | kmalloc 可用 | 清零 PCB，UNINIT、pid=-1、pgdir=boot_pgdir_pa，初始化链表节点 | 返回 NULL，不登记 |
| copy_mm | current 有效且 mm=NULL | 子线程 mm=NULL，返回 0 | 不支持用户空间；断言检查前提 |
| copy_thread | 已分配 KSTACKSIZE 栈，输入 tf 有效 | tf 位于栈顶减 sizeof(trapframe)，复制内容；a0=0；sp 使用非零 esp，否则为 tf 地址；context.ra=forkret，context.sp=tf | 无额外分配 |
| do_fork | current/tf 有效，满足内核线程约定 | parent=current；独立栈；唯一正 PID；登记一次、计数一次；最后唤醒；返回 PID | 上限返回 -E_NO_FREE_PROC，内存失败返回 -E_NO_MEM；逆序释放栈与 PCB，不遗留登记 |
| kernel_thread | fn/arg 满足调用者使用约定 | 临时 tf 清零；s0=fn、s1=arg、epc=kernel_thread_entry；SPP/SPIE 置位、SIE 清零；调用 do_fork(flags\|CLONE_VM,0,&tf) | 原样传播错误码 |
| proc_init | 启动初始化只执行一次 | 初始化链表/哈希；idle PID0、bootstack、runnable、need_resched=1、current=idle；创建 PID1 init，父线程为 idle | 创建失败 panic，禁止继续使用无效指针 |

登记临界区保存原中断状态，复查进程数上限，分配 PID、挂哈希/链表、递增计数、唤醒后恢复原状态。失败时尚未登记。copy_mm 当前不取得额外资源，后续若扩展，必须同步扩展回滚。

set_proc_name 固定读取 PROC_NAME_LEN 字节，因此本模块传入至少 PROC_NAME_LEN+1 字节的补零名称数组。

## 交给成员 C 的首次运行约定

switch_to 恢复 context → forkret → forkrets(current->tf) → __trapret → sret → kernel_thread_entry → fn(arg)。tf.status 的 SPP=1 使首次返回留在 S 模式，SPIE=1 使 sret 后允许中断，SIE=0 避免恢复现场中途提前打开中断。

context.sp 与 tf.gpr.sp 都初始化为 tf 地址（内核线程 stack=0 路径）；二者分别用于进入 forkret 和最终恢复线程运行栈。init 只有在登记和现场准备完毕后才变为 runnable；正在运行仍使用 RUNNABLE，由 current 区分。

## 验收

模块对象编译；检查进程数上限、分配失败清理、PID唯一性、栈独立性；整合后通过原有 Process Create 测试并与 C 追踪首次入口链。Process Create 10/10 和全组 30/30 是待运行的验收目标，不能由编译通过推断。
