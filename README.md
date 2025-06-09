# 📊 ESP8266 OLED Crypto, Weather & Time Dashboard

A sleek real-time dashboard built with ESP8266 and an OLED display. This project shows cryptocurrency prices (BTC & ETH), weather data, and the current time—rotating elegantly on the screen. It also includes a web interface for monitoring and configuration.

#### 3D model here 👉 [MakerWorld](https://makerworld.com/en/models/1393341-desktop-wifi-informer-esp8266-oled-ssd1306#profileId-1443943)

## ✨ Features

- 📶 **Wi-Fi Auto Setup** via [WiFiManager](https://github.com/tzapu/WiFiManager)
- 🕒 **Time Sync** using [NTPClient](https://github.com/arduino-libraries/NTPClient)
- ☁️ **Weather Info** from [OpenWeatherMap](https://openweathermap.org/)
- 💸 **Crypto Prices** (BTC/USD & ETH/USD) from [CoinGecko API](https://www.coingecko.com/)
- 🖥 **OLED Display Slides** with data rotation:
  - BTC price
  - ETH price
  - Time
  - Temperature & Weather
  - Simple line graph (BTC) and dots (ETH)
- 🌐 **Web UI** with:
  - Live data (text + SVG chart)
  - City input
  - Theme inversion toggle
  - Contrast control
  - Manual data refresh
- 🚀 **OTA Updates** (Over-the-Air firmware upload)
- 🔧 **EEPROM Settings Persistence**

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

### 1. 🔄 Clone and Open the Project

Use the Arduino IDE or [PlatformIO](https://platformio.org/). Select a board like `NodeMCU 1.0`.

### 2. 📥 Install Dependencies

Install all listed libraries above using Library Manager or in `platformio.ini`.

### 3. 🔑 Set Weather API Key

In your code, insert your OpenWeatherMap API key:

```
String weatherApiKey = "YOUR_API_KEY_HERE";
```

Sign up and get it [Here](https://home.openweathermap.org/api_keys).

### 4. ⬆️ Upload & Boot

- Connect the board.
- Upload via USB.
- On first boot, it will create a Wi-Fi AP: NodeMCU-Finance.

### 5. 📡 Connect & Configure

- Connect to the ESP's AP.
- A captive portal will open.
- Choose your Wi-Fi network and connect.

### 6. 🌍 Web Dashboard

Once online, OLED shows data and you can access the web interface by entering the device’s IP in your browser (e.g., http://192.168.1.45).

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

![Web UI](https://imgur.com/saONzd7.jpg)

---

## 🔌 Simple Circuit Diagram

![Circuit Diagram](https://imgur.com/R3Pnb0F.jpg)

#### 🧠 Notes

- Time offset is set to UTC+3 (10800s) — adjust if needed.
- Weather city and display theme are stored in EEPROM.
- You can switch display theme (invert mode) via the web UI.
