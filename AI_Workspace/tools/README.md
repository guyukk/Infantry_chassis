# 工具使用说明

`projects.json` 集中设置 Keil、CubeMX、两端工程相对位置和 Target。脚本依据底盘 Keil 工程标识定位根目录，运行时工作目录不会改变工程选择。云台位置相对于底盘根目录解析；工作目录结构发生变化时更新此配置。

## 编译、快照和审计

- `build.ps1 -Board both`：两端全量重编译；`chassis`、`gimbal` 可选择单端。日志默认位于 `AI_Workspace/artifacts/logs/build-时间戳/`。
- `build.ps1 -LogDirectory <阶段包>/build`：向阶段包保存编译结果；已有同名日志时拒绝覆盖。
- `snapshot.py capture <阶段包>`：保存两端源码、库、工程配置、`.ioc`、`.mxproject` 和约定及其哈希；排除 AI 目录和 Keil 输出。目标目录必须尚不存在。
- `snapshot.py verify <阶段包> --result <新结果路径>`：检查 ZIP 与清单完整性，并列出当前文件差异。比较旧包时使用 `--result` 保留历史证据。
- `audit.py <阶段包>`：读取包内源码 ZIP 和 `build/results.json`，索引当前源码并保存新构建的固件、MAP、容量与警告。产物路径依据 `.uvprojx` 解析。对新阶段包执行，避免覆盖旧证据。

## 既有验证

Python 依赖位于 `../cache/dependencies/packages/`，为 Unicorn 2.1.4 和 pyelftools 0.32。重建方式：

```powershell
& 'C:\Program Files\Python39\python.exe' -m pip install `
  --target '.\AI_Workspace\cache\dependencies\packages' `
  'unicorn==2.1.4' 'pyelftools==0.32'
```

从底盘根目录运行：

```powershell
# 第一批源码简化的函数比较，保留该批已经复查的差异许可。
& 'C:\Program Files\Python39\python.exe' -B `
  '.\AI_Workspace\tools\validate_invariants.py' `
  --baseline '.\AI_Workspace\snapshots\before-batch-01' `
  --results '.\AI_Workspace\artifacts\validation\rerun-batch-01'

# 与其他阶段比较时不沿用第一批的差异许可。
& 'C:\Program Files\Python39\python.exe' -B `
  '.\AI_Workspace\tools\validate_invariants.py' `
  --baseline '.\AI_Workspace\snapshots\before-structure-01' `
  --results '.\AI_Workspace\artifacts\validation\structure-01'

# 运行实际 ARMCC 编译的板间帧和校准加载检查。
& 'C:\Program Files\Python39\python.exe' -B `
  '.\AI_Workspace\tools\validate_runtime.py' `
  --results '.\AI_Workspace\artifacts\validation\structure-01'
```

两个验证脚本均支持 `--cache`，用于指定临时材料目录。默认缓存为 `../cache/validation/current/`，默认结果为 `../artifacts/validation/latest/`。历史 batch-01 结果位于 `../artifacts/validation/batch-01/`，复跑结果使用新目录。

函数比较关注保留函数的预处理后函数体；运行验证使用真实帧处理函数、结构体布局及校准加载器，但板级 I/O 使用替身。两者不模拟实际外设、RTOS 调度或电机。
