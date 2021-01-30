/*
  M5StickC - ESP32 remote CW keyer by OK1CDJ ondra@ok1cdj.com
  -----------------------------------------------------------
  Primary desiged for M5StickC but can be used with any ESP32 board
  If you wold like try another ESP32 board emove #define M5Stick and correct Pin assigment

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

  Features:
          Responsive Web interface based on Boostrap
          http based service for remote control from another appication
          Simple linux cwdaemon emulation - currently support only, send message, change speed and break trasmition
                                          - tested with Tucnak VHF Contest log http://tucnak.nagano.cz/wiki/Main_Page

  Usage:
          CW output is on G26 - connect optocoupler or transistor for keying and connect radio
          UDP server listen on 6789
          WEB http://url/?apikey=XXXXXXXX when button A is pressed QR code with url is displayed
          Config - http://url/cfg or hold button B on start when button A is pressed QR code with url is displayed
          API http://url/sendmorse?apikey=XXXXXX&message=ok1cdj&speed=33

  TODO:
          make compatibility with ESP32 with ethernet like OLIMEX ESP32-POE
*/

#define M5Stick

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncUDP.h>

#ifdef M5Stick
#include <M5StickC.h>
#endif

#include <SPIFFS.h>
#include <Preferences.h>

#define UDP_PORT 6789 // 

// Pin assigment
#define LED_PIN 10
#define CW_PIN 26



AsyncWebServer server(80);
AsyncUDP udp;
Preferences preferences;

String ssid       = "";
String password   = "";
String apikey     = "";
String sIP        = "";
String sGateway   = "";
String sSubnet    = "";
String sPrimaryDNS = "";
String sSecondaryDNS = "";


String hMessage = "Remote CW Keyer by @OK1CDJ";

// web server requests
const char* PARAM_MESSAGE = "message";
const char* PARAM_SPEED = "speed";
const char* PARAM_SSID = "ssid";
const char* PARAM_PASSWORD = "password";
const char* PARAM_APIKEY = "apikey";
const char* PARAM_WIFI = "wifi";
const char* PARAM_CON = "con";

// CW variables
String message;
String sspeed = "20";

bool wifiConfigRequired = false;
bool dhcp = true; // dhcp enable disble

IPAddress IP;
IPAddress gateway;
IPAddress subnet;
IPAddress primaryDNS;
IPAddress secondaryDNS;



struct t_mtab {
  char c, pat;
} ;

struct t_mtab morsetab[] = {
  {'.', 106},
  {',', 115},
  {'?', 76},
  {'/', 41},
  {'A', 6},
  {'B', 17},
  {'C', 21},
  {'D', 9},
  {'E', 2},
  {'F', 20},
  {'G', 11},
  {'H', 16},
  {'I', 4},
  {'J', 30},
  {'K', 13},
  {'L', 18},
  {'M', 7},
  {'N', 5},
  {'O', 15},
  {'P', 22},
  {'Q', 27},
  {'R', 10},
  {'S', 8},
  {'T', 3},
  {'U', 12},
  {'V', 24},
  {'W', 14},
  {'X', 25},
  {'Y', 29},
  {'Z', 19},
  {'1', 62},
  {'2', 60},
  {'3', 56},
  {'4', 48},
  {'5', 32},
  {'6', 33},
  {'7', 35},
  {'8', 39},
  {'9', 47},
  {'0', 63}
} ;

#define N_MORSE  (sizeof(morsetab)/sizeof(morsetab[0]))

int dotlen;
int dashlen;
int speed = 20;

char *str;


// Morse code tokens. 0 = dit, 1 = dah any other character will generate 8 dit error.

// NOTE: These need to be in ASCII code sequence to work

char tokens [][7] = { "110011", "100001", // ,,-
                      "010101", "10010", // .,/
                      "11111", "01111", "00111", "00011", "00001", "00000", // 0,1,2,3,4,5
                      "10000", "11000", "11100", "11110", // 6,7,8,9
                      "111000", "10101", "<", // :,;,<"=",">","001100", // =,>,?
                      "@", // @,
                      "01", "1000", "1010", // A,B,C
                      "100", "0", "0010", // D,E,F
                      "110", "0000", "00", // G,H,I
                      "0111", "101", "0100", // J,K,L
                      "11", "10", "111", // M,N,O
                      "0110", "1101", "010", // P,Q,R
                      "000", "1", "001", // S,T,U
                      "0001", "011", "1001", // V,W,X
                      "1011", "1100"
                    }; // Y,Z

// Icons
extern const unsigned char bat0_icon16x16[];
extern const unsigned char bat1_icon16x16[];
extern const unsigned char bat2_icon16x16[];
extern const unsigned char bat3_icon16x16[];
extern const unsigned char bat4_icon16x16[];
extern const unsigned char plug_icon16x16[];
extern const unsigned char wifi1_icon16x16[];

// Routines to output a dahs and dit

void dit()
{

  digitalWrite(LED_PIN, 0);
  digitalWrite(CW_PIN, 1);
  delay(dotlen);
  digitalWrite(CW_PIN, 0);
  digitalWrite(LED_PIN, 1);
  delay(dotlen);
}


void dash()
{
  digitalWrite(LED_PIN, 0);
  digitalWrite(CW_PIN, 1);
  delay(dashlen);
  digitalWrite(CW_PIN, 0);
  digitalWrite(LED_PIN, 1);
  delay(dotlen);
}


// Look up a character in the tokens array and send it

void send(char c)
{
  int i ;
  if (c == ' ') {

    delay(7 * dotlen) ;
    return ;
  }
  for (i = 0; i < N_MORSE; i++) {
    if (morsetab[i].c == c) {
      unsigned char p = morsetab[i].pat ;

      while (p != 1) {
        if (p & 1)
          dash() ;
        else
          dit() ;
        p = p / 2 ;
      }
      delay(2 * dotlen) ;
      return ;
    }
  }
  //if we drop off the end, then we send a space

}


void sendmsg(char *str)
{
  while (*str)
    send(*str++) ;

}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}


// Send morse code in another task to keep runnig during web requests

void SendMorse ( void * parameter) {
  Serial.print("Send morse running on core ");
  Serial.println(xPortGetCoreID());
  for (;;) { //infinite loop
    if (message != "") {
      message.toUpperCase();
      int stri = message.length();
      for (int i = 0; i < stri; i++) {
        char c = message.charAt(i);
        send(c);
        if (stri < message.length()) {
          stri = message.length();
        }
      }
      message = "";
    }
    vTaskDelay(10);
  }
}


void update_speed() {
  dotlen = (1200 / speed);
  dashlen = (3 * (1200 / speed));
}

#ifdef M5Stick
int8_t getBatteryLevel()
{

  float dta;
  int8_t batLev = 0;
  dta = M5.Axp.GetBatVoltage();
  if (dta > 4.1)
    batLev = 100;
  else if (dta > 3.9)
    batLev = 75;
  else if (dta > 3.75)
    batLev = 50;
  else if (dta > 3.6)
    batLev = 25;
  return batLev;
}


void drawBatteryStatus(int16_t x, int16_t y) {
  int8_t battLevel = getBatteryLevel();
  M5.Lcd.fillRect(x, y, 16, 17, TFT_BLACK);
  if (battLevel != -1) {
    switch (battLevel) {
      case 0:
        drawIcon(x, y + 1, (uint8_t*)bat0_icon16x16, RED);
        break;
      case 25:
        drawIcon(x, y + 1, (uint8_t*)bat1_icon16x16, YELLOW);
        break;
      case 50:
        drawIcon(x, y + 1, (uint8_t*)bat2_icon16x16, WHITE);
        break;
      case 75:
        drawIcon(x, y + 1, (uint8_t*)bat3_icon16x16, WHITE);
        break;
      case 100:
        drawIcon(x, y + 0, (uint8_t*)plug_icon16x16, WHITE);
        break;
    }
  }
}

void drawIcon(int16_t x, int16_t y, const uint8_t *bitmap, uint16_t color) {
  int16_t w = 16;
  int16_t h = 16;
  int32_t i, j, byteWidth = (w + 7) / 8;
  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      if (pgm_read_byte(bitmap + j * byteWidth + i / 8) & (128 >> (i & 7))) {
        M5.Lcd.drawPixel(x + i, y + j, color);
      }
    }
  }
}
#endif

// process replacement in html pages
String processor(const String& var) {
  if (var == "SPEED") {
    return sspeed;
  }
  if (var == "NETWORKS" ) {
    String rsp;
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
      rsp += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + " (RSSI: " + WiFi.RSSI(i) + ")</option>";
    }
    return rsp;
  }


  if (var == "APIKEY") {
    return apikey;
  }
#ifdef M5Stick
  if (var == "ETHDISABLE") {

    String rsp = "disabled";
    return  rsp;

#endif
  }
  if (var == "DHCP") {

    String rsp = "";
    if (dhcp) rsp = "checked";

    return  rsp;
  }


  if (var == "LOCALIP") {
    return sIP;
  }
  if (var == "SUBNET") {
    return sSubnet;
  }

  if (var == "GATEWAY") {
    return sGateway;
  }

  if (var == "PDNS") {
    return sPrimaryDNS;
  }
  if (var == "SDNS") {
    return sSecondaryDNS;
  }

  return String();
}

void savePrefs()
{
  preferences.begin("keyer", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.putString("apikey", apikey);
  preferences.putBool("dhcp", dhcp);
  preferences.getString("ip", sIP);
  preferences.getString("gateway", sGateway);
  preferences.getString("subnet", sSubnet);
  preferences.getString("pdns", sPrimaryDNS);
  preferences.getString("sdns", sSecondaryDNS);

  preferences.end();

}

void setup() {
#ifdef M5Stick
  M5.begin();
  // set display brightnes
  M5.Axp.ScreenBreath(10);
#endif
  Serial.begin(115200);
  Serial.println(hMessage);


  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
#ifdef M5Stick
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
#endif
  // output ports
  pinMode(CW_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(CW_PIN, 0);
  digitalWrite(LED_PIN, 1);

  // read preferences
  preferences.begin("keyer", false);
  dhcp = preferences.getBool("dhcp", 1);
  ssid = preferences.getString("ssid");
  password = preferences.getString("password");
  apikey = preferences.getString("apikey");
  sIP        = preferences.getString("ip", "192.168.1.200");
  sGateway   = preferences.getString("gateway", "192.168.1.1");
  sSubnet    = preferences.getString("subnet", "255.255.255.0");
  sPrimaryDNS = preferences.getString("pdns", "8.8.8.8");
  sSecondaryDNS = preferences.getString("sdns", "8.8.4.4");

  preferences.end();


  // Calcutale init  cw speed
  update_speed();

  // Create task for sending morse on separate core

  xTaskCreatePinnedToCore(SendMorse, "Task1", 10000, NULL, 1, NULL,  0);


  // Try connect to WIFI
  if (!wifiConfigRequired)
  {
#ifdef M5Stick
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println(hMessage);
    M5.Lcd.setCursor(20, 20);
    M5.Lcd.println("Waiting for WiFi...");
#endif
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      WiFi.disconnect();
      wifiConfigRequired = true;
    }
    if (!wifiConfigRequired) {
      Serial.print("WIFI IP Address: ");
      IP = WiFi.localIP();
      Serial.println(IP);
    }
#ifdef M5Stick
    M5.Lcd.fillRect(0, 0, 160, 80, 0);
#endif
  }

  // not connected and make AP
#ifdef M5Stick
  if (M5.BtnB.isPressed() || wifiConfigRequired) {
#else
  if (wifiConfigRequired) {
#endif
    wifiConfigRequired = true;
    Serial.println("Start AP");
    Serial.printf("WiFi is not connected or configured. Starting AP mode\n");
    ssid = "KEYER";
    WiFi.softAP(ssid.c_str());
    IP = WiFi.softAPIP();
    Serial.printf("SSID: %s, IP: ", ssid.c_str());
    Serial.println(IP);
  }


  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {

    if ((request->hasParam(PARAM_APIKEY) && request->getParam(PARAM_APIKEY)->value() == apikey) || wifiConfigRequired) {

      if (request->hasParam(PARAM_MESSAGE)) {
        message = request->getParam(PARAM_MESSAGE)->value();
      }

      if (request->hasParam(PARAM_SPEED)) {
        sspeed = request->getParam(PARAM_SPEED)->value();
        speed = sspeed.toInt();
        update_speed();
      }
      request->send(SPIFFS, "/index.html", String(), false,   processor);
    }
    else {
      request->send(401, "text/plain", "Unauthorized");
    }
  });

  server.on("/cfg", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    if ((request->hasParam(PARAM_APIKEY) && request->getParam(PARAM_APIKEY)->value() == apikey) || wifiConfigRequired) {

      request->send(SPIFFS, "/cfg.html", String(), false,   processor);
    } else request->send(401, "text/plain", "Unauthorized");
  });
  server.on("/cfg-save", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    if (request->hasParam("ssid") && request->hasParam("password")) {
      ssid = request->getParam(PARAM_SSID)->value();
      password = request->getParam(PARAM_PASSWORD)->value();
      //apikey = request->getParam(PARAM_APIKEY)->value();
      savePrefs();
      request->send(200, "text/plain", "Config saved - SSID:" + ssid + " APIKEY: " + apikey + " rest in 5 seconds");
      delay(5000);
      ESP.restart();
      //request->redirect("/");
    }
    else
      request->send(400, "text/plain", "Missing required parameters");
  });


  // Send a GET request to <IP>/get?message=<message>
  server.on("/sendmorse", HTTP_GET, [] (AsyncWebServerRequest * request) {
    if (request->hasParam(PARAM_APIKEY) && request->getParam(PARAM_APIKEY)->value() == apikey) {
      if (request->hasParam(PARAM_MESSAGE)) {
        message = request->getParam(PARAM_MESSAGE)->value();
      }

      if (request->hasParam(PARAM_SPEED)) {
        sspeed = request->getParam(PARAM_SPEED)->value();
        speed = sspeed.toInt();
        update_speed();
      }
      request->send(200, "text/plain", "OK");
    } else request->send(401, "text/plain", "Unauthorized");


  });

  server.on("/js/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/js/bootstrap.bundle.min.js", "text/javascript");
  });

  server.on("/js/bootstrap4-toggle.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/js/bootstrap4-toggle.min.js", "text/css");
  });


  server.on("/js/jquery.mask.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/js/jquery.mask.min.js", "text/css");
  });

  server.on("/js/jquery-3.5.1.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/js/jquery-3.5.1.min.js", "text/css");
  });

  server.on("/css/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/css/bootstrap.min.css", "text/css");
  });


  server.on("/css/bootstrap4-toggle.min.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/css/bootstrap4-toggle.min.css", "text/css");
  });


  server.onNotFound(notFound);

  server.begin();

  if (udp.listen(UDP_PORT)) {
    Serial.print("UDP running at IP: ");
    Serial.print(IP);
    Serial.println(" port: 6789");
    udp.onPacket([](AsyncUDPPacket packet) {
      Serial.print("Type of UDP datagram: ");
      Serial.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast");
      Serial.print(", Sender: ");
      Serial.print(packet.remoteIP());
      Serial.print(":");
      Serial.print(packet.remotePort());
      Serial.print(", Receiver: ");
      Serial.print(packet.localIP());
      Serial.print(":");
      Serial.print(packet.localPort());
      Serial.print(", Message lenght: ");
      Serial.print(packet.length());
      Serial.print(", Payload: ");
      Serial.write(packet.data(), packet.length());
      Serial.println();
      String udpdataString = (const char*)packet.data();
      char bb[2];
      int command = (int)packet.data()[1] - 48;
      if (packet.data()[0] == 27) {

        switch (command ) {
          case 0: //break cw
            message = "";
            break;
          case 2:  // set speed
            bb[0] = packet.data()[2];
            bb[1] = packet.data()[3];
            speed = atoi(bb);
            update_speed();
            break;

          default:
            break;
        }


      } else {
        udpdataString.toUpperCase();
        message += udpdataString;
      }

    });
  }


}
int i;
void loop() {
#ifdef M5Stick

  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println(hMessage);
  M5.Lcd.println();
  M5.Lcd.print("SSID: ");
  M5.Lcd.println(ssid);
  M5.Lcd.print("IP:");
  M5.Lcd.println(IP);
  M5.Lcd.fillRect(0, 35, 160, 25, 0);
  M5.Lcd.println();
  M5.Lcd.println(message);
  M5.Lcd.setCursor(0, 60);
  M5.Lcd.print("SPEED: ");
  M5.Lcd.println(sspeed);
  drawIcon(120, 60, (uint8_t*)wifi1_icon16x16, WHITE);
  getBatteryLevel();
  drawBatteryStatus(140, 60);
  if (M5.BtnA.wasReleased()) {
    M5.Lcd.fillRect(0, 0, 160, 80, 0);
    if (wifiConfigRequired) {
      String url = "http://";
      url += IP.toString();
      url += "/cfg";
      M5.Lcd.qrcode(url, 0, 0, 80, 2);
      delay(10000);
    }
    else {
      String url = "http://";
      url += IP.toString();
      url += "/?akpikey=" + apikey;
      M5.Lcd.qrcode(url, 0, 0, 80, 2);
      delay(10000);
    }
    M5.Lcd.fillRect(0, 0, 160, 80, 0);
  }

  M5.update();
  //  delay(100);
#endif
}
