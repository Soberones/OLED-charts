# 📊 ESP8266 OLED Crypto, Weather & Time Dashboard

OLED mini-dashboard for ESP8266 that shows Binance prices, OpenWeather data, NTP time, and ships with a web UI for configuration.

#### 3D model here 👉 [MakerWorld](https://makerworld.com/en/models/1393341-desktop-wifi-informer-esp8266-oled-ssd1306#profileId-1443943)

## ✨ Features

- 📶 Auto Wi‑Fi setup via [WiFiManager](https://github.com/tzapu/WiFiManager) (AP `NodeMCU-Finance`)
- 🕒 NTP time (UTC+3 offset by default) shown on OLED
- ☁️ Weather from OpenWeather (city and API key via web UI, stored in EEPROM)
- 💸 Crypto from Binance (defaults BTCUSDT/ETHUSDT; selectable list stored in EEPROM)
- 🖥 OLED slides (rotate every 8 s):
  - Crypto1 price
  - Crypto2 price
  - Time
  - Weather
  - Chart of last 5 points for both coins (line + dots)
- 🌐 Web page:
  - Live values + SVG chart for first coin
  - Forms: refresh data, invert OLED, set contrast (0–255), city, API key, two coin selections
  - Auto-refresh every 30 s
- 🚀 OTA firmware updates
- 💾 All settings (city, API key, crypto pairs, invert/contrast) saved to EEPROM

## 📦 Libraries Used

- [`GyverOLED`](https://github.com/GyverLibs/GyverOLED)
- [`WiFiManager`](https://github.com/tzapu/WiFiManager)
- [`NTPClient`](https://github.com/arduino-libraries/NTPClient)
- [`ESP8266WebServer`](https://github.com/esp8266/Arduino)
- [`ESP8266HTTPClient`](https://github.com/esp8266/Arduino)
- [`ArduinoJson`](https://arduinojson.org/)
- [`ArduinoOTA`](https://github.com/esp8266/Arduino)
- `EEPROM.h`, `WiFiClientSecure.h` (from ESP8266 core)

## ⚙️ Getting Started

### 1) Flash
- Open the project in [PlatformIO](https://platformio.org/) (env `nodemcuv2`, 115200 baud, see `platformio.ini`).
- Build and flash over USB.

### 2) First boot & Wi‑Fi
- After boot the ESP starts AP `NodeMCU-Finance`.
- Connect and the WiFiManager portal opens. Pick your Wi‑Fi and password; it will be saved.

### 3) Configure via web
- Find the ESP IP (serial monitor or router) and open `http://<ip>/`.
- Enter city and OpenWeather API key (get one [here](https://home.openweathermap.org/api_keys)), save.
- Choose crypto pairs (from Binance list) and save.
- Optional: invert OLED, set contrast (0–255), manual refresh.

### 4) Data cadence
- Crypto and weather fetched every 5 minutes; OLED slides switch every 8 seconds.
- Browser page auto-reloads every 30 seconds (Refresh button triggers immediate fetch).

## 🖼 OLED Slide Preview

The OLED screen cycles through the following:

1. ₿ BTC Price
2. 💵 ETH Price
3. 🕒 Time
4. 🌡 Weather (with icon)
5. 📈 Mini Chart of BTC & ETH over time

## 📷 OLED Preview

![Crypto Dashboard](https://makerworld.bblmw.com/makerworld/model/US831a375163e43e/design/2025-05-07_b964d579cc8b8.jpg?x-oss-process=image/resize,w_1920/format,webp)

---

## 🌐 Web UI (Charts, Buttons, Inputs)

![Web UI](https://i.imgur.com/6IMjLav.png)

---

## 🔌 Simple Circuit Diagram

![Circuit Diagram](https://imgur.com/R3Pnb0F.jpg)

#### 🧠 Notes

- Time offset: UTC+3 (`NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800);`). Adjust for your timezone.
- Binance data via public HTTPS `api.binance.com`; without network you’ll see `0`.
- Weather is skipped without an API key.
- All entered values (city, API key, cryptos, invert/contrast) persist in EEPROM.
