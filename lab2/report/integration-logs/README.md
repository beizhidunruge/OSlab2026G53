# 2026-10-25 最终代码集成验证

这些结果来自合入 C 的 Buddy 后，使用同一源树的独立副本构建并实际运行 QEMU。当前源码默认 Best-Fit，`LAB2_PMM_BUDDY` 可选择 Buddy，未修改课程测试或评分脚本。

| 项目 | 结果 | 日志 |
| --- | --- | --- |
| 默认 Best-Fit 编译与自检 | PASS | [构建](best-fit-build.txt)、[运行](best-fit-qemu.txt) |
| 原有 make grade | 20/20 | [评分](best-fit-grade.txt) |
| Buddy 编译与自检 | PASS | [构建](buddy-build.txt)、[运行](buddy-qemu.txt) |
| 非法释放模式 1：容量过小 | 命中预期块长断言 | [构建](buddy-illegal-1-build.txt)、[运行](buddy-illegal-1-qemu.txt) |
| 非法释放模式 2：容量过大 | 命中预期块长断言 | [构建](buddy-illegal-2-build.txt)、[运行](buddy-illegal-2-qemu.txt) |
| 非法释放模式 3：起点未对齐 | 命中预期对齐断言 | [构建](buddy-illegal-3-build.txt)、[运行](buddy-illegal-3-qemu.txt) |

本次环境见 [environment.txt](environment.txt)，汇总见 [summary.json](summary.json)，所测源码摘要见 [source-sha256.json](source-sha256.json)。构建工作目录为 `lab2/_local/integration-20261025/`，评分临时目录也指定在其下，均不加入提交。

正常内核在检查后进入等待循环，采集进程在 5 秒后结束 QEMU，所以运行日志末尾可能有 SIGTERM 退出信息。非法配置先命中指定断言后停机，采集器随后同样结束模拟器；没有把超时或任意 panic 当作通过。

本轮未重新编译调试版 QEMU 或重跑 A 的全部 GDB/边界场景；这些历史证据保留于 `../member-a/logs/`。20/20 对应课程现有初始化和 Best-Fit 两项评分，Buddy 依据自己的自检与负向模式独立验收。
