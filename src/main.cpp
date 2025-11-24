#include <Wire.h>
#include <GyverOLED.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#include <bootImage.h>

GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800);  // смещение +3 часа

ESP8266WebServer server(80);

// ======= EEPROM =======
const int EEPROM_SIZE = 512;

const int EEPROM_CITY_OFFSET   = 0;
const int EEPROM_CITY_LEN      = 32;

const int EEPROM_API_OFFSET    = 40;
const int EEPROM_API_LEN       = 64;

const int EEPROM_CR1_OFFSET    = 120;  // "BTCUSDT"
const int EEPROM_CR1_LEN       = 16;

const int EEPROM_CR2_OFFSET    = 140;  // "ETHUSDT"
const int EEPROM_CR2_LEN       = 16;

// ======= КРИПТА (Binance) =======
String crypto1Symbol = "BTCUSDT";
String crypto2Symbol = "ETHUSDT";

float crypto1History[5] = {0};
float crypto2History[5] = {0};

// ===== СПИСОК ВАЛЮТ ДЛЯ DROPDOWN =====
struct CoinOption {
  const char* symbol;
  const char* label;
};

const CoinOption coinOptions[] = {
  { "BTCUSDT",  "BTC"  },
  { "ETHUSDT",  "ETH"  },
  { "BNBUSDT",  "BNB"  },
  { "SOLUSDT",  "SOL"  },
  { "DOGEUSDT", "DOGE" },
  { "XRPUSDT",  "XRP"  },
  { "ADAUSDT",  "ADA"  },
  { "TRXUSDT",  "TRX"  },
  { "LTCUSDT",  "LTC"  },
  { "LINKUSDT", "LINK" },
  { "MATICUSDT","MATIC"}
};
const int coinOptionsCount = sizeof(coinOptions) / sizeof(coinOptions[0]);

// ======= ПОГОДА =======
String weatherCity   = "Hrodna";
String weatherApiKey = "";      // задаётся с веба
String weatherUrl    = "";

// ======= ПРОЧЕЕ =======
float temperature         = 0.0;
String weatherDescription = "";

unsigned long lastUpdate      = 0;
unsigned long lastSlideChange = 0;
const unsigned long slideInterval = 8000;
int currentSlide = 0;
const int totalSlides = 5;

bool invertMode    = false;
int  contrastValue = 127;

// ===== ПРОТОТИПЫ =====
void loadSettings();
void saveSettings();

void handleRoot();
bool updateData();
void displayData();

float getCryptoRate(const String& symbol);
void getWeather();

void handleSettingsUpdate();
void handleThemeUpdate();
void handleContrastUpdate();
void handleApiKeyUpdate();
void handleCryptoUpdate();

void updateHistory(float history[], float newValue);
void calcMinMax(const float *data, int n, float &minVal, float &maxVal);

void saveStringToEEPROM(int offset, int maxLen, const String& value);
String readStringFromEEPROM(int offset, int maxLen);
String getBaseAsset(const String& symbol);

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  loadSettings();

  // Сборка URL погоды (если будет ключ)
  weatherUrl = "https://api.openweathermap.org/data/2.5/weather?q=" +
               weatherCity + "&appid=" + weatherApiKey + "&units=metric";

  oled.init();
  oled.invertDisplay(invertMode);
  oled.clear();
  oled.setContrast(contrastValue);

  delay(500);
  oled.drawBitmap(0, 0, BootImage_128x64, 128, 64, BITMAP_NORMAL, BUF_ADD);
  oled.update();
  delay(1000);

  oled.clear();
  oled.setScale(1);
  oled.setCursor(0, 0);
  oled.print("Connecting WiFi...");
  oled.update();

  // WiFi Manager
  WiFiManager wifiManager;
  wifiManager.autoConnect("NodeMCU-Finance");

  timeClient.begin();

  // OTA
  ArduinoOTA.setHostname("NodeMCU-Finance");
  ArduinoOTA.begin();

  // HTTP сервер
  server.on("/",        handleRoot);
  server.on("/refresh", HTTP_POST, []() {
    updateData();
    server.sendHeader("Location", "/");
    server.send(303);
  });
  server.on("/settings", HTTP_POST, handleSettingsUpdate);
  server.on("/invert",   HTTP_POST, handleThemeUpdate);
  server.on("/contrast", HTTP_POST, handleContrastUpdate);
  server.on("/apikey",   HTTP_POST, handleApiKeyUpdate);
  server.on("/crypto",   HTTP_POST, handleCryptoUpdate);
  server.begin();

  oled.clear();
  oled.setCursor(0, 0);
  oled.print("WiFi Connected!");
  oled.update();

  delay(800);
  updateData();
  lastUpdate      = millis();
  lastSlideChange = millis();
}

// ================== LOOP ==================
void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  timeClient.update();

  // Обновление данных каждые 5 минут
  if (millis() - lastUpdate > 300000UL) {
    updateData();
    lastUpdate = millis();
  }

  // Переключение слайдов
  if (millis() - lastSlideChange > slideInterval) {
    currentSlide = (currentSlide + 1) % totalSlides;
    lastSlideChange = millis();
  }

  displayData();
}

// ================== КРИПТА (Binance) ==================
float getCryptoRate(const String& symbol) {
  if (symbol.length() == 0) return 0.0f;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;

  String url = "https://api.binance.com/api/v3/ticker/price?symbol=" + symbol;

  if (https.begin(client, url)) {
    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();
      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, payload);
      https.end();

      if (!err) {
        String priceStr = doc["price"].as<String>();
        float price = priceStr.toFloat();
        return price;
      } else {
        Serial.print("Binance JSON error: ");
        Serial.println(err.c_str());
      }
    } else {
      Serial.print("Binance HTTP error: ");
      Serial.println(httpCode);
    }
    https.end();
  } else {
    Serial.println("HTTPS begin failed (Binance)");
  }

  return 0.0f;
}

// ================== ПОГОДА (OpenWeather) ==================
void getWeather() {
  if (weatherApiKey.length() == 0) {
    Serial.println("No OpenWeather API key set");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;

  if (https.begin(client, weatherUrl)) {
    Serial.println(weatherUrl);
    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();
      StaticJsonDocument<1024> doc;
      DeserializationError err = deserializeJson(doc, payload);

      if (!err) {
        temperature = doc["main"]["temp"].as<float>();
        weatherDescription = doc["weather"][0]["main"].as<String>();
        Serial.print("Temp: ");
        Serial.println(temperature);
      } else {
        Serial.print("Weather JSON error: ");
        Serial.println(err.c_str());
      }
    } else {
      Serial.print("Weather HTTP error: ");
      Serial.println(httpCode);
    }
    https.end();
  } else {
    Serial.println("HTTPS begin failed (weather)");
  }
}

// ================== ЛОГИКА ОБНОВЛЕНИЯ ==================
void updateHistory(float history[], float newValue) {
  for (int i = 4; i > 0; i--) {
    history[i] = history[i - 1];
  }
  history[0] = newValue;
}

bool updateData() {
  float newCr1 = getCryptoRate(crypto1Symbol);
  float newCr2 = getCryptoRate(crypto2Symbol);

  if (newCr1 > 0 && newCr2 > 0) {
    updateHistory(crypto1History, newCr1);
    updateHistory(crypto2History, newCr2);
    getWeather();
    return true;
  }

  Serial.println("Failed to update crypto data");
  return false;
}

// ================== OLED ==================
void calcMinMax(const float *data, int n, float &minVal, float &maxVal) {
  if (n <= 0) {
    minVal = 0;
    maxVal = 1;
    return;
  }
  minVal = data[0];
  maxVal = data[0];
  for (int i = 1; i < n; i++) {
    if (data[i] < minVal) minVal = data[i];
    if (data[i] > maxVal) maxVal = data[i];
  }
  if (minVal == maxVal) {
    maxVal = minVal + 1.0f;  // чтобы не делить на 0
  }
}

String getBaseAsset(const String& symbol) {
  // Если заканчивается на "USDT" — отрезаем
  if (symbol.endsWith("USDT")) {
    return symbol.substring(0, symbol.length() - 4);
  }
  return symbol;
}

void displayData() {
  oled.clear();

  // Заголовок
  oled.setScale(1);
  oled.setCursor(0, 0);
  oled.print(" Finance Monitor ");
  oled.line(0, 10, 127, 10);

  switch (currentSlide) {
    case 0: {
      // ===== Слайд 0: Crypto1 =====
      oled.setScale(1);
      oled.setCursor(0, 2);
      oled.print("   ");
      oled.print(getBaseAsset(crypto1Symbol));
      oled.print(" / USDT");

      oled.setScale(2);
      oled.setCursor(10, 4);
      if (crypto1History[0] > 0) {
        oled.print("$");
        oled.print(crypto1History[0], 0);
      } else {
        oled.print("N/A");
      }
      break;
    }

    case 1: {
      // ===== Слайд 1: Crypto2 =====
      oled.setScale(1);
      oled.setCursor(0, 2);
      oled.print("   ");
      oled.print(getBaseAsset(crypto2Symbol));
      oled.print(" / USDT");

      oled.setScale(2);
      oled.setCursor(10, 4);
      if (crypto2History[0] > 0) {
        oled.print("$");
        oled.print(crypto2History[0], 0);
      } else {
        oled.print("N/A");
      }
      break;
    }

    case 2: {
      // ===== Слайд 2: Время =====
      oled.setScale(1);
      oled.setCursor(0, 2);
      oled.print("   Time (NTP)");

      oled.setScale(3);
      oled.setCursor(10, 4);
      String t = timeClient.getFormattedTime();
      oled.print(t.substring(0, 5)); // HH:MM
      break;
    }

    case 3: {
      // ===== Слайд 3: Погода =====
      oled.setScale(1);
      oled.setCursor(0, 2);
      oled.print(weatherCity);

      oled.setScale(3);
      oled.setCursor(0, 4);
      if (weatherApiKey.length() > 0) {
        oled.print(temperature, 1);
        oled.print("C");
      } else {
        oled.print("--.-C");
      }

      oled.setScale(1);
      oled.setCursor(80, 2);
      oled.print(weatherDescription);
      break;
    }

    case 4: {
      // ===== Слайд 4: График двух крипт =====
      oled.setScale(1);
      oled.setCursor(0, 1);
      oled.print(getBaseAsset(crypto1Symbol));
      oled.print(" & ");
      oled.print(getBaseAsset(crypto2Symbol));
      oled.print(" (5)");

      int x0 = 5;
      int y0 = 63;
      oled.line(x0, 20, x0, y0);     // Y
      oled.line(x0, y0, 127, y0);    // X

      float min1, max1, min2, max2;
      calcMinMax(crypto1History, 5, min1, max1);
      calcMinMax(crypto2History, 5, min2, max2);

      float scale1 = 35.0f / (max1 - min1);
      float scale2 = 35.0f / (max2 - min2);

      // Линия Crypto1
      for (int i = 0; i < 4; i++) {
        int xp0 = x0 + 10 + i * 25;
        int xp1 = x0 + 10 + (i + 1) * 25;

        int yp0 = y0 - (int)((crypto1History[4 - i]     - min1) * scale1);
        int yp1 = y0 - (int)((crypto1History[4 - (i+1)] - min1) * scale1);

        oled.line(xp0, yp0, xp1, yp1);
      }

      // Точки Crypto2
      for (int i = 0; i < 5; i++) {
        int xp = x0 + 10 + i * 25;
        int yp = y0 - (int)((crypto2History[4 - i] - min2) * scale2);
        oled.dot(xp,   yp,   1);
        oled.dot(xp+1, yp,   1);
        oled.dot(xp,   yp+1, 1);
        oled.dot(xp+1, yp+1, 1);
      }

      break;
    }
  }

  oled.update();
}

// ================== EEPROM ==================
void saveStringToEEPROM(int offset, int maxLen, const String& value) {
  int i = 0;
  for (; i < maxLen - 1 && i < (int)value.length(); i++) {
    EEPROM.write(offset + i, value[i]);
  }
  EEPROM.write(offset + i, 0);
  i++;
  for (; i < maxLen; i++) {
    EEPROM.write(offset + i, 0);
  }
}

String readStringFromEEPROM(int offset, int maxLen) {
  String res = "";
  for (int i = 0; i < maxLen; i++) {
    uint8_t c = EEPROM.read(offset + i);
    if (c == 0 || c == 0xFF) break;
    res += char(c);
  }
  return res;
}

void saveSettings() {
  saveStringToEEPROM(EEPROM_CITY_OFFSET, EEPROM_CITY_LEN, weatherCity);
  saveStringToEEPROM(EEPROM_API_OFFSET,  EEPROM_API_LEN,  weatherApiKey);
  saveStringToEEPROM(EEPROM_CR1_OFFSET,  EEPROM_CR1_LEN,  crypto1Symbol);
  saveStringToEEPROM(EEPROM_CR2_OFFSET,  EEPROM_CR2_LEN,  crypto2Symbol);
  EEPROM.commit();
}

void loadSettings() {
  String c  = readStringFromEEPROM(EEPROM_CITY_OFFSET, EEPROM_CITY_LEN);
  if (c.length() > 0) weatherCity = c;

  String k  = readStringFromEEPROM(EEPROM_API_OFFSET, EEPROM_API_LEN);
  if (k.length() > 0) weatherApiKey = k;

  String s1 = readStringFromEEPROM(EEPROM_CR1_OFFSET, EEPROM_CR1_LEN);
  if (s1.length() > 0) crypto1Symbol = s1;

  String s2 = readStringFromEEPROM(EEPROM_CR2_OFFSET, EEPROM_CR2_LEN);
  if (s2.length() > 0) crypto2Symbol = s2;
}

// ================== HTTP HANDLERS ==================
void handleSettingsUpdate() {
  if (server.hasArg("city")) {
    weatherCity = server.arg("city");
    weatherCity.trim();
    saveSettings();

    weatherUrl = "https://api.openweathermap.org/data/2.5/weather?q=" +
                 weatherCity + "&appid=" + weatherApiKey + "&units=metric";

    updateData();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleThemeUpdate() {
  invertMode = !invertMode;
  oled.invertDisplay(invertMode);
  oled.update();

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleContrastUpdate() {
  if (server.hasArg("contrast")) {
    contrastValue = server.arg("contrast").toInt();
    if (contrastValue < 0)   contrastValue = 0;
    if (contrastValue > 255) contrastValue = 255;
    oled.setContrast(contrastValue);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleApiKeyUpdate() {
  if (server.hasArg("apikey")) {
    weatherApiKey = server.arg("apikey");
    weatherApiKey.trim();
    saveSettings();

    weatherUrl = "https://api.openweathermap.org/data/2.5/weather?q=" +
                 weatherCity + "&appid=" + weatherApiKey + "&units=metric";

    updateData();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleCryptoUpdate() {
  bool changed = false;

  if (server.hasArg("crypto1")) {
    crypto1Symbol = server.arg("crypto1");
    crypto1Symbol.trim();
    changed = true;
  }
  if (server.hasArg("crypto2")) {
    crypto2Symbol = server.arg("crypto2");
    crypto2Symbol.trim();
    changed = true;
  }

  if (changed) {
    // Сброс истории при смене монет
    for (int i = 0; i < 5; i++) {
      crypto1History[i] = 0;
      crypto2History[i] = 0;
    }
    saveSettings();
    updateData();

    // Сразу показываем первую валюту на OLED
    currentSlide = 0;
    lastSlideChange = millis();
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

// ================== ВЕБ-СТРАНИЦА ==================
void handleRoot() {
  String trend1 = "-";
  String trend2 = "-";

  if (crypto1History[0] > 0 && crypto1History[1] > 0) {
    trend1 = (crypto1History[0] > crypto1History[1]) ? "📈" : "📉";
  }
  if (crypto2History[0] > 0 && crypto2History[1] > 0) {
    trend2 = (crypto2History[0] > crypto2History[1]) ? "📈" : "📉";
  }

  // Подготовим точки для SVG графика первой крипты
  float min1, max1;
  calcMinMax(crypto1History, 5, min1, max1);
  float scale = 40.0f / (max1 - min1);

  String points = "";
  for (int i = 4; i >= 0; i--) {
    int x = 10 + (4 - i) * 25;
    int y = 60 - (int)((crypto1History[i] - min1) * scale);
    points += String(x) + "," + String(y) + " ";
  }

  String html;
  html.reserve(5000);

  html += "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>Finance Monitor</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;background:radial-gradient(circle at top,#2b5876,#0b0f1a);color:#ffffff;display:flex;justify-content:center;align-items:center;min-height:100vh;}";
  html += ".wrapper{max-width:420px;width:90%;padding:20px;}";
  html += ".card{background:rgba(12,12,20,0.9);border-radius:16px;padding:20px;box-shadow:0 15px 40px rgba(0,0,0,0.45);backdrop-filter:blur(10px);}";
  html += "h1{margin:0 0 10px;font-size:24px;}";
  html += ".subtitle{font-size:12px;opacity:0.7;margin-bottom:20px;}";
  html += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:20px;}";
  html += ".tile{background:#181820;border-radius:12px;padding:12px;text-align:left;}";
  html += ".label{font-size:12px;opacity:0.7;}";
  html += ".value{font-size:18px;margin-top:4px;}";
  html += ".emoji{font-size:18px;margin-right:4px;}";
  html += ".weather{margin-top:8px;font-size:12px;opacity:0.8;}";
  html += "svg{width:100%;height:90px;margin:10px 0 4px;}";
  html += ".buttons{display:flex;flex-direction:column;gap:8px;margin-top:10px;}";
  html += "input,button,select{border-radius:999px;border:none;padding:8px 14px;font-size:12px;outline:none;}";
  html += "input,select{background:#101018;color:white;}";
  html += "button{background:#4caf50;color:white;cursor:pointer;transition:transform .1s,box-shadow .1s,background .1s;}";
  html += "button:hover{transform:translateY(-1px);box-shadow:0 4px 12px rgba(0,0,0,0.4);background:#5ecf62;}";
  html += "button.secondary{background:#30303c;}";
  html += ".footer{margin-top:12px;font-size:11px;opacity:0.6;}";
  html += ".form-row{display:flex;flex-direction:column;gap:4px;margin-top:6px;}";
  html += ".form-row small{font-size:10px;opacity:0.6;}";
  html += "</style></head><body>";

  html += "<div class='wrapper'><div class='card'>";
  html += "<h1>Finance Monitor</h1>";
  html += "<div class='subtitle'>ESP8266 • OLED • Binance + Weather</div>";

  // ===== Две крипты =====
  html += "<div class='grid'>";

  // Crypto1 tile
  html += "<div class='tile'>";
  html += "<div class='label'><span class='emoji'>₿</span>";
  html += getBaseAsset(crypto1Symbol);
  html += " / USDT</div>";
  html += "<div class='value'>$";
  html += String(crypto1History[0], 2);
  html += " ";
  html += trend1;
  html += "</div>";
  html += "<div class='weather' style='font-size:11px;opacity:0.6;'>symbol: ";
  html += crypto1Symbol;
  html += "</div>";
  html += "</div>";

  // Crypto2 tile
  html += "<div class='tile'>";
  html += "<div class='label'><span class='emoji'>Ξ</span>";
  html += getBaseAsset(crypto2Symbol);
  html += " / USDT</div>";
  html += "<div class='value'>$";
  html += String(crypto2History[0], 2);
  html += " ";
  html += trend2;
  html += "</div>";
  html += "<div class='weather' style='font-size:11px;opacity:0.6;'>symbol: ";
  html += crypto2Symbol;
  html += "</div>";
  html += "</div>";

  html += "</div>"; // .grid

  // ===== График первой крипты =====
  html += "<div class='tile'>";
  html += "<div class='label'>";
  html += getBaseAsset(crypto1Symbol);
  html += " history (5 points)</div>";
  html += "<svg viewBox='0 0 120 70'>";
  html += "<polyline fill='none' stroke='#4caf50' stroke-width='2' points='";
  html += points;
  html += "' /></svg>";
  html += "<div class='label'>Relative last 5 updates</div>";
  html += "</div>";

  // ===== Погода =====
  html += "<div class='tile' style='margin-top:12px;'>";
  html += "<div class='label'><span class='emoji'>☁</span>Weather</div>";
  html += "<div class='value'>";
  html += weatherCity;
  html += " — ";
  html += String(temperature, 1);
  html += "°C</div>";
  html += "<div class='weather'>";
  html += weatherDescription;
  html += "</div>";
  html += "</div>";

  // ===== ФОРМЫ / КНОПКИ =====
  html += "<div class='buttons'>";

  // Refresh
  html += "<form method='POST' action='/refresh'>";
  html += "<button type='submit'>🔄 Refresh now</button>";
  html += "</form>";

  // Invert
  html += "<form method='POST' action='/invert'>";
  html += "<button type='submit' class='secondary'>Invert OLED</button>";
  html += "</form>";

  // Contrast
  html += "<form method='POST' action='/contrast'>";
  html += "<div class='form-row'>";
  html += "<input name='contrast' placeholder='Contrast (0-255)'>";
  html += "<button type='submit' class='secondary'>Save contrast</button>";
  html += "</div>";
  html += "</form>";

  // City
  html += "<form method='POST' action='/settings'>";
  html += "<div class='form-row'>";
  html += "<input name='city' placeholder='City' value='";
  html += weatherCity;
  html += "'>";
  html += "<button type='submit' class='secondary'>Save city</button>";
  html += "</div>";
  html += "</form>";

  // API key
  html += "<form method='POST' action='/apikey'>";
  html += "<div class='form-row'>";
  html += "<input name='apikey' placeholder='OpenWeather API key' value='";
  html += weatherApiKey;
  html += "'>";
  html += "<small>Key stored in EEPROM (for weather)</small>";
  html += "<button type='submit' class='secondary'>Save API key</button>";
  html += "</div>";
  html += "</form>";

  // Crypto selection (dropdown)
  html += "<form method='POST' action='/crypto'>";
  html += "<div class='form-row'>";
  html += "<small>Crypto 1 (left tile / first slide)</small>";
  html += "<select name='crypto1'>";
  for (int i = 0; i < coinOptionsCount; i++) {
    html += "<option value='";
    html += coinOptions[i].symbol;
    html += "'";
    if (crypto1Symbol == coinOptions[i].symbol) html += " selected";
    html += ">";
    html += coinOptions[i].label;
    html += " (";
    html += coinOptions[i].symbol;
    html += ")</option>";
  }
  html += "</select>";

  html += "<small>Crypto 2 (right tile / second slide)</small>";
  html += "<select name='crypto2'>";
  for (int i = 0; i < coinOptionsCount; i++) {
    html += "<option value='";
    html += coinOptions[i].symbol;
    html += "'";
    if (crypto2Symbol == coinOptions[i].symbol) html += " selected";
    html += ">";
    html += coinOptions[i].label;
    html += " (";
    html += coinOptions[i].symbol;
    html += ")</option>";
  }
  html += "</select>";

  html += "<button type='submit' class='secondary'>Save cryptos & update</button>";
  html += "</div>";
  html += "</form>";

  html += "</div>"; // .buttons

  html += "<div class='footer'>Auto refresh every 30 sec • Binance public API • ESP8266</div>";

  html += "</div></div>"; // .card .wrapper

  html += "<script>setInterval(function(){location.reload();},30000);</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}
