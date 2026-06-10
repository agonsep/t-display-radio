// Internet radio for LilyGo T-Display-S3 AMOLED + I2S amp (NS4168 / MAX98357A)
//
// Streams one station over WiFi, plays it via I2S and shows station /
// now-playing info on the 536x240 AMOLED.

#include <Arduino.h>
#include <WiFi.h>
#include <Audio.h>
#include <Arduino_GFX_Library.h>

#include "config.h"
#include "fonts/FreeSans9pt7b.h"
#include "fonts/FreeSans12pt7b.h"
#include "fonts/FreeSansBold18pt7b.h"

// ---------------------------------------------------------------- display
static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
static Arduino_RM67162 *panel = new Arduino_RM67162(bus, LCD_RST, 1 /* landscape */);
static Arduino_Canvas *gfx = new Arduino_Canvas(536, 240, panel);

// ---------------------------------------------------------------- audio
static Audio audio;

// ---------------------------------------------------------------- state
static String stationName = STATION_NAME;
static String artistLine = "";
static String trackLine = "connecting...";
static uint32_t bitrate = 0;
static bool paused = false;
static uint8_t volume = DEFAULT_VOLUME;
static bool uiDirty = true;

static int16_t scrollX = 0;
static int16_t trackWidth = 0;

// colors (RGB565) — true black background suits the AMOLED
static const uint16_t C_BG      = BLACK;
static const uint16_t C_ACCENT  = 0x471A;  // turquoise
static const uint16_t C_TEXT    = WHITE;
static const uint16_t C_SUB     = 0xAD75;  // light grey
static const uint16_t C_DIM     = 0x632C;  // mid grey
static const uint16_t C_LINE    = 0x2104;  // near-black grey
static const uint16_t C_PAUSED  = 0xFD20;  // orange

// ------------------------------------------------------------ UI helpers
static int16_t textWidth(const String &s, const GFXfont *font, int16_t *xOff = nullptr) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx->setFont(font);
    gfx->getTextBounds(s.c_str(), 0, 100, &x1, &y1, &w, &h);
    if (xOff) *xOff = x1;
    return (int16_t)w;
}

static void drawCentered(const String &s, const GFXfont *font, uint16_t color,
                         int16_t baselineY) {
    int16_t xOff;
    int16_t w = textWidth(s, font, &xOff);
    gfx->setTextColor(color);
    gfx->setCursor((536 - w) / 2 - xOff, baselineY);
    gfx->print(s);
}

static void drawWifiBars(int16_t x, int16_t y) {  // y = baseline of the bars
    int32_t rssi = WiFi.RSSI();
    int bars = (rssi > -55) ? 4 : (rssi > -65) ? 3 : (rssi > -75) ? 2 : 1;
    for (int i = 0; i < 4; i++) {
        int16_t h = 7 + i * 5;
        gfx->fillRoundRect(x + i * 9, y - h, 6, h, 2,
                           (i < bars) ? C_ACCENT : C_LINE);
    }
}

static void drawVolumeBar(int16_t x, int16_t y, int16_t w) {
    // small speaker glyph
    gfx->fillRect(x - 16, y + 1, 4, 6, C_DIM);
    gfx->fillTriangle(x - 13, y + 4, x - 6, y - 2, x - 6, y + 10, C_DIM);
    // track + fill
    gfx->fillRoundRect(x, y, w, 8, 4, C_LINE);
    int16_t fill = (int16_t)((uint32_t)w * volume / 21);
    if (fill > 7) gfx->fillRoundRect(x, y, fill, 8, 4, C_ACCENT);
}

static void drawUI() {
    gfx->fillScreen(C_BG);
    gfx->setTextWrap(false);

    // ---- header
    gfx->fillCircle(22, 27, 5, paused ? C_PAUSED : C_ACCENT);
    gfx->setFont(&FreeSans12pt7b);
    gfx->setTextColor(C_TEXT);
    gfx->setCursor(38, 35);
    gfx->print(stationName);
    drawWifiBars(481, 37);
    gfx->drawFastHLine(16, 52, 504, C_LINE);

    // ---- now playing
    uint16_t trackColor = paused ? C_DIM : C_TEXT;
    if (artistLine.length()) {
        drawCentered(artistLine, &FreeSans12pt7b, C_SUB, 112);
    }
    int16_t trackBaseline = artistLine.length() ? 162 : 145;
    int16_t xOff;
    trackWidth = textWidth(trackLine, &FreeSansBold18pt7b, &xOff);
    gfx->setTextColor(trackColor);
    if (trackWidth <= 504) {
        gfx->setCursor((536 - trackWidth) / 2 - xOff, trackBaseline);
    } else {
        gfx->setCursor(16 - xOff - scrollX, trackBaseline);
    }
    gfx->print(trackLine);

    // ---- footer
    gfx->drawFastHLine(16, 190, 504, C_LINE);

    if (paused) {  // pause bars
        gfx->fillRoundRect(20, 206, 6, 24, 2, C_PAUSED);
        gfx->fillRoundRect(31, 206, 6, 24, 2, C_PAUSED);
    } else {       // play triangle
        gfx->fillTriangle(20, 205, 20, 229, 39, 217, C_ACCENT);
    }

    gfx->setFont(&FreeSans9pt7b);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(54, 224);
    if (paused) {
        gfx->setTextColor(C_PAUSED);
        gfx->print("paused");
    } else if (bitrate) {
        gfx->printf("%lu kbps", (unsigned long)(bitrate / 1000));
    } else {
        gfx->print("buffering...");
    }

    drawCentered(WiFi.localIP().toString(), &FreeSans9pt7b, C_LINE, 224);
    drawVolumeBar(396, 211, 124);

    gfx->flush();
}

static void splash(const String &line1, const String &line2) {
    gfx->fillScreen(C_BG);
    gfx->setTextWrap(false);
    drawCentered(line1, &FreeSansBold18pt7b, C_TEXT, 110);
    drawCentered(line2, &FreeSans12pt7b, C_DIM, 155);
    gfx->flush();
}

// ------------------------------------------------------- audio callbacks
// FreeSans covers ASCII only — drop other bytes so titles never show garbage
static String asciiOnly(const char *s) {
    String out;
    for (; *s; s++) {
        if ((uint8_t)*s >= 0x20 && (uint8_t)*s < 0x7F) out += *s;
    }
    out.trim();
    return out;
}

void audio_showstation(const char *info) {
    if (strlen(info)) stationName = asciiOnly(info);
    uiDirty = true;
}

void audio_showstreamtitle(const char *info) {
    String t = asciiOnly(info);
    // streams usually send "Artist - Track"
    int sep = t.indexOf(" - ");
    if (sep > 0) {
        artistLine = t.substring(0, sep);
        trackLine = t.substring(sep + 3);
    } else {
        artistLine = "";
        trackLine = t.length() ? t : "...";
    }
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
    splash("Internet Radio", "connecting to " WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
    }
    Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());

    splash("Internet Radio", "buffering " STATION_NAME);

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
        if (trackWidth > 504 && !paused) {
            scrollX += 4;
            if (scrollX > trackWidth + 60) scrollX = -520;
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
        artistLine = "";
        trackLine = "reconnecting...";
        uiDirty = true;
        audio.connecttohost(STATION_URL);
    }
}
