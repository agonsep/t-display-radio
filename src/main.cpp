// Internet radio for LilyGo T-Display-S3 AMOLED + I2S amp (NS4168 / MAX98357A)
//
// - Streams one of 5 stations over WiFi, plays via I2S, shows info on the AMOLED.
// - Joystick 1: up/down = volume, left/right = change station, push = play/pause.
// - Joystick 2: left/right = drive a NEMA17 stepper (A4988) forward/back, with
//   speed proportional to how far the stick is pushed.

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

// ---------------------------------------------------------------- stations
struct Station {
    const char *name;
    const char *url;
};
static const Station kStations[] = {
    {"Groove Salad",   "http://ice1.somafm.com/groovesalad-128-mp3"},
    {"Drone Zone",     "http://ice1.somafm.com/dronezone-128-mp3"},
    {"DEF CON Radio",  "http://ice1.somafm.com/defcon-128-mp3"},
    {"Radio Paradise", "http://stream.radioparadise.com/mp3-128"},
    {"Indie Pop Rocks","http://ice1.somafm.com/indiepop-128-mp3"},
};
static const int kStationCount = sizeof(kStations) / sizeof(kStations[0]);
static int stationIndex = 0;

// ---------------------------------------------------------------- state
static String stationName = kStations[0].name;
static String artistLine = "";
static String trackLine = "connecting...";
static uint32_t bitrate = 0;
static bool paused = false;
static uint8_t volume = DEFAULT_VOLUME;
static bool uiDirty = true;

static int16_t scrollX = 0;
static int16_t trackWidth = 0;

// stepper
static int stepHz = 0;       // current (ramped) step frequency
static bool stepDirFwd = true;

// colors (RGB565) — true black background suits the AMOLED
static const uint16_t C_BG      = BLACK;
static const uint16_t C_ACCENT  = 0x471A;  // turquoise
static const uint16_t C_TEXT    = WHITE;
static const uint16_t C_SUB     = 0xAD75;  // light grey
static const uint16_t C_DIM     = 0x632C;  // mid grey
static const uint16_t C_LINE    = 0x2104;  // near-black grey
static const uint16_t C_PAUSED  = 0xFD20;  // orange
static const uint16_t C_MOTOR   = 0xFE60;  // amber

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
    gfx->fillRect(x - 16, y + 1, 4, 6, C_DIM);                    // speaker glyph
    gfx->fillTriangle(x - 13, y + 4, x - 6, y - 2, x - 6, y + 10, C_DIM);
    gfx->fillRoundRect(x, y, w, 8, 4, C_LINE);                    // track
    int16_t fill = (int16_t)((uint32_t)w * volume / 21);
    if (fill > 7) gfx->fillRoundRect(x, y, fill, 8, 4, C_ACCENT); // fill
}

static void drawUI() {
    gfx->fillScreen(C_BG);
    gfx->setTextWrap(false);

    // ---- header: status dot + station name + index + wifi
    gfx->fillCircle(22, 27, 5, paused ? C_PAUSED : C_ACCENT);
    gfx->setFont(&FreeSans12pt7b);
    gfx->setTextColor(C_TEXT);
    gfx->setCursor(38, 35);
    gfx->print(stationName);

    char idx[8];
    snprintf(idx, sizeof(idx), "%d/%d", stationIndex + 1, kStationCount);
    gfx->setFont(&FreeSans9pt7b);
    int16_t iw = textWidth(idx, &FreeSans9pt7b);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(470 - iw, 33);
    gfx->print(idx);
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

    // footer center: motor status while running, else IP address
    if (stepHz > 0) {
        char m[20];
        snprintf(m, sizeof(m), "%s %d", stepDirFwd ? "FWD >>" : "<< REV", stepHz);
        drawCentered(m, &FreeSans9pt7b, C_MOTOR, 224);
    } else {
        drawCentered(WiFi.localIP().toString(), &FreeSans9pt7b, C_LINE, 224);
    }

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
    int sep = t.indexOf(" - ");           // streams usually send "Artist - Track"
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

// ---------------------------------------------------------------- tuning
static void tuneTo(int idx) {
    stationIndex = (idx % kStationCount + kStationCount) % kStationCount;
    stationName = kStations[stationIndex].name;
    artistLine = "";
    trackLine = "tuning...";
    bitrate = 0;
    scrollX = 0;
    uiDirty = true;
    audio.connecttohost(kStations[stationIndex].url);
}

// ------------------------------------------------------- joystick reading
// Returns signed deflection -1000..+1000 with a center deadzone (0 = centered).
static int readAxis(int pin) {
    int d = analogRead(pin) - ADC_CENTER;
    if (abs(d) < JOY_DEADZONE) return 0;
    int v;
    if (d > 0) v = map(d, JOY_DEADZONE, ADC_MAX - ADC_CENTER, 0, 1000);
    else       v = map(d, -(ADC_CENTER), -JOY_DEADZONE, -1000, 0);
    return constrain(v, -1000, 1000);
}

// --------------------------------------------------------- input handling
static void handleAudioInputs() {
    static uint32_t lastVolStep = 0;
    static bool stationLatched = false;
    static uint32_t lastSw = 0;

    // ---- volume (J1 up/down), auto-repeats while held
    int vy = readAxis(J1_VRY);
    if (!J1_VRY_UP_IS_HIGH) vy = -vy;
    if (vy != 0 && millis() - lastVolStep > 140) {
        lastVolStep = millis();
        if (vy > 0 && volume < 21) volume++;
        else if (vy < 0 && volume > 0) volume--;
        audio.setVolume(volume);
        uiDirty = true;
    }

    // ---- station (J1 left/right), one step per push, must recenter to repeat
    int sx = readAxis(J1_VRX);
    if (!J1_VRX_RIGHT_IS_HIGH) sx = -sx;
    if (!stationLatched && sx > 600) { tuneTo(stationIndex + 1); stationLatched = true; }
    else if (!stationLatched && sx < -600) { tuneTo(stationIndex - 1); stationLatched = true; }
    else if (abs(sx) < 200) stationLatched = false;

    // ---- play / pause: joystick push OR onboard buttons
    bool playPause = (digitalRead(J1_SW) == LOW) || (digitalRead(PIN_BUTTON_BOOT) == LOW);
    if (playPause && millis() - lastSw > 300) {
        lastSw = millis();
        paused = !paused;
        audio.pauseResume();
        uiDirty = true;
    }
    if (digitalRead(PIN_BUTTON_KEY) == LOW && millis() - lastSw > 300) {
        lastSw = millis();
        volume = (volume >= 21) ? 6 : volume + 3;
        audio.setVolume(volume);
        uiDirty = true;
    }
}

// ----------------------------------------------------- stepper (LEDC + ramp)
// Joystick deflection -> target step frequency; hardware LEDC emits the pulse
// train so audio decoding is never blocked. Frequency is ramped toward target
// to avoid stalling, and direction only changes once the motor has stopped.
static void handleStepper() {
    int jx = readAxis(J2_VRX);
    if (!J2_FWD_IS_HIGH) jx = -jx;

    bool wantFwd = (jx >= 0);
    int targetHz = (jx == 0) ? 0 : map(abs(jx), 0, 1000, STEP_MIN_HZ, STEP_MAX_HZ);

    // require a full stop before reversing
    if (jx != 0 && wantFwd != stepDirFwd && stepHz > 0) targetHz = 0;

    // ramp toward target
    if (stepHz < targetHz)      stepHz = min(targetHz, stepHz + STEP_ACCEL);
    else if (stepHz > targetHz) stepHz = max(targetHz, stepHz - STEP_ACCEL);

    // apply new direction only while stopped
    if (stepHz == 0 && jx != 0 && wantFwd != stepDirFwd) {
        stepDirFwd = wantFwd;
        digitalWrite(DIR_PIN, stepDirFwd ? HIGH : LOW);
    }

    if (stepHz <= 0) {
        ledcWriteTone(STEP_LEDC_CHANNEL, 0);
        digitalWrite(EN_PIN, HIGH);   // disable driver (active low) -> motor idle
    } else {
        digitalWrite(EN_PIN, LOW);    // enable driver
        ledcWriteTone(STEP_LEDC_CHANNEL, stepHz);
    }
}

// ------------------------------------------------------------------ setup
void setup() {
    Serial.begin(115200);
    analogReadResolution(ADC_BITS);

    pinMode(PIN_BUTTON_BOOT, INPUT_PULLUP);
    pinMode(PIN_BUTTON_KEY, INPUT_PULLUP);
    pinMode(J1_SW, INPUT_PULLUP);
    pinMode(38, OUTPUT);              // board LED / display power enable
    digitalWrite(38, HIGH);

    // stepper pins — disable the driver before anything else so it doesn't
    // sit there energized and hot at boot
    pinMode(EN_PIN, OUTPUT);
    digitalWrite(EN_PIN, HIGH);
    pinMode(DIR_PIN, OUTPUT);
    digitalWrite(DIR_PIN, HIGH);
    ledcSetup(STEP_LEDC_CHANNEL, 1000, 8);
    ledcAttachPin(STEP_PIN, STEP_LEDC_CHANNEL);
    ledcWriteTone(STEP_LEDC_CHANNEL, 0);

    gfx->begin(80000000);
    splash("Internet Radio", "connecting to " WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
    }
    Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());

    splash("Internet Radio", String("buffering ") + kStations[0].name);

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(volume);
    audio.connecttohost(kStations[stationIndex].url);
}

// ------------------------------------------------------------------- loop
void loop() {
    audio.loop();
    handleAudioInputs();

    // stepper ramp on a steady 20 ms cadence so accel is time-consistent
    static uint32_t lastStep = 0;
    if (millis() - lastStep >= 20) {
        lastStep = millis();
        int prevHz = stepHz;
        handleStepper();
        if (stepHz != prevHz) uiDirty = true;
    }

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
        audio.connecttohost(kStations[stationIndex].url);
    }
}
