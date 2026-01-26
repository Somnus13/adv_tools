#include <WiFi.h>
#include <SPI.h>
#include <FS.h>
#include <Wire.h>
#include <SD.h>
#include <M5Unified.h>
#include "M5Cardputer.h"
#include <ESP32Ping.h>


#define MP3ROOT "/mp3"
#define CACHE_LIST_PATH "/.cache_list.txt"

// micro SD
#define SD_SCK  40
#define SD_MISO 39
#define SD_MOSI 14
#define SD_CS   12





M5Canvas sprite(&M5Cardputer.Display);

// 定义设备状态
enum State {
  STATE_MENU,
  STATE_WIFI,
  STATE_TERMIANL,
  STATE_PLAYER,
  STATE_RECORD,
  STATE_SD,
  STATE_TEST,
};
State currentState = STATE_MENU;

// 菜单项
const char* menuItems[] = {"1. WiFi Connect", "2. Terminal", "3. Music Player", "4. Recorder", "5. SD", "6. test" };
int selectItem = 0;
const int totalItems = 6;


// 定义多个 wifi，选择其中一个连接
const char* wifi_ssid[] = {"mainstreet", "摸金校尉"};
const char* wifi_password[] = {"ms000609", "pd7zcfnf"};
int wifi_index = 0;
int wifi_count = sizeof(wifi_ssid) / sizeof(wifi_ssid[0]);

//ping
String inputAddr = "google.com"; // 默认地址
bool isPinging = false;

// 函数声明
void drawMenu();
void drawWifi();
void drawTerminal();
void drawPlayer();
void drawRecorder();
void drawSd();
void drawTest();

void handleMenuChange();
void handleWifiChange();
void handleTerminalChange();
// void handlePlayerChange();
// void handleRecorderChange();
// void handleSdChange();
// void handleTestChange();

void connectWifi(int index) ;

void setup(){
  Serial.begin(115200);

  // init M5Cardputer and SD card
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(100);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  // M5Cardputer.Speaker.begin();
  sprite.createSprite(240,135);

  // SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  // if (!SD.begin(SD_CS)) {
  //     Serial.println(F("ERROR: SD Mount Failed!"));
  // }
  
  delay(100);
  drawMenu();
}

void loop() {
  M5Cardputer.update();

  // btn A
  if (M5Cardputer.BtnA.wasPressed()) {
    if (currentState != STATE_MENU) {
      currentState = STATE_MENU;
      drawMenu();
    }
  }

  switch (currentState)
  {
  case STATE_MENU:
    if (M5Cardputer.Keyboard.isChange()) {

      handleMenuChange();
    }
    break;
  case STATE_WIFI:
    if (M5Cardputer.Keyboard.isChange()) {
      handleWifiChange();
    }
    break;
  case STATE_TERMIANL:
    if (M5Cardputer.Keyboard.isChange()) {
      handleTerminalChange();
    }
    break;
  case STATE_PLAYER:
    if (M5Cardputer.Keyboard.isChange()) {
      // handlePlayerChange();
    }
    break;
  case STATE_RECORD:
    if (M5Cardputer.Keyboard.isChange()) {
      // handleRecorderChange();
    }
    break;
  case STATE_SD:
    if (M5Cardputer.Keyboard.isChange()) {
      // handleSdChange();
    }
    break;
  case STATE_TEST:
    if (M5Cardputer.Keyboard.isChange()) {
      // handleTestChange();
    }
    break;
  default:
    break;
  }
  delay(100);
}

void handleMenuChange(){
  // 列表上下移动 
  if (M5Cardputer.Keyboard.isKeyPressed(';')) {
    selectItem = (selectItem - 1 + totalItems) % totalItems;
    drawMenu();
  } 
  // 列表上下移动
  if (M5Cardputer.Keyboard.isKeyPressed('.')) {
    selectItem = (selectItem + 1) % totalItems;
    drawMenu();
  } 
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
    switch (selectItem)
    {   
    case 0:
      currentState = STATE_WIFI;
      drawWifi();
      break;
    case 1:
      currentState = STATE_TERMIANL;
      drawTerminal();
      break;
    case 2:
      currentState = STATE_PLAYER;
      drawPlayer();
      break;
    case 3:
      currentState = STATE_RECORD;
      drawRecorder();
      break;
    case 4:
      currentState = STATE_SD;
      drawSd();
      break;
    case 5:
      currentState = STATE_TEST;
      drawTest();
      break;
    default:
      break;
    }
  }
}

void drawMenu(){
  Serial.println("drawMenu");
  const int ITEM_HEIGHT = 25;
  const int START_Y = 34;
  const int VISIBLE_COUNT = 4;
  const int TITLE_AREA_HEIGHT = 32; // 标题区域高度，确保菜单项不会绘制到这里  
  M5Cardputer.Display.setFont(&fonts::efontCN_16);
  // 使用 startWrite/endWrite 确保绘制操作的原子性，避免被其他绘制打断
  M5Cardputer.Display.startWrite();

  M5Cardputer.Display.fillRect(0,0,240,135,TFT_BLACK);

  int startIndex = 0;
  if (selectItem >= VISIBLE_COUNT) {
    startIndex = selectItem - (VISIBLE_COUNT - 1);
  }

  // 先绘制菜单项，确保不会超出标题区域
  for (int i = 0; i < VISIBLE_COUNT; i++)
  {
    int currentIndex = startIndex + i;
    // boundry
    if(currentIndex >= totalItems) break;

    int drawY = START_Y + (i * ITEM_HEIGHT);

    if (currentIndex == selectItem) {
      // 确保背景不会覆盖标题区域
      M5Cardputer.Display.fillRect(0, drawY, 240, ITEM_HEIGHT, TFT_GREEN);
      M5Cardputer.Display.setTextColor(TFT_BLACK); // 选中时文字黑色
    } else {
      M5Cardputer.Display.setTextColor(TFT_WHITE); // 未选中时文字白色
    }
    M5Cardputer.Display.drawString(menuItems[currentIndex], 10, drawY + 3);
  }
  
  // 最后绘制标题和分隔线，确保它们在最上层，不会被菜单项遮挡
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.drawString("Menu", 10,6);
  M5Cardputer.Display.drawFastHLine(0,31,240,TFT_DARKGREEN);
  
  M5Cardputer.Display.endWrite();
}

void drawWifi(){
  Serial.println("drawWifi");
  M5Cardputer.Display.setFont(&fonts::efontCN_16);
  M5Cardputer.Display.startWrite();
  M5Cardputer.Display.fillRect(0,0,240,135,TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.drawString("WiFi [;] Down [.] Up [Enter] Connect", 10,6);
  M5Cardputer.Display.drawFastHLine(0,31,240,TFT_DARKGREEN);
  M5Cardputer.Display.endWrite();

  // 遍历 wifi_ssid 和 wifi_password，显示 wifi 列表
  for (int i = 0; i < sizeof(wifi_ssid) / sizeof(wifi_ssid[0]); i++) {
    if (i == wifi_index) {
      M5Cardputer.Display.setTextColor(TFT_GREEN);
    } else {
      M5Cardputer.Display.setTextColor(TFT_WHITE);
    }
    M5Cardputer.Display.drawString(wifi_ssid[i], 10, 32 + (i * 25));
  }
  
}
void handleWifiChange(){
  if (M5Cardputer.Keyboard.isKeyPressed(';')) {
    wifi_index = (wifi_index - 1 + wifi_count) % wifi_count;
    drawWifi();
  }
  if (M5Cardputer.Keyboard.isKeyPressed('.')) {
    wifi_index = (wifi_index + 1) % wifi_count;
    drawWifi();
  }
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
    connectWifi(wifi_index);
  }
}

void connectWifi(int index) {
  Serial.println("Connecting to WiFi: " + String(wifi_ssid[wifi_index]));
  // 依次尝试连接 wifi
  WiFi.begin(wifi_ssid[index], wifi_password[index]);
  M5Cardputer.Display.setCursor(10,110);
  M5Cardputer.Display.print("Connecting." );
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    M5Cardputer.Display.print(".");
    count++;
    if (count > 10) {
      M5Cardputer.Display.println("WiFi connection failed");
      return;
    }
    delay(1000);
  }
  M5Cardputer.Display.fillRect(0,110,240,25,TFT_BLACK);
  M5Cardputer.Display.setCursor(10,110);

  M5Cardputer.Display.println("WiFi connected");
  Serial.println("WiFi connected");
}

void drawTerminal(){
  Serial.println("drawTerminal");
  M5Cardputer.Display.setFont(&fonts::efontCN_16);

  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setCursor(10, 0);
  M5Cardputer.Display.setTextColor(TFT_GREEN);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.println("    --- Ping Terminal ---    ");
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.println("Type IP/Host and press Enter to Ping, [Go] Back");
  M5Cardputer.Display.setCursor(10,112);

  M5Cardputer.Display.print("> ");
  inputAddr = "";
  
}

void handleTerminalChange(){
  Serial.println("handleTerminalChange");

  if (WiFi.status() != WL_CONNECTED) {
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.println("Error: WiFi Not Connected!");
    delay(2000);
    return; 
  }
  auto status = M5Cardputer.Keyboard.keysState();
  
  for (auto c : status.word) {
      inputAddr += c; // 拼接输入
      M5Cardputer.Display.print(c);
  }

  // 处理退格键 (Backspace)
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) && inputAddr.length() > 0) {
      inputAddr.remove(inputAddr.length() - 1);
      // 简单的屏幕擦除逻辑（擦除最后一个字符位置）
      int x = M5Cardputer.Display.getCursorX();
      int y = M5Cardputer.Display.getCursorY();
      M5Cardputer.Display.fillRect(x - 10, y, 10, 20, TFT_BLACK);
      M5Cardputer.Display.setCursor(x - 10, y);
  }

  // 2. 按下回车开始 Ping
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
    // 检查 inputAddr 的合法性
    if (inputAddr.trim().length() == 0) {
      M5Cardputer.Display.setTextColor(TFT_RED);
      M5Cardputer.Display.println("Error: Input is empty!");
      delay(2000);
      return;
    }
    // 检查 inputAddr 是否为 IP 地址 或者 域名  xxx.xxx.xxx.xxx 或者 xxx.xxx
    if (inputAddr.indexOf('.') != -1) {
      M5Cardputer.Display.setTextColor(TFT_RED);
      M5Cardputer.Display.println("Error: Input is not a valid IP address or domain name!");
      delay(2000);
      return;
    }

    // 清空上一次的结果
    M5Cardputer.Display.fillRect(0,50,240,80,TFT_BLACK);
    M5Cardputer.Display.setCursor(10,50);
    M5Cardputer.Display.println();
    M5Cardputer.Display.printf("Pinging %s...\n", inputAddr.c_str());
    
    bool success = Ping.ping(inputAddr.c_str(), 4); // Ping 3次
    
    if (success) {
        float avgTime = Ping.averageTime();
        M5Cardputer.Display.setTextColor(TFT_GREEN);
        M5Cardputer.Display.printf("Success! Avg: %.2fms\n", avgTime);
    } else {
        M5Cardputer.Display.setTextColor(TFT_RED);
        M5Cardputer.Display.println("Ping Failed (Timeout)");
    }
    
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.println();
    M5Cardputer.Display.print("> "); // 提示符
    inputAddr = ""; // 清空缓存准备下一次输入
  }
  

}

void drawPlayer(){
  Serial.println("drawPlayer");
  M5Cardputer.Display.setFont(&fonts::efontCN_16);
  M5Cardputer.Display.startWrite();
  M5Cardputer.Display.fillRect(0,0,240,135,TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.drawString("Player", 10,6);
  M5Cardputer.Display.drawFastHLine(0,31,240,TFT_DARKGREEN);
  M5Cardputer.Display.endWrite();
}

void drawRecorder(){
  Serial.println("drawRecorder");
  M5Cardputer.Display.setFont(&fonts::efontCN_16);
  M5Cardputer.Display.startWrite();
  M5Cardputer.Display.fillRect(0,0,240,135,TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.drawString("Recorder", 10,6);
  M5Cardputer.Display.drawFastHLine(0,31,240,TFT_DARKGREEN);
  M5Cardputer.Display.endWrite();
}

void drawSd(){
  Serial.println("drawSd");
  M5Cardputer.Display.setFont(&fonts::efontCN_16);
  M5Cardputer.Display.startWrite();
  M5Cardputer.Display.fillRect(0,0,240,135,TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.drawString("SD", 10,6);
  M5Cardputer.Display.drawFastHLine(0,31,240,TFT_DARKGREEN);
  M5Cardputer.Display.endWrite();
}

void drawTest(){
  Serial.println("drawTest");
  M5Cardputer.Display.setFont(&fonts::efontCN_16);
  M5Cardputer.Display.startWrite();
  M5Cardputer.Display.fillRect(0,0,240,135,TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.drawString("Test", 10,6);
  M5Cardputer.Display.drawFastHLine(0,31,240,TFT_DARKGREEN);
  M5Cardputer.Display.endWrite();
}