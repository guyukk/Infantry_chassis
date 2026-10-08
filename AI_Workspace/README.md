# AI 工作资料与维护入口

本目录集中保存底盘与云台的调查、实施方案、执行记录、工具及恢复依据。当前工程为 F407 舵轮底盘与 H723 步兵云台，代码行为以实际编译配置和运行入口为准。

## 当前资料

- [工程结构优化实施方案](reports/工程结构优化实施方案.md)
- [工程结构优化执行记录](reports/工程结构优化执行记录.md)
- [第一批代码简化结果](reports/simplification/batch-01.md)
- [简化前置调查](reports/simplification/README.md)：历史基线与当时的候选分析。
- [历史维护资料](reports/history/README.md)：原 `Project_Maintainer` 记录。
- [工具说明](tools/README.md)

## 目录用途

| 目录 | 用途 | 保留方式 |
| --- | --- | --- |
| `reports/` | 方案、当前执行记录、历史调查 | 文本纳入 Git；注明适用阶段 |
| `tools/` | 编译、快照、审计和验证脚本 | 纳入 Git；工程路径统一由 `projects.json` 配置 |
| `snapshots/` | 源码 ZIP、哈希、固件、编译及审计证据组成的完整阶段包 | 本地保留，禁止覆盖旧包；大文件不纳入 Git |
| `artifacts/` | 已完成的验证结果、原始日志和迁移清单 | 保留重要证据；大型镜像和日志目录按规则忽略 |
| `cache/` | 解压副本、临时编译输出、Python 依赖与 CubeMX 验证副本 | 可重新生成，全部忽略 |

Git 中的索引与报告不能替代本地恢复 ZIP。恢复时使用阶段包中的校验清单，选择需要恢复的文件，保留期间新增的工作。

## 阶段包

- `snapshots/baseline-2026-10-08/`：首次简化前基线。
- `snapshots/before-batch-01/`：第一批源码删减前恢复包。
- `snapshots/after-batch-01/`：第一批删减及修复后的构建证据。
- `snapshots/before-structure-01/`：结构整理前最新工作区、构建证据、原始工具和文档备份。
- `snapshots/after-structure-01/`：结构整理后的阶段包，完成验收后生成。

原阶段包内记录的旧路径是历史来源。当前存放位置见 `artifacts/structure-01/migration-manifest.json`；迁移时已核对文件哈希。

## 常用命令

在底盘根目录运行：

```powershell
# 两端全量重编译；自动建立新的时间戳日志目录。
.\AI_Workspace\tools\build.ps1 -Board both

# 新阶段快照，已存在目录会拒绝覆盖。
& 'C:\Program Files\Python39\python.exe' -B `
  '.\AI_Workspace\tools\snapshot.py' capture `
  '.\AI_Workspace\snapshots\before-next-batch'

# 验证旧快照，并将本次比较结果写到旧阶段包之外。
& 'C:\Program Files\Python39\python.exe' -B `
  '.\AI_Workspace\tools\snapshot.py' verify `
  '.\AI_Workspace\snapshots\before-structure-01' `
  --result '.\AI_Workspace\artifacts\structure-01\verification.json'
```

`verify` 返回非零值可能表示当前源码与所选快照存在预期差异，应查看结果明细。ZIP 与清单哈希不符会作为完整性错误处理。

Keil 编译已获授权；这些脚本不执行烧录或实体设备操作。
