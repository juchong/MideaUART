# Air-conditioner protocol architecture

The air-conditioner API separates transport validity, confirmed appliance state, diagnostic
data groups, capabilities, and property deltas. A decoded value is never published or cached
until its complete payload passes CRC8 or legacy additive-checksum validation.

## Confirmed state and control

`AirConditionerState` is the authoritative read model. Its `Optional` fields distinguish an
unreported field from a legitimate zero or false value. `valid`, `received_at`, and `revision`
describe the confirmed C0 snapshot.

Controls are deltas. The library merges rapid calls to `control()` and applies the result to a
fresh, validated C0 snapshot. If no fresh snapshot exists, it first requests one; it does not
construct a full-state write from protocol defaults. This matters because Midea's 0x40 command
contains several unrelated settings in one frame.

The classic C0/0x40 path owns mode, power, target temperature, fan, swing, presets, temperature
unit, purifier/anion, auxiliary heat, and target humidity. The purifier/anion control remains
on this path: known firmware may advertise property ID `0x021E` while ignoring B0 writes to it.

## Protocol surfaces

| Surface | Purpose | Validation |
|---|---|---|
| C0 / 0x40 | Confirmed state and full-state control | Minimum C0 prefix plus CRC8 or additive checksum |
| B5 | Capability records | Integrity, record count, record header, and declared-length bounds |
| C1 | Read-only data groups | Integrity, fixed header, requested group ID, and group-specific minimum length |
| B1 / B0 | Property query / property write | Integrity, record count, status, and declared-length bounds |

Unknown B5 records are retained in `Capabilities::records()` so a new device feature does not
require a parser rewrite merely to inspect its advertised data. B0/B1 callbacks likewise return
every successfully framed property, including IDs without a typed state mapping.

## C1 groups

The on-wire group query is the captured short form `41 21 01 (40 | group) 00 01 CRC`.
It deliberately has no message ID and must not be padded to the length of the legacy status
query. The exact known group-4 capture (`41 21 01 44 00 01 09`) is regression-tested.

Group polling is consumer-driven through `requestGroup()`. Only the legacy energy query is
enabled by default; integrations should request additional groups only when at least one
configured entity consumes their data.

| Group | Typed data |
|---:|---|
| 0 | Electrified, total-operating, and current-operating hours |
| 1 | Compressor frequency/target/current/voltage and thermistor temperatures |
| 2 | Indoor-fan target/actual speed plus raw fault and protection bitfields |
| 3 | Outdoor-fan speed |
| 4 | Total/current energy and real-time power with selectable encoding |
| 5 | Indoor humidity, outdoor-fan speed, and defrost state |
| 7 | Outdoor-unit power |
| 11 | Horizontal and vertical louver angles |

Group 2's diagnostic bytes intentionally remain raw. Public protocol references disagree with
older implementations that labeled one bit as “water pump running.” The separately defined
`WATER_PUMP` B0/B1 property is still exposed as typed state.

Group 4 supports BCD, binary, mixed-base-100, binary 0.01 kWh, and BCD-energy/binary-power
formats through `setPowerFormat()`.

## Request behavior

Each request can override the global retry count. Diagnostic group polling defaults to one
attempt so unsupported optional queries do not stall normal climate traffic. A request for a
group already pending, or a second B0/B1 transaction while one is pending, is rejected locally.
The caller receives `false` and can retry later.

## Verification

Run the native protocol suite with:

```sh
python test/run_protocol_tests.py
```

It covers captured valid and malformed C0 frames, both integrity schemes, bounds checks, all
implemented C1 groups and power encodings, B0/B1 framing, B5 parsing, and confirmed-state
control coalescing. PlatformIO CI separately compiles the library for the ESP8266 `esp12e`,
ESP32 Arduino, and ESP32 ESP-IDF targets.
