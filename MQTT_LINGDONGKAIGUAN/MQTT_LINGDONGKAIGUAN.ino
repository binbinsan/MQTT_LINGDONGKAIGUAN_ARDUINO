/*
 * ESP32-C2 MQTT智能开关控制程序
 * 功能：通过MQTT协议控制设备开关，支持WiFi配网，支持参数保存
 * 硬件：ESP32-C2开发板
 * 作者：bin_wang
 * 更新日期：20250326
 */

#include <WiFi.h>          // ESP32 WiFi库
#include <WiFiManager.h>    // WiFi配网管理库
#include <PubSubClient.h>   // MQTT客户端库
#include <Preferences.h>    // ESP32参数保存库

// 默认引脚定义
#define DEFAULT_BUTTON_PIN 9     // 长按重置按钮引脚
#define DEFAULT_LED_PIN 8        // 默认LED指示灯引脚
#define DEFAULT_POWER_PIN 7      // 默认电源控制引脚
#define RESET_HOLD_TIME 5000    // 长按重置时间阈值（毫秒）

// MQTT服务器参数配置（默认值）
char mqtt_server[40] = "";
char mqtt_port[6] = "1883";
char mqtt_user[40] = "";
char mqtt_password[40] = "";
char mqtt_topic[40] = "esp32";

// GPIO配置变量
char button_pin[3] = "9";    // 长按重置按钮
char led_pin[3] = "8";    // LED引脚配置
char power_pin[3] = "7";  // 电源引脚配置

WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wifiManager;
Preferences preferences;

bool shouldSaveConfig = false;
unsigned long buttonPressTime = 0;
bool longPressActive = false;

// 保存配置回调函数
void saveConfigCallback() {
  shouldSaveConfig = true;  // 标记需要保存配置
}

void setup() {
  Serial.begin(115200);  // 初始化串口通信
  
  // 从Preferences加载保存的配置
  preferences.begin("iot_config", true); // 以只读模式打开配置存储
  
  // 从存储中读取MQTT配置参数，如果没有则使用默认值
  String savedServer = preferences.getString("server", "");
  if (savedServer != "") {
    strcpy(mqtt_server, savedServer.c_str());
    strcpy(mqtt_port, preferences.getString("port", "1883").c_str());
    strcpy(mqtt_user, preferences.getString("user", "binbin").c_str());
    strcpy(mqtt_password, preferences.getString("password", "wb021102-").c_str());
    strcpy(mqtt_topic, preferences.getString("topic", "esp32c2").c_str());
  }
  
  // 加载GPIO配置
  String savedLedPin = preferences.getString("led_pin", "");
  if (savedLedPin != "") {
    strcpy(led_pin, savedLedPin.c_str());
  }
  
  String savedPowerPin = preferences.getString("power_pin", "");
  if (savedPowerPin != "") {
    strcpy(power_pin, savedPowerPin.c_str());
  }
    String savedbuttonPin = preferences.getString("button_pin", "");
  if (savedbuttonPin != "") {
    strcpy(button_pin, savedbuttonPin.c_str());
  }
  
  preferences.end();

  // 初始化GPIO引脚（使用配置的引脚号）
  pinMode(atoi(button_pin), INPUT_PULLUP);    // 设置重置按钮为上拉输入
  pinMode(atoi(led_pin), OUTPUT);             // 设置LED为输出
  pinMode(atoi(power_pin), OUTPUT);           // 设置电源控制为输出
  digitalWrite(atoi(led_pin), LOW);           // LED初始状态为关闭
  digitalWrite(atoi(power_pin), HIGH);        // 初始化时开启电源

  // 添加配置参数到WiFiManager页面
  WiFiManagerParameter custom_mqtt_server("server", "MQTT服务器", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT端口", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT用户名", mqtt_user, 40);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT密码", mqtt_password, 40);
  WiFiManagerParameter custom_mqtt_topic("topic", "MQTT主题", mqtt_topic, 40);
  
  // 添加GPIO配置参数
  WiFiManagerParameter custom_button_pin("button_pin", "下载长按重置按键", button_pin, 3);
  WiFiManagerParameter custom_led_pin("led_pin", "LED引脚号", led_pin, 3);
  WiFiManagerParameter custom_power_pin("power_pin", "电源控制引脚号", power_pin, 3);
  
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_mqtt_topic);
  wifiManager.addParameter(&custom_button_pin);
  wifiManager.addParameter(&custom_led_pin);
  wifiManager.addParameter(&custom_power_pin);

  // 设置保存配置回调
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(180);    // 配置门户超时时间180秒
  wifiManager.setDebugOutput(false);          // 关闭调试输出

  // 尝试连接WiFi
  if (!wifiManager.autoConnect("ESP32C2_AP")) {
    Serial.println("连接失败，启动配置门户");
    digitalWrite(atoi(led_pin), HIGH); // LED亮表示配置模式
    wifiManager.startConfigPortal("ESP32C2_AP");
    digitalWrite(atoi(led_pin), LOW);
  }

  // 保存从配置页面获取的参数
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
  strcpy(button_pin, custom_button_pin.getValue());
  strcpy(led_pin, custom_led_pin.getValue());
  strcpy(power_pin, custom_power_pin.getValue());

  // 如果需要保存配置
  if (shouldSaveConfig) {
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
    
    // 重新初始化GPIO引脚
    pinMode(atoi(button_pin), INPUT_PULLUP);
    pinMode(atoi(led_pin), OUTPUT);
    pinMode(atoi(power_pin), OUTPUT);
    digitalWrite(atoi(led_pin), LOW);
    digitalWrite(atoi(power_pin), HIGH);
  }

  // 设置MQTT服务器
  client.setServer(mqtt_server, atoi(mqtt_port));
  
  // 连接MQTT并发送初始消息
  if (connectMQTT()) {
    publishData("system_startup");
    delay(500);
    if (digitalRead(atoi(button_pin)) == HIGH) {
      digitalWrite(atoi(power_pin), LOW); // 关闭电源
    } 
  }
}

// 主循环函数
void loop() {
  checkResetButton();     // 检查重置按钮状态
  
  // 确保MQTT保持连接
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();          // 处理MQTT消息
  
  delay(100);            // 短暂延时防止CPU占用过高
}

/**
 * 连接MQTT服务器
 * @return bool 连接成功返回true，失败返回false
 */
bool connectMQTT() {
  int retry = 0;
  while (!client.connected() && retry < 3) {
    Serial.print("尝试MQTT连接...");
    
    if (client.connect("ESP32C2_Client", mqtt_user, mqtt_password)) {
      Serial.println("已连接");
      return true;
    } else {
      Serial.print("失败, rc=");
      Serial.print(client.state());
      Serial.println(" 2秒后重试");
      delay(2000);
      retry++;
    }
  }
  return false;
}

/**
 * 重新连接MQTT服务器并发送重连消息
 */
void reconnectMQTT() {
  if (connectMQTT()) {
    publishData("reconnected");
  }
}

/**
 * 发布MQTT消息
 * @param status 状态信息
 */
void publishData(const char* status) {
  if (client.connected()) {
    char message[50];
    snprintf(message, 50, "{\"status\":\"%s\"}", status);
    client.publish(mqtt_topic, message);
    Serial.println("MQTT数据已发送");
  }
}

/**
 * 检查重置按钮状态
 * 长按触发重置，短按可以添加其他功能
 */
void checkResetButton() {
  if (digitalRead(atoi(button_pin)) == LOW) {
    if (buttonPressTime == 0) {
      buttonPressTime = millis();
    } 
    else if (!longPressActive && (millis() - buttonPressTime > RESET_HOLD_TIME)) {
      longPressActive = true;
      resetConfig();
    }
  } else {
    if (buttonPressTime > 0 && !longPressActive) {
      // 短按可以添加其他功能
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
  // LED快速闪烁表示重置
  for (int i = 0; i < 10; i++) {
    digitalWrite(atoi(led_pin), !digitalRead(atoi(led_pin)));
    delay(100);
  }
  
  // 重置WiFi设置
  wifiManager.resetSettings();
  
  // 重启设备
  ESP.restart();
}
