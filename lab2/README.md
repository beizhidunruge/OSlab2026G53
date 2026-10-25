# Lab2：物理内存和页表

最终源码位于 `code/`，默认运行 Best-Fit。成员 C 的 Buddy System 与独立自检已合入同一工程；公共数据结构、课程检查函数、A 的初始化及启动修复、B 的 Best-Fit 算法均保留。

## 构建与运行

```bash
cd /home/oslab/Lab/lab2/code
make clean
make qemu
```

看到 `memory management: best_fit_pmm_manager` 和 `check_alloc_page() succeeded!` 表示默认自检已返回。按 Ctrl+A，再按 X 退出 QEMU，运行课程评分：

```bash
make grade
```

## Buddy System 验证

```bash
make clean
make DEFS=-DLAB2_PMM_BUDDY qemu
```

此配置选择 `buddy_pmm_manager`，调用 `buddy_check()`。Buddy 申请 n 页会向上取整成二次幂容量；释放必须从分配块首页归还整个实际容量，不支持任意部分释放。不同 n 只要取整容量相同可以释放同一块。

三种非法释放分别单独启动，预期命中对应断言并停止内核：

```bash
make clean
make DEFS='-DLAB2_PMM_BUDDY -DBUDDY_ILLEGAL_FREE_TEST=1' qemu
```

模式 1、2、3 分别为释放容量过小、过大、起点不对齐。退出后先 `make clean` 再换模式。完成后再次 `make clean` 并运行普通 `make qemu`，即恢复默认 Best-Fit。不同配置之间必须清理；现有 Makefile 不追踪编译标志变化。

## 报告与集成来源

- [最终实验报告](report/实验报告.md)：按小组模板编写，少量未知身份、环境、工具/模型信息留待填写。
- [三人提示词汇编](report/prompt.md)、[材料索引](report/README.md)、[本次集成验证](report/integration-logs/README.md)。
- C 来源为 `origin/lab-l` 提交 `94403d6`。该分支与 He 无共同祖先且目录不同，合并保留当前树，仅导入 Lab2 Buddy 文件和 C 报告材料，未覆盖 Lab1 或现有 A/B 代码。

2026-10-25 已实际验证默认 Best-Fit 自检、课程评分 20/20、Buddy 正常自检和三种预期非法释放断言。SLUB 属于选做，当前未实现。
