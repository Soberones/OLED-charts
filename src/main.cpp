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
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800);

ESP8266WebServer server(80);

const String btcUrl = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";
const String ethUrl = "https://api.coingecko.com/api/v3/simple/price?ids=ethereum&vs_currencies=usd";

String weatherCity = "Hrodna";
String weatherApiKey = "";
String weatherUrl = "https://api.openweathermap.org/data/2.5/weather?q=" + weatherCity + "&appid=" + weatherApiKey + "&units=metric";

float btcHistory[5] = {0};
float ethHistory[5] = {0};

int historyIndex = 0;
float temperature = 0.0;
String weatherDescription = "";

unsigned long lastUpdate = 0;
unsigned long lastSlideChange = 0;
const unsigned long slideInterval = 8000;
int currentSlide = 0;
const int totalSlides = 5;

bool invertMode = false;
int contrastValue = 127;

// Function prototypes
void loadSettings();
void handleRoot();
bool updateData();
void displayData();
float getETHRate();
float getBTCRate();
void getWeather();
void handleSettingsUpdate();
void handleThemeUpdate();
void handleContrastUpdate();

void setup() {
    Serial.begin(115200);
    EEPROM.begin(128);

    loadSettings();

    oled.init();
    oled.invertDisplay(invertMode);
    oled.clear();
    oled.setContrast(contrastValue);

    delay(1000);
    oled.drawBitmap(0, 0, BootImage_128x64, 128, 64, BITMAP_NORMAL, BUF_ADD);
    delay(1000);

    oled.setScale(1);
    oled.setCursor(0, 0);
    oled.print("Connecting WiFi...");
    oled.update();


    WiFiManager wifiManager;
    wifiManager.autoConnect("NodeMCU-Finance");

    timeClient.begin();
    ArduinoOTA.setHostname("NodeMCU-Finance");
    ArduinoOTA.begin();

    server.on("/", handleRoot);
    server.on("/refresh", HTTP_POST, []() {
        updateData();
        server.sendHeader("Location", "/");
        server.send(303);
    });
    server.on("/settings", HTTP_POST, handleSettingsUpdate);
    server.on("/invert", HTTP_POST, handleThemeUpdate);
    server.on("/contrast", HTTP_POST, handleContrastUpdate);
    server.begin();

    oled.clear();
    oled.setCursor(0, 0);
    oled.print("WiFi Connected!");
    oled.update();

    delay(1000);
    updateData();
}

void loop() {
    ArduinoOTA.handle();
    server.handleClient();
    timeClient.update();

    if(millis() - lastUpdate > 300000) {
        updateData();
        lastUpdate = millis();
    }

    if(millis() - lastSlideChange > slideInterval) {
        currentSlide = (currentSlide + 1) % totalSlides;
        lastSlideChange = millis();
    }

    displayData();
}

float getBTCRate() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    if(https.begin(client, btcUrl)) {
        int httpCode = https.GET();
        if(httpCode == HTTP_CODE_OK) {
            ArduinoJson::DynamicJsonDocument doc(1024);
            deserializeJson(doc, https.getString());
            https.end();
            return doc["bitcoin"]["usd"];
        }
        https.end();
    }
    return 0;
}

float getETHRate() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    if(https.begin(client, ethUrl)) {
        int httpCode = https.GET();
        if(httpCode == HTTP_CODE_OK) {
            ArduinoJson::DynamicJsonDocument doc(1024);
            deserializeJson(doc, https.getString());
            https.end();
            return doc["ethereum"]["usd"];
        }
        https.end();
    }
    return 0;
}


void getWeather() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;

    if(https.begin(client, weatherUrl)) {
        Serial.println(weatherUrl);
        int httpCode = https.GET();
        if(httpCode == HTTP_CODE_OK) {
            ArduinoJson::DynamicJsonDocument doc(1024);
            deserializeJson(doc, https.getString());

            temperature = doc["main"]["temp"];
            Serial.println((temperature));

            weatherDescription = doc["weather"][0]["main"].as<String>();
        }
        https.end();
    }
}

void updateHistory(float history[], float newValue) {
    for(int i = 4; i > 0; i--) {
        history[i] = history[i - 1];
    }
    history[0] = newValue;
}

bool updateData() {
    float newBtc = getBTCRate();
    float newEth = getETHRate();
    if(newBtc > 0 && newEth > 0) {
        updateHistory(btcHistory, newBtc);
        updateHistory(ethHistory, newEth);
        getWeather();
        return true;
    }
    return false;
}

void displayData() {
    oled.clear();
    oled.setScale(1);
    oled.setCursor(0, 0);
    oled.print("Finance Monitor");
    oled.line(0, 10, 127, 10);
    oled.setScale(2);

    switch(currentSlide) {
    case 0:
        // Slide 1: BTC
        oled.setCursor(20, 2);
        oled.print("BTC:");
        oled.setCursor(20, 4);
        if(btcHistory[0] > 0) {
            oled.print("$");
            oled.print(btcHistory[0], 0);
        } else {
            oled.print("N/A");
        }
        break;

    case 1:
        // Slide 2: ETH
        oled.setCursor(20, 2);
        oled.print("ETH:");
        oled.setCursor(20, 4);
        if(ethHistory[0] > 0) {
            oled.print("$");
            oled.print(ethHistory[0], 0);
        } else {
            oled.print("N/A");
        }
        break;

    case 2:
        // Slide 3: Время
        oled.setCursor(20, 2);
        oled.print("Time:");
        oled.setCursor(20, 4);
        oled.print(timeClient.getFormattedTime().substring(0, 5));
        break;

    case 3:
        // Slide 4: Weather
        oled.setScale(1);
        oled.setCursor(0, 2);
        oled.print(weatherCity);

        oled.setScale(4);
        oled.setCursor(0, 3);
        oled.print(temperature, 1);
        oled.print("C");

        // Display weather description
        oled.setScale(1);
        oled.setCursor(70, 2);
        oled.print(weatherDescription);
        break;

    case 4:
        // Slide 5: Chart BTC и ETH
        oled.clear();
        oled.setScale(1);
        oled.setCursor(0, 0);
        oled.print("BTC/USD & ETH/USD");

        // Write axis
        oled.line(0, 63, 127, 63); // ось X
        oled.line(0, 20, 0, 63);   // ось Y

        // Scales
        float maxBtc = *std::max_element(btcHistory, btcHistory + 5);
        float minBtc = *std::min_element(btcHistory, btcHistory + 5);
        float scaleBtc = (maxBtc - minBtc) != 0 ? (40.0 / (maxBtc - minBtc)) : 1;

        float maxEth = *std::max_element(ethHistory, ethHistory + 5);
        float minEth = *std::min_element(ethHistory, ethHistory + 5);
        float scaleEth = (maxEth - minEth) != 0 ? (40.0 / (maxEth - minEth)) : 1;

        // Chart BTC (line)
        for(int i = 0; i < 4; i++) {
            int x0 = i * 30 + 10;
            int x1 = (i + 1) * 30 + 10;

            int y0 = 63 - int((btcHistory[4 - i] - minBtc) * scaleBtc);
            int y1 = 63 - int((btcHistory[4 - (i + 1)] - minBtc) * scaleBtc);

            oled.line(x0, y0, x1, y1);
        }

        // Chart ETH (dots)
        for(int i = 0; i < 5; i++) {
            int x = i * 30 + 10;
            int y = 63 - int((ethHistory[4 - i] - minEth) * scaleEth);
            oled.dot(x, y, 1);
            oled.dot(x + 1, y, 1);
            oled.dot(x, y + 1, 1);
            oled.dot(x + 1, y + 1, 1);
        }
        break;
    }

    oled.update();
}

void saveSettings() {
    for(size_t i = 0; i < weatherCity.length(); i++) {
        EEPROM.write(i, weatherCity[i]);
    }
    EEPROM.write(weatherCity.length(), '\0');
    EEPROM.commit();
}

void loadSettings() {
    char buf[32];
    for(int i = 0; i < 32; i++) {
        buf[i] = EEPROM.read(i);
        if(buf[i] == '\0')
            break;
    }
    weatherCity = String(buf);
    weatherUrl = "https://api.openweathermap.org/data/2.5/weather?q=" + weatherCity + "&appid=" + weatherApiKey + "&units=metric";
}

void handleSettingsUpdate() {
    if(server.hasArg("city")) {
        weatherCity = server.arg("city");
        saveSettings();
        updateData();
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleThemeUpdate() {
    Serial.println("Invert display handler called");
    invertMode = !invertMode;
    Serial.print("New invert mode: ");
    Serial.println(invertMode ? "ON" : "OFF");

    oled.invertDisplay(invertMode);
    oled.update();

    server.sendHeader("Location", "/");
    server.send(303);
}

void handleContrastUpdate() {
    Serial.println("handleContrastUpdate handler called");
    if(server.hasArg("contrast")) {
        contrastValue = server.arg("contrast").toInt();
        Serial.println(contrastValue);
        oled.setContrast(contrastValue);
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleRoot() {
    String trendBtc = (btcHistory[0] > btcHistory[1]) ? "📈" : "📉";
    String trendEth = (ethHistory[0] > ethHistory[1]) ? "📈" : "📉";
    String html = R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><title>Finance</title>
<style>
body { font-family:sans-serif; background:#121212; color:white; text-align:center; padding:20px;}
.card { background:#1e1e1e; padding:20px; border-radius:10px; display:inline-block;}
input,button { padding:10px; margin:10px; }
svg { height:80px; }
</style></head><body>
<div class="card">
<h1>Finance Monitor</h1>
<div><strong>₿ BTC/USD:</strong> $)rawliteral" +
                  String(btcHistory[0], 2) + " " + trendBtc + R"rawliteral(</div>
<svg width="100%">
<polyline fill="none" stroke="#4caf50" stroke-width="2"
points=")rawliteral";

    for(int i = 4; i >= 0; i--) {
        int x = 10 + (4 - i) * 20;
        int y = 60 - int((btcHistory[i] / btcHistory[0]) * 40);
        html += String(x) + "," + String(y) + " ";
    }

    html += R"rawliteral("/></svg>
<div><strong>💵 USD/Eth:</strong> )rawliteral" +
            String(ethHistory[0], 3) + " " + trendEth + R"rawliteral(</div>
<div><strong>🌡 Weather (")rawliteral" +
            weatherCity + R"rawliteral("):</strong> )rawliteral" + String(temperature, 1) + "°C, " + weatherDescription + R"rawliteral(</div>
<form method="POST" action="/refresh"><button>🔄 Refresh</button></form>
<form method="POST" action="/settings">
<input name="city" placeholder="City" value=")" + weatherCity + R"rawliteral(">
<button>💾 Save</button>
</form>
<form method="POST" action="/invert"><button>🔄 Invert</button></form>
<form method="POST" action="/contrast">
<input name="contrast" placeholder="Contrast">
<button>💾 Save Contrast</button>
</form>
</div></body></html>)rawliteral";

    server.send(200, "text/html", html);

}