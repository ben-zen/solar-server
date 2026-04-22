# Renogy Solar Controller Library

A C++20 library for communicating with Renogy solar charge controllers over
Modbus RTU (RS232).  It reads live telemetry (battery state, solar panel
output, load, daily and lifetime statistics) and controller identification
data from Renogy charge controllers such as the Wanderer and Rover series.

## Attribution

The Modbus register map, data parsing logic, and protocol knowledge in this
library are derived from
[**sophienyaa/NodeRenogy**](https://github.com/sophienyaa/NodeRenogy)
by Mick Wheeler, licensed under the MIT License.  The original project
provides the same functionality in Node.js, publishing data to MQTT for use
with Home Assistant and similar systems.

## Architecture

| File | Purpose |
|------|---------|
| `serial.hh` / `serial.cc` | POSIX serial port RAII wrapper (termios, 8N1) |
| `modbus.hh` / `modbus.cc` | Modbus RTU protocol: CRC-16, request framing, response parsing |
| `renogy.hh` / `renogy.cc` | Renogy-specific data types (`controller_data`, `controller_info`) and register-to-struct parsing |
| `renogy_test.cc` | Unit tests (Modbus CRC, frame construction/parsing, Renogy register decoding) |

## Building

The Meson build produces a static library (`librenogy.a`) and a test binary:

```sh
CC=clang CXX=clang++ meson setup build src
meson compile -C build
```

Or using Just from the repository root:

```sh
just renogy build
```

## Testing

```sh
meson test -C build -v
```

Or:

```sh
just renogy test
```

## Supported Registers

### Controller Information (0x00A – 0x01A)

| Register | Description |
|----------|-------------|
| 0x00A | Voltage / current rating |
| 0x00B | Discharge current / controller type |
| 0x00C – 0x013 | Model name (ASCII) |
| 0x014 – 0x015 | Software version |
| 0x016 – 0x017 | Hardware version |
| 0x018 – 0x019 | Serial number |
| 0x01A | Modbus address |

### Live Telemetry (0x100 – 0x122)

| Register | Description | Unit |
|----------|-------------|------|
| 0x100 | Battery capacity (SOC) | % |
| 0x101 | Battery voltage | V |
| 0x102 | Battery charge current | A |
| 0x103 | Controller / battery temperature | °C |
| 0x104 | Load voltage | V |
| 0x105 | Load current | A |
| 0x106 | Load power | W |
| 0x107 | Solar panel voltage | V |
| 0x108 | Solar panel current | A |
| 0x109 | Solar panel power | W |
| 0x10B | Min battery voltage today | V |
| 0x10C | Max battery voltage today | V |
| 0x10D | Max charge current today | A |
| 0x10E | Max discharge current today | A |
| 0x10F | Max charge power today | W |
| 0x110 | Max discharge power today | W |
| 0x111 | Charge amp-hours today | Ah |
| 0x112 | Discharge amp-hours today | Ah |
| 0x113 | Charge watt-hours today | Wh |
| 0x114 | Discharge watt-hours today | Wh |
| 0x115 | Controller uptime | days |
| 0x116 | Total battery over-discharges | count |
| 0x117 | Total battery full charges | count |
| 0x118 – 0x119 | Total charge amp-hours | Ah |
| 0x11A – 0x11B | Total discharge amp-hours | Ah |
| 0x11C – 0x11D | Cumulative power generated | kWh |
| 0x11E – 0x11F | Cumulative power consumed | kWh |
| 0x120 | Load status / charging state | — |
| 0x121 – 0x122 | Fault codes | — |

## Hardware Notes

Renogy controllers that use RS232 (e.g. Wanderer, Rover) expose a RJ12 jack
for serial communication.  You will need a USB-to-RS232 adapter and a custom
RJ12-to-DB9 cable.  See the
[NodeRenogy README](https://github.com/sophienyaa/NodeRenogy#connecting-your-controller)
for wiring details.

**Important:** TTL serial (Raspberry Pi GPIO, microcontrollers) is *not* the
same as RS232.  Do not connect them directly — use a level-shifting adapter
(e.g. MAX3232).

## License

MIT
