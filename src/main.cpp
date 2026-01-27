#include <WiFi.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <M5Unified.h>
#include "M5Cardputer.h"
#include <ESP32Ping.h>
#include "Audio.h"  // https://github.com/schreibfaul1/ESP32-audioI2S  

// micro SD
#define SD_SCK  40
#define SD_MISO 39
#define SD_MOSI 14
#define SD_CS   12

#define MP3ROOT "/mp3"
#define CACHE_LIST_PATH "/.cache_list.txt"
// I2S
#define I2S_DOUT 42
#define I2S_BCLK 41
#define I2S_LRCK 43
#define MAX_FILES 100


Audio audio;
M5Canvas sprite(&M5Cardputer.Display);

// 定义设备状态
enum State {
  STATE_MENU,
  STATE_WIFI,
  STATE_TERMIANL,
  STATE_WIFI_SCAN,
  STATE_PLAYER,
  STATE_RECORD,
  STATE_SD,
  STATE_TEST,
};
State currentState = STATE_MENU;

//  ====================菜单项============================
const char* menuItems[] = {
  "1. WiFi Connect",
  "2. Ping Terminal", 
  "3. WiFi Scan", 
  "4. Player", 
  "5. Recorder", 
  "6. SD", 
  "7. Test" 
};
int selectItem = 0;
const int totalItems = 7;
// ========================================================


//  ====================wifi============================
// 定义多个 wifi，选择其中一个连接
const char* wifi_ssid[] = {"mainstreet", "摸金校尉"};
const char* wifi_password[] = {"ms000609", "pd7zcfnf"};
int wifi_index = 0;
int wifi_count = sizeof(wifi_ssid) / sizeof(wifi_ssid[0]);
// ========================================================


// ====================ping============================
String inputAddr = "google.com"; // 默认地址
// ========================================================


// ====================WiFi scan============================
// WiFi scan
bool isScanning = false;
int scanTimeout = 10;
// 暂存扫描结果
// {ssid: string, rssi: int}
std::vector<std::pair<String, int32_t>> scanResults;
int page = 0;
int pageSize = 4;
// ========================================================

// ====================Player============================
// Player
enum PlayerMode { SEQ, SHUFFLE, REPEAT };
PlayerMode playerMode = SHUFFLE;
bool isPlaying = false;
int playSeconds = 0;
int totalSeconds = 0;

// 模拟均衡器数据
int eqHeights[10] = {0};


// 存储文件名（用于显示）
String audioFileNames[MAX_FILES];
// 存储完整路径（用于播放）
String audioFilePaths[MAX_FILES];
int fileCount = 0;
int n = 0;
int volume = 2; //2 - 10
int bri = 0;
int brightness[5] = {50, 100, 150, 200, 250};
// ========================================================

//  ====================函数声明============================
void drawMenu();
void drawWifi();
void drawTerminal();
void drawWiFiScan();
void drawPlayer();
void drawRecorder();
void drawSd();
void drawTest();

void handleMenuChange();
void handleWifiChange();
void handleTerminalChange();
void handleWiFiScanChange();
void handlePlayerChange();

void connectWifi(int index);
void scanWiFi();
void drawScanResults();
void initPlayer();
void listFiles(fs::FS &fs, const char* path, uint8_t levels);
void audio_eof_mp3(const char *info);
bool hasExtension(const char* filename, const char* ext);



void setup(){
  Serial.begin(115200);

  // init M5Cardputer and SD card
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(brightness[bri]);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  sprite.createSprite(240,135);
  
  delay(100);
  drawMenu();
}

void loop() {
  M5Cardputer.update();

  // btn A 返回菜单
  if (M5Cardputer.BtnA.wasPressed()) {
    if (currentState != STATE_MENU) {
      // reset player
      if (currentState == STATE_PLAYER) {
        audio.stopSong(); // 停止播放而不是暂停
        isPlaying = false;
      }
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
  case STATE_WIFI_SCAN:
    if (M5Cardputer.Keyboard.isChange()) {
      handleWiFiScanChange();
    }
    break;
  case STATE_PLAYER:
    // 必须频繁调用 audio.loop() 才能流畅播放音频，不能有延迟
    audio.loop();
    
    if (M5Cardputer.Keyboard.isChange()) {
      handlePlayerChange();
    }
    
    // 更新播放时间和总时长（降低更新频率，减少 CPU 占用）
    static unsigned long lastTimeUpdate = 0;
    if (isPlaying && millis() - lastTimeUpdate > 200) {
      playSeconds = audio.getAudioCurrentTime();
      totalSeconds = audio.getAudioFileDuration();
      lastTimeUpdate = millis();
    }

    // 降低 UI 刷新频率，减少对音频播放的影响（500ms 刷新一次）
    static unsigned long lastDraw = 0;
    if (millis() - lastDraw > 500) {
        drawPlayer();
        lastDraw = millis();
    }
    break;
  case STATE_RECORD:
    // TODO: 实现录音功能
    break;
  case STATE_SD:
    // TODO: 实现 SD 卡管理功能
    break;
  case STATE_TEST:
    // TODO: 实现测试功能
    break;
  default:
    break;
  }
  
  // 只在非播放器状态下使用 delay，播放器状态需要频繁调用 audio.loop()
  if (currentState != STATE_PLAYER) {
    delay(100);
  } else {
    // 播放器状态下使用小延迟，确保 audio.loop() 能频繁调用
    delay(10);
  }
}

void handleMenuChange(){
  // 列表上下移动
  if (M5Cardputer.Keyboard.isKeyPressed(';')) {
    selectItem = (selectItem - 1 + totalItems) % totalItems;
    drawMenu();
  } 
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
      currentState = STATE_WIFI_SCAN;
      drawWiFiScan();
      break;
    case 3:
      currentState = STATE_PLAYER;
      drawPlayer();
      initPlayer();
      break;
    case 4:
      currentState = STATE_RECORD;
      drawRecorder();
      break;
    case 5:
      currentState = STATE_SD;
      drawSd();
      break;
    case 6:
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
    inputAddr.trim(); // trim() 返回 void，需要先调用
    if (inputAddr.length() == 0) {
      M5Cardputer.Display.setTextColor(TFT_RED);
      M5Cardputer.Display.println("Error: Input is empty!");
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

void drawWiFiScan(){
  Serial.println("drawWiFiScan");
  M5Cardputer.Display.setFont(&fonts::efontCN_16);
  M5Cardputer.Display.startWrite();
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setCursor(10, 0);
  M5Cardputer.Display.setTextColor(TFT_GREEN);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.println("    --- WiFi Scan ---    ");
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.println("Scanning WiFi nearby [Enter] Start [R] Rescan");
}

void handleWiFiScanChange(){
  if (isScanning) {
    return;
  }
  // enter scan  r rescan
  Serial.println("handleWiFiScanChange");
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
    scanWiFi();
  }
  if (M5Cardputer.Keyboard.isKeyPressed('r')) {
    scanWiFi();
  }
  if (M5Cardputer.Keyboard.isKeyPressed(';')) {
    if (scanResults.size() == 0) {
      return;
    }
    // 计算总页数：向上取整 (size + pageSize - 1) / pageSize
    int totalPages = (scanResults.size() + pageSize - 1) / pageSize;
    page = (page - 1 + totalPages) % totalPages;
    drawScanResults();
  }
  if (M5Cardputer.Keyboard.isKeyPressed('.')) {
    if (scanResults.size() == 0) {
      return;
    }
    // 计算总页数：向上取整 (size + pageSize - 1) / pageSize
    int totalPages = (scanResults.size() + pageSize - 1) / pageSize;
    page = (page + 1) % totalPages;
    drawScanResults();
  }
}

/*
RSSI 信号强度解释：

-30 dBm：极强（就在路由器旁边）。

-60 dBm：良好，非常稳定。

-80 dBm：较弱，连接可能会掉线。

-90 dBm：极弱，几乎不可用。 
*/
void scanWiFi(){
  Serial.println("scanWiFi");
  isScanning = true;
  page = 0;
  scanResults.clear();
  
  // 显示扫描中状态
  M5Cardputer.Display.startWrite();
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setCursor(10, 0);
  M5Cardputer.Display.setTextColor(TFT_GREEN);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.println("    --- WiFi Scan ---    ");
  M5Cardputer.Display.setTextColor(TFT_YELLOW);
  M5Cardputer.Display.setCursor(10, 32);
  M5Cardputer.Display.println("Scanning...");
  M5Cardputer.Display.endWrite();

  // 开始扫描
  int n = WiFi.scanNetworks();
  
  // 等待扫描完成，显示进度
  int count = 0;
  int dotCount = 0;
  while (count < scanTimeout * 10) { // scanTimeout 是秒数，这里每 100ms 检查一次
    if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
      break;
    }
    // 更新扫描进度显示
    if (count % 10 == 0) { // 每秒更新一次
      M5Cardputer.Display.setCursor(10, 32);
      M5Cardputer.Display.fillRect(10, 32, 230, 20, TFT_BLACK);
      M5Cardputer.Display.setTextColor(TFT_YELLOW);
      M5Cardputer.Display.print("Scanning");
      for (int i = 0; i < (dotCount % 4); i++) {
        M5Cardputer.Display.print(".");
      }
      dotCount++;
    }
    delay(100);
    count++;
  }
  
  if (count >= scanTimeout * 10) {
    M5Cardputer.Display.setCursor(10, 32);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.println("Scan timed out");
    isScanning = false;
    return;
  }
  
  // 扫描完成，显示结果
  M5Cardputer.Display.startWrite();
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.printf("Found %d Networks:\n", n);
  M5Cardputer.Display.endWrite();

  if (n == 0) {
    M5Cardputer.Display.setCursor(10, 40);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.println("No networks found.");
  } else {
    scanResults.clear();
    for (int i = 0; i < n; ++i) {
      scanResults.push_back(std::make_pair(WiFi.SSID(i), WiFi.RSSI(i)));
    }
    // 排序 强到弱
    std::sort(scanResults.begin(), scanResults.end(), [](const std::pair<String, int32_t>& a, const std::pair<String, int32_t>& b) {
      return a.second > b.second;
    });
    drawScanResults();
  }
  isScanning = false;
}

void drawScanResults(){
  // 清空结果区域（保留标题）
  M5Cardputer.Display.startWrite();
  // 清空列表区域（y: 20-110）和分页信息区域（y: 110-135）
  M5Cardputer.Display.fillRect(0, 20, 240, 115, TFT_BLACK);
  M5Cardputer.Display.setCursor(0, 20);
  
  const int startIndex = page * pageSize;
  // 展示 4 个，上下翻页
  for (int i = startIndex; i < startIndex + pageSize && i < scanResults.size(); ++i) {
    // 信号强度指示 (RSSI)
    // RSSI 是负值，值越大（越接近0）信号越强
    // 例如：-30 dBm 很强，-60 dBm 中等，-90 dBm 很弱
    int32_t rssi = scanResults[i].second;
    uint32_t color;
    // 修复颜色显示：直接反转比较逻辑
    // 如果信号差的显示绿色，说明需要反转判断
    // 尝试：RSSI 值越小（越负）信号越差，应该显示红色
    if (rssi < -80) {
      color = 0xFFFF0000;    // 信号弱（-80, -90 等）-> 红色
    } else if (rssi < -60) {
      color = 0xFFFFFF00; // 信号中等（-60, -70 等）-> 黄色
    } else {
      color = 0xFF00FF00;  // 信号强（-30, -40, -50 等）-> 绿色
    }
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.printf("%d: %s", i + 1, scanResults[i].first.c_str());
    
    M5Cardputer.Display.setTextColor(color);
    M5Cardputer.Display.printf(" (%ddBm)\n", rssi);
  }
  
  // 清空分页信息区域，确保不被遮挡
  M5Cardputer.Display.fillRect(0, 110, 240, 25, TFT_BLACK);
  
  // 显示分页信息和操作提示
  int totalPages = (scanResults.size() + pageSize - 1) / pageSize;
  M5Cardputer.Display.setCursor(10, 116);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.printf("Page: %d/%d [;] Prev [.] Next ", page + 1, totalPages);
  M5Cardputer.Display.endWrite();
}


void drawPlayer(){
  sprite.fillScreen(TFT_BLACK);
    
  sprite.setFont(&fonts::efontCN_12);
  sprite.fillRect(0, 0, 240, 25, TFT_DARKGREY); // 深灰背景
  sprite.setTextColor(TFT_BLACK);
  sprite.setTextDatum(middle_left);
  // 修复：使用 String 类拼接字符串
  String displayName = "> " + audioFileNames[n];
  sprite.drawString(displayName.c_str(), 5, 12);

  // 播放/暂停图标
  sprite.setFont(&fonts::efontCN_24);
  sprite.setTextDatum(middle_center);
  sprite.setTextColor(isPlaying ? TFT_GREEN : TFT_RED);
  sprite.drawString(isPlaying ? "PLAYING" : "PAUSED", 120, 45);

   // --- 2. 中部：假的均衡器动效 (Visualizer) ---
  // 优化：减少随机数生成，降低 CPU 占用
  int eqBaseX = 40;
  static unsigned long lastEqUpdate = 0;
  if (millis() - lastEqUpdate > 100) { // 每 100ms 更新一次均衡器
    for (int i = 0; i < 10; i++) {
        if (isPlaying) {
            eqHeights[i] = random(5, 30); // 播放时跳动
        } else {
            // 修复：使用三元运算符替代 max 函数，避免头文件依赖
            eqHeights[i] = (eqHeights[i] - 2 > 2) ? (eqHeights[i] - 2) : 2; // 暂停时缓缓落下
        }
    }
    lastEqUpdate = millis();
  }
  // 绘制均衡器
  for (int i = 0; i < 10; i++) {
      sprite.fillRect(eqBaseX + i * 16, 90 - eqHeights[i], 12, eqHeights[i], TFT_GREEN);
  }
  // seq  00:00  vol
  sprite.setFont(&fonts::efontCN_16);
  sprite.setTextDatum(middle_center);
  sprite.setTextColor(TFT_GREEN);
  // 修复：将枚举转换为字符串，不能直接用 %s 格式化枚举
  const char* modeStr = "";
  switch (playerMode) {
    case SEQ: modeStr = "SEQ"; break;
    case SHUFFLE: modeStr = "RANDOM"; break;
    case REPEAT: modeStr = "LOOP"; break;
  }
  sprite.drawString(modeStr, 60, 100);

  int curMin = playSeconds / 60;
  int curSec = playSeconds % 60;

  sprite.setTextColor(TFT_WHITE);
  sprite.setTextDatum(middle_center);
  // 格式化时间 00:00（Arduino String 没有 padStart，手动格式化）
  char timeBuf[10];
  sprintf(timeBuf, "%02d:%02d", curMin, curSec);
  sprite.drawString(timeBuf, 120, 100);

  // vol 进度条：20x4，滑块：4x6
  // 绘制背景条
  sprite.fillRoundRect(170, 100, 20, 4, 2, TFT_GREEN);
  // 计算滑块位置：volume 范围 2-10，映射到 0-20 的进度条
  // 修复：先乘后除，避免整数除法精度丢失
  int percent = (volume - 2) * 20 / 8; // (volume-2) 范围 0-8，映射到 0-20
  // 绘制滑块（滑块宽度4，居中在进度条上）
  sprite.fillRect(170 + percent, 99, 4, 6, TFT_DARKGREY);
  
  // bottom
  sprite.fillRect(0, 115, 240, 20, TFT_DARKGREY);
  sprite.setFont(&fonts::efontCN_12);
  sprite.setTextColor(TFT_BLACK);
  sprite.setTextDatum(middle_center);
  sprite.drawString("[ENTER] P/P   [M] Mode   [Go] Exit", 120, 125);

  sprite.pushSprite(0, 0);
  
}


void handlePlayerChange(){
  // [ENTER] P/P   [M] Mode   [Go] Exit
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
    isPlaying = !isPlaying;
    audio.pauseResume(); // 暂停/播放
    drawPlayer(); // 立即刷新 UI 显示状态变化
  }
  if (M5Cardputer.Keyboard.isKeyPressed('M') || M5Cardputer.Keyboard.isKeyPressed('m')) {
    playerMode = (PlayerMode)((playerMode + 1) % 3);
    drawPlayer(); // 立即刷新 UI 显示状态变化
  }
  // n 下一首 p 上一首（根据播放模式选择）
  if (M5Cardputer.Keyboard.isKeyPressed('n')) {
    // 修复爆音：先停止当前播放
    audio.stopSong();
    delay(50); // 等待音频缓冲区清空
    
    // 根据播放模式选择下一首
    if (playerMode == SHUFFLE) {
      // 随机模式：随机选择下一首
      n = random(0, fileCount);
    } else if (playerMode == REPEAT) {
      // 重复模式：保持当前歌曲不变（或者也可以选择下一首，这里选择下一首）
      n = (n + 1) % fileCount;
    } else {
      // 顺序模式：顺序播放下一首
      n = (n + 1) % fileCount;
    }
    
    drawPlayer(); // 立即刷新 UI 显示状态变化
    // 播放下一首
    audio.connecttoFS(SD, audioFilePaths[n].c_str());
    isPlaying = true; // 自动开始播放
  }
  if (M5Cardputer.Keyboard.isKeyPressed('p')) {
    // 修复爆音：先停止当前播放
    audio.stopSong();
    delay(50); // 等待音频缓冲区清空
    
    // 根据播放模式选择上一首
    if (playerMode == SHUFFLE) {
      // 随机模式：随机选择上一首（实际也是随机）
      n = random(0, fileCount);
    } else if (playerMode == REPEAT) {
      // 重复模式：保持当前歌曲不变（或者也可以选择上一首，这里选择上一首）
      n = (n - 1 + fileCount) % fileCount;
    } else {
      // 顺序模式：顺序播放上一首
      n = (n - 1 + fileCount) % fileCount;
    }
    
    drawPlayer(); // 立即刷新 UI 显示状态变化
    // 播放上一首
    audio.connecttoFS(SD, audioFilePaths[n].c_str());
    isPlaying = true; // 自动开始播放
  }
  // j vol — k vol +
  if (M5Cardputer.Keyboard.isKeyPressed('j')) {
    volume -= 2;
    if (volume < 2) {
      volume = 2;
    }
    audio.setVolume(volume);
    drawPlayer(); // 立即刷新 UI 显示状态变化
  }
  if (M5Cardputer.Keyboard.isKeyPressed('k')) {
    volume += 2;
    if (volume > 10) {
      volume = 10;
    }
    audio.setVolume(volume);
    drawPlayer();
  }
  // v brightness
  if (M5Cardputer.Keyboard.isKeyPressed('v')) {
    bri = (bri + 1) % 5;
    M5Cardputer.Display.setBrightness(brightness[bri]);
  }
}

void initPlayer(){
  static bool initialized = false;
  
  // 只初始化一次，避免重复初始化
  if (initialized) {
    Serial.println("Player already initialized, skipping...");
    return;
  }
  
  Serial.println("Initializing player...");
  M5Cardputer.Speaker.begin();
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS)) {
      Serial.println(F("ERROR: SD Mount Failed!"));
      return;
  }

  // 如果缓存列表文件不存在。则创建一个空文件
  if (!SD.exists(CACHE_LIST_PATH)) { 
    File file = SD.open(CACHE_LIST_PATH, FILE_WRITE);
    file.close();
    listFiles(SD, MP3ROOT, MAX_FILES);
  } else {
    File file = SD.open(CACHE_LIST_PATH, FILE_READ);
    Serial.println("Reading cache file");
    if (file) {
      String line = file.readStringUntil('\n');
      fileCount = line.toInt();
      Serial.printf("Found %d files in cache\n", fileCount);
      for(int i = 0; i < fileCount && i < MAX_FILES; i++) {
        audioFileNames[i] = file.readStringUntil('\n');
        audioFilePaths[i] = file.readStringUntil('\n');
      }
      file.close();
    } else {
      // 如果读取失败，重新扫描文件
      Serial.println("Cache file read failed, rescanning...");
      listFiles(SD, MP3ROOT, MAX_FILES);
    }
  }

  // init audio output
  audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
  audio.setVolume(volume); // 2...10
  // 设置音频结束回调函数（ESP32-audioI2S 会自动调用 audio_eof_mp3）
  audio.setFileLoop(false);
  
  if (fileCount > 0) {
    Serial.printf("Connecting to file: %s\n", audioFilePaths[n].c_str());
    audio.connecttoFS(SD, audioFilePaths[n].c_str());
    isPlaying = true; // 自动开始播放
  } else {
    Serial.println("No audio files found!");
  }
  
  initialized = true;
  Serial.println("Player initialization complete");
}

// 检查文件是否有指定扩展名
bool hasExtension(const char* filename, const char* ext) {
  if (!filename || !ext) return false;
  int filenameLen = strlen(filename);
  int extLen = strlen(ext);
  if (extLen > filenameLen) return false;
  return strcasecmp(filename + filenameLen - extLen, ext) == 0;
}

void audio_eof_mp3(const char *info) {
  Serial.print("eof_mp3   ");
  Serial.println(info);
  
  if (fileCount == 0) {
    isPlaying = false;
    return;
  }
  
  // 修复爆音：先停止当前播放，等待音频完全停止
  audio.stopSong();
  // 等待音频缓冲区清空，避免爆音
  delay(50);
  
  if (playerMode == SHUFFLE) {
    // 随机模式：随机选择下一首
    n = random(0, fileCount);
  } else if (playerMode == REPEAT) {
    // 重复模式：重新播放当前歌曲（n 不变）
  } else {
    // 顺序模式：顺序播放下一首
    n = (n + 1) % fileCount;
  }
  
  // 使用完整路径播放下一首
  if (n < fileCount && fileCount > 0) {
    Serial.printf("Playing next: %s\n", audioFilePaths[n].c_str());
    audio.connecttoFS(SD, audioFilePaths[n].c_str());
    isPlaying = true; // 确保播放状态为 true
  } else {
    isPlaying = false;
  }
}


void listFiles(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listen directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
      Serial.println("Failed to open directory");
      return;
  }
  if (!root.isDirectory()) {
      Serial.println("Not a directory");
      root.close();
      return;
  }

  File file = root.openNextFile();
  while (file && fileCount < MAX_FILES) {
    if (file.isDirectory()) {
      const char* name = file.name();
      Serial.print("DIR: ");
      Serial.println(name);

      if (strcmp(name, "System Volumn Information") == 0 || strcmp(name, "LOST.DIR") == 0 || strcmp(name, "lost+found") == 0 || name[0] == '.') {
        file.close();
        file = root.openNextFile();
        continue;
      }

      if (levels > 0 ) {
        String fullPath = String(dirname);
        if (!fullPath.endsWith("/")) fullPath += "/";
        fullPath += name;
        file.close();
        listFiles(fs, fullPath.c_str(), levels = 1);
      }
    } else {
      const char* name = file.name();
      // 非隐藏文件 非.xxx开头
      if (hasExtension(name, ".mp3") && name[0] != '.') {
        Serial.print("FILE: ");
        Serial.println(name);

        // 构建完整路径
        String fullPath = String(dirname);
        if (!fullPath.endsWith("/")) fullPath += "/";
        fullPath += name;
        
        // 存储文件名（用于显示）
        audioFileNames[fileCount] = String(name);
        // 存储完整路径（用于播放）
        audioFilePaths[fileCount] = fullPath;

        fileCount++;
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  // 将文件名和完整路径写入缓存文件 
  File fileCache = SD.open(CACHE_LIST_PATH, FILE_WRITE);
  fileCache.println(fileCount);
  for(int i = 0; i < fileCount; i++) {
    fileCache.println(audioFileNames[i]);
    fileCache.println(audioFilePaths[i]);
  }
  fileCache.close();
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