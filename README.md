# T-Display Internet Radio

Internet radio for the **LilyGo T-Display-S3 AMOLED** (ESP32-S3, 1.91" RM67162
536x240 AMOLED) with an **NS4168 / MAX98357A** I2S amplifier module.

Streams one station (SomaFM Groove Salad by default), plays it through the amp
and shows the station + now-playing track title on the screen.

## Wiring (amp -> board)

| Amp pin | T-Display-S3 AMOLED pin |
|---------|-------------------------|
| VIN     | 3V3 (or 5V)             |
| GND     | GND                     |
| DIN     | GPIO 11                 |
| BCLK    | GPIO 12                 |
| LRC     | GPIO 13                 |
| SD      | leave unconnected       |
| GAIN    | leave unconnected       |

Speaker (4-8 ohm) on the amp's speaker terminals. All pins are configurable in
[include/config.h](include/config.h).

## Configuration

WiFi credentials and the station URL live in [include/config.h](include/config.h).
Any plain MP3/AAC stream URL works for `STATION_URL`.

## Build & flash

```sh
pio run -t upload        # build + flash over USB
pio device monitor       # serial log at 115200
```

If the upload doesn't start, hold **BOOT** (bottom button), tap **RST**, then
release BOOT to force the bootloader.

## Controls

- **BOOT button** (GPIO 0): play / pause
- **KEY button** (GPIO 21): volume step (6 -> 21, then wraps)

## Display

- Top: station name
- Middle: now-playing title (auto-scrolls when too long)
- Bottom: bitrate, volume, IP address
