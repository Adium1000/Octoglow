![banner](/.github/banner.png)

A compact smart desk clock firmware that connects to Wi-Fi, syncs time automatically, and displays time, date, temperature, and atmospheric pressure  all in one firmware


## Table of Contents

- [Features](#features)
- [DIY](#diy)
- [Bill of Materials (BOM)](#bill-of-materials-bom)
- [Octoglow Sender](#octoglow-sender)
- [Find my Octoglow's IP adress](#how-can-i-find-my-octoglows-ip-adress)



## Features
Octoglow displays information in "tiles" that rotate one after another, each with its own display duration, all configurable from the dashboard (you can enable/disable each tile and reorder them via drag & drop), Here is a list with all tiles:

![#clock](.github/tiles/Clocktile.png)

This tile shows the current time, synced automatically once the clock connects to the internet, so you never have to set it by hand.

![Clocktile](.github/tiles/clock.png)

![datatile](.github/tiles/datetile.png)

This tile shows the current date, pulled from the same internet time sync as the clock, so it always stays accurate.

![Datatile](.github/tiles/data.png)

![temptile](.github/tiles/temptile.png)

This tile shows the ambient temperature at your clock's location, as long as you have a temperature sensor connected to the i2c rails.

![Temptile](.github/tiles/Temp.png)

![meteotile](.github/tiles/meteotile.png)

This tile fetches live weather data for your set location once you add an OpenWeatherMap API key. It scrolls through the current temperature (in Celsius or Fahrenheit, your choice), humidity percentage, and a short text description of the conditions, along with a matching weather icon.

![WeatherTile](.github/tiles/meteo.png)

![canvastile](.github/tiles/canvastile.png)

This tile turns the whole 8x32 LED matrix into a drawing surface, letting you sketch or design your own custom pixel art directly on the display.

![CanvasTile](.github/tiles/canvas.png)

![sstile](.github/tiles/sstile.png)

This tile plays one of three animations when idle: a self-playing Pong match between two paddles, a fireworks show with launching rockets and bursting particles, or an audio equalizer style animation with bouncing bars. You can pick a specific one or let it choose randomly each time.

![screenssavertile](.github/tiles/screensaver.png)

![cstile](.github/tiles/cstile.png)

This tile shows the exchange rate for a currency you select, and optionally compares it against a second currency. It also displays a trend arrow showing whether the rate has gone up, down, or stayed flat compared to a month ago.

![curencytile](.github/tiles/curency.png)

![aptile](.github/tiles/aptile.png)

This tile shows the current atmospheric pressure in hPa, as long as your clock has a barometric pressure sensor connected to the i2c rails. It also shows a trend arrow based on how the pressure has changed over recent readings.

![aptile](.github/tiles/ap.png)

![mementotile](.github/tiles/meteotile.png)

This tile lets you create short personal notes or reminders that scroll across the display.

![Memento?Nahbro](.github/tiles/memento.png)

![musictile](.github/tiles/musictile.png)

This tile displays the artist and track title of whatever you're currently listening to on your device, sent live from the companion Watch Sender app.

![HmmNowPlayingGuta?](.github/tiles/music.png)

![notiftile](.github/tiles/notiftile.png)

This tile shows notifications received on your computer as they come in, forwarded through the Watch Sender app and scrolled across the display.

![Ugotmailtile](.github/tiles/message.png)

![ets2](.github/tiles/ets2gaugetile.png)

This tile shows your real-time speed in km/h while driving in Euro Truck Simulator 2, streamed live from the game through the Watch Sender app.

![Ets2tile](.github/tiles/ets2.png)

![timertile](.github/tiles/timertile.png)

This tile runs a countdown timer that you can set to a custom duration or pick from preset values, showing the remaining time and alerting you with a sound once it hits zero.

![tikitikitikitile](.github/tiles/timer.png)

![stopwatchtile](.github/tiles/stopwatchtile.png)

This tile lets you time things using your clock, with start, pause, and reset controls right from the interface.

![alt text](.github/tiles/crono.png)


## Configurations and Settings

![Brightness & Dimming](.github/titles/brightness.png)

- 17 brightness levels (0 = display fully off, 1-16 = actual intensity on the MAX7219 matrix).
- **Scheduled auto-dimming**: set a time window (e.g. 10 PM - 7 AM) during which the display automatically switches to a lower brightness, so it doesn't blind you at night.

![Touch Sensor](.github/titles/touch.png)

A TTP223 capacitive sensor enables physical interaction with the clock, with separately configurable actions for **tap** and **double tap**:

![Buzzer & Event Sounds](.github/titles/buzzer.png)

- Buzzer on/off 
- Sound presets: Calm, Loud, Urgent, Soft, Double Beep, Triple Beep.
- Separately configurable sounds for each event type: tile switching, Wi-Fi disconnection, new notification, ETS2 speeding, touch sensor tap  with a wider tone library available here too (Chime, Bell, Doorbell, Xylophone, Harp, Marimba, etc.).

![Wi-Fi & Access Point](.github/titles/Wifi.png)

- Connects to your Wi-Fi network (STA mode) via in-browser provisioning: network scan, password entry.
- If no Wi-Fi is configured (or if you choose to switch manually), it starts its own **AP hotspot** (defaults to "Octoglow"), with a configurable SSID and password, for direct access to the dashboard.
- You can switch between AP mode and Wi-Fi mode from the dashboard at any time.

![Account & Authentication](.github/titles/ACC.png)

The dashboard is protected with a username + password (cookie-based session). You can change your username and password anytime from the account section.

![OTA (Over-The-Air) Updates](.github/titles/ota.png)

The firmware can check its own version against the GitHub repo and update itself wirelessly, straight from the dashboard:

1. The clock reads the published version from the repo and compares it against the installed version (`FW_VERSION`).
2. If a newer version is available, the dashboard shows the changelog and an install button.
3. The update (`.bin`) is downloaded and flashed onto the ESP32 via the `Update` library, followed by an automatic restart.

## DIY

![Compatibility](.github/titles/Comp.png)

The firmware should work on any ESP board, although it was tested on the following boards:

- ESP32-S3
- ESP32-C3

![Schematics](.github/titles/schematics.png)

Here is a full schematic you can build if you want your Octoglow to run on battery
(the battery circuit is optional and can be skipped).

![schematics](.github/Schematics.png)


| Component | GPIO |
|---|---|
| MAX7219 DIN | GPIO1 |
| MAX7219 CLK | GPIO3 |
| MAX7219 CS | GPIO2 |
| BMP280 SDA | GPIO5 |
| BMP280 SCL | GPIO4 ||
| Touch I/O | GPIO6 |
|Buzzer| GPIO7|
---

![Bill of Materials (BOM)](.github/titles/bom.png)

| Item | Description | Qty | Unit Price ($) | Total ($) | URL |
|------|-------------|:---:|---------------:|----------:|-----|
| LED Matrix 4x MAX7219 | 4-in-1 chained MAX7219 red LED matrix module | 1 | $6.98 | $6.98 | [Link](https://sigmanortec.ro/en/led-matrix-module-4x-max7219-red) |
| ESP32-C3 SuperMini | WiFi/BT microcontroller board 3.3V | 1 | $6.09 | $6.09 | [Link](https://sigmanortec.ro/en/esp32-c3-supermini-development-board-33v-wifi-bluetooth) |
| TTP223 Touch Sensor | Capacitive touch button module | 1 | $0.69 | $0.69 | [Link](https://sigmanortec.ro/en/capacitive-button-ttp223-touch) |
| TP4056 + Boost 5-24V | Battery charger with boost converter 5-24V | 1 | $1.75 | $1.75 | [Link](https://sigmanortec.ro/modul-incarcare-baterie-cu-ridicator-5-24v-tp4056) |
| IP2312 Charger USB-C 3A | Li-Ion charger 5V→4.2V CC/CV Type-C 3A QC | 1 | $2.75 | $2.75 | [Link](https://sigmanortec.ro/modul-incarcare-litiu-5v-la-42v-ip2312-cv-cc-type-c-3a-qc) |
| Mini Switch | Small slide switch, 2 positions | 1 | $0.22 | $0.22 | [Link](https://sigmanortec.ro/en/mini-switch-2-positions) |
| 18650 2500mAh x2 | Samsung 25R 18650 3.7V 2500mAh - set of 2 | 1 | $15.55 | $15.55 | [Link](https://sigmanortec.ro/set-2-acumulator-li-ion-25r-18650-37v-2500mah-8c) |
| BMP280 Sensor 5V | Pressure and temperature sensor 5V | 1 | $2.03 | $2.03 | [Link](https://sigmanortec.ro/en/pressure-and-temperature-sensor-bmp280-5v) |
| 3D Printing | Custom 3D printed case (Printing Legion) | 1 | $12.00 | $12.00 | - |
| **TOTAL** | | | | **$48.06** | |

---

![Firmware Setup](.github/titles/fs.png)

1. Open `Octoglow.ino` in the Arduino IDE (You can get it form `/Firmware/`)
2. Install the ESP32 board package from Boards Manager, if you don't have it yet.
3. Install the required libraries from the Library Manager:
   - `MD_Parola`
   - `MD_MAX72xx`
   - `Adafruit_BMP280`
   - `ArduinoJson`
   - (`WiFi`, `SPI`, `Wire`, `WebServer`, `Preferences`, `HTTPClient`, `Update`, `StreamString`, and `mbedtls` are already bundled with the ESP32 core)
4. Select your board and the correct serial port.
5. Upload the firmware.
6. On first boot, the clock starts in **AP mode**  connect to its Wi-Fi network and open `192.168.4.1` in your browser to set up its connection to your home network.


## Octoglow Sender 

Octoglow sender is a app that makes your matchine comunicate with your octoglow device by sending data over the internet to your octoglow device

![octoglowsender](<.github/Octoglow Sender.png>)

![What it does?](.github/titles/wid.png)

- **Now Playing**  detects the song currently playing on your Matchine (Spotify, browser, etc.) via the Windows Media Session and sends it every few seconds.
- **Windows Notifications**  listens for toast notifications on Windows and forwards them to the clock (with automatic diacritics removal and truncation to a maximum character count).
- **Euro Truck Simulator 2**  if the game is running, reads live telemetry (current speed) and streams it to the clock in real time. Requires an extra one-time plugin install in-game  see Setting Up ETS2 Speed Integration.

![Setting Up ](.github/titles/su.png)

First, you will need to get you Octoglow's IP adress and insert it in the `IP Adress` text box (you can ignore the port most of the times)

### How can I find my Octoglow's IP adress?

## By router's dashboard

- You can find your Octoglow's IP adress in your router's dashboard 
 - Go to `192.168.0.1` or `192.168.1.1` depending on your router 
   - Login to your router's dashboard (the credentials are ushually writen on the back of your router or you can try User : user and Password:admin )
      - Once logged in look for a category named Clients or Devices or Conections or Whatever it really depends on your router
         - Once u find that category you should see a list of devices, each with an IP adress, and you should see a device named esp... that may be octoglow, copy the IPV4 adress and paste it in the adress bar, if you see the octoglow login screen , you are on the octoglow's dashboard

! Warning : Your octoglow IP adress may change after a while, your IP adress will change so you need to reserve IP address

## By tapping the touch sensor on your Octoglow (Not implemented yeet :3)

- If you did not unasign the "show IP adress" in the touch sensor options it should show your IP , but your IP adress will change so you need to reserve IP address

![Setting Up ETS2 Speed Integration](.github/titles/sets2.png)


The ETS2 tile shows your truck's current speed on the clock, but it needs one extra piece set up in-game before it works: ETS2 doesn't expose any telemetry data on its own  a small plugin has to be installed first.

### Install the SCS SDK plugin (in-game)

1. Download the latest **scs-sdk-plugin** (by RenCloud) from:
   `https://github.com/RenCloud/scs-sdk-plugin/releases`
    grab the Windows archive (`win_x64`).
2. Open your ETS2 install folder. By default it's:
   ```
   C:\Program Files (x86)\Steam\steamapps\common\Euro Truck Simulator 2\bin\win_x64\plugins
   ```
   Create the `plugins` folder if it doesn't exist yet.
3. Copy the `.dll` from the downloaded archive into that `plugins` folder.
4. Launch ETS2. On first launch you may see a confirmation message that the SDK was enabled  click OK.

> If you don't see the confirmation message, make sure the DLL sits directly in `bin\win_x64\plugins\`, not in a subfolder.

Made with 🖤 by Adrian
