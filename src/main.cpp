// Internet radio for LilyGo T-Display-S3 AMOLED + I2S amp (NS4168 / MAX98357A)
//
// Streams one station over WiFi, plays it via I2S and shows station /
// now-playing info on the 536x240 AMOLED.

#include <Arduino.h>
#include <WiFi.h>
#include <Audio.h>
#include <Arduino_GFX_Library.h>

#include "config.h"

// ---------------------------------------------------------------- display
static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
static Arduino_RM67162 *panel = new Arduino_RM67162(bus, LCD_RST, 1 /* landscape */);
static Arduino_Canvas *gfx = new Arduino_Canvas(536, 240, panel);

// ---------------------------------------------------------------- audio
static Audio audio;

// ---------------------------------------------------------------- state
static String stationName = STATION_NAME;
static String streamTitle = "connecting...";
static uint32_t bitrate = 0;
static bool paused = false;
static uint8_t volume = DEFAULT_VOLUME;
static bool uiDirty = true;

static int16_t scrollX = 0;
static int16_t titleWidth = 0;

// colors (RGB565)
static const uint16_t C_BG      = BLACK;
static const uint16_t C_ACCENT  = 0x05FF;  // cyan-blue
static const uint16_t C_TITLE   = WHITE;
static const uint16_t C_DIM     = 0x8410;  // grey
static const uint16_t C_PAUSED  = 0xFD20;  // orange

// ------------------------------------------------------------ UI drawing
static int16_t textWidth(const String &s, uint8_t size) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx->setTextSize(size);
    gfx->getTextBounds(s.c_str(), 0, 0, &x1, &y1, &w, &h);
    return (int16_t)w;
}

static void drawUI() {
    gfx->fillScreen(C_BG);

    // top bar: station name
    gfx->setTextColor(C_ACCENT);
    gfx->setTextSize(3);
    gfx->setCursor(12, 14);
    gfx->print(stationName);
    gfx->drawFastHLine(0, 52, 536, C_DIM);

    // middle: now playing (marquee if too wide)
    gfx->setTextColor(paused ? C_DIM : C_TITLE);
    gfx->setTextSize(3);
    titleWidth = textWidth(streamTitle, 3);
    if (titleWidth <= 512) {
        gfx->setCursor((536 - titleWidth) / 2, 110);
        gfx->print(streamTitle);
    } else {
        gfx->setCursor(12 - scrollX, 110);
        gfx->print(streamTitle);
    }

    // bottom bar
    gfx->drawFastHLine(0, 188, 536, C_DIM);
    gfx->setTextSize(2);
    gfx->setCursor(12, 206);
    if (paused) {
        gfx->setTextColor(C_PAUSED);
        gfx->print("PAUSED");
    } else {
        gfx->setTextColor(C_DIM);
        gfx->printf("%lu kbps", (unsigned long)(bitrate / 1000));
    }

    gfx->setTextColor(C_DIM);
    String right = "vol " + String(volume) + "  " + WiFi.localIP().toString();
    gfx->setCursor(536 - 12 - textWidth(right, 2), 206);
    gfx->print(right);

    gfx->flush();
}

static void splash(const String &line1, const String &line2) {
    gfx->fillScreen(C_BG);
    gfx->setTextColor(C_ACCENT);
    gfx->setTextSize(3);
    gfx->setCursor((536 - textWidth(line1, 3)) / 2, 90);
    gfx->print(line1);
    gfx->setTextColor(C_DIM);
    gfx->setTextSize(2);
    gfx->setCursor((536 - textWidth(line2, 2)) / 2, 140);
    gfx->print(line2);
    gfx->flush();
}

// ------------------------------------------------------- audio callbacks
void audio_showstation(const char *info) {
    if (strlen(info)) stationName = info;
    uiDirty = true;
}

void audio_showstreamtitle(const char *info) {
    streamTitle = strlen(info) ? info : "...";
    scrollX = 0;
    uiDirty = true;
}

void audio_bitrate(const char *info) {
    bitrate = atol(info);
    uiDirty = true;
}

void audio_info(const char *info) {
    Serial.printf("[audio] %s\n", info);
}

// ---------------------------------------------------------------- buttons
static void handleButtons() {
    static uint32_t lastPress = 0;
    if (millis() - lastPress < 300) return;

    if (digitalRead(PIN_BUTTON_BOOT) == LOW) {  // play / pause
        lastPress = millis();
        paused = !paused;
        audio.pauseResume();
        uiDirty = true;
    } else if (digitalRead(PIN_BUTTON_KEY) == LOW) {  // volume step
        lastPress = millis();
        volume = (volume >= 21) ? 6 : volume + 3;
        audio.setVolume(volume);
        uiDirty = true;
    }
}

// ------------------------------------------------------------------ setup
void setup() {
    Serial.begin(115200);

    pinMode(PIN_BUTTON_BOOT, INPUT_PULLUP);
    pinMode(PIN_BUTTON_KEY, INPUT_PULLUP);
    pinMode(38, OUTPUT);  // board LED / display power enable
    digitalWrite(38, HIGH);

    gfx->begin(80000000);
    splash("Internet Radio", "connecting to " WIFI_SSID " ...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
    }
    Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());

    splash("Internet Radio", "buffering " STATION_NAME " ...");

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(volume);
    audio.connecttohost(STATION_URL);
}

// ------------------------------------------------------------------- loop
void loop() {
    audio.loop();
    handleButtons();

    // marquee + redraw, throttled so audio decoding gets the CPU
    static uint32_t lastTick = 0;
    if (millis() - lastTick >= 80) {
        lastTick = millis();
        if (titleWidth > 512 && !paused) {
            scrollX += 4;
            if (scrollX > titleWidth + 60) scrollX = -524;
            uiDirty = true;
        }
        if (uiDirty) {
            uiDirty = false;
            drawUI();
        }
    }

    // simple auto-reconnect if the stream drops
    static uint32_t lastRunning = 0;
    if (audio.isRunning() || paused) {
        lastRunning = millis();
    } else if (millis() - lastRunning > 5000) {
        lastRunning = millis();
        streamTitle = "reconnecting...";
        uiDirty = true;
        audio.connecttohost(STATION_URL);
    }
}
