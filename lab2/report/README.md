# Lab2 报告资料索引

本目录保存按模板整合的最终报告、三人原始材料与提示词，以及 2026-10-25 合并后代码的独立验证日志。姓名、部分环境/AI 配置及最终日期等缺失信息在报告中以 `[待填写]` 标出。

## 当前材料

| 内容 | 文件 |
| --- | --- |
| 最终小组报告（按实验报告模板） | [实验报告.md](实验报告.md) |
| 最终代码配置与运行命令 | [代码说明](../README.md) |
| B：First-Fit 理解、Best-Fit 实现及运行结果 | [成员 B 报告](成员B_2412591_First-Fit与Best-Fit报告.md) |
| A/B/C 提示词与规格汇编 | [prompt.md](prompt.md) |
| B 原始四段式提示词 | [prompt_best_fit.md](prompt_best_fit.md) |
| A 原始规格与真实 AI 协作记录 | [spec-and-prompts.md](member-a/spec-and-prompts.md) |
| A：物理内存初始化 | [初始化报告](member-a/初始化报告.md) |
| A：页表、MMU、TLB 调试 | [MMU 调试报告](member-a/MMU调试报告.md) |
| A：交付边界、接入说明及问答 | [交付说明](member-a/交付说明.md) |
| A 材料入口及原始日志 | [成员 A 目录](member-a/README.md)、`member-a/logs/` |
| B：编译、启动自检、20/20 评分截图 | `images/01_best_fit_build_and_boot.png`、`images/02_best_fit_self_check_pass.png`、`images/03_lab2_grade_20_of_20.png` |
| C：原始报告、两份提示词与三张截图 | [成员 C 材料](member-c/README.md) |
| 本次合并代码的 Best-Fit、Buddy、非法释放验证与评分 | [集成日志说明](integration-logs/README.md)、[结果摘要](integration-logs/summary.json) |

## A 材料提取说明

来源为 `../Lab2_成员A_2414034/lab2/report/member-a/`。五份 Markdown 根据正文标题恢复为可读文件名，41 份日志按原名、原内容复制。报告中的复现工具链接已调整到原解压包，原包及主工程均保持不变；没有把工具、源码快照、Git bundle 或补丁重新合入主工程。

A 的正文记录的是其交付时的 First-Fit 基线、初始化边界测试和 MMU 调试。文中“Best-Fit 尚未实现”等表述保留为历史状态，当前 B 已完成 Best-Fit 并保存自检及评分截图；两者并不冲突。原始复现命令应结合 A 原包说明使用，不能因报告被提取就认定当前主工程具有同名工具路径。

A 的提示词文件包含用户原始任务摘要、助手实施规格和实际修改轨迹，本次按来源汇编，未替 A 补造一份此前未使用的标准模板。B 的原提示词也单独留存，便于区分模块生成与后续接入操作。

## 集成状态与待补内容

- 已将 C 的 `origin/lab-l`（`94403d6`）按 Lab2 范围合入当前 `He`；保留原有 `code/` 结构和 A/B 实现，默认 Best-Fit，可用编译开关选择 Buddy。
- 默认自检、课程评分 20/20、Buddy 正常检查及三种非法释放预期断言均已在合并代码副本上验证。A 的历史 GDB 和边界日志保留来源，未声称本轮全部重跑。
- A/B 独立章节按 2026-10-17/21 的开发状态保留；C 原报告中的模板占位和缺图说明也作为原始材料保留。最终正文已去除过时占位，采用实际存在的六张成员截图与本次日志。
- 组员补齐个人姓名、AI 工具/模型、缺少的环境信息和最终完成日期后，复核最终报告即可。SLUB 未实现，不列为已完成选做。
