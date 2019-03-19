#include <vector>
#include <M5Stack.h>
#include <M5StackUpdater.h>     // https://github.com/tobozo/M5Stack-SD-Updater/
#include <M5TreeView.h>         // https://github.com/lovyan03/M5Stack_TreeView
#include <M5OnScreenKeyboard.h> // https://github.com/lovyan03/M5Stack_OnScreenKeyboard/
#include <MenuItemSD.h>
#include <MenuItemSPIFFS.h>
#include <MenuItemToggle.h>
#include <MenuItemWiFiClient.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_partition.h>
#include <nvs_flash.h>

#include "src/MenuItemSDUpdater.h"
#include "src/Header.h"
#include "src/SystemInfo.h"
#include "src/I2CScanner.h"
#include "src/WiFiWPS.h"
#include "src/BinaryViewer.h"
#include "src/CBFTPserver.h"
#include "src/CBSDUpdater.h"
#include "src/CBFSBench.h"
#include "src/CBWiFiSetting.h"
#include "src/CBWiFiSmartConfig.h"

M5TreeView treeView;
M5OnScreenKeyboard osk;
constexpr uint8_t NEOPIXEL_pin = 15;
constexpr char* preferName     ( "LovyanLauncher" );
constexpr char* preferKeyIP5306( "IP5306CTL0" );
constexpr char* preferKeyStyle ( "TVStyle" );
void drawFrame() {
  Rect16 r = treeView.clientRect;
  r.inflate(1);
  M5.Lcd.drawRect(r.x -1, r.y, r.w +2, r.h, MenuItem::frameColor[1]);
  M5.Lcd.drawRect(r.x, r.y -1, r.w, r.h +2, MenuItem::frameColor[1]);
  M5.Lcd.setTextFont(0);
  M5.Lcd.setTextColor(0x8410,0);
  M5.Lcd.drawString("- LovyanLauncher -", 207, 191, 1);
  M5.Lcd.drawString("@lovyan03    v0.1.3", 204, 201, 1);
  M5.Lcd.drawString("http://git.io/fhdJV", 204, 211, 1);
}

void setStyle(int tag)
{
  switch (tag) {
  default: return;
  case 0:
    M5ButtonDrawer::height = 14;
    M5ButtonDrawer::setTextFont(1);
    treeView.setTextFont(1);
    treeView.itemHeight = 18;
    osk.keyHeight = 14;
    osk.setTextFont(1);
    break;

  case 1:
    M5ButtonDrawer::height = 18;
    M5ButtonDrawer::setTextFont(2);
    treeView.setTextFont(2);
    treeView.itemHeight = 20;
    osk.keyHeight = 18;
    osk.setTextFont(2);
    break;

  case 2:
    M5ButtonDrawer::height = 18;
    M5ButtonDrawer::setTextFont(2);
    treeView.setFreeFont(&FreeSans9pt7b);
    treeView.itemHeight = 24;
    osk.keyHeight = 18;
    osk.setTextFont(2);
    break;
  }
  treeView.updateDest();
  M5.Lcd.fillRect(0, 218, M5.Lcd.width(), 22, 0);
}

void callBackStyle(MenuItem* sender)
{
  setStyle(sender->tag);
  Preferences p;
  p.begin(preferName);
  p.putUChar(preferKeyStyle, sender->tag);
  p.end();
}

void callBackWiFiClient(MenuItem* sender)
{
  MenuItemWiFiClient* mi = static_cast<MenuItemWiFiClient*>(sender);
  if (!mi) return;

  if (mi->ssid == "") return;

  Preferences preferences;
  preferences.begin("wifi-config");
  preferences.putString("WIFI_SSID", mi->ssid);
  String wifi_passwd = preferences.getString("WIFI_PASSWD");

  if (mi->auth != WIFI_AUTH_OPEN) {
    osk.setup(wifi_passwd);
    while (osk.loop()) { delay(1); }
    wifi_passwd = osk.getString();
    osk.close();
    WiFi.disconnect();
    WiFi.begin(mi->ssid.c_str(), wifi_passwd.c_str());
    preferences.putString("WIFI_PASSWD", wifi_passwd);
  } else {
    WiFi.disconnect();
    WiFi.begin(mi->ssid.c_str(), "");
    preferences.putString("WIFI_PASSWD", "");
  }
  preferences.end();
  while (M5.BtnA.isPressed()) M5.update();
}

void callBackWiFiOff(MenuItem* sender)
{
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect(true);
}

void callBackFormatSPIFFS(MenuItem* sender)
{
  M5.Lcd.fillRect(20, 100, 160, 30, 0);
  M5.Lcd.drawRect(23, 103, 154, 24, 0xFFFF);
  M5.Lcd.setTextFont(0);
  M5.Lcd.setTextColor(0xFFFF, 0);
  M5.Lcd.drawCentreString("SPIFFS Format...", 90, 106, 2);
  SPIFFS.begin();
  SPIFFS.format();
  SPIFFS.end();
}

void callBackFormatNVS(MenuItem* sender)
{
  M5.Lcd.fillRect(20, 100, 160, 30, 0);
  M5.Lcd.drawRect(23, 103, 154, 24, 0xFFFF);
  M5.Lcd.setTextFont(0);
  M5.Lcd.setTextColor(0xFFFF, 0);
  M5.Lcd.drawCentreString("NVS erase...", 90, 106, 2);
  nvs_flash_init();
  nvs_flash_erase();
  nvs_flash_deinit();
  nvs_flash_init();
  delay(1000);
}

void callBackDeepSleep(MenuItem* sender)
{
  M5.Lcd.setBrightness(0);
  M5.Lcd.sleep();
  esp_deep_sleep_start();
}

uint8_t getIP5306REG(uint8_t reg)
{
  Wire.beginTransmission(0x75);
  Wire.write(reg);
  if (Wire.endTransmission(false) == 0
   && Wire.requestFrom(0x75, 1)) {
    return Wire.read();
  }
  return 0;
}

void setIP5306REG(uint8_t reg, uint8_t data)
{
  Wire.beginTransmission(0x75);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

void callBackBatteryIP5306CTL0(MenuItem* sender)
{
  MenuItemToggle* mi((MenuItemToggle*)sender); 
  uint8_t data = getIP5306REG(0);
  data = mi->value ? (data | mi->tag) : (data & ~(mi->tag));
  Preferences p;
  p.begin(preferName);
  p.putUChar(preferName, data);
  p.end();
  setIP5306REG(0, data);
}

void sendNeoPixelBit(bool flg) {
  digitalWrite(NEOPIXEL_pin, HIGH);
  for (int i = 0; i < 4; ++i) digitalWrite(NEOPIXEL_pin, flg);
  digitalWrite(NEOPIXEL_pin, LOW);
}
void sendNeoPixelColor(uint32_t color) {
  for (uint8_t i = 0; i < 24; ++i) {
    sendNeoPixelBit(color & 0x800000);
    color = color << 1;
  }
}

void callBackFIRELED(MenuItem* sender)
{
  MenuItemToggle* mi((MenuItemToggle*)sender); 
  if (mi->value) {
    sendNeoPixelColor(0xFFFFFF);
  } else {
    sendNeoPixelColor(0);
  }
}

void callBackRollBack(MenuItem* sender)
{
  if( Update.canRollBack() )  {
    Update.rollBack();
    ESP.restart();
  }
}

template <class T>
void callBackExec(MenuItem* sender)
{
  T menucallback;
  menucallback(sender);
}

//======================================================================//
typedef std::vector<MenuItem*> vmi;

void setup() {
  M5.begin();
  M5.Speaker.begin();
  Wire.begin();
// for fire LED off
  pinMode(NEOPIXEL_pin, OUTPUT);
  sendNeoPixelColor(1);
  delay(1);
  sendNeoPixelColor(0);


  if(digitalRead(BUTTON_A_PIN) == 0) {
     Serial.println("Will Load menu binary");
     updateFromFS(SD);
     ESP.restart();
  }

  M5ButtonDrawer::width = 106;

  treeView.clientRect.x = 2;
  treeView.clientRect.y = 16;
  treeView.clientRect.w = 196;
  treeView.clientRect.h = 200;
  treeView.itemWidth = 176;

  treeView.useFACES       = true;
  treeView.useCardKB      = true;
  treeView.useJoyStick    = true;
  treeView.usePLUSEncoder = true;
  osk.useFACES       = true;
  osk.useCardKB      = true;
  osk.usePLUSEncoder = true;
  osk.useJoyStick    = true;

  drawFrame();

// restore setting
  Preferences p;
  p.begin(preferName, true);
  setIP5306REG(0, p.getUChar(preferName, getIP5306REG(0))
                 |((getIP5306REG(0x70) & 0x04) ? 0x20: 0) ); //When using battery, Prohibit battery non-use setting.
  setStyle(p.getUChar(preferKeyStyle, 1));
  p.end();

  treeView.setItems(vmi
               { new MenuItemSDUpdater("SD Updater", callBackExec<CBSDUpdater>)
               , new MenuItem("WiFi ", vmi
                 { new MenuItemWiFiClient("WiFi Client", callBackWiFiClient)
                 , new MenuItem("WiFi WPS", callBackExec<WiFiWPS>)
                 , new MenuItem("WiFi SmartConfig"     , callBackExec<CBWiFiSmartConfig>)
                 , new MenuItem("WiFi Setting(AP&HTTP)", callBackExec<CBWiFiSetting>)
                 , new MenuItem("WiFi Off", callBackWiFiOff)
                 } )
               , new MenuItem("Tools", vmi
                 { new MenuItem("System Info", callBackExec<SystemInfo>)
                 , new MenuItem("I2C Scanner", callBackExec<I2CScanner>)
                 , new MenuItem("FTP Server (SDCard)", callBackExec<CBFTPserverSD>)
                 , new MenuItem("FTP Server (SPIFFS)", callBackExec<CBFTPserverSPIFFS>)
                 , new MenuItem("Benchmark (SDCard)", callBackExec<CBFSBenchSD>)
                 , new MenuItem("Benchmark (SPIFFS)", callBackExec<CBFSBenchSPIFFS>)
                 , new MenuItem("Format SPIFFS", vmi
                   { new MenuItem("Format Execute", callBackFormatSPIFFS)
                   } )
                 , new MenuItem("Erase NVS(Preferences)", vmi
                   { new MenuItem("Erase Execute", callBackFormatNVS)
                   } )
                 , new MenuItem("Style ", callBackStyle, vmi
                   { new MenuItem("FreeSans9pt7b", 2)
                   , new MenuItem("Font 2" , 1)
                   , new MenuItem("Font 1", 0)
                   } )
                 } )
               , new MenuItem("Binary Viewer", vmi
                 { new MenuItemSD(    "SDCard", callBackExec<BinaryViewerFS>)
                 , new MenuItemSPIFFS("SPIFFS", callBackExec<BinaryViewerFS>)
                 , new MenuItem("FLASH", vmi
                   { new MenuItem("2nd boot loader", 0, callBackExec<BinaryViewerFlash>)
                   , new MenuItem("partition table", 1, callBackExec<BinaryViewerFlash>)
                   , new MenuItem("nvs",         0x102, callBackExec<BinaryViewerFlash>)
                   , new MenuItem("otadata",     0x100, callBackExec<BinaryViewerFlash>)
                   , new MenuItem("app0",        0x010, callBackExec<BinaryViewerFlash>)
                   , new MenuItem("app1",        0x011, callBackExec<BinaryViewerFlash>)
                   , new MenuItem("eeprom",      0x199, callBackExec<BinaryViewerFlash>)
                   , new MenuItem("spiffs",      0x182, callBackExec<BinaryViewerFlash>)
                   } )
                 } )
               , new MenuItem("Power", vmi
                 { new MenuItemToggle("BatteryCharge" , getIP5306REG(0) & 0x10, 0x10, callBackBatteryIP5306CTL0)
                 , new MenuItemToggle("BatteryOutput" , getIP5306REG(0) & 0x20, 0x20, callBackBatteryIP5306CTL0)
                 , new MenuItemToggle("Boot on load"  , getIP5306REG(0) & 0x04, 0x04, callBackBatteryIP5306CTL0)
                 , new MenuItemToggle("FIRE LED", false, callBackFIRELED)
                 , new MenuItem("DeepSleep", callBackDeepSleep)
                 })
               , new MenuItem("OTA Rollback", vmi
                   { new MenuItem("Rollback Execute", callBackRollBack)
                   } )
               } );
  treeView.begin();
}

uint8_t loopcnt = 0xF;
long lastctrl = millis();
MenuItem* miFocus;
void loop() {
  if (NULL != treeView.update()) {
    lastctrl = millis();
  }
  if (treeView.isRedraw()) {
    drawFrame();
    loopcnt = 0xF;
  }
  if (0 == (++loopcnt & 0xF)) {
    Header.draw();
    if ( 600000 < millis() - lastctrl ) {
      Serial.println( "goto sleep" );
      callBackDeepSleep(NULL);
    }
  }
}
