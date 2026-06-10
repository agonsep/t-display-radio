#pragma once

// ---------- WiFi ----------
#define WIFI_SSID     "LexGonSep"
#define WIFI_PASSWORD "DucatiV4!"

// ---------- Radio station ----------
// Any MP3/AAC stream URL works. A few alternatives:
//   http://ice1.somafm.com/groovesalad-128-mp3   (SomaFM Groove Salad)
//   http://ice1.somafm.com/defcon-128-mp3        (SomaFM DEF CON Radio)
//   http://stream.radioparadise.com/mp3-128      (Radio Paradise)
#define STATION_URL  "http://ice1.somafm.com/groovesalad-128-mp3"
#define STATION_NAME "SomaFM Groove Salad"

// ---------- I2S amp wiring (NS4168 / MAX98357A) ----------
// Amp pin  ->  T-Display-S3 AMOLED pin
//   VIN    ->  3V3 (or 5V)
//   GND    ->  GND
//   DIN    ->  GPIO 11
//   BCLK   ->  GPIO 12
//   LRC    ->  GPIO 13
//   SD/GAIN -> leave unconnected
#define I2S_DOUT 11
#define I2S_BCLK 12
#define I2S_LRC  13

// ---------- Buttons ----------
#define PIN_BUTTON_BOOT 0   // bottom button: play / pause
#define PIN_BUTTON_KEY  21  // top button: volume step

// ---------- Display (RM67162 AMOLED over QSPI, fixed by board) ----------
#define LCD_CS   6
#define LCD_SCK  47
#define LCD_D0   18
#define LCD_D1   7
#define LCD_D2   48
#define LCD_D3   5
#define LCD_RST  17

#define DEFAULT_VOLUME 12  // 0..21
