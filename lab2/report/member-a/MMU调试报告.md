# Lab2 成员 A：QEMU 页表与 TLB 调试（2414034）

实验日期：2026-10-17。本章对应手册第 17 章。使用真实内核执行的访存作为触发条件，没有通过 GDB 人工调用地址翻译函数。

## 1. 环境与调试方法

Windows 主机的 WSL Ubuntu 24.04 中，日常 QEMU 为 8.2.2，独立调试版本为 QEMU 4.1.1，宿主 GDB 与客户机 gdb-multiarch 为 15.1。内核由 riscv64-unknown-elf-gcc 13.2 构建。独立源码与二进制分别位于 `lab2/_local/qemu-4.1.1` 和 `lab2/_local/qemu-build/riscv64-softmmu/qemu-system-riscv64`。未执行 `make install`，系统 QEMU 保持不变。

宿主 GDB 调试 x86-64 的 QEMU C 代码；客户机 gdb-multiarch 通过 QEMU stub 观察 RISC-V 指令、寄存器及内核符号。本次采用手册末尾允许的条件断点方案：先用客户机调试确认真实目标指令和地址，再让宿主 GDB 从启动开始捕获这些地址。两个层面的记录分别采集，不冒称三个终端同时调试。宿主断点还约束 `access_type==1`（写）和 `mmu_idx==1`（S 模式），避免把固件访问或调试器读取误认为目标内核写操作。

完整可执行命令见 [工具说明](../../Lab2_成员A_2414034/lab2/tools/member_a/README.md)。最小复现顺序是在 `lab2/` 中执行：

```bash
python3 tools/member_a/run_first_fit.py
bash tools/member_a/build_debug_qemu.sh
python3 tools/member_a/capture_guest.py tools/member_a/guest_memory.gdb guest-memory
python3 tools/member_a/run_host_mmu.py
```

QEMU 配置包含 `--enable-debug`、`--target-list=riscv64-softmmu`，关闭文档和图形前端等非必要组件。旧版 QEMU 配合自身 `pc-bios` 中的固件运行；不是将新 OpenSBI 直接配给旧 CPU 模型。源码文件位置、断点和变量以 4.1.1 为准。

## 2. 从内核指令选定真实访存

客户机证据：[系统 QEMU 记录](logs/guest-memory.txt) 与 [QEMU 4.1.1 记录](logs/guest-memory-qemu411.txt)；两版结果一致。宿主证据：[完整源码单步记录](logs/host-mmu.txt)，包含 `HOST_WALK_PASS BARE`、`HOST_WALK_PASS SV39`、三次 `TLB_HIT_PASS` 及 `HOST_MMU_PASS`。

| 阶段 | 指令地址 | 指令 | 有效访问地址 | satp |
| --- | --- | --- | --- | --- |
| 尚未分页 | `0x80200008` | `sd a0,0(t0)` | `0x80206000`，保存 hartid | `0` |
| Sv39 | `0xffffffffc0201808` | `sb a1,-1(a5)` | `0xffffffffc0206018`，清 BSS 首字节 | `0x8000000000080205` |

第二条指令来自 `kern_init → memset(edata,0,end-edata)`。断在 memset 后按实际反汇编走到存储指令，`a5-1` 是有效地址；指令所在的 VA 与它读写的数据 VA 不是同一个概念。代码或编译器改变后地址可能变化，必须重新反汇编，不能把本表当作不变常量。

## 3. Sv39 页表遍历与 VA→PA 计算

当前 `satp.MODE=8`，PPN=`0x80205`，根页表物理地址为 `0x80205000`。Sv39 参数为 `levels=3, ptidxbits=9, ptesize=8`。目标 VA `0xffffffffc0206018` 符合 Sv39 高位符号扩展要求。

1. 第一轮循环 `i=0`、`ptshift=18`，根索引为 `(VA>>30)&511=511`。
2. 页表项地址 `base+idx*8 = 0x80205000+511*8 = 0x80205ff8`。
3. 从客户机物理 RAM 读取 8 字节，PTE=`0x200000cf`，PPN=`0x80000`。V/R/W/X/A/D 都为 1，U=0。
4. V 有效且 R/W/X 非零，这是叶子项。S 模式允许访问；写操作满足 W；超级页 PPN 低 18 位为 0，满足对齐。A/D 已设置，不需要在本次执行中修改 PTE。
5. 叶子位于根级，映射 1 GiB。该函数按 `((PTE>>10) | ((VA>>12)&0x3ffff))<<12` 返回 4 KiB 页基址 `0x80206000`，随后访问保留原始页内偏移 `0x18`，完整 PA 为 `0x80206018`。

也可按超级页算：`0x80000000 | (VA & 0x3fffffff) = 0x80206018`，与内核固定偏移计算 `VA-0xffffffff40000000` 一致。根表虚拟地址是 `0xffffffffc0205000`，不要把它直接填入 satp。

`levels=3` 表示最多可查三级，不表示本次一定读取三张表。当前 `entry.S` 仅根表第 511 项有效且为叶子，因此实际第一轮就返回。若遇到 V=1 且 R/W/X 全零的非叶子项，才令 `base=ppn<<12` 并进入下一层。

## 4. 关键代码路径与分支

数据访问的慢路径为：

```text
TCG 翻译后的客户机写指令
  → 对应的 store helper → store_helper
  → tlb_victim_hit 未命中 → tlb_fill
  → RISCVCPU 的 riscv_cpu_tlb_fill
  → get_physical_address
  → 访问权限 / PMP 检查
  → tlb_set_page → tlb_set_page_with_attrs
  → 回到 store_helper 执行实际写入
```

关键位置在 `target/riscv/cpu_helper.c` 和 `accel/tcg/cputlb.c`：

- `get_physical_address` 首先处理特权模式和 MPRV；M 模式直接地址路径与 S 模式 Bare 路径不能混为一谈。本次选择 S 模式以便比较 satp 的影响。
- `satp.MODE` 选择 Bare 或 Sv39。Bare 将 `*physical=addr` 后返回；Sv39 配置层数、索引位数和 PTE 宽度，先检查 VA 规范性，再遍历。
- PTE 分支分别检查 V、非叶子、非法权限组合、U/S 访问、超级页对齐、R/W/X 许可，再处理 A/D 更新并拼接物理页号。本次实际走合法根级叶子分支；其他分支属于源码分析，没有声称逐一故障注入。
- `riscv_cpu_tlb_fill` 的翻译或保护检查失败时进入访问异常处理；成功则以页对齐的 VA、PA 和权限填充软件 TLB。
- `store_helper` 和 `get_page_addr_code` 都有 C 层 TLB 查询；先尝试主表，必要时查 victim cache，再 refill。不能把所有未命中都等同于立刻页表遍历。

## 5. 软件 TLB 与真实硬件 TLB

实际捕获的两次填充如下，均为 `mmu_idx=1`、`prot=7`、`size=0x1000`：

| 项目 | Bare 写 hartid | Sv39 写 BSS |
| --- | --- | --- |
| 输入 VA 页 | `0x80206000` | `0xffffffffc0206000` |
| PA 页 | `0x80206000` | `0x80206000` |
| 主表 index | `6` | `6` |
| addr_read / addr_write / addr_code | 均为 `0x80206000` | 均为 `0xffffffffc0206000` |
| addend（本轮） | `0x7fff5fe00000` | `0x80001fe00000` |

随后断在 `accel/tcg/cputlb.c:1030` 的 `if (unlikely(!tlb_hit(entry->addr_code, addr)))`。地址 `0xffffffffc0201806`、`0xffffffffc0201810` 对应 index=1，标签为 `0xffffffffc0201000`。执行 next 后直接到 1039 行，越过 1031～1037 行 refill 分支；脚本同时断言标签匹配和到达行内容，得到三次 `TLB_HIT_PASS`。这些是实际取指查询，两个地址不意味着读了两张页表。

QEMU TCG 的软件 TLB 同时服务客户机 MMU 模拟和“客户机地址→宿主内存”的快速访问。TLB 项包含 `addr_read/addr_write/addr_code` 标签与 `addend`；对普通 RAM 命中时，可用客户机地址加 addend 定位宿主内存。它还携带 RAM/MMIO、权限等处理信息。addend 不是本内核的 `va_pa_offset`，宿主地址受运行时内存分配影响，重复运行可变化。

真实硬件 TLB 通常缓存页表翻译结果及权限等元数据；它没有 QEMU 为访问宿主进程 RAM 而使用的 C 结构和宿主指针 addend。硬件具体组相联和替换实现也不能由 QEMU 这套软件缓存推断。Bare 模式不进行 Sv39 页表转换，但 QEMU 仍可填充软件 TLB，以便将客户机物理地址快速对应到宿主存储，这是本实验的重要对照。

普通 TCG 数据访问命中的快速路径常被直接生成为宿主机器码，因此不是每次读写都会经过 C helper。x86 后端的 `tcg/i386/tcg-target.inc.c` 中 `tcg_out_tlb_load` 负责生成这类查询代码。本次还在 `get_page_addr_code` 的 C 查询处分支单步，用后续内核取指说明已填充条目的命中行为；不能将取指证据写成 BSS 数据写的第二次命中。

本例页表为 1 GiB 大页，但 QEMU 4.1.1 的该路径将访问对应的 4 KiB 子页填入软件 TLB。这也是模拟器实现粒度与客户机页表映射粒度的区别。

## 6. 调试经历与理解

真实问题之一是新 QEMU/OpenSBI 下启用分页后低地址取指失败。只看到固件输出不能证明内核初始化已经执行。客户机 GDB 显示故障发生在进入 C 之前，故先修复入口，再检查 page_init。详见初始化报告的原始故障日志。

最初的临时 stvec 修复在 QEMU 8.2.2 成功，但 4.1.1 仍未到达 BSS 断点。[旧版异常现场](logs/trap-qemu411.txt) 显示首次指令页故障时 PC 已到低地址 `0x80200050`，satp 已开启、stvec 却已清零。结合入口反汇编可知，旧版已翻译指令块继续执行了清 stvec 指令。最终在 satp/sfence 后显式 `jr t2` 到高地址标签，并将标签按 4 字节对齐；只在高地址入口清 stvec。两个版本都能启动，4.1.1 的客户机及宿主断点也都到达。没有为了获得预期页表数据而修改 QEMU 的 MMU 算法。

其他真实环境细节：旧 QEMU 构建缺少 flex/bison，安装依赖后继续编译；WSL 在 Windows 挂载盘解压大量小文件很慢，转用 Windows 解压后继续构建。构建产生旧 Python 写法和时钟偏差警告，但已成功链接并实际运行。源码解压中的非目标 ROM 符号链接不用于此次 riscv64-softmmu 构建。本地保存配置、版本和哈希，复现脚本默认使用 Linux tar。

另一处容易误读的是 `get_physical_address` 返回值：Sv39 成功时输出参数是页基址，Bare 路径则直接给输入地址；函数本身的 int 返回值才是成功/失败状态。必须结合调用者的页掩码与最终访问偏移解释，不能拿 `0x80206000` 宣称完整数据地址末尾的 `0x18` 丢失。

客户机 GDB 看的是模拟的寄存器与内存，宿主 GDB 看的是执行模拟逻辑的进程。如果宿主暂停，客户机 stub 也无法继续响应；多终端手动调试必须按层次恢复执行。本次以已核对地址的条件断点减少了多终端控制及无关访问干扰。

本章对应地址空间、页表遍历、权限、缓存和模拟器执行机制；没有实现进程私有地址空间、真实芯片 TLB 替换策略、缺页换入或多核 shootdown。

依据：[课程 QEMU/MMU 调试要求](http://oslab.mobisys.top/lab2026/_book/lab2/lab2_4_qemu_mmu_debug.html)。QEMU 路径与分支分析来自本次下载的官方 4.1.1 源码，实际证据由随附脚本生成。
