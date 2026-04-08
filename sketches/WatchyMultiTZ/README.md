# WatchyMultiTZ

A three-timezone watchface for SQFMI Watchy v1.5 (ESP32-PICO-D4). Shows the
currently selected "home" zone in a large DSEG7 slot with the full date, plus
two other zones in compact strips above and below it. Day-delta badges (`+1d`,
`-1d`, ...) are shown on the small slots whenever they fall on a different
calendar day than the main zone. The bottom strip draws a battery gauge, a
WiFi marker (`W` if provisioned), and three dots indicating which zone is
currently selected.

## Zones

Edit `settings.h` to change the `ZONES[]` table. Each entry is a POSIX TZ
string with full DST rules, so DST transitions are handled automatically
without any network round-trip.

## Build

Arduino IDE or `arduino-cli`:

```
arduino-cli compile --fqbn esp32:esp32:watchy:Revision=v15,PartitionScheme=min_spiffs WatchyMultiTZ
```

Depends only on `sqfmi/Watchy` 1.4.15 and the ESP32 Arduino core 2.0.17.

## Buttons (watchface state)

- **UP**: cycle to next zone (becomes the new "home" / big slot)
- **DOWN**: force a WiFi+NTP resync now
- **MENU**: open the stock Watchy menu
- **BACK**: reserved (no-op)

As a safety net, the watchface also forces an NTP resync once every ~24h
(1440 minute-ticks) in case the weather poll has not run.
