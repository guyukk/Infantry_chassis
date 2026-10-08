# 当前机甲车简化执行记录

日期：2026-10-08。保留 F407 舵轮底盘和 H723 步兵云台；判定依据为实际 Keil 配置、预处理后的代码、调用关系和链接结果。

## 本批结果

| 工程 | 删除文件 | Keil 工程条目 | 全量编译 | Code + RO-data | RW-data + ZI-data |
| --- | ---: | --- | --- | --- | --- |
| 底盘 | 33 | 125 → 111 | 0 错误，0 警告 | 87444 → 83748 字节，减少 3696 | 62408 → 60480 字节，减少 1928 |
| 云台 | 77 | 139 → 110 | 0 错误，40 警告 | 124012 → 115908 字节，减少 8104 | 64304 → 59824 字节，减少 4480 |

共删除 110 个文件，Keil 工程条目减少 43 项。上述容量为链接统计，不等同于剩余堆空间。云台原有 42 条警告，删除 GRAVITY 和 OLED 后减少两条；按文件、诊断编号及内容逐条比较，没有新增警告。

## 删除与保留依据

底盘移除了未运行的本地云台、射击、自瞄实现及其专用接口，滑移抑制与预测限功率模块，OLED、INA226、无消费者的支撑代码，以及没有业务消费者的忙循环 idle 任务。移除了输入不可达的回正分支、只服务该分支的 PID/字段，以及未初始化看门狗的刷新调用。当前限功率函数只把 PID 输出赋给电流，已将这两组赋值保留在底盘控制循环中。

底盘启动校准改为仅加载仍有消费者的 HEAD 和 GYR 数据。保留旧扇区位置、HEAD 的 8 字节布局、GM 的 20 字节占位，以及 GYR 从偏移 36 开始的布局；有效标志仍为 0x55。保留原先的温度读取、无效 HEAD 时纬度默认 22.0，以及有效 GYR 时加载 offset/scale 的行为。删除原来没有任务入口的写入和交互校准链。

底盘 UI 中的本地云台、射击和自瞄变量没有活跃写入者，原有显示结果保留为固定占位字符串。机器人 ID、底盘模式、HP、PM01 电压等实际更新内容仍保留。四个驱动轮、四个舵向电机、遥控合法性判断、IMU、裁判/UI、PM01 响应、LED、servo 和 USB 初始化仍保留。

云台删除其他车型参数、其他底盘/云台/发射机构实现、未运行的本地底盘与 UI/OLED/观察器任务、旧 USB 封装、备份文件、EKF/Kalman/VMC 及没有消费者的通用控制器。保留当前 yaw_pitch_direct、shoot_3508、Mahony、BMI088、板间输入、自瞄和实际运行的 Communication/example/device 链路；该目录虽名为 example，实际承担 USB、相机、云台通道、时间同步与 TFmini 业务。

Pitch 的 DM 反馈、Motor_enable、EMIT 参数及输出保留；校准仍会读取 DJI Pitch 反馈指针，因此该指针和 getter 也保留。云台校准任务与 W25Q64 读写保持原样，包括原来的 108 字节写入缓冲布局。

云台离线检测任务原本没有创建，现有 toe_is_error 接口继续保留；删除的 OLED 回调只存在于未运行的检测初始化中。没有借本次删减新增检测任务或改变其初始化行为。

## 复查中修正的两处边界

1. 两端 VTM 结构体实际为 21 字节，但原板间代码从偏移 2 复制 26 字节，发送端会读取相邻全局变量，接收端会写出结构体边界。现改为复制 19 字节；46 字节帧、其他字段偏移和帧尾保持不变，19 字节载荷之后的 7 个预留字节发送为零、接收时忽略。两端增加 ARMCC 编译期结构体大小检查。这是为避免删减改变内存布局后放大原有越界问题而进行的独立修复。
2. 原云台 INS 编译单元没有得到 ROBOT_MODE/release 定义，旧条件实际按 0 == 0 编译，温控调用一直生效。整理头文件时发现该差异，已收敛为明确保留原来的温控调用；不能仅根据 robot_param.h 的 debug 配置推断原固件没有加热。

## 验证与恢复

- 两端使用用户授权的 `D:\keil5\UV4\UV4.exe` 和原 ARMCC 5.06 update 7 全量重编译；新 AXF、HEX、MAP、日志及哈希保存在 [after-batch-01](../../snapshots/after-batch-01)。
- 对保留的本地编译单元进行原始快照/当前源码的 ARMCC 预处理对比：817 个函数体中，803 个保持相同；其余 14 个差异均对应上述经过复查的改动。Yaw/Pitch 控制、射击、IMU 温控、惯导运算、云台校准、自瞄、USB 通信等保留函数体未出现意外变化。脚本对新增的非预期函数体差异报错；此检查不证明外设或调度等价。
- 将实际四个板间收发函数、两端实际 VTM 布局及简化后的校准加载器用 ARMCC 编译，在 Unicorn 的 Cortex-M/Thumb 模拟环境执行：42 项检查通过，包括两种帧的浮点字段/模式/ID、19 字节载荷、7 字节预留区、两侧内存哨兵、旧校准有效和无效标志读取。板级 I/O 与所属控制对象使用测试替身；未模拟 STM32 外设、中断、RTOS 或电机。
- 验证脚本为 [validate_invariants.py](../../tools/validate_invariants.py)、[validate_runtime.py](../../tools/validate_runtime.py)。结果位于 `AI_Workspace/artifacts/validation/batch-01/function-comparison.json` 和 `AI_Workspace/artifacts/validation/batch-01/runtime-results.json`。运行时依赖仅安装在 `AI_Workspace/cache/dependencies/packages`，版本为 Unicorn 2.1.4、pyelftools 0.32。
- 删除前可恢复源码为 [before-batch-01](../../snapshots/before-batch-01) 的两端 ZIP 与 SHA256 清单，原始已验证固件仍在 [baseline-2026-10-08](../../snapshots/baseline-2026-10-08)。云台目录没有 Git，恢复应从 ZIP 选择所需文件，避免覆盖用户后续改动。
- 当前更改清单位于 `AI_Workspace/artifacts/validation/batch-01/change-summary.json`，完整删除清单位于 `AI_Workspace/artifacts/validation/batch-01/chassis-removed-files.txt` 和 `AI_Workspace/artifacts/validation/batch-01/gimbal-removed-files.txt`。删减后的源码快照与当前源码只差 CAN_receive.c 文件末尾空行整理，已在 `after-batch-01/verification.json` 记录；ZIP 完整性通过。

未执行烧录或实体设备操作。仍需上机确认 idle 任务删除后的调度、遥控/键鼠、静止/跟随/小陀螺、舵向换向、Yaw/Pitch、摩擦轮/拨弹、板间通信、自瞄、裁判/UI、测距与校准持久化。原先已记录的云台帧接收长度检查、离线检测任务缺失及剩余 40 条警告，仍属于后续单独处理的事项。

本批没有批量裁剪厂商 HAL、FreeRTOS 和 USB 中间件，也没有删除已运行且硬件用途尚未明确的辅助输出。
