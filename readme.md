# F407 舵轮底盘工程

本工程用于当前机甲车的 STM32F407 舵轮底盘，配套云台为 STM32H723 步兵云台。车型配置已收敛至这台车，功能判断以当前编译配置、任务入口和实际调用为准。

## 工程位置

- 底盘 Keil 工程：`MDK-ARM/standard_tpye_c.uvprojx`，Target 为 `standard_tpye_c`。
- 配套云台：`D:\文档\Printk\bubing\cloud_platform\H7_new\H7_new\DM-balanceV1`。
- 云台 Keil 工程：`MDK-ARM/CtrlBoard-H7_IMU.uvprojx`，Target 为 `CtrlBoard-H7_IMU`。
- 编译器：ARM Compiler 5.06 update 7（build 960）；两端工程配置使用 Keil DFP 3.1.1。
- 底盘 CubeMX 配置：`standard_tpye_c.ioc`，记录的 F4 固件包为 V1.28.3。

## 当前保留功能

底盘控制四个驱动轮和四个舵向电机，使用惯导、遥控输入及板间状态实现当前底盘控制。保留 BMI088、AHRS、IMU 温控、裁判解析与 UI、PM01 响应、LED、servo 和现有 USB 初始化。IST8310 仍参与初始化，磁力计运行读取已注释。

底盘启动校准加载器继续读取既有 HEAD 和 GYR 数据及其布局。旧本地云台、发射、自瞄和交互校准写入链已移除；Yaw/Pitch、发射、自瞄和云台校准由配套 H723 工程承担。

PM01 的 CAN 接收及 UI 电压显示仍有实际调用。

## 当前底盘任务

| 入口 | 优先级 | osThreadDef 栈参数 | 职责 |
| --- | --- | ---: | --- |
| `message_task` | Normal | 128 | 板间状态发送 |
| `INS_task` | Realtime | 1024 | 惯导与 IMU 温控 |
| `led_RGB_flow_task` | Normal | 128 | LED |
| `chassis_task` | High | 1024 | 舵轮底盘控制 |
| `referee_usart_task` | Normal | 128 | 裁判数据解析 |
| `servo_task` | Normal | 128 | 舵机 PWM 输出 |
| `UI_task` | Normal | 512 | 裁判 UI |

FreeRTOS 堆配置为 24576 字节。任务创建及生成方式见结构优化执行记录。

## 源码结构

| 目录 | 内容 |
| --- | --- |
| `application/` | 业务、任务、控制和通信 |
| `bsp/` | 板级 I/O 接口 |
| `components/` | 算法、控制器、器件和支持库 |
| `Src/`、`Inc/` | CubeMX 生成代码及用户区 |
| `Drivers/`、`Middlewares/` | HAL、CMSIS、FreeRTOS、USB |
| `UI/` | 裁判 UI 业务 |
| `MDK-ARM/` | Keil 工程、启动文件、调试配置及构建输出 |
| `doc/` | 工程技术资料和历史参考素材 |
| `AI_Workspace/` | 调查、执行记录、工具、恢复快照、验证证据和缓存 |

`MDK-ARM/standard_tpye_c/` 是可重新生成的输出目录。调试配置、工程文件、启动文件与 RTE 按工程依赖保留。

## 编译与维护

在底盘根目录运行：

```powershell
.\AI_Workspace\tools\build.ps1 -Board both
```

单端可用 `-Board chassis` 或 `-Board gimbal`。工程位置与工具路径统一配置于 `AI_Workspace/tools/projects.json`。用户已授权自主使用 `D:\keil5\UV4\UV4.exe` 编译；脚本不执行烧录或实体设备操作。

- [AI 工作资料与恢复入口](AI_Workspace/README.md)
- [结构优化执行记录](AI_Workspace/reports/工程结构优化执行记录.md)
- [编译、快照和验证工具说明](AI_Workspace/tools/README.md)
- [第一批代码简化结果](AI_Workspace/reports/simplification/batch-01.md)
- [工程技术资料](doc/README.md)

## 板间接口与验证边界

两端 USART1 按固定帧交换状态：底盘至云台 28 字节，云台至底盘 46 字节。后者保留 19 字节 VTM 有效载荷与 7 字节预留区。修改帧、模式枚举或校准存储布局时，需要独立评估两端兼容性。

已有编译、函数比较和 42 项帧/校准模拟检查见执行报告。云台接收长度校验、离线检测链路及既有警告修复分别记录在后续事项中；实体设备测试按用户安排执行。

原工程说明保存在 [历史资料](AI_Workspace/reports/history/README.md)，许可证文件 `license.txt` 保留。
