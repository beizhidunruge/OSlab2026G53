# Lab3 编译与运行

在 Linux 或 WSL 的本目录执行：

```bash
make clean
make
make qemu
make grade
```

正常启动应完成内存自检，输出 10 行 `100 ticks` 后自动退出。评分预期为 `100/100`。手动退出 QEMU：先按 Ctrl+A，再按 X。`make grade` 会清理普通构建产物；之后执行 `make qemu` 会自动重新构建。

Windows 本机入口：

```powershell
wsl -d Ubuntu-24.04
```

```bash
cd /mnt/d/操作系统/OSlab2026G53/lab3/code
make qemu
```

## 新版环境启动修复

2026-11-07 在 Ubuntu-24.04、GCC 13.2.0、QEMU 8.2.2、OpenSBI 1.3 上复现并修复：

1. 仅用 `-device loader` 加载镜像时，OpenSBI 日志显示 `Domain0 Next Address : 0x0`，无法进入内核。普通运行、调试和评分统一改用 `-kernel`，正确传入内核入口 `0x80200000`。
2. 初始页表仅映射内核高地址，开启 Sv39 后当前低地址 PC 无映射。在开启分页前将 `stvec` 指向四字节对齐的高地址启动标签，通过首次取指页故障进入有效映射，随后刷新 TLB 并继续初始化。正常 Trap 入口仍由后续 `idt_init()` 设置。
3. `tools/grade.sh` 固定为 LF 换行，避免 Windows 检出导致 shell 的 `Illegal option` 错误。

本机验证：干净构建通过；普通启动内存自检通过，10 次时钟输出后正常退出（退出码 0）；官方三个测试分别为 35/35、25/25、40/40，总分 100/100。评分仅调整启动参数，未修改测试用例或判分条件。修复后的旧版 QEMU 4.1.1 环境尚未重新验证。
