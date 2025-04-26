/*
 * ESP32-C2 MQTT智能开关控制程序
 * 功能：通过MQTT协议控制设备开关，支持WiFi配网，支持参数保存
 * 硬件：ESP32-C2开发板
 * 作者：bin_wang
 * 更新日期：20250326
 * 
 * ===== 程序说明 =====
 * 本程序实现了一个基于ESP32-C2的MQTT智能开关控制器：
 * 1. 开机后立即打开电源引脚(power_pin)高电平输出
 * 2. 连接WiFi和MQTT服务器，发送一条启动消息
 * 3. 发送完启动消息后自动关闭电源引脚
 * 4. 进入深度睡眠模式，等待唤醒
 * 5. 支持通过MQTT远程控制开关状态
 * 6. 支持WiFi配网和参数保存
 * 7. 支持长按按钮重置设备配置
 * 
 * ===== 引脚定义 =====
 * - 按钮引脚：默认为GPIO 9，长按重置设备
 * - LED引脚：默认为GPIO 8，指示设备状态
 * - 电源引脚：默认为GPIO 7，控制外部设备供电
 * 
 * ===== MQTT主题 =====
 * - 发布：{mqtt_topic}，发送设备状态信息
 * - 订阅：{mqtt_topic}/control，接收控制命令
 * 
 * ===== MQTT控制命令 =====
 * - "on"：打开电源
 * - "off"：关闭电源
 * - "clear_all_data"：清除所有配置数据，恢复出厂设置
 */

#include <WiFi.h>          // ESP32 WiFi库，用于WiFi网络连接
#include <WiFiManager.h>    // WiFi配网管理库，提供配网服务和配置保存功能
#include <PubSubClient.h>   // MQTT客户端库，用于MQTT通信
#include <Preferences.h>    // ESP32参数保存库，用于将配置保存到非易失性存储器

// 程序版本定义 - 当版本号变更时会触发所有参数重置
#define FIRMWARE_VERSION "1.0.0"  // 当前固件版本号，修改此版本号会触发参数重置
#define VERSION_KEY "fw_version"  // 版本存储键名，用于存储当前版本

// 默认引脚定义 - 可在配置界面中修改
#define DEFAULT_BUTTON_PIN 9     // 长按重置按钮引脚，默认为GPIO 9
#define DEFAULT_LED_PIN 8        // 默认LED指示灯引脚，默认为GPIO 8
#define DEFAULT_POWER_PIN 7      // 默认电源控制引脚，默认为GPIO 7
#define RESET_HOLD_TIME 5000    // 长按重置时间阈值（毫秒），按住按钮超过此时间将触发重置

// MQTT服务器参数配置（默认值）- 可在配置界面中修改
char mqtt_server[40] = "";        // MQTT服务器地址，默认为空
char mqtt_port[6] = "1883";       // MQTT服务器端口，默认为1883
char mqtt_user[40] = "";          // MQTT用户名，默认为空
char mqtt_password[40] = "";      // MQTT密码，默认为空
char mqtt_topic[40] = "esp32";    // MQTT主题，默认为esp32

// GPIO配置变量 - 存储当前使用的引脚配置
char button_pin[3] = "9";    // 长按重置按钮引脚配置，默认为GPIO 9
char led_pin[3] = "8";       // LED引脚配置，默认为GPIO 8
char power_pin[3] = "7";     // 电源引脚配置，默认为GPIO 7

// 标记是否已经发送第一条MQTT消息 - 用于控制电源引脚关闭时机
bool firstMessageSent = false;

// 创建网络和存储对象
WiFiClient espClient;            // WiFi客户端对象
PubSubClient client(espClient);  // MQTT客户端对象，使用WiFi客户端
WiFiManager wifiManager;         // WiFi管理器对象，用于配网
Preferences preferences;         // 参数存储对象，用于保存配置

// 全局状态变量
bool shouldSaveConfig = false;         // 标记是否需要保存配置
unsigned long buttonPressTime = 0;     // 记录按钮按下时间
bool longPressActive = false;          // 标记长按是否已激活

/**
 * 在程序一开始就初始化power_pin，确保第一时间输出高电平
 * 这是本程序最关键的部分，确保电源控制在启动时立即生效
 */
void setupPowerPin() {
  // 从存储器中读取保存的power_pin值
  preferences.begin("iot_config", true); // 以只读模式打开配置存储
  String savedPowerPin = preferences.getString("power_pin", "");
  if (savedPowerPin != "") {
    strcpy(power_pin, savedPowerPin.c_str());  // 如果有已保存的值，则使用它
  }
  preferences.end();
  
  // 立即初始化并设置电源引脚为高电平
  pinMode(atoi(power_pin), OUTPUT);         // 设置为输出模式
  digitalWrite(atoi(power_pin), HIGH);      // 输出高电平，打开外部设备电源
  Serial.println("电源引脚已设置为高电平");  // 调试信息
}

/**
 * WiFiManager配置保存回调函数
 * 当WiFiManager配置页面保存配置时会调用此函数
 */
void saveConfigCallback() {
  shouldSaveConfig = true;  // 标记需要保存配置到存储器
  Serial.println("需要保存新的配置数据");
}

/**
 * 清除所有数据（恢复出厂设置）
 * 通过MQTT发送"clear_all_data"命令可触发此函数
 */
void clearAllData() {
  Serial.println("开始清除所有数据...");
  
  // LED快速闪烁表示重置过程进行中
  for (int i = 0; i < 5; i++) {
    digitalWrite(atoi(led_pin), HIGH);
    delay(100);
    digitalWrite(atoi(led_pin), LOW);
    delay(100);
  }
  
  // 清除所有配置数据
  preferences.begin("iot_config", false);  // 打开配置存储
  preferences.clear();                     // 清除所有配置
  preferences.end();
  
  preferences.begin("system_flags", false); // 打开系统标志存储
  preferences.clear();                      // 清除所有系统标志
  preferences.end();
  
  // 重置WiFi设置
  wifiManager.resetSettings();
  
  Serial.println("数据清除完成，设备将重启");
  delay(1000);
  
  // 重启设备以应用新设置
  ESP.restart();
}

/**
 * 检查固件版本是否更新，如有更新则重置所有参数
 * 每次启动时会检查版本号，版本变更时自动重置所有参数
 */
void checkFirmwareVersion() {
  preferences.begin("system_flags", false);
  String savedVersion = preferences.getString(VERSION_KEY, "");
  
  // 如果保存的版本与当前版本不同，则重置参数
  if (savedVersion != FIRMWARE_VERSION) {
    Serial.print("固件版本变更: ");
    Serial.print(savedVersion);
    Serial.print(" -> ");
    Serial.println(FIRMWARE_VERSION);
    Serial.println("开始重置所有参数...");
    
    // 清除所有配置数据
    preferences.end();
    preferences.begin("iot_config", false);
    preferences.clear();
    preferences.end();
    
    // 重置WiFi设置
    wifiManager.resetSettings();
    
    // 更新版本信息
    preferences.begin("system_flags", false);
    preferences.putString(VERSION_KEY, FIRMWARE_VERSION);
    
    // LED快速闪烁表示重置进行中
    pinMode(DEFAULT_LED_PIN, OUTPUT);  // 使用默认LED引脚，因为此时可能还没有读取配置
    for (int i = 0; i < 10; i++) {
      digitalWrite(DEFAULT_LED_PIN, HIGH);
      delay(100);
      digitalWrite(DEFAULT_LED_PIN, LOW);
      delay(100);
    }
    
    Serial.println("参数重置完成");
  } else {
    Serial.print("当前固件版本: ");
    Serial.println(FIRMWARE_VERSION);
  }
  
  preferences.end();
}

/**
 * MQTT消息回调函数
 * 当收到MQTT消息时会调用此函数处理消息
 * 
 * @param topic 收到消息的主题
 * @param payload 消息内容的字节数组
 * @param length 消息内容的长度
 */
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("收到消息 [");
  Serial.print(topic);
  Serial.print("] ");
  
  // 将接收到的payload转换为字符串，方便处理
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);
  
  // 检查是否是清除数据命令，收到"clear_all_data"时清除所有数据
  if (strcmp(message, "clear_all_data") == 0) {
    clearAllData();
    return;
  }
  
  // 检查是否是开关命令
  if (strcmp(message, "on") == 0) {         // 打开命令
    digitalWrite(atoi(power_pin), HIGH);    // 打开电源
    publishData("on");                      // 发布状态更新
  } else if (strcmp(message, "off") == 0) { // 关闭命令
    digitalWrite(atoi(power_pin), LOW);     // 关闭电源
    publishData("off");                     // 发布状态更新
  }
}

/**
 * 发布MQTT消息并在第一次消息后关闭电源引脚
 * 
 * @param status 状态信息，将被发送到MQTT服务器
 */
void publishData(const char* status) {
  if (client.connected()) {
    // 构建JSON格式的消息
    char message[50];
    snprintf(message, 50, "{\"status\":\"%s\"}", status);
    
    // 发布消息到MQTT主题
    client.publish(mqtt_topic, message);
    Serial.println("MQTT数据已发送");
    
    // 特殊处理：如果是第一次系统启动消息，发送后关闭电源引脚
    if (!firstMessageSent && strcmp(status, "system_startup") == 0&&digitalRead(atoi(button_pin)) == HIGH) {
      firstMessageSent = true;
      delay(500); // 短暂延时确保消息发送完成
      Serial.println("关闭电源引脚");
      digitalWrite(atoi(power_pin), LOW); // 关闭电源
      delay(100);
    }
  }
}

/**
 * 设备启动时的初始化函数
 * 按顺序执行初始化操作，连接网络，发送启动消息
 */
void setup() {
  Serial.begin(115200);  // 初始化串口通信，波特率115200
  Serial.println("设备启动中...");
  
  // 立即初始化电源引脚，确保第一时间输出高电平
  setupPowerPin();
  
  // 使用版本检查，如果版本变更则重置所有参数
  checkFirmwareVersion();
  
  // 从存储器加载配置
  preferences.begin("iot_config", true); // 以只读模式打开配置存储
  
  // 从存储中读取MQTT配置参数，如果没有则使用默认值
  String savedServer = preferences.getString("server", "");
  if (savedServer != "") {
    strcpy(mqtt_server, savedServer.c_str());
    strcpy(mqtt_port, preferences.getString("port", "1883").c_str());
    strcpy(mqtt_user, preferences.getString("user", "").c_str());
    strcpy(mqtt_password, preferences.getString("password", "").c_str());
    strcpy(mqtt_topic, preferences.getString("topic", "esp32c2").c_str());
  }
  
  // 加载其他GPIO配置
  String savedLedPin = preferences.getString("led_pin", "");
  if (savedLedPin != "") {
    strcpy(led_pin, savedLedPin.c_str());
  }
  
  String savedbuttonPin = preferences.getString("button_pin", "");
  if (savedbuttonPin != "") {
    strcpy(button_pin, savedbuttonPin.c_str());
  }
  
  preferences.end();
  Serial.println("参数读取完成");
  
  // 初始化其他GPIO引脚
  pinMode(atoi(button_pin), INPUT_PULLUP);    // 设置重置按钮为上拉输入
  pinMode(atoi(led_pin), OUTPUT);             // 设置LED为输出
  digitalWrite(atoi(led_pin), LOW);           // LED初始状态为关闭
  
  Serial.println("IO引脚初始化完成");
  
  // 添加配置参数到WiFiManager页面
  // 这些参数将显示在WiFiManager配置门户中，允许用户进行修改
  WiFiManagerParameter custom_mqtt_server("server", "MQTT服务器", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT端口", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT用户名", mqtt_user, 40);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT密码", mqtt_password, 40);
  WiFiManagerParameter custom_mqtt_topic("topic", "MQTT主题", mqtt_topic, 40);
  
  // 添加GPIO配置参数
  WiFiManagerParameter custom_button_pin("button_pin", "下载长按重置按键", button_pin, 3);
  WiFiManagerParameter custom_led_pin("led_pin", "LED引脚号", led_pin, 3);
  WiFiManagerParameter custom_power_pin("power_pin", "电源控制引脚号", power_pin, 3);
  
  // 将参数添加到WiFiManager
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_mqtt_topic);
  wifiManager.addParameter(&custom_button_pin);
  wifiManager.addParameter(&custom_led_pin);
  wifiManager.addParameter(&custom_power_pin);

  // 设置保存配置回调，当用户保存配置时将调用回调函数
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(180);    // 配置门户超时时间180秒
  wifiManager.setDebugOutput(false);          // 关闭调试输出以减少日志

  // 设置MQTT服务器和回调函数
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(callback);
  
  // 尝试连接WiFi，如果连接失败则启动配置门户
  Serial.println("尝试连接WiFi...");
  if (!wifiManager.autoConnect("ESP32C2_AP")) {
    Serial.println("连接失败，启动配置门户");
    digitalWrite(atoi(led_pin), HIGH); // LED亮表示设备处于配置模式
    wifiManager.startConfigPortal("ESP32C2_AP");
    digitalWrite(atoi(led_pin), LOW);
  }
  Serial.println("WiFi已连接");

  // 从配置页面获取用户输入的参数
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
  strcpy(button_pin, custom_button_pin.getValue());
  strcpy(led_pin, custom_led_pin.getValue());
  strcpy(power_pin, custom_power_pin.getValue());

  // 如果需要保存配置（用户在配置页面点击了保存）
  if (shouldSaveConfig) {
    Serial.println("保存新配置到存储器...");
    preferences.begin("iot_config", false); // 读写模式
    preferences.putString("server", mqtt_server);
    preferences.putString("port", mqtt_port);
    preferences.putString("user", mqtt_user);
    preferences.putString("password", mqtt_password);
    preferences.putString("topic", mqtt_topic);
    preferences.putString("button_pin", button_pin);
    preferences.putString("led_pin", led_pin);
    preferences.putString("power_pin", power_pin);
    preferences.end();
    Serial.println("配置已保存");
    
    // 重新初始化GPIO引脚，应用新的配置
    pinMode(atoi(button_pin), INPUT_PULLUP);
    pinMode(atoi(led_pin), OUTPUT);
    pinMode(atoi(power_pin), OUTPUT);
    digitalWrite(atoi(led_pin), LOW);
    digitalWrite(atoi(power_pin), HIGH);
  }

  // 连接MQTT并发送初始消息
  Serial.println("尝试连接MQTT服务器...");
  if (connectMQTT()) {
    // 发送系统启动消息，此消息发送后会触发power_pin关闭
    publishData("system_startup");
    delay(500);
    Serial.println("准备进入休眠模式");
    
    // 如果第一次消息已发送，进入深度睡眠模式节省能源
    if (firstMessageSent&&digitalRead(atoi(button_pin)) == HIGH) {
      Serial.println("正在进入深度睡眠模式...");
      delay(500);
      esp_deep_sleep_start();
    }
  }
}

/**
 * 主循环函数
 * 在setup完成后循环执行，处理按钮和MQTT消息
 */
void loop() {
  checkResetButton();     // 检查重置按钮状态
  Serial.println("主程序循环中...");
  
  // 确保MQTT保持连接状态
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();          // 处理MQTT消息
  
  delay(100);            // 短暂延时防止CPU占用过高
}

/**
 * 连接MQTT服务器
 * 
 * @return bool 连接成功返回true，失败返回false
 */
bool connectMQTT() {
  int retry = 0;
  while (!client.connected() && retry < 3) {
    Serial.print("尝试MQTT连接...");
    
    // 尝试连接MQTT服务器，使用设定的用户名和密码
    if (client.connect("ESP32C2_Client", mqtt_user, mqtt_password)) {
      Serial.println("已连接成功");
      
      // 订阅控制主题，格式为{mqtt_topic}/control
      char controlTopic[50];
      snprintf(controlTopic, 50, "%s/control", mqtt_topic);
      client.subscribe(controlTopic);
      Serial.print("已订阅主题: ");
      Serial.println(controlTopic);
      
      return true;
    } else {
      // 连接失败，输出错误代码
      Serial.print("连接失败, 错误代码=");
      Serial.print(client.state());
      Serial.println(" 2秒后重试");
      retry++;
      delay(2000);
    }
  }
  return false;
}

/**
 * 重新连接MQTT服务器并发送重连消息
 * 当MQTT连接断开时，在loop中调用此函数尝试重连
 */
void reconnectMQTT() {
  if (connectMQTT()) {
    // 连接成功后发送重连消息
    publishData("reconnected");
  }
}

/**
 * 检查重置按钮状态
 * 长按触发重置，短按可以添加其他功能
 */
void checkResetButton() {
  // 检测按钮是否被按下（低电平有效）
  if (digitalRead(atoi(button_pin)) == LOW) {
    // 首次按下时记录时间
    if (buttonPressTime == 0) {
      buttonPressTime = millis();
    } 
    // 判断是否达到长按时间阈值
    else if (!longPressActive && (millis() - buttonPressTime > RESET_HOLD_TIME)) {
      longPressActive = true;
      resetConfig();  // 执行重置操作
    }
  } else {
    // 按钮释放
    if (buttonPressTime > 0 && !longPressActive) {
      // 短按可以添加其他功能，当前未实现
      // 例如：可以添加切换设备状态、手动触发同步等功能
    }
    buttonPressTime = 0;
    longPressActive = false;
  }
}

/**
 * 重置设备配置
 * 清除WiFi设置并重启设备
 */
void resetConfig() {
  Serial.println("执行重置配置...");
  
  // LED快速闪烁表示重置进行中
  for (int i = 0; i < 10; i++) {
    digitalWrite(atoi(led_pin), !digitalRead(atoi(led_pin)));  // 翻转LED状态
    delay(100);
  }
  
  // 重置WiFi设置
  wifiManager.resetSettings();
  Serial.println("WiFi设置已重置");
  
  // 重启设备以应用新设置
  Serial.println("设备即将重启...");
  ESP.restart();
}