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
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800);  // +3 часа

ESP8266WebServer server(80);

// ======= НАСТРОЙКИ EEPROM =======
const int EEPROM_SIZE = 512;

const int EEPROM_CITY_OFFSET       = 0;
const int EEPROM_CITY_LEN         = 32;

const int EEPROM_API_OFFSET       = 40;
const int EEPROM_API_LEN          = 64;

const int EEPROM_CR1_ID_OFFSET    = 120;
const int EEPROM_CR1_ID_LEN       = 32;

const int EEPROM_CR1_SYM_OFFSET   = 160;
const int EEPROM_CR1_SYM_LEN      = 16;

const int EEPROM_CR2_ID_OFFSET    = 180;
const int EEPROM_CR2_ID_LEN       = 32;

const int EEPROM_CR2_SYM_OFFSET   = 220;
const int EEPROM_CR2_SYM_LEN      = 16;

// ======= КРИПТА =======
String crypto1Id     = "bitcoin";   // CoinGecko id
String crypto1Symbol = "BTC";       // отображаемое имя

String crypto2Id     = "ethereum";
String crypto2Symbol = "ETH";

float crypto1History[5] = {0};
float crypto2History[5] = {0};

// ======= ПОГОДА =======
String weatherCity   = "Hrodna";
String weatherApiKey = "";          // задаётся через веб
String weatherUrl    = "";

// ======= ДРУГИЕ ГЛОБАЛЬНЫЕ =======
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

float getCryptoRate(const String& id);
void getWeather();

void handleSettingsUpdate();
void handleThemeUpdate();
void handleContrastUpdate();
void handleApiKeyUpdate();
void handleCryptoUpdate();

void updateHistory(float history[], float newValue);
void calcMinMax(const float *data, int n, float &minVal, float &maxVal);

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  loadSettings();

  // Сборка URL погоды
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
  server.on("/", handleRoot);
  server.on("/refresh",  HTTP_POST, []() {
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

// ================== ЛОГИКА ПОЛУЧЕНИЯ ДАННЫХ ==================
float getCryptoRate(const String& id) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;

  String url = "https://api.coingecko.com/api/v3/simple/price?ids=" + id + "&vs_currencies=usd";

  if (https.begin(client, url)) {
    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();
      StaticJsonDocument<512> doc;
      DeserializationError err = deserializeJson(doc, payload);
      https.end();

      if (!err) {
        // ключ JSON = id криптовалюты
        return doc[id]["usd"].as<float>();
      } else {
        Serial.print("JSON error: ");
        Serial.println(err.c_str());
      }
    } else {
      Serial.print("HTTP error: ");
      Serial.println(httpCode);
    }
    https.end();
  } else {
    Serial.println("HTTPS begin failed");
  }
  return 0.0f;
}

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

void updateHistory(float history[], float newValue) {
  for (int i = 4; i > 0; i--) {
    history[i] = history[i - 1];
  }
  history[0] = newValue;
}

bool updateData() {
  float newCr1 = getCryptoRate(crypto1Id);
  float newCr2 = getCryptoRate(crypto2Id);

  if (newCr1 > 0 && newCr2 > 0) {
    updateHistory(crypto1History, newCr1);
    updateHistory(crypto2History, newCr2);
    getWeather();
    return true;
  }

  Serial.println("Failed to update crypto data");
  return false;
}

// ================== ОТРИСОВКА OLED ==================
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

void displayData() {
  oled.clear();

  // Заголовок
  oled.setScale(1);
  oled.setCursor(0, 0);
  oled.print(" Finance Monitor ");
  oled.line(0, 10, 127, 10);

  switch (currentSlide) {
    case 0: {
      // ===== Слайд 1: Crypto1 =====
      oled.setScale(1);
      oled.setCursor(0, 2);
      oled.print("   ");
      oled.print(crypto1Symbol);
      oled.print(" / USD");

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
      // ===== Слайд 2: Crypto2 =====
      oled.setScale(1);
      oled.setCursor(0, 2);
      oled.print("   ");
      oled.print(crypto2Symbol);
      oled.print(" / USD");

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
      // ===== Слайд 3: Время =====
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
      // ===== Слайд 4: Погода =====
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
      // ===== Слайд 5: График двух крипт =====
      oled.setScale(1);
      oled.setCursor(0, 1);
      oled.print(crypto1Symbol);
      oled.print(" & ");
      oled.print(crypto2Symbol);
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
        oled.dot(xp, yp, 1);
        oled.dot(xp + 1, yp, 1);
        oled.dot(xp, yp + 1, 1);
        oled.dot(xp + 1, yp + 1, 1);
      }

      break;
    }
  }

  oled.update();
}

// ================== EEPROM НАСТРОЙКИ ==================
void saveStringToEEPROM(int offset, int maxLen, const String& value) {
  int i = 0;
  // записываем строку
  for (; i < maxLen - 1 && i < (int)value.length(); i++) {
    EEPROM.write(offset + i, value[i]);
  }
  // нуль-терминатор
  EEPROM.write(offset + i, 0);
  i++;
  // чистим остаток
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
  saveStringToEEPROM(EEPROM_CITY_OFFSET,     EEPROM_CITY_LEN,     weatherCity);
  saveStringToEEPROM(EEPROM_API_OFFSET,      EEPROM_API_LEN,      weatherApiKey);
  saveStringToEEPROM(EEPROM_CR1_ID_OFFSET,   EEPROM_CR1_ID_LEN,   crypto1Id);
  saveStringToEEPROM(EEPROM_CR1_SYM_OFFSET,  EEPROM_CR1_SYM_LEN,  crypto1Symbol);
  saveStringToEEPROM(EEPROM_CR2_ID_OFFSET,   EEPROM_CR2_ID_LEN,   crypto2Id);
  saveStringToEEPROM(EEPROM_CR2_SYM_OFFSET,  EEPROM_CR2_SYM_LEN,  crypto2Symbol);
  EEPROM.commit();
}

void loadSettings() {
  String c = readStringFromEEPROM(EEPROM_CITY_OFFSET, EEPROM_CITY_LEN);
  if (c.length() > 0) weatherCity = c;

  String k = readStringFromEEPROM(EEPROM_API_OFFSET, EEPROM_API_LEN);
  if (k.length() > 0) weatherApiKey = k;

  String id1 = readStringFromEEPROM(EEPROM_CR1_ID_OFFSET, EEPROM_CR1_ID_LEN);
  if (id1.length() > 0) crypto1Id = id1;

  String sym1 = readStringFromEEPROM(EEPROM_CR1_SYM_OFFSET, EEPROM_CR1_SYM_LEN);
  if (sym1.length() > 0) crypto1Symbol = sym1;

  String id2 = readStringFromEEPROM(EEPROM_CR2_ID_OFFSET, EEPROM_CR2_ID_LEN);
  if (id2.length() > 0) crypto2Id = id2;

  String sym2 = readStringFromEEPROM(EEPROM_CR2_SYM_OFFSET, EEPROM_CR2_SYM_LEN);
  if (sym2.length() > 0) crypto2Symbol = sym2;

  // Нормализуем
  crypto1Id.toLowerCase();
  crypto2Id.toLowerCase();
  crypto1Symbol.toUpperCase();
  crypto2Symbol.toUpperCase();
}

// ================== HANDLERS HTTP ==================
void handleSettingsUpdate() {
  if (server.hasArg("city")) {
    weatherCity = server.arg("city");
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

  if (server.hasArg("crypto1id")) {
    crypto1Id = server.arg("crypto1id");
    crypto1Id.trim();
    crypto1Id.toLowerCase();
    changed = true;
  }
  if (server.hasArg("crypto1sym")) {
    crypto1Symbol = server.arg("crypto1sym");
    crypto1Symbol.trim();
    crypto1Symbol.toUpperCase();
    changed = true;
  }
  if (server.hasArg("crypto2id")) {
    crypto2Id = server.arg("crypto2id");
    crypto2Id.trim();
    crypto2Id.toLowerCase();
    changed = true;
  }
  if (server.hasArg("crypto2sym")) {
    crypto2Symbol = server.arg("crypto2sym");
    crypto2Symbol.trim();
    crypto2Symbol.toUpperCase();
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
  html.reserve(4000);

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
  html += ".buttons{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px;}";
  html += "input,button{border-radius:999px;border:none;padding:8px 14px;font-size:12px;outline:none;}";
  html += "input{background:#101018;color:white;}";
  html += "button{background:#4caf50;color:white;cursor:pointer;transition:transform .1s,box-shadow .1s,background .1s;}";
  html += "button:hover{transform:translateY(-1px);box-shadow:0 4px 12px rgba(0,0,0,0.4);background:#5ecf62;}";
  html += "button.secondary{background:#30303c;}";
  html += ".footer{margin-top:12px;font-size:11px;opacity:0.6;}";
  html += ".form-row{display:flex;flex-direction:column;gap:4px;margin-top:6px;}";
  html += ".form-row small{font-size:10px;opacity:0.6;}";
  html += "</style></head><body>";

  html += "<div class='wrapper'><div class='card'>";
  html += "<h1>Finance Monitor</h1>";
  html += "<div class='subtitle'>NodeMCU • OLED dashboard</div>";

  // ====== Две крипты ======
  html += "<div class='grid'>";
  // Crypto1
  html += "<div class='tile'>";
  html += "<div class='label'><span class='emoji'>₿</span>";
  html += crypto1Symbol;
  html += " / USD</div>";
  html += "<div class='value'>$";
  html += String(crypto1History[0], 2);
  html += " ";
  html += trend1;
  html += "</div>";
  html += "<div class='weather' style='font-size:11px;opacity:0.6;'>id: ";
  html += crypto1Id;
  html += "</div>";
  html += "</div>";

  // Crypto2
  html += "<div class='tile'>";
  html += "<div class='label'><span class='emoji'>Ξ</span>";
  html += crypto2Symbol;
  html += " / USD</div>";
  html += "<div class='value'>$";
  html += String(crypto2History[0], 2);
  html += " ";
  html += trend2;
  html += "</div>";
  html += "<div class='weather' style='font-size:11px;opacity:0.6;'>id: ";
  html += crypto2Id;
  html += "</div>";
  html += "</div>";

  html += "</div>"; // .grid

  // ====== График первой крипты ======
  html += "<div class='tile'>";
  html += "<div class='label'>"; 
  html += crypto1Symbol;
  html += " history (5 points)</div>";
  html += "<svg viewBox='0 0 120 70'>";
  html += "<polyline fill='none' stroke='#4caf50' stroke-width='2' points='";
  html += points;
  html += "' />";
  html += "</svg>";
  html += "<div class='label'>Relative last 5 updates</div>";
  html += "</div>";

  // ====== Погода ======
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

  // ====== КНОПКИ И ФОРМЫ ======
  html += "<div class='buttons'>";

  // Refresh
  html += "<form method='POST' action='/refresh'>";
  html += "<button type='submit'>🔄 Refresh</button>";
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
  html += "<small>Key is stored in EEPROM</small>";
  html += "<button type='submit' class='secondary'>Save API key</button>";
  html += "</div>";
  html += "</form>";

  // Crypto settings
  html += "<form method='POST' action='/crypto'>";
  html += "<div class='form-row'>";
  html += "<input name='crypto1id' placeholder='Crypto 1 id (coingecko)' value='";
  html += crypto1Id;
  html += "'>";
  html += "<input name='crypto1sym' placeholder='Crypto 1 symbol' value='";
  html += crypto1Symbol;
  html += "'>";
  html += "<input name='crypto2id' placeholder='Crypto 2 id (coingecko)' value='";
  html += crypto2Id;
  html += "'>";
  html += "<input name='crypto2sym' placeholder='Crypto 2 symbol' value='";
  html += crypto2Symbol;
  html += "'>";
  html += "<small>Example ids: bitcoin, ethereum, dogecoin, litecoin...</small>";
  html += "<button type='submit' class='secondary'>Save cryptos</button>";
  html += "</div>";
  html += "</form>";

  html += "</div>"; // .buttons

  html += "<div class='footer'>Auto refresh every 30 sec • Served by ESP8266</div>";

  html += "</div></div>"; // card, wrapper

  html += "<script>setInterval(function(){location.reload();},30000);</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}
