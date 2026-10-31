# Solar controller firmware (ESPHome)

ESPHome firmware for the rev 0.1 board (ESP32-C6-MINI-1U-H4). It drives 10 globes
(Sun + 9 planets, inner to outer, 7 SK6812 RGBW LEDs each) and shows up in Home
Assistant through the ESPHome integration.

## Build and flash

```bash
uv tool install --python 3.13 esphome
cp secrets.example.yaml secrets.yaml   # then fill in Wi-Fi and keys
esphome run solar-controller.yaml      # USB the first time, OTA after that
```

`esphome logs solar-controller.yaml` shows the log over Wi-Fi or USB.

## Home Assistant

The device is discovered as **Solar Chain**. Adding it asks for the
`api_encryption_key` from `secrets.yaml`.

| Entity | What it does |
| --- | --- |
| Solar Chain (light) | On/off, brightness, one colour for every globe, or the **Planets** effect |
| Sun ... Pluto (lights) | Each globe's colour in the Planets effect. Disabled by default; enable them in HA to tune colours. Off = that globe dark |
| LED current | Measured from the eFuse ILM pin |
| Fault | None, eFuse thermal shutdown, or Overcurrent. Latched until cleared |
| Clear fault | Clears a latched fault (turning the knob or pressing it does too) |
| Current budget | Limit for the estimated LED current, 50-1600 mA |
| Connected LEDs | LEDs used for the current estimate, for partial chains on the bench |
| Estimated LED current, Current limiter, eFuse FLT, LED power, Wi-Fi signal | Diagnostics |

Colours are linear (no gamma), so the planet values in `solar-controller.yaml`
are what goes out on the wire at 100 % brightness.

## Knob

- **Turn:** brightness in 30 perceptual steps from 2 % to 100 %. One step below
  2 % turns the light off; turning up from off starts the Planets effect.
- **Press:** Planets, then warm white (the Sun's colour on every globe), then off.

## Protection

- The light's current is estimated from the pixel values (16 mA per channel,
  1 mA idle per LED) and scaled down to fit the budget.
- A measured guard lowers the scale further while the ILM reading is above
  110 % of the budget.
- 1800 mA measured for 3 samples (300 ms) or the eFuse FLT pin going low turns
  the LEDs off and latches a fault.

## Status LED (D301)

| Pattern | Meaning |
| --- | --- |
| Fast blink | Fault latched |
| Short flash every second | Not connected to Home Assistant |
| On | LED supply on |
| Off | LED supply off |
