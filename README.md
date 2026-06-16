# T-Display Internet Radio

Internet radio for the **LilyGo T-Display-S3 AMOLED** (ESP32-S3, 1.91" RM67162
536x240 AMOLED) with an **NS4168 / MAX98357A** I2S amplifier, two **KY-023
analog joysticks**, and a **NEMA17 stepper** on an **A4988/DRV8825** driver.

- Streams one of 5 stations, plays through the amp, shows now-playing on screen.
- Joystick 1 controls the radio (volume / station / play-pause).
- Joystick 2 jogs the stepper forward/back, speed set by how far you push.

All pins live in [include/config.h](include/config.h); the station list is in
[src/main.cpp](src/main.cpp) (`kStations[]`).

## Wiring

### I2S amp (NS4168 / MAX98357A)

| Amp pin | Board pin |
|---------|-----------|
| VIN     | 3V3 (or 5V) |
| GND     | GND |
| DIN     | GPIO 11 |
| BCLK    | GPIO 12 |
| LRC     | GPIO 13 |
| SD / GAIN | leave unconnected |

Speaker (4-8 ohm) on the amp's speaker terminals.

### Joystick 1 — radio control (KY-023)

> Power the joysticks from **3V3, not 5V** — the ESP32-S3 ADC tops out at 3.3 V.

| Joystick pin | Board pin | Action |
|--------------|-----------|--------|
| +V  | 3V3 | |
| GND | GND | |
| VRx | GPIO 1  | left / right -> previous / next station |
| VRy | GPIO 2  | up / down -> volume |
| SW  | GPIO 16 | push -> play / pause |

### Joystick 2 — stepper control (KY-023)

| Joystick pin | Board pin | Action |
|--------------|-----------|--------|
| +V  | 3V3 | |
| GND | GND | |
| VRx | GPIO 10 | left / right -> reverse / forward, speed by deflection |
| VRy, SW | leave unconnected | |

### Stepper driver (A4988 / DRV8825) + NEMA17

| Driver pin | Connect to |
|------------|-----------|
| VDD (logic) | 3V3 |
| GND (logic) | GND |
| VMOT | 12V supply (+) |
| GND (motor) | 12V supply (-), **shared with board GND** |
| STEP | GPIO 43 |
| DIR  | GPIO 44 |
| ~ENABLE | GPIO 15 |
| RESET | tie to SLEEP (both high) |
| MS1, MS2 | high; MS3 low -> **1/8 microstepping** |
| 1A / 1B | one motor coil |
| 2A / 2B | other motor coil |

Add a **100µF cap across VMOT/GND** at the driver, and set the driver's current
limit before connecting the motor. The firmware de-energizes the motor (ENABLE
high) whenever the stick is centered, so it stays cool at idle.

> The motor only spins with its own 12V supply connected — USB power alone runs
> the logic and radio but won't turn the motor.

## Build & flash

```sh
pio run -t upload        # build + flash over USB
pio device monitor       # serial log at 115200
```

If the upload doesn't start, hold **BOOT** (bottom button), tap **RST**, then
release BOOT to force the bootloader.

## Controls

| Input | Action |
|-------|--------|
| Joystick 1 up / down | volume (auto-repeats while held) |
| Joystick 1 left / right | previous / next station |
| Joystick 1 push, or BOOT button | play / pause |
| KEY button (GPIO 21) | volume step (fallback) |
| Joystick 2 left / right | stepper reverse / forward; further = faster |

If any joystick axis feels reversed, flip its `*_IS_HIGH` flag in
[include/config.h](include/config.h) — no rewiring needed.

## Display

- **Top:** status dot, station name, station index (e.g. `2/5`), WiFi signal
- **Middle:** artist (grey) + track title (bold, auto-scrolls when long)
- **Bottom:** play/pause icon, bitrate, volume bar, and either the IP address
  or the live motor status (`FWD >> 1400`) while the stepper is moving

## Notes / tuning

- **Stepper too fast / stalls:** the defaults assume 1/8 microstepping. For
  full-step wiring, lower `STEP_MAX_HZ` (~700) in config.h. `STEP_ACCEL` sets the
  ramp rate; `STEP_MIN_HZ` is the speed at the edge of the deadzone.
- STEP pulses come from the ESP32 hardware timer (LEDC), so motor jogging never
  interferes with audio decoding.
- Analog joystick axes must stay on ADC1 pins (GPIO 1-10) — ADC2 is unavailable
  while WiFi is active.
