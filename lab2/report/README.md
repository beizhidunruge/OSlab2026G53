# Lab2 报告资料索引

本目录当前保存成员 A、B 的独立材料与提示词，供后续整合使用。成员 C 材料尚未收到，本轮未生成最终小组报告，也未修改现有代码或目录结构。

## 当前材料

| 内容 | 文件 |
| --- | --- |
| B：First-Fit 理解、Best-Fit 实现及运行结果 | [成员 B 报告](成员B_2412591_First-Fit与Best-Fit报告.md) |
| A/B 提示词与规格汇编 | [prompt.md](prompt.md) |
| B 原始四段式提示词 | [prompt_best_fit.md](prompt_best_fit.md) |
| A 原始规格与真实 AI 协作记录 | [spec-and-prompts.md](member-a/spec-and-prompts.md) |
| A：物理内存初始化 | [初始化报告](member-a/初始化报告.md) |
| A：页表、MMU、TLB 调试 | [MMU 调试报告](member-a/MMU调试报告.md) |
| A：交付边界、接入说明及问答 | [交付说明](member-a/交付说明.md) |
| A 材料入口及原始日志 | [成员 A 目录](member-a/README.md)、`member-a/logs/` |
| B：编译、启动自检、20/20 评分截图 | `images/01_best_fit_build_and_boot.png`、`images/02_best_fit_self_check_pass.png`、`images/03_lab2_grade_20_of_20.png` |

## A 材料提取说明

来源为 `../Lab2_成员A_2414034/lab2/report/member-a/`。五份 Markdown 根据正文标题恢复为可读文件名，41 份日志按原名、原内容复制。报告中的复现工具链接已调整到原解压包，原包及主工程均保持不变；没有把工具、源码快照、Git bundle 或补丁重新合入主工程。

A 的正文记录的是其交付时的 First-Fit 基线、初始化边界测试和 MMU 调试。文中“Best-Fit 尚未实现”等表述保留为历史状态，当前 B 已完成 Best-Fit 并保存自检及评分截图；两者并不冲突。原始复现命令应结合 A 原包说明使用，不能因报告被提取就认定当前主工程具有同名工具路径。

A 的提示词文件包含用户原始任务摘要、助手实施规格和实际修改轨迹，本次按来源汇编，未替 A 补造一份此前未使用的标准模板。B 的原提示词也单独留存，便于区分模块生成与后续接入操作。

## 后续整合事项

- 收到 C 的 Buddy 实现、两份 prompt、报告与实际测试证据后再整合。
- 分别核对初始化、Best-Fit、Buddy 和 GDB 的证据，不把现有评分 20/20 解释为所有任务均已验收。
- 三人核对各自章节后，统一全篇结构、术语、图片和路径，再提交最终报告。
