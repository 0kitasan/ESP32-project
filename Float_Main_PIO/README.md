## Float Main PIO

用于带浮力引擎的浮标正式任务与调试。

当前 `main` 默认运行正式 `Mission` 状态机：

- 岸上通过 MQTT 下发任务开始命令
- 第 1 段下潜到 `1.1 m` 并悬停 `30 s`
- 第 2 段上浮到 `0.4 m` 并悬停 `30 s`
- 随后强制上浮
- 浮出水面并重新连上 MQTT 后回传历史数据

## MQTT Topic

当前 topic 统一使用 `/float_sjtu` 前缀。命令 topic 已按测试入口拆开：

- `MQTT_TOPIC_CMD_MISSION = /float_sjtu/cmd/mission`
- `MQTT_TOPIC_CMD_DEBUG_MISSION = /float_sjtu/cmd/debug_mission`
- `MQTT_TOPIC_CMD_MOTOR = /float_sjtu/cmd/motor`
- `MQTT_TOPIC_CMD_PUMP = /float_sjtu/cmd/pump`
- `MQTT_TOPIC_CMD_COUNTER = /float_sjtu/cmd/counter`
- `MQTT_TOPIC_STATUS = /float_sjtu/status`
- `MQTT_TOPIC_PARAM = /float_sjtu/param`
- `MQTT_TOPIC_HISTORY = /float_sjtu/history`
- `MQTT_TOPIC_DEBUG = /float_sjtu/debug`
- `MQTT_TOPIC_REALTIME = /float_sjtu/telemetry`

其中：

- `/float_sjtu/cmd/mission`
正式任务状态机命令。
- `/float_sjtu/cmd/debug_mission`
单目标 `debugDepthMission(...)` 调试命令。
- `/float_sjtu/cmd/motor`
电机直驱测试命令。
- `/float_sjtu/cmd/pump`
泵测试命令。
- `/float_sjtu/cmd/counter`
`debugMQTT(...)` 计数测试命令。
- `/float_sjtu/status`
浮标状态信息。上电待命时会主动输出当前深度，方便确认深度零点是否正常。
- `/float_sjtu/param`
任务控制参数信息。任务开始时会单独发布当前使用的 `kp/kd/lead_*` 参数。
- `/float_sjtu/history`
浮标出水后回传的历史深度-时间数据。
- `/float_sjtu/debug`
串口日志的 MQTT 镜像。

## 调试入口与 Topic

当前几条主要调试入口如下：

- `debugDepthMission(...)`
使用 `/float_sjtu/cmd/debug_mission` 接收单目标调试命令，使用 `/float_sjtu/status` 回传任务状态，使用 `/float_sjtu/param` 回传当前控制参数，使用 `/float_sjtu/history` 回传历史数据。
- `debugMotorRemote(...)`
使用 `/float_sjtu/cmd/motor` 接收电机推力命令，调试日志发到 `/float_sjtu/debug`。
- `debugPumpRemote(...)`
使用 `/float_sjtu/cmd/pump` 接收泵推力命令，调试日志发到 `/float_sjtu/debug`。
- `debugFakeHistoryUpload(...)`
不接收命令，启动后直接向 `/float_sjtu/history` 发送假数据。
- `debugSensor(...)`
不接收命令，只在本地串口和 `/float_sjtu/debug` 输出传感器读数。

## 正式 Mission

正式 `Mission` 是一个非阻塞状态机，每次 `loop()` 调用一次。

默认任务参数定义在 `include/Config.h`：

- `MISSION_STAGE1_TARGET_DEPTH_M_DEFAULT = 1.10`
- `MISSION_STAGE1_HOLD_MS_DEFAULT = 30000`
- `MISSION_STAGE2_TARGET_DEPTH_M_DEFAULT = 0.40`
- `MISSION_STAGE2_HOLD_MS_DEFAULT = 30000`
- `MISSION_SURFACE_DRAIN_DURATION_MS_DEFAULT = 10000`

控制器默认参数仍然沿用：

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

正式任务流程：

1. `WAIT_START_CMD`
上电待命，通过 `/float_sjtu/status` 周期上报当前深度。

2. `STAGE1_TO_DEPTH`
下潜到第一阶段目标深度，默认 `1.1 m`。

3. `HOLD_STAGE1`
在第一阶段目标附近持续悬停，默认 `30 s`。

4. `STAGE2_TO_DEPTH`
上浮到第二阶段目标深度，默认 `0.4 m`。

5. `HOLD_STAGE2`
在第二阶段目标附近持续悬停，默认 `30 s`。

6. `FORCE_SURFACE`
第二阶段完成后固定全速排水，默认持续 `10000 ms`。

7. `UPLOADING_HISTORY`
浮出水面并重新连上 MQTT 后，按 `50ms/条` 节奏回传历史数据。

正式任务的历史数据不是高频调试曲线，而是比赛包。

每个悬停阶段会记录 `7` 个数据包，对应：

- `0 s`
- `5 s`
- `10 s`
- `15 s`
- `20 s`
- `25 s`
- `30 s`

其中 `0 s` 表示“第一次进入规定深度范围”的起点。

当前正式任务最多记录两段悬停，因此缓冲区容量固定为 `14` 条。

UTC 时间当前使用板载系统时钟；如果没有额外做 NTP/RTC 对时，则会回传占位值 `00:00:00 UTC`。

### 正式 Mission 命令格式

发送到 `/float_sjtu/cmd/mission`。

使用默认任务参数直接开始：

```text
start
```

覆盖任务参数与控制参数：

```text
start,stage1_depth_m=1.10,stage1_hold_ms=30000,stage2_depth_m=0.40,stage2_hold_ms=30000,surface_drain_duration_ms=10000,kp=1.10,kd=0.45,lead_enable=1,lead_gain=1.00,lead_tau_s=0.15,lead_alpha=0.35
```

也支持 JSON：

```json
{
  "start": 1,
  "stage1_depth_m": 1.10,
  "stage1_hold_ms": 30000,
  "stage2_depth_m": 0.40,
  "stage2_hold_ms": 30000,
  "surface_drain_duration_ms": 10000,
  "kp": 1.10,
  "kd": 0.45,
  "lead_enable": 1,
  "lead_gain": 1.00,
  "lead_tau_s": 0.15,
  "lead_alpha": 0.35
}
```

也支持以下命令：

- `status`
立即回传一次当前状态。
- `depth?`
等价于 `status`。
- `surface`
如果任务还未开始，会被忽略。

## `debugDepthMission` 调试入口

`debugDepthMission(...)` 仍然保留，用于单目标深度调试。它现在监听：

```text
/float_sjtu/cmd/debug_mission
```

调试命令格式仍然是原来的单目标格式，例如：

```text
start:2.5,kp=1.10,kd=0.45,lead_enable=1,lead_gain=1.00,lead_tau_s=0.15,lead_alpha=0.35,drain_after_ms=30000,drain_duration_ms=10000
```

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
  "stage": 0,
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
  "target_depth_m": 1.100,
  "stage": 1,
  "history_count": 0,
  "upload_index": 0
}
```

任务启动时 `/float_sjtu/param` 会额外发送：

```json
{
  "stage1_depth_m": 1.100,
  "stage1_hold_ms": 30000,
  "stage2_depth_m": 0.400,
  "stage2_hold_ms": 30000,
  "surface_drain_duration_ms": 10000,
  "kp": 1.100,
  "kd": 0.450,
  "lead_enable": 1,
  "lead_gain": 1.000,
  "lead_tau_s": 0.150,
  "lead_alpha": 0.350
}
```

## History 示例

出水后 `/float_sjtu/history` 会逐条发送比赛格式文本，例如：

```text
PN01 01:51:42 UTC 9.8 kpa 1.00 meters
```

## 绘图脚本

- `tools/plot_depth_json.py`
用于绘制 debug 历史 JSON/JSONL，坐标采用左上为原点、时间向右、深度向下。
- `tools/plot_vpd_text.py`
用于绘制正式比赛包文本格式，例如 `PN01 01:51:42 UTC 9.8 kpa 1.00 meters`。
