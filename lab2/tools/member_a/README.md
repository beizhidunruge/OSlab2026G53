# 成员 A 复现工具（2414034）

在 WSL Ubuntu 24.04 内，从 `lab2/` 目录运行。日志自动写入 `report/member-a/logs/`，不提交个人报告和日志。

## 1. 初始化和 First-Fit

需要 `make`、`riscv64-unknown-elf-gcc`、`qemu-system-riscv64`、`gdb-multiarch`、Python 3。

```bash
python3 tools/member_a/run_first_fit.py
python3 tools/member_a/capture_guest.py tools/member_a/validate_layout.gdb layout
python3 tools/member_a/run_boundaries.py
python3 tools/member_a/capture_guest.py tools/member_a/guest_memory.gdb guest-memory
```

首次脚本将源码复制到 `_local/first-fit/`，排除尚未实现、不能编译的 `best_fit_pmm.c`，保留原 First-Fit 检查函数。这个副本只用于 A 的初始化验收，不能用来证明 B 的 Best-Fit 通过。它使用 `-kernel bin/ucore.img` 向当前系统 OpenSBI 提供内核入口。64/128/256 MiB 均应输出 `PASS`；内核自检后正常进入等待循环，脚本读取输出后结束 QEMU。

`layout` 逐一检查全部 Page 的保留标志与空闲链表计数。`boundaries` 在 GDB 中注入 8 种 DTB 内存参数，验证异常输入、上界裁剪、页对齐、零空闲页等。它们不会更改内核测试函数。

## 2. 独立 QEMU 4.1.1

构建依赖（Ubuntu）：

```bash
sudo apt install build-essential ninja-build pkg-config libglib2.0-dev libpixman-1-dev zlib1g-dev python3-venv python3-pip libslirp-dev flex bison gdb
bash tools/member_a/build_debug_qemu.sh
python3 tools/member_a/run_host_mmu.py
```

不执行 `make install`。调试二进制为 `_local/qemu-build/riscv64-softmmu/qemu-system-riscv64`，源码、构建目录和归档均位于 `_local/`，不加入 Git。Windows 挂载盘的源码解压及编译可能较慢。

宿主 GDB 根据两个真实访存地址设置条件断点，先单步 Bare，再单步 Sv39，检查软件 TLB 填充和后续取指命中。`host_mmu.py` 的断点及变量名对应 QEMU 4.1.1，不保证适用于其他版本。它不会通过 GDB 人工调用地址翻译函数。

用调试版 QEMU 重复客户机指令验证：

```bash
LAB2_QEMU="$PWD/_local/qemu-build/riscv64-softmmu/qemu-system-riscv64" \
LAB2_QEMU_BIOS_DIR="$PWD/_local/qemu-4.1.1/pc-bios" \
python3 tools/member_a/capture_guest.py tools/member_a/guest_memory.gdb guest-memory-qemu411
```

当前构建的两个目标地址为 `0x80206000`（启动期存 hartid）与 `0xffffffffc0206018`（BSS 首字节写）。若修改源码、链接布局或编译器导致地址变化，先重跑客户机脚本，使用实际输出设置 `LAB2_BARE_ADDR` / `LAB2_VIRTUAL_ADDR`，不要照抄旧地址。`guest_memory.gdb` 中指令步数也需对照新反汇编核对。

## 3. 给 B、C 的集成边界

A 的内核改动为 `pmm.c/page_init` 和 `entry.S` 的启动兼容修复，管理器选择仍是 First-Fit。B 在完成 Best-Fit 后切换管理器并运行统一评分；C 独立接入和验证 Buddy。Page 布局、分配器接口、原 First-Fit/Best-Fit 检查函数没有修改。

原启动页表只映射高地址，启用分页后的低 PC 会发生取指页故障。新增临时 `stvec` 将其引到高地址 `paging_enabled`，并显式 `jr t2` 兼容旧模拟器继续执行已翻译的低地址指令块。只在高地址入口刷新 TLB、清理临时向量并进入 C。异常路径依赖 OpenSBI 将指令页故障委托给 S 模式；调试证据及分析在本地 A 报告中。
