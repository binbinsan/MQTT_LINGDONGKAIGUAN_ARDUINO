# ESP32-C2 MQTT智能开关

这是一个基于ESP32-C2的MQTT智能开关控制程序，支持WiFi配网、参数保存、远程控制等功能。

## 功能特点

- 支持WiFi无感配网
- MQTT远程控制
- 参数持久化存储
- LED状态指示
- 支持长按重置
- 可配置GPIO引脚
- 断电记忆功能

## 硬件要求

- ESP32-C2开发板
- LED指示灯
- 按钮开关
- 继电器模块（用于控制电源）

## 默认引脚配置

- 重置按钮：GPIO9
- LED指示灯：GPIO8
- 电源控制：GPIO7

## 使用说明

### 首次使用

1. 将程序烧录到ESP32-C2开发板
2. 设备首次启动时会自动进入配网模式
3. 使用手机或电脑连接名为"ESP32C2_AP"的WiFi热点
4. 在弹出的配置页面中设置：
   - WiFi网络信息
   - MQTT服务器参数
   - GPIO引脚配置

### MQTT配置

默认MQTT参数：
- 服务器：home.ajk.life
- 端口：1883
- 用户名：binbin
- 密码：wb021102-
- 主题：esp32c2

### LED指示状态

- LED常亮：设备处于配置模式
- LED熄灭：设备正常工作
- LED快闪：设备正在重置

### 按键操作

- 长按重置按钮5秒：重置设备配置
- 短按：可自定义功能（预留）

### 重置设备

如需重置设备：
1. 长按重置按钮5秒以上
2. LED灯将快速闪烁表示正在重置
3. 设备将自动重启并进入配网模式

## MQTT消息格式

设备发送的消息格式为JSON：
```json
{
    "status": "状态信息"
}
```

状态信息包括：
- system_startup: 系统启动
- reconnected: MQTT重新连接

## 故障排除

1. 无法连接WiFi
   - 检查WiFi信号强度
   - 确认WiFi密码正确
   - 尝试重置设备重新配网

2. MQTT连接失败
   - 检查MQTT服务器地址是否正确
   - 验证MQTT用户名密码
   - 确认网络连接正常

3. 设备无响应
   - 检查电源供电是否正常
   - 尝试重置设备
   - 检查LED指示灯状态

## 注意事项

1. 首次使用需要配置WiFi和MQTT参数
2. 修改GPIO配置后需要重新接线
3. 请确保供电稳定，避免频繁断电
4. 建议使用5V-12V直流电源供电

## 技术支持

如有问题请提交Issue或联系开发者。
