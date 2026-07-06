# LilyGO T5 ESPHome DeskHub

ESPHome dashboard for the LILYGO T5 4.7 inch ESP32-S3 e-paper board.

The project is meant to sit on a desk and show Home Assistant data: weather,
temperatures, local device health, Bluetooth proxy status, and investment
statistics from the companion XTB Home Assistant integration.

## Base

The display driver is based on
[nickolay/esphome-lilygo-t547plus](https://github.com/nickolay/esphome-lilygo-t547plus),
which targets the same LILYGO T5 4.7 inch ESP32-S3 / V2.3 board and exposes the
ED047TC1 panel as an ESPHome display component.

This repository vendors the `t547` component locally in `components/t547` and
adapts it for current ESPHome / Arduino 3 builds. The minimal ED047TC1 driver
sources are vendored from
[Xinyuan-LilyGO/LilyGo-EPD47](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47)
so the ESPHome build does not depend on PlatformIO library discovery.

I reviewed [jtenniswood/espcontrol](https://github.com/jtenniswood/espcontrol)
as a strong Home Assistant control-panel reference. For this e-paper build,
the first version stays closer to native ESPHome display pages because e-paper
needs slower, deliberate refreshes rather than a continuously animated GUI.

## Features

- 960x540 ED047TC1 e-paper display through the external `t547` ESPHome component.
- Grayscale rendering enabled for weather, charts and dashboard typography.
- ESPHome display pages for Home, Weather, Investments and System status.
- Home Assistant imported sensors/text sensors for weather and XTB data.
- Graph component for local temperature history.
- Battery voltage and percentage from the board ADC on GPIO14.
- ESP32 internal temperature, WiFi RSSI, uptime and network details.
- GT911 touchscreen bus configured on GPIO18/GPIO17 with interrupt on GPIO47.
- Physical button on GPIO21 to cycle pages.
- Home Assistant buttons for next page, refresh, restart and safe mode.
- Bluetooth proxy package enabled by the default `lilygo-t5-deskhub.yaml`.
- `lilygo-t5-deskhub-lite.yaml` without Bluetooth for Windows builds and first bring-up.
- Optional web server/captive portal package for browser diagnostics when the firmware budget allows it.

## Quick Start

1. Copy `secrets.yaml.example` to `secrets.yaml`.
2. Fill WiFi, OTA and API values.
3. Edit substitutions in `lilygo-t5-deskhub.yaml` to match your Home Assistant
   entity IDs.
4. Build and flash the full Bluetooth-proxy variant:

```powershell
python -m esphome run lilygo-t5-deskhub.yaml
```

Use USB for the first flash. OTA should work after the device joins WiFi.

On Windows, the ESP32-S3 Arduino 3 build can exceed the command-line length
limit when Bluetooth support is enabled. The configuration reads
`ESPHOME_BUILD_PATH`, so use a short path when compiling or flashing:

```powershell
$env:ESPHOME_BUILD_PATH = "C:/t5b"
python -m esphome run lilygo-t5-deskhub.yaml --device COM6
```

If Windows still reports `The command line is too long`, build the full
Bluetooth variant from the Home Assistant ESPHome add-on, Docker, WSL, or Linux.
For local display/touch/UI bring-up on Windows, use the lite variant:

```powershell
$env:ESPHOME_BUILD_PATH = "C:/t5b"
python -m esphome run lilygo-t5-deskhub-lite.yaml --device COM6
```

To enable browser diagnostics later, add this package to
`lilygo-t5-deskhub.yaml`. It also enables fallback AP and captive portal:

```yaml
packages:
  web_server: !include packages/web_server_optional.yaml
```

## Build Status

Validated locally with ESPHome `2026.6.0-dev` from `kyvaith/esphome`.

- `python -m esphome config lilygo-t5-deskhub.yaml` passes.
- `python -m esphome config lilygo-t5-deskhub-lite.yaml` passes.
- `python -m esphome compile lilygo-t5-deskhub-lite.yaml` passes on Windows.
- The full Bluetooth variant can hit the Windows command-line length limit
  during linking. Build it from the Home Assistant ESPHome add-on, Docker, WSL,
  or Linux if local Windows compilation fails.

## Pages And Entities

Pages are defined in `packages/display_pages.yaml`.

Home Assistant entities are defined in `packages/ha_entities.yaml`. The top-level
`lilygo-t5-deskhub.yaml` file provides substitutions for the default entity IDs,
so the normal edit path is:

```yaml
substitutions:
  entity_indoor_temperature: sensor.office_temperature
  entity_xtb_portfolio_value: sensor.xtb_portfolio_value
```

More examples are in [docs/ADDING_PAGES.md](docs/ADDING_PAGES.md).

## Refresh Strategy

The current build uses the `t547` component, which performs full grayscale
refreshes. That is a good baseline for a desk dashboard and supports the panel
grayscale path cleanly.

The fast "magic refresh" used in the Paperboy emulator is a separate binary
drive path. It is useful for animation, but not yet exposed as a reusable
ESPHome component. If we want that in ESPHome later, the likely next step is a
custom component in `kyvaith/esphome` with:

- binary fast-refresh mode for selected widgets,
- full grayscale refresh for static pages,
- a configurable full-refresh cadence to fight ghosting.

## Notes

- Bluetooth proxy is enabled, but ESPHome warns that memory can become tight on
  some boards. Use `lilygo-t5-deskhub-lite.yaml` for first bring-up or Windows
  builds that hit the command-line limit.
- The display is set to `rotation: 90` for a portrait desk orientation. Change
  it to `0`, `180`, or `270` if your case/stand is different.
- The default XTB entity names are placeholders until the XTB Home Assistant
  add-on publishes its final sensors.
