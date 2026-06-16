#pragma once

// ---------- WiFi ----------
#define WIFI_SSID     "LexGonSep"
#define WIFI_PASSWORD "DucatiV4!"

// ---------- Stations ----------
// 5 MP3 streams. Switch between them with joystick 1 left/right.
// Station list itself lives in src/main.cpp (kStations[]).
#define DEFAULT_VOLUME 12  // 0..21

// ---------- I2S amp wiring (NS4168 / MAX98357A) ----------
//   VIN -> 3V3   GND -> GND   DIN -> 11   BCLK -> 12   LRC -> 13
#define I2S_DOUT 11
#define I2S_BCLK 12
#define I2S_LRC  13

// ---------- Onboard buttons ----------
#define PIN_BUTTON_BOOT 0   // bottom button: play / pause
#define PIN_BUTTON_KEY  21  // top button: volume step

// ---------- Joystick 1: audio control (KY-023) ----------
// Power the joystick from 3V3 (NOT 5V) so the wiper range fits the ADC.
//   +V -> 3V3   GND -> GND
#define J1_VRX 1    // left / right -> previous / next station   (ADC1)
#define J1_VRY 2    // up / down    -> volume up / down           (ADC1)
#define J1_SW  16   // push         -> play / pause               (digital, pull-up)
// If an axis feels reversed, flip the matching sign flag below.
#define J1_VRY_UP_IS_HIGH  1   // set to 0 if pushing up lowers volume
#define J1_VRX_RIGHT_IS_HIGH 1 // set to 0 if right selects the previous station

// ---------- Joystick 2: stepper control (KY-023) ----------
//   +V -> 3V3   GND -> GND
#define J2_VRX 10   // left / right -> reverse / forward, speed by deflection (ADC1)
// J2 VRy and SW are left unconnected.
#define J2_FWD_IS_HIGH 1   // set to 0 to swap forward/back

// ---------- Stepper: NEMA17 via A4988 / DRV8825 (STEP/DIR) ----------
//   VDD  -> 3V3        GND(logic) -> GND
//   VMOT -> 12V        GND(motor) -> 12V supply GND (share GND with the board)
//   100uF cap across VMOT/GND. RESET tied to SLEEP (both high).
//   Set the driver to 1/8 microstepping (A4988: MS1=H, MS2=H, MS3=L).
//   1A/1B/2A/2B -> the two NEMA17 coils.
#define STEP_PIN  43
#define DIR_PIN   44
#define EN_PIN    15   // A4988 ~ENABLE (active LOW): motor de-energized when idle
#define STEP_LEDC_CHANNEL 4
#define STEP_MIN_HZ  120    // step rate at the edge of the deadzone
#define STEP_MAX_HZ  2600   // step rate at full deflection (~97 rpm @ 1/8 step)
#define STEP_ACCEL   70     // max Hz change per 20 ms tick (stall-avoidance ramp)

// ---------- Joystick calibration ----------
#define ADC_BITS     12
#define ADC_MAX      4095
#define ADC_CENTER   2048
#define JOY_DEADZONE 350    // ignore small wiggles around center

// ---------- Display (RM67162 AMOLED over QSPI, fixed by board) ----------
#define LCD_CS   6
#define LCD_SCK  47
#define LCD_D0   18
#define LCD_D1   7
#define LCD_D2   48
#define LCD_D3   5
#define LCD_RST  17
