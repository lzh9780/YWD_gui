# YWD CAN-FD 智能电机控制协议 · 聚合帧章节

> 版本: V0.1 (草案)
> 日期: 2026-08-26
> 适用范围: HPM5100 3 轴电机驱动器 (MOTOR_NUM=3)
> 上位机: TPHAND (winformCtrlapp/TPHAND)
> 下位机: mrDriverHpm5100_3AXISMAIN

本文档专门描述 **聚合帧** 的发送与接收，覆盖下行聚合控制帧 (0x001/0x002/0x003) 与上行聚合反馈帧 (0x701/0x702/0x703)。其他章节 (单电机控制帧 0x100/0x180/0x200、参数读写 0x480/0x680、系统命令 0x400、反馈帧 0x600) 见协议主文档。

---

## 1. 概述

聚合帧用于一帧报文控制 / 回传多台电机，减少总线占用。3 轴电机驱动器单帧最多携带 4 条记录 (YWD_AGG_MAX_RECORD=4)。

| 方向 | CAN ID | 用途 |
|------|--------|------|
| H → D 下行 | 0x001 | MIT 阻抗组多电机控制 (Body 12B) |
| H → D 下行 | 0x002 | 位置速度组多电机控制 (Body 10B) |
| H → D 下行 | 0x003 | 恒速组多电机控制 (Body 6B) |
| D → H 上行 | 0x701 | MIT 组多电机聚合反馈 (Body 16B + 末尾 6B CRC) |
| D → H 上行 | 0x702 | 位置速度组多电机聚合反馈 |
| D → H 上行 | 0x703 | 恒速组多电机聚合反馈 |

下行广播帧不带目标节点号 (CAN ID 固定)，节点号在帧内每条记录中携带；上行反馈帧 CAN ID 与下行对称 (0x001 ↔ 0x701 …)。

---

## 2. 公共定义

### 2.1 CAN ID 宏

| 宏 | 值 | 说明 |
|----|----|----|
| YWD_ID_AGG_MIT         | 0x001 | 下行 MIT 组聚合控制帧 |
| YWD_ID_AGG_POSVEL      | 0x002 | 下行 位置速度组聚合控制帧 |
| YWD_ID_AGG_CONSTVEL    | 0x003 | 下行 恒速组聚合控制帧 |
| YWD_ID_AGG_FB_RSP_MIT     | 0x701 | 上行 MIT 组聚合反馈帧 |
| YWD_ID_AGG_FB_RSP_POSVEL | 0x702 | 上行 位置速度组聚合反馈帧 |
| YWD_ID_AGG_FB_RSP_CVEL    | 0x703 | 上行 恒速组聚合反馈帧 |

### 2.2 长度 / 容量宏

| 宏 | 值 | 说明 |
|----|----|----|
| YWD_AGG_MAX_RECORD        | 4 | 单帧最大记录数 |
| YWD_AGG_BODY_LEN_MIT      | 12 | MIT 单条 Body 长度 |
| YWD_AGG_BODY_LEN_POSVEL   | 10 | 位置速度 单条 Body 长度 |
| YWD_AGG_BODY_LEN_CVEL     | 6  | 恒速 单条 Body 长度 |
| YWD_AGG_REC_LEN_MIT       | 13 | MIT 单条记录 = 1B NODE_ID + 12B Body |
| YWD_AGG_REC_LEN_POSVEL    | 11 | 位置速度 单条记录 |
| YWD_AGG_REC_LEN_CVEL      | 7  | 恒速 单条记录 |
| MOTOR_NUM                 | 3 | 电机数量 (BSW_MOTOR_NUM) |

### 2.3 满量程常量

| 宏 | 值 | 说明 |
|----|----|----|
| FS_POS   | 2147483647 (int32)  | 位置满量程 ±2147483647 ↔ ±PMAX rad |
| FS_VEL   | 32767 (int16)       | 速度满量程 ±32767 ↔ ±VMAX rad/s |
| FS_TORQ  | 32767 (int16)      | 力矩满量程 ±32767 ↔ ±CURR_PEAK_A (标幺值) |
| VBus 量化| 0.01 V/LSB         | 母线电压 (int16) ↔ 实际 V |
| Kp 量化  | 0.01 N·m/rad /LSB  | 阻抗 Kp (uint16) ↔ 实际 N·m/rad |
| Kd 量化  | 0.01 N·m·s/rad/LSB | 阻抗 Kd (uint16) ↔ 实际 N·m·s/rad |

### 2.4 模式枚举 (YwdCtrlMode_t)

| 值 | 宏 | 模式 |
|----|----|------|
| 1 | YWD_CTRL_MODE_MIT      | MIT 阻抗 |
| 2 | YWD_CTRL_MODE_POS_VEL  | 位置速度 |
| 3 | YWD_CTRL_MODE_CONSTVEL| 恒速 |

### 2.5 状态枚举 (反馈帧)

| 值 | 宏 | 状态 |
|----|----|------|
| 0 | YWD_STATE_DISABLED | 失能 |
| 1 | YWD_STATE_ENABLED  | 使能 |
| 2 | YWD_STATE_FAULT    | 故障 |
| 3 | YWD_STATE_CALIB    | 校准中 |

### 2.6 故障码 (反馈帧 fault 字段)

| 值 | 宏 | 故障类型 |
|----|----|----------|
| 0x00 | YWD_FAULT_NONE          | 无故障 |
| 0x01 | YWD_FAULT_OVER_VOLT     | 过压 |
| 0x02 | YWD_FAULT_UNDER_VOLT    | 欠压 |
| 0x03 | YWD_FAULT_OVER_CURR     | 过流 |
| 0x04 | YWD_FAULT_MOS_OVERTMP   | MOS 过温 |
| 0x05 | YWD_FAULT_COIL_OVERTMP  | 线圈过温 |
| 0x06 | YWD_FAULT_COMM_TIMEOUT  | 通信超时 |
| 0x07 | YWD_FAULT_OVERLOAD      | 过载 |
| 0x08 | YWD_FAULT_ENCODER_ERR   | 编码器错误 |
| 0x09 | YWD_FAULT_MODE_MISMATCH | 模式不匹配 |

---

## 3. 聚合帧头 (YwdAggHeader_t)

聚合帧第一字节为 Header：

```
  Bit 7 6 5 4 3 | 2 1 0
  +-------------+-------+
  |  reserved   | rec_cnt |
  +-------------+-------+
```

| 字段 | 位宽 | 含义 |
|------|------|------|
| rec_cnt | bit0~2 (3b) | 记录数 N，范围 1~4 (0 视为非法) |
| reserved | bit3~7 (5b) | 保留，发送时填 0，接收时忽略 |

C 结构体 (GCC RISC-V 小端位字段)：

```c
typedef union
{
    uint8_t raw;
    struct
    {
        uint8_t rec_cnt : 3;
        uint8_t rsv     : 5;
    } bits;
} YwdAggHeader_t;
```

---

## 4. 下行聚合控制帧 (H → D)

### 4.1 帧通用格式

```
+---------+============================+
| Header  | Record[0] ... Record[N-1]  |
| (1B)    +------+---------------------+
|         | NODE | Body (变长)         |
|         | (1B) |                     |
+---------+------+---------------------+
```

- 总长 = 1 + N × (1 + BodyLen)
- 当 N=4、MIT 模式时 = 1 + 4×13 = 53B，落在 CAN-FD DLC 64B 内
- 当 N=3、MIT 模式时 = 1 + 3×13 = 40B，落在 CAN-FD DLC 48B 内

### 4.2 MIT 组 (0x001, BodyLen=12)

CAN ID = 0x001，单条 Body 长度 12B：

```
偏移  字段     类型    量化                            范围
0     p_des    int32   PMAX rad ↔ ±2147483647          -PMAX~+PMAX rad
4     v_des    int16   VMAX rad/s ↔ ±32767             -VMAX~+VMAX rad/s
6     kp       uint16  0.01 N·m/rad /LSB               0~655.35 N·m/rad
8     kd       uint16  0.01 N·m·s/rad/LSB             0~655.35 N·m·s/rad
10    tff      int16   TMAX N·m ↔ ±32767               -TMAX~+TMAX N·m
```

打包函数: `COM_Motor_PackMit()` —— `COM_Motor_PackI32LE(p_des) + PackI16LE(v_des) + PackU16LE(kp) + PackU16LE(kd) + PackI16LE(tff)`

### 4.3 位置速度组 (0x002, BodyLen=10)

CAN ID = 0x002，单条 Body 长度 10B：

```
偏移  字段     类型    量化                            范围
0     p_des    int32   PMAX rad ↔ ±2147483647          -PMAX~+PMAX rad
4     vmax     uint16  0.01 rad/s /LSB                 0~655.35 rad/s
6     acc      uint16  0.01 rad/s² /LSB                0~655.35 rad/s²
8     dec      uint16  0.01 rad/s² /LSB                0~655.35 rad/s²
```

打包函数: `COM_Motor_PackPosVel()` —— `PackI32LE(p_des) + PackU16LE(vmax) + PackU16LE(acc) + PackU16LE(dec)`

### 4.4 恒速组 (0x003, BodyLen=6)

CAN ID = 0x003，单条 Body 长度 6B：

```
偏移  字段     类型    量化                            范围
0     v_des    int16   VMAX rad/s ↔ ±32767             -VMAX~+VMAX rad/s
2     acc      uint16  0.01 rad/s² /LSB                0~655.35 rad/s²
4     dec      uint16  0.01 rad/s² /LSB                0~655.35 rad/s²
```

打包函数: `COM_Motor_PackConstVel()` —— `PackI16LE(v_des) + PackU16LE(acc) + PackU16LE(dec)`

### 4.5 下行执行规则

下位机 `COM_CAN_ParseAggCtrlFrame()` 执行规则 (协议第 6.2 章)：

1. 遍历帧内全部记录，仅匹配本机 `NODE_ID` (g_nodeID + mIndex) 的记录执行
2. **重复 NODE_ID 取第一条有效** (用 `matched[]` 标记)
3. 无匹配记录的电机保持上一周期指令 (不动作)
4. 匹配后触发 `COM_CAN_OnCtrlFrameReceived()` 按 `RID_FB_DIV` 分频回送 0x600|node
5. 长度不足或 Header 非法则整体跳过 (不执行任何电机)

---

## 5. 上行聚合反馈帧 (D → H)

### 5.1 帧通用格式

```
+---------+========================================+=============+
| Header  | Record[0] ... Record[N-1]              | CRC 统计    |
| (1B)    +----+------------------+                | (6B, 新版)  |
|         |NODE| YwdFeedback_t(16B)+                |             |
+---------+----+------------------+----------------+-------------+
```

- 总长 = 1 + N×17 + 6 (新版带 CRC) 或 1 + N×17 (旧版)
- 当 N=3 时 = 1 + 3×17 + 6 = 58B (CAN-FD DLC 64B 内)
- 下位机: `COM_CAN_SendAggFeedback()` 在 `COM_CAN_Task` (1ms 调度器) 中调用

### 5.2 单条反馈记录 (17B)

```
偏移  字段       类型    说明
0     NODE_ID    uint8   反馈节点号 (g_nodeID + mIndex)
1     state_mode uint8   高4位=STATE, 低4位=MODE
2     fault      uint8   故障码 (见 2.6)
3     pos        int32   位置原始值 (LE), 量化见 2.3
7     vel        int16   速度原始值 (LE), 量化见 2.3
9     torque     int16   力矩原始值 (LE, 标幺值 ±32767 ↔ ±iq/peak_A)
11    t_mos      int8    MOS 温度 (℃)
12    t_rotor    int8    转子温度 (℃)
13    vbus       int16   母线电压 (LE), 0.01 V/LSB
15    seq        uint8   反馈帧序号 (packNum 低8位)
16    rsv        uint8   保留, 发送填 0
```

C 结构体 (DLC=16，不含 NODE_ID)：

```c
typedef struct
{
    union {
        uint8_t raw;
        struct { uint8_t mode:4; uint8_t state:4; } bits;
    } state_mode;
    uint8_t  fault;
    int32_t  pos;
    int16_t  vel;
    int16_t  torque;
    int8_t   t_mos;
    int8_t   t_rotor;
    int16_t  vbus;
    uint8_t  seq;
    uint8_t  rsv;
} YwdFeedback_t;  // sizeof = 16
```

### 5.3 末尾 CRC 统计字段 (6B, 新版追加)

末尾追加 3 路编码器 UART CRC 校验失败累计次数 (uint16 LE, 每路 2B, 共 6B)。映射顺序与 `BSW_Encoder.c` 中 motor→UART 一致：

| 偏移 (相对帧头) | 字段 | 类型 | 对应电机 | 对应 UART |
|-----------------|------|------|----------|-----------|
| 1 + N×17 + 0    | EncCrcErrM1 | uint16 LE | Motor 1 (轴 0) | UART3 |
| 1 + N×17 + 2    | EncCrcErrM2 | uint16 LE | Motor 2 (轴 1) | UART2 |
| 1 + N×17 + 4    | EncCrcErrM3 | uint16 LE | Motor 3 (轴 2) | UART1 |

- 该字段为**可选**追加，由下位机 `COM_CAN_SendAggFeedback()` 在末尾附加
- 上位机通过 `len >= 1 + N×17 + 6` 判断是否包含 (`aggFb.HasCrcErr`)
- 旧版下位机无此字段时 `HasCrcErr=false`，上位机显示 "-" 占位

### 5.4 上行触发条件

下位机收到下行聚合控制帧后**立即回送**对应聚合反馈帧：

```
H 发 0x001 (MIT) → D 收到 → COM_CAN_Task → 回送 0x701
H 发 0x002 (PV)  → D 收到 → COM_CAN_Task → 回送 0x702
H 发 0x003 (CV)  → D 收到 → COM_CAN_Task → 回送 0x703
```

聚合反馈帧与 0x600|node 单电机反馈帧并行存在：
- 收到聚合控制帧后立即回送 0x701/0x702/0x703 (双向兼容保留)
- 收到聚合控制帧并匹配本机 NODE_ID 后按 `RID_FB_DIV` 分频回送 0x600|node

### 5.5 上位机解析

| 函数 | 作用 |
|------|------|
| `YwdFrameCodec.TryDecodeAggregateFeedback()` | 解析 0x701/0x702/0x703 帧，输出 `YwdAggregateFeedback` |
| `Form1.HandleAggregateFeedbackFrame()` | 更新 4 区视图：3 轴表格 dgvFb3Axis、示波器入队、lvAggResult 列表行、反馈 tab、控制 tab 反馈区 |

---

## 6. 中断与延迟处理

下位机 `COM_CAN_PackRes()` 运行于 **CAN 接收中断**。聚合帧解析 + 电机控制 + 2 次反馈发送耗时较长，会阻塞编码器串口中断导致读取异常。因此采用**延迟处理**模式：

```
CAN 中断 (COM_CAN_PackRes)
  ↓ 仅拷贝数据 + 置标志 aggFramePending=true
  ↓ 立即返回
1ms 调度器 (COM_CAN_Task)
  ↓ 检测 aggFramePending
  ↓ 拷贝到 localBuf (防止中断覆盖)
  ↓ COM_CAN_ParseAggCtrlFrame() 解析+执行
  ↓ COM_CAN_SendAggFeedback() 回送 0x701/0x702/0x703
  ↓ 清 aggFramePending
```

关键变量 (com_can.c):

```c
static volatile bool aggFramePending = false;  // 有待处理聚合帧
static uint32_t aggFrameCanID;                 // 待处理聚合帧 CAN ID
static uint8_t  aggFrameLen;                   // 待处理聚合帧数据长度
static uint8_t  aggFrameBuffer[64];            // 待处理聚合帧数据缓存
```

---

## 7. 数据编码与缩放

### 7.1 位置编码

```
raw = (int32_t)(pos_rad / PMAX * 2147483647)     // 下发
pos_rad = raw / 2147483647 * PMAX                 // 反馈
```

clamp 到 ±PMAX；`PMAX` 来自公共量程寄存器 `RID_PMAX` (0x11)。

### 7.2 速度编码

```
raw = (int16_t)(vel_rad_s / VMAX * 32767)        // 下发
vel_rad_s = raw / 32767 * VMAX                    // 反馈
```

clamp 到 ±VMAX；`VMAX` 来自公共量程寄存器 `RID_VMAX` (0x12)。

### 7.3 力矩编码

反馈力矩为**标幺值**：

```
pu_t = iqAct_A / peak_A                            // 范围 -1.0 ~ +1.0
raw = (int16_t)(pu_t * 32767)                     // 反馈
```

下发前馈力矩 `tff` 用物理量：

```
raw = (int16_t)(tff_nm / TMAX * 32767)            // 下发
```

`TMAX` 来自公共量程寄存器 `RID_TMAX` (0x13)。

### 7.4 母线电压编码

```
raw = (int16_t)(vbus_V / 0.01)                    // 反馈
vbus_V = raw * 0.01                               // 解析
```

### 7.5 阻抗系数编码 (仅 MIT 组下发)

```
kp_raw = (uint16_t)(kp_NmPerRad / 0.01)           // 下发
kd_raw = (uint16_t)(kd_NmPerSRad / 0.01)          // 下发
```

---

## 8. 量程来源与同步

为保证下发 / 反馈 / 示波器 / 3 轴表格 共用一套量程，避免显示比例错位：

| 来源 | 内容 | 下位机寄存器 |
|------|------|--------------|
| 下位机 `COM_RegBank_Read(RID_PMAX)` | 默认 MATH_TWO_PI (≈6.283 rad) | 0x11 (float, RW) |
| 下位机 `COM_RegBank_Read(RID_VMAX)` | 默认由电机参数计算 (≈22.44 rad/s) | 0x12 (float, RW) |
| 下位机 `COM_RegBank_Read(RID_TMAX)` | 默认由电机参数计算 (≈20.475 N·m) | 0x13 (float, RW) |
| 上位机 `_sysNumPmax/Vmax/Tmax` | 系统命令区公共量程控件 | - |
| 上位机 `numFbPmax/Vmax/Tmax` | 反馈 tab 量程 (旧版, 已隐藏) | - |
| 上位机 `numAggPmax/Vmax/Tmax` | 聚合帧 tab 量程 | - |
| 上位机 `numMIT_Pmax/...` | MIT 控制体 tab 量程 | - |

闭环同步流程：

1. **连接成功后立即读取** (Form1.ReadRangesFromNodes)：上位机向 3 节点顺序发送读请求读 0x11/0x12/0x13，收到 0x680 响应后回写到 `_sysNumPmax/Vmax/Tmax`，再自动发送下一节点
2. **下发聚合帧** 用 `_sysNumPmax/Vmax/Tmax` 编码
3. **解析聚合反馈帧** 用同一套 `_sysNumPmax/Vmax/Tmax` 解码
4. **3 轴表格 dgvFb3Axis + 示波器 ScopeEnqueue** 共用 `_sysNumPmax/Vmax/Tmax`

---

## 9. 下位机关键代码位置

| 函数 | 文件 | 行号 | 作用 |
|------|------|------|------|
| `COM_CAN_PackRes` | com_can.c | 415 | CAN 接收中断回调，延迟拷贝聚合帧 |
| `COM_CAN_ParseAggCtrlFrame` | com_can.c | 283 | 解析下行聚合控制帧，执行电机控制 |
| `COM_CAN_OnCtrlFrameReceived` | com_can.c | 319 | 控制帧接收后分频触发反馈 |
| `COM_CAN_SendFeedback` | com_can.c | 230 | 发送单电机反馈帧 0x600\|node |
| `COM_CAN_SendAggFeedback` | com_can.c | 235 | 发送聚合反馈帧 0x701/0x702/0x703 (末尾追加 6B CRC) |
| `COM_CAN_PackOneFeedback` | com_can.c | 135 | 打包单条 16B YwdFeedback_t |
| `COM_CAN_Task` | com_can.c | 339 | 1ms 调度器，延迟处理待处理聚合帧 |
| `COM_Motor_PackMit` | com_motor.c | 217 | MIT Body 12B 打包 |
| `COM_Motor_PackPosVel` | com_motor.c | 234 | 位置速度 Body 10B 打包 |
| `COM_Motor_PackConstVel` | com_motor.c | 250 | 恒速 Body 6B 打包 |
| `COM_Motor_PackAggFrame` | com_motor.c | 295 | 聚合帧完整打包 (Header + N×Record) |
| `COM_Motor_FloatToRawPos` | com_motor.c | 129 | 位置 float → int32 编码 |
| `COM_Motor_FloatToRawVel` | com_motor.c | 143 | 速度 float → int16 编码 |
| `COM_Motor_FloatToRawTorque` | com_motor.c | 157 | 力矩 float → int16 编码 |
| `COM_Motor_FloatToVbus` | com_motor.c | 197 | 母线电压 float → int16 编码 |

## 10. 上位机关键代码位置

| 函数 | 文件 | 作用 |
|------|------|------|
| `YwdFrameCodec.TryDecodeAggregateFeedback` | YwdFrameCodec.cs | 解析 0x701/0x702/0x703 反馈帧 (支持末尾 CRC 字段) |
| `YwdFrameCodec.BuildAggregateMitFrame` | YwdFrameCodec.cs | 打包 0x001 MIT 下行 |
| `YwdFrameCodec.BuildAggregatePosVelFrame` | YwdFrameCodec.cs | 打包 0x002 位置速度下行 |
| `YwdFrameCodec.BuildAggregateConstVelFrame` | YwdFrameCodec.cs | 打包 0x003 恒速下行 |
| `Form1.HandleAggregateFrame` | Form1.cs | 收到下行聚合控制帧 echo 处理 |
| `Form1.HandleAggregateFeedbackFrame` | Form1.cs | 收到 0x701/0x702/0x703 反馈帧处理 (4 区视图更新 + CRC 显示) |
| `Form1.Init3AxisDgv` | Form1.cs | 3 轴表格初始化 (含 "编码器CRC" 列) |
| `Form1.Refresh3AxisDgv` | Form1.cs | 3 轴表格刷新 (每轴 CRC 计数刷新) |
| `Form1.ReadRangesFromNodes` | Form1.cs | 顺序读取 3 节点量程 (PMAX/VMAX/TMAX) |

---

## 11. 示例

### 11.1 下行 MIT 组 (0x001) 示例

3 轴同时控制，N=3，帧长 = 1 + 3×13 = 40B：

```
CAN ID: 0x001  DLC: 40
Data (hex):
  03                              -- Header: rec_cnt=3
  01 00 00 00 80 00 00 00 64 00 64 00 00 00   -- 节点1: p=0, v=0, kp=1.0, kd=1.0, tff=0
  02 00 00 80 3F 80 00 00 00 64 00 64 00 00   -- 节点2: p≈0.25 PMAX, ...
  03 00 00 00 00 00 00 80 3F 00 00 64 00 00   -- 节点3: v=0.5 VMAX, ...
```

### 11.2 上行聚合反馈 (0x701) 示例 (带 CRC)

3 轴反馈，N=3，帧长 = 1 + 3×17 + 6 = 58B：

```
CAN ID: 0x701  DLC: 58
Data (hex):
  03                                           -- Header: rec_cnt=3
  01 10 00 00 00 00 00 7F 00 00 7F 19 1A 9C 0F 01 00   -- 节点1: state=1(EN) mode=0 pos=0 vel=0 torque=32767 tmos=25 trott=26 vbus=3996 (39.96V) seq=1
  02 10 00 00 12 34 56 78 9A BC DE F0 19 1A 9C 0F 02   -- 节点2: ...
  03 20 08 00 00 00 00 00 00 00 00 00 19 1A 9C 0F 03   -- 节点3: state=2(FAULT) fault=0x08 (ENC_ERR)
  05 00 00 00 02 00 00 00 00                  -- CRC: M1=5, M2=0, M3=2
```

### 11.3 CRC 字段解析示例

末尾 6B `05 00 00 00 02 00 00 00` (实际为 6B: `05 00 00 00 02 00`)：

| 字节 | 字段 | 解析 |
|------|------|------|
| 0x05 0x00 | EncCrcErrM1 = 0x0005 | Motor 1 编码器 (UART3) CRC 错误 5 次 |
| 0x00 0x00 | EncCrcErrM2 = 0x0000 | Motor 2 编码器 (UART2) CRC 错误 0 次 |
| 0x02 0x00 | EncCrcErrM3 = 0x0002 | Motor 3 编码器 (UART1) CRC 错误 2 次 |

---

## 12. 修订记录

| 日期 | 版本 | 修订内容 |
|------|------|----------|
| 2026-08-26 | V0.1 | 初版：从协议主文档抽离聚合帧章节，新增聚合反馈帧末尾 6B CRC 字段说明 |
