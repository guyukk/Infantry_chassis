# 当前机甲车代码简化：前置准备

本文件记录简化前的调查与基线。已完成的源码删减、最终编译结果及验证边界见 [batch-01.md](batch-01.md)。其中图传越界复制已修正为 19 字节有效载荷加 7 字节预留区；下文协议表保留的是原代码行为。

日期：2026-10-08。范围已由用户确认：只保留当前 F407 舵轮底盘与 H723 步兵云台，允许移除其他车型、底盘和云台方案。本轮只建立基线和清单，没有删除功能代码、修改控制参数或烧录设备。

## 1. 编译基线与恢复依据

| 工程 | Keil Target | 全量重编译 | Code | RO-data | RW-data | ZI-data |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| 底盘 | standard_tpye_c | 0 错误，0 警告 | 82692 | 4752 | 1472 | 60936 |
| 云台 | CtrlBoard-H7_IMU | 0 错误，42 警告 | 119588 | 4424 | 1380 | 62924 |

大小单位为字节，是本轮 Keil 日志中的原始字段；不直接等同于压缩后的 ROM 占用或运行时剩余堆空间。两端使用 ARM Compiler 5.06 update 7（build 960），编译工具为用户授权的 `D:\keil5\UV4\UV4.exe`。云台进程退出码 1 对应有警告的成功编译，不能简单按非零退出码判定失败。

基线目录为 `baseline-2026-10-08`：

- `chassis-sources.zip`、`gimbal-sources.zip`：编译前的源码、库文件及工程配置快照，分别包含 326 和 1469 个文件；排除 Git、主机示例 build 目录和本准备目录。不是整个工作目录的备份。
- `*-sha256.json`：快照内每个文件的校验值。
- `baseline.json`、`*-keil-files.csv`：工程配置和 Keil 显式文件清单；两端分别有 125 和 139 项，包含源文件、头文件、库和启动文件，均存在。
- `build/*.log`、`build/results.json`：本轮全量重编译日志与进程结果。
- `build-metrics.json`、`*.axf`、`*.hex`、`*.map`：确认新产物生成后的大小、哈希和链接证据。
- `*-warnings.csv`：警告明细，后续用于识别新增警告，不能只比较总数。
- `*-local-files.csv`、`*-includes.csv`：本地 C/H 文件与去除注释后的 include 索引。
- `*-linker-removed.csv`、`*-object-sizes.csv`：链接器移除段和对象实际占用。
- `verification.json`、`*-rte-build-change.diff`：编译前后文件差异与 Keil 自动生成文件变化。

底盘初始 Git HEAD 为 `8b02a24dff0b66f5bd625a493073315e331d6d9e`，初始未跟踪项只有此前新增的 `AGENTS.md`；云台目录没有 Git，因此两端均保留独立源码快照。恢复时先选择需要恢复的文件，再与 SHA256 清单比较，不覆盖用户后续新增的工作。

本轮 Keil 自动新增底盘 `.sct`，并重生成云台 `RTE_Components.h`（包括移除 `CMSIS_device_header` 定义）；前后版本已保存。这些生成文件变化与手工修改分开记录。两个 `AGENTS.md` 仅增加用户确认的简化范围和本报告位置。

## 2. 当前保留边界

| 部分 | 当前真实入口与作用 | 先保留的依赖 |
| --- | --- | --- |
| 底盘控制 | `Src/freertos.c` 创建 `chassis_task`；控制四个 3508 驱动轮、四个 6020 舵向电机；循环实际 `osDelay(1)` | `chassis_task`、`chassis_behaviour`、`chassis_calculate`、PID、CAN 收发、遥控输入及合法性判断 |
| 底盘惯导 | 创建 `INS_task`；BMI088 + AHRS；初始化还调用 IST8310，磁力计运行读取已注释 | IMU SPI/DMA/中断、姿态数据、温控与启动校准参数 |
| 底盘通信 | `message_task` 经 USART1 发送状态；串口中断解析云台帧；USART6 裁判解析 + UI | 板间协议、DMA 接收、裁判 FIFO/CRC/结构体和 UI 接口 |
| 底盘辅助 | 当前还创建 LED、servo、idle 任务 | 是否删除按候选清单处理；不能把它们误记为未启动 |
| 云台控制 | `ROBOT_BOARD=gimbal_board`，实际创建 `gimbal_task`；同一循环执行 `shoot_control_loop` | `yaw_pitch_direct.c`、`gimbal_behaviour`、`shoot.c`、`shoot_3508.c`、电机反馈与 CAN 输出 |
| 云台电机 | Yaw 使用 DJI 接口；Pitch 从 `can_bsp.c` 获取 DM 反馈并通过 EMIT 发送位置/速度/电流限制 | Pitch DM 初始化使能、ID、零点和 EMIT 参数；不能因旧独立 DM 驱动未编入就删除这些活跃接口 |
| 云台惯导/校准 | 创建 `INS_task`、`calibrate_task`；惯导实际调用 Mahony；校准调用外部 W25Q64 读写 | BMI088、Mahony、DWT、温控、校准钩子及 OSPI/Flash |
| 云台 USB/自瞄 | defaultTask 初始化 USB；`comm_app_task` 驱动 uproto；`auto_aim_task` 处理自瞄增量 | `Communication/core`、`example/device`、gimbal/time_sync 通道、TFmini、USB 收发与发送仲裁、TIM24 时间源 |
| 云台板间输入 | `message_task` 发送云台状态；USART1 回调解析底盘状态；当前 `RC_TYPE=0` 走遥控串口 | USART1、UART5 输入、字段与枚举兼容性；图传备选输入先按数据链核对 |

云台 `ROBOT_TYPE=Infantry_robot`、`ROBOT_CHASSIS=Steering_wheel`、`ROBOT_GIMBAL=yaw_pitch_direct`、`ROBOT_FRICTION=friction_3508`、`ROBOT_MODE=debug`、`ROBOT_CAP=Cap_off`。`ROBOT_CHASSIS=Steering_wheel` 并不表示云台端的底盘任务已创建。

## 3. 板间协议冻结点

两端 USART1 当前按固定长度交换原始字节。简化时先保留长度、偏移、float 字节序及周期；任何协议调整单独成批修改两端。

| 方向 | 字节位置 | 当前内容 |
| --- | --- | --- |
| 底盘 → 云台，28 字节 | 0 / 27 | `'s'` / `'e'` |
| 同上 | 1–4 / 5–8 / 9–12 | 底盘 yaw / pitch / roll，fp32 |
| 同上 | 13 / 14 | 底盘行为模式 / 机器人 ID |
| 同上 | 15–18 / 19–22 / 23–26 | vx / vy / wz，fp32 |
| 云台 → 底盘，46 字节 | 0 / 45 | `0xFE` / `0xFD` |
| 同上 | 1–4 / 5–8 / 9–12 / 13–16 | Yaw 编码角 / 云台 yaw / pitch / roll，fp32 |
| 同上 | 17 / 18 | 云台行为模式 / 射击模式 |
| 同上 | 19–44 | 图传结构体从偏移 2 开始的 26 字节 |

证据：底盘 `application/message_task.c`、`bsp/boards/bsp_usart.c`；云台 `User/APP/message_task.c`、`User/Bsp/bsp_usart.c`。底盘发送任务循环延时 10 ms；云台为 3 ms。表中底盘行为模式不应混同于两端本地 `chassis_mode_e`。

已发现的兼容边界：底盘 `gimbal_behaviour_e` 的 MOTIONLESS/SPIN 是 5/6，云台是 6/5；两端 `chassis_mode_e` 的顺序也不同。删除枚举项会移动隐式值，必须保留线上原值或在独立协议修改中显式映射。底盘接收的行为数据目前写入 `chassis_move.gimbal_behaviour`，UI 却读取本地 `gimbal_behaviour`，不应在清理时把两者直接合并并假定原来显示正确。

校准文件按设备条目顺序、结构体长度和标志位排列。移除某条校准逻辑时，先保留存储布局和既有数据的读取语义；布局迁移单独处理。

## 4. 待删候选及先决条件

以下是待执行清单，不是“已经确认全部可直接删除”。完整候选还见 `candidates.csv`。

| 批次 | 候选 | 实际证据与删减边界 |
| --- | --- | --- |
| 1 | 云台 Hero/Balance/Sentinel/Engineer 配置；麦轮/全向/平衡底盘；联动/双轴/机械臂云台；双级/无摩擦轮方案 | 当前宏不选这些实现；同时删除工程条目、头文件引用和条件分支。先检查分支外 include 与宏定义，保留当前步兵参数 |
| 1 | 云台 `*_backup` 文件、未编入的 test/旧 USB/idle/LED/servo/profiler 源文件；底盘 INA226 | Keil 清单已列出未编入的 C 文件；仍被 include 的头文件先去除引用。`usb_common.h` 正在被主程序和自瞄 include，不能与 `.c` 一并盲删 |
| 2 | 底盘 `slip_control.c/h` 及其专用 Kalman 依赖 | 控制调用已注释；去除 include、结构体字段和没有其他消费者的算法文件后删除 |
| 2 | 底盘遗留 `auto_aim.c`、`shoot.c`、云台控制实现 | 未创建对应任务，但 `gimbal_task.o` 仍有 764 字节 Code、68 RW、632 ZI；校准函数指针、初始化 getter、UI 全局变量形成链接依赖。先保留必要协议类型，解除依赖，再去除整套实现 |
| 2 | 云台本地底盘控制、Observe/OLED/UI 模块 | 对应任务未创建，但当前 steering 实现仍有 3988 字节 Code，`chassis_task.o` 还有 1408 字节 ZI。部分实现带 `used`，不能仅按任务创建表推断固件占用。先拆出板间状态数据及被外部引用的类型 |
| 2 | 云台 EKF/Kalman/VMC 残留 | 当前姿态主循环使用 Mahony，但 INS 头文件仍 include EKF，底盘类型头仍 include VMC。核对数据类型与其他调用后再移除文件及 include 路径 |
| 3 | 底盘忙循环 `idle_task` | 当前以 Normal 优先级创建，循环没有延时；`idleTime` 没有业务消费者。可优先移除该统计任务，但删除会改变调度占用，应单独验证 |
| 3 | 底盘预测限功率、超电开关和 PM01 | 预测限功率调用已注释，`chassis_power_control` 当前只赋 PID 输出；PM01 CAN 回调和 UI 电压显示仍活跃。可先收敛无效预测代码；整套 PM01 删除另行处理当前显示链路 |
| 3 | 底盘回正遗留分支 | 模式选择处的回正入口已注释，仅余状态字段、PID 和计算分支。初步阅读时将其列为功能，此处细化为“当前常规输入不可达的遗留候选” |
| 4 | 底盘 USB、servo/LED，以及多余外设初始化 | 底盘 USB 当前接收仅重启收包，未见应用消费；servo 当前实际写 TIM1/TIM8 PWM；LED 正在运行。需确定这台车的辅助输出用途后再删除对应初始化、IRQ/DMA/MSP 和 HAL 文件 |

源码索引不执行 C 预处理，include 计数也包含未选中分支；链接映射中的交叉引用可包含随后被移除的段。最终判定须结合当前宏、活跃根、函数指针、回调、保留段和整批编译，不能仅依赖某一张表。

清理已经被链接器移除的代码主要降低维护成本，通常不会明显缩小固件。每批分别记录删除源码行数、编译文件数、ROM/RAM 变化；不预先承诺容量收益。

## 5. 与删减分开处理的现有问题

| 现状 | 证据 | 处理方式 |
| --- | --- | --- |
| 云台离线检测接口仍被控制调用，检测任务没有创建 | `freertos.c` 无 detect_task 创建；`toe_is_error` 只读 `error_list`；初始化在未运行的 detect_task 中 | 不把“调用了检查”认作保护有效；后续单独决定建立检测链路或收敛失效逻辑 |
| 云台 USART1 帧解析缺少完整的长度/边界验证 | `gimbal_memcpy_data_from_chassis` 找到帧头后按固定长度复制；回调 Size 未用于完整帧校验 | 通信修复单独成批，避免与删除文件同时改变行为 |
| 云台 42 条已有警告 | 包括隐式函数声明、返回类型、无效 packed 属性、未使用变量/函数、文件末尾换行及不可达初始化 | 已保存逐条基线；删减中不增加新诊断，影响布局/调用接口的修复单独评估 |
| 底盘看门狗初始化已注释但主循环仍 Refresh | `Src/main.c` 和 `application/chassis_task.c` | 作为死链候选记录，不能用该调用证明看门狗正在保护系统 |
| 底盘回正入口、UI 状态落点、模式枚举存在历史不一致 | 第 3、4 节 | 明确当前行为，功能修复不混进纯简化批次 |

## 6. 执行顺序和每批验收

1. 固定当前步兵/直驱云台/3508 发射配置，删除其他方案和无依赖的未编译备份，同步 Keil 工程清单及 include 路径。
2. 解除底盘遗留云台/发射/UI 依赖；解除云台本地底盘、观察器和备选算法依赖。保留板间数据类型与校准存储布局。
3. 删除不承担当前业务的任务和不可达功能块，分别检查任务周期、优先级、输出行为和保护逻辑。
4. 最后收敛外设、HAL、中间件和调试工具；根据实际 IRQ/DMA/回调链判断，避免先扫空 Drivers 或 Middlewares。

每批只处理一组依赖：核对变更清单 → 全量重编译受影响端（涉及共享字段时两端）→ 比较新增警告与 map → 记录源码/固件变化 → 保留可恢复版本。纯结构简化先保持参数、符号单位、控制周期、协议、使能和故障输出语义。

编译只能证明当前配置能构建。改变任务/输出/通信后的上机验证清单已准备：遥控与键鼠输入、静止/跟随/小陀螺、舵向换向、Yaw/Pitch 动作、摩擦轮/拨弹/舱门、自瞄开关与超时、板间状态、裁判/UI、测距/时间戳、校准持久化。后续确需验证时由用户安排实体设备操作；本轮没有执行这些检查。

## 7. 当前工具与存放位置

本报告是简化前置调查，历史基线和当时的运行描述保留。结构整理后的工具说明见 [工具入口](../../tools/README.md)，当前执行状态见 [集中索引](../../README.md)。

阶段包已整体迁移到 `AI_Workspace/snapshots/`，包括 `baseline-2026-10-08`、`before-batch-01`、`after-batch-01`。历史结果保存在 `AI_Workspace/artifacts/validation/batch-01/`；原始 ZIP 和校验清单保持不变。旧路径与新位置的关系见 `AI_Workspace/artifacts/structure-01/migration-manifest.json`。
