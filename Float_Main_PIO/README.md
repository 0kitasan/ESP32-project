## Float Main PIO

用于带浮力引擎的浮标调试。当前推荐使用 `debugDepthMission(...)` 这条链路：

- 岸上通过 MQTT 下发目标深度命令
- 浮标收到命令后开始下潜
- 任务运行设定时长后直接强制上浮
- 浮出水面后再通过 MQTT 回传历史深度数据

## MQTT Topic

当前 topic 统一使用 `/float_sjtu` 前缀。命令 topic 已按测试入口拆开：

- `MQTT_TOPIC_CMD_MISSION = /float_sjtu/cmd/mission`
- `MQTT_TOPIC_CMD_MOTOR = /float_sjtu/cmd/motor`
- `MQTT_TOPIC_CMD_PUMP = /float_sjtu/cmd/pump`
- `MQTT_TOPIC_CMD_COUNTER = /float_sjtu/cmd/counter`
- `MQTT_TOPIC_STATUS = /float_sjtu/status`
- `MQTT_TOPIC_HISTORY = /float_sjtu/history`
- `MQTT_TOPIC_DEBUG = /float_sjtu/debug`
- `MQTT_TOPIC_REALTIME = /float_sjtu/telemetry`

其中：

- `/float_sjtu/cmd/mission`
任务状态机命令。
- `/float_sjtu/cmd/motor`
电机直驱测试命令。
- `/float_sjtu/cmd/pump`
泵测试命令。
- `/float_sjtu/cmd/counter`
`debugMQTT(...)` 计数测试命令。
- `/float_sjtu/status`
浮标状态信息。上电待命时会主动输出当前深度，方便确认深度零点是否正常。
- `/float_sjtu/history`
浮标出水后回传的历史深度-时间数据。
- `/float_sjtu/debug`
串口日志的 MQTT 镜像。

## 调试入口与 Topic

当前几条主要调试入口如下：

- `debugDepthMission(...)`
使用 `/float_sjtu/cmd/mission` 接收任务命令，使用 `/float_sjtu/status` 回传状态，使用 `/float_sjtu/history` 回传历史数据。
- `debugMotorRemote(...)`
使用 `/float_sjtu/cmd/motor` 接收电机推力命令，调试日志发到 `/float_sjtu/debug`。
- `debugPumpRemote(...)`
使用 `/float_sjtu/cmd/pump` 接收泵推力命令，调试日志发到 `/float_sjtu/debug`。
- `debugFakeHistoryUpload(...)`
不接收命令，启动后直接向 `/float_sjtu/history` 发送假数据。
- `debugSensor(...)`
不接收命令，只在本地串口和 `/float_sjtu/debug` 输出传感器读数。

## `debugDepthMission` 思路

`debugDepthMission(...)` 是一个非阻塞状态机，每次 `loop()` 调用一次。

PD 控制器有一套默认参数，定义在 `include/Config.h`：

- `CTRL_KP_DEFAULT`
- `CTRL_KD_DEFAULT`
- `CTRL_OUTPUT_LIMIT_DEFAULT`
- `CTRL_MIN_ACTUATION_CMD_DEFAULT`
- `CTRL_HOLD_ENTER_BAND_M_DEFAULT`
- `CTRL_HOLD_EXIT_BAND_M_DEFAULT`
- `CTRL_DERIVATIVE_FILTER_ALPHA_DEFAULT`
- `CTRL_LEAD_ENABLE_DEFAULT`
- `CTRL_LEAD_GAIN_DEFAULT`
- `CTRL_LEAD_TAU_S_DEFAULT`
- `CTRL_LEAD_ALPHA_DEFAULT`

每次新任务开始时，先加载这套默认参数；如果 MQTT 开始命令里带了新的 `kp/kd/lead_*`，就用命令里的值覆盖默认值。

深度数据会先经过 `SensorDriver::getDepthFilter()` 的滤波链路：

- 先做 `5` 点中位数滤波，消除单个异常毛刺
- 再做一层 EMA，当前 `alpha = 0.35`

`debugDepthMission(...)` 中的控制输入、状态上报和历史记录，当前都使用这个滤波后的深度值。

方向约定统一为：

- 深度向下为正，越深数值越大
- 泵命令 `> 0` 表示吸水增重下潜
- 泵命令 `< 0` 表示排水减重上浮

如果硬件电机接线反了，只在 `Pump` 内部做极性修正，不再改变上层控制语义。

1. `WAIT_START_CMD`
浮标上电后先进入待命状态，读取当前深度，并通过 `/float_sjtu/status` 周期上报当前深度。

2. 收到目标深度命令
岸上通过 `/float_sjtu/cmd/mission` 下发目标深度后，浮标开始任务。

3. `CONTROL_TO_DEPTH`
使用 `Control` 中的 PD 控制器驱动泵，让浮标向目标深度移动并尝试悬停。
如果启用了超前校正，还会在 PD 输出后串联一个一阶超前校正环节：

```text
G_lead(s) = K * (T s + 1) / (alpha T s + 1)
```

其中：

- `lead_enable`
是否启用超前校正，`0/1`。
- `lead_gain`
超前环节增益 `K`。
- `lead_tau_s`
超前环节时间常数 `T`，单位秒。
- `lead_alpha`
极点/零点比例系数 `alpha`。当前实现按超前环节使用，建议取 `0 < alpha < 1`。

4. 记录历史数据
任务过程中每 `500ms` 记录一条深度样本到 RAM 缓冲区。
当前每条样本只包含：

- `time_ms`
- `depth_m`

当前缓冲区容量是 `256` 条。

5. `FORCE_DRAIN`
任务运行满设定时长后，直接强制排水固定时长，再进入历史上传阶段。

默认参数：

- `force_drain_after_ms = 30000`
- `force_drain_duration_ms = 10000`

6. `UPLOADING_HISTORY`
检测到浮标浮出水面并重新连上 MQTT 后，按 `50ms/条` 的节奏把历史数据逐条发到 `/float_sjtu/history`。

## 命令格式

推荐发送到 `/float_sjtu/cmd/mission` 的命令：

```text
start:2.5
```

表示开始任务，并把目标深度设为 `2.5 m`。

也支持 JSON：

```json
{"target_depth_m": 2.5}
```

如果要在任务开始时覆盖 PD 参数，可以这样发：

```text
start:2.5,kp=1.10,kd=0.45
```

或者：

```json
{"target_depth_m": 2.5, "kp": 1.10, "kd": 0.45}
```

如果要同时覆盖超前校正参数，可以这样发：

```text
start:2.5,kp=1.10,kd=0.45,lead_enable=1,lead_gain=1.00,lead_tau_s=0.15,lead_alpha=0.35
```

```json
{
  "target_depth_m": 2.5,
  "kp": 1.10,
  "kd": 0.45,
  "lead_enable": 1,
  "lead_gain": 1.00,
  "lead_tau_s": 0.15,
  "lead_alpha": 0.35
}
```

也支持同时覆盖强制排水时序：

```text
start:2.5,kp=1.10,kd=0.45,lead_enable=1,lead_gain=1.00,lead_tau_s=0.15,lead_alpha=0.35,drain_after_ms=30000,drain_duration_ms=10000
```

```json
{
  "target_depth_m": 2.5,
  "kp": 1.10,
  "kd": 0.45,
  "lead_enable": 1,
  "lead_gain": 1.00,
  "lead_tau_s": 0.15,
  "lead_alpha": 0.35,
  "force_drain_after_ms": 30000,
  "force_drain_duration_ms": 10000
}
```

还支持以下调试命令：

- `status`
立即回传一次当前状态和当前深度。
- `depth?`
等价于 `status`。
- `surface`
如果任务还未开始，会被忽略。后续可以继续扩展为手动强制上浮命令。

## `debugMotorRemote` 命令

如果在 `main` 中切换到 `debugMotorRemote(myMqtt, myMotor);`，则可通过
`/float_sjtu/cmd/motor` 直接测试电机。

命令格式：

```text
0.80,3000
motor:0.80,3000
```

表示以 `0.80` 的归一化推力运行 `3000 ms`。

停止命令：

```text
stop
motor stop
motor_stop
```

## `debugPumpRemote` 命令

如果在 `main` 中切换到 `debugPumpRemote(myMqtt, myPump);`，则可通过 `/float_sjtu/cmd/pump`
直接测试泵，不经过 PD 控制器。

## 并行测试说明

命令 topic 拆开后，`debugDepthMission(...)`、`debugMotorRemote(...)`、`debugPumpRemote(...)`
不会再因为共用 `/cmd` 而互相抢命令。

但如果两个调试入口控制的是同一个执行器，仍然不能安全并行，例如：

- `debugDepthMission(...)` 和 `debugPumpRemote(...)` 都会控制 `Pump`
- `debugMotorRemote(...)` 和 `debugPumpRemote(...)` 都会控制同一个底层电机

命令格式：

```text
pump:0.80,3000
```

表示以 `0.80` 的归一化推力吸水运行 `3000 ms`；负值表示排水。

也可以同时指定积分限幅开关：

```text
pump:0.80,3000,limit=1
pump:-0.90,2000,limit=0
```

其中：

- `limit=1`
启用泵内部体积积分限幅。
- `limit=0`
关闭泵内部体积积分限幅。
- `stop`
立即停止泵。

## `debugFakeHistoryUpload`

如果在 `main` 中切换到 `debugFakeHistoryUpload(myMqtt);`，则不需要发送命令。
板子会在 MQTT 可用后自动向 `/float_sjtu/history` 发送假历史数据。

## Status 示例

待命时 `/float_sjtu/status` 会发送类似：

```json
{
  "state": "ready",
  "time_ms": 0,
  "depth_m": 0.003,
  "target_depth_m": 0.000,
  "history_count": 0,
  "upload_index": 0
}
```

任务启动后会发送：

```json
{
  "state": "started",
  "time_ms": 0,
  "depth_m": 0.012,
  "target_depth_m": 2.500,
  "kp": 1.100,
  "kd": 0.450,
  "lead_enable": 1,
  "lead_gain": 1.000,
  "lead_tau_s": 0.150,
  "lead_alpha": 0.350,
  "history_count": 1,
  "upload_index": 0
}
```

## History 示例

出水后 `/float_sjtu/history` 会逐条发送：

```json
{
  "idx": 12,
  "time_ms": 6000,
  "depth_m": 2.347
}
```
