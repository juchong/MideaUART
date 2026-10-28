# Air-conditioner protocol architecture and coverage

This document is the coverage contract for the `0xAC` appliance implementation. It separates
facts that are safe to expose generically from model-specific observations that require a
qualified fixture. “Supported” means parsed or emitted with bounds/integrity checks and covered
by a regression test; it does not mean every Midea-derived appliance implements the feature.

The principal comparison implementation is the current
[`midea-local` AC message implementation](https://github.com/midea-lan/midea-local/blob/main/midealocal/devices/ac/message.py).
Captured UART frames in `test/fixtures/` and exact command captures take precedence over a
comparison implementation when they disagree.

## Evidence levels

| Level | Meaning | Library policy |
|---|---|---|
| Captured | Observed on a UART appliance or preserved fixture | Typed decoding/control is allowed |
| Corroborated | Same layout and semantics in independent maintained implementations | Typed decoding is allowed; optional queries remain opt-in |
| Model-qualified | Valid only for a known model/subtype | Never enabled by the generic UART path |
| Unknown | Tag/bytes are known but semantics are not | Preserve raw data; do not label or control it |

## Protocol-family matrix

| Family | Purpose | Current implementation | Validation | Remaining qualification |
|---|---|---|---|---|
| `C0` response / `0x40` control | Core state and full-state control | Typed, confirmed-state model | CRC8 or additive checksum, minimum field length, captured malformed frames | Extended/model-specific fields remain optional by frame length |
| `B5` | Capability discovery | Typed known value sets plus lossless raw records | Integrity, record count, header/length bounds, trailer bounds, multi-page merge | Unknown capability IDs remain raw |
| `C1` | Engineering/diagnostic groups | Groups 0, 1, 2, 3, 4, 5, 7, 11 | Integrity, fixed header, requested group and group-specific minimum length | Appliance support is discovered by bounded probing |
| `B1` / `B0` | Property query/write | Complete known tag vocabulary, typed safe subset, lossless raw records | Integrity, record count, declared lengths, exact trailer shape, expected-ID correlation | Unsupported tags must be isolated and quarantined |
| `A0` / `A1` | Alternate query/notify layouts | Not emitted; no generic state merge | Raw transport can receive the outer frame | Needs UART fixtures proving when it supersedes `C0` |
| `BB` subprotocol | Model-specific indoor/outdoor/fresh-air groups | Not implemented generically | N/A | Requires model/subtype detection and fixtures for each layout |
| Property `0x007E` | Alternate temperature payload | Vocabulary/raw preservation only | B0/B1 record validation | Known to supersede stale `C0` values only on qualified models |

The last three rows are deliberately not inferred from Wi-Fi/cloud implementations. Those
protocols contain model routing information that the UART bridge does not currently possess.

## Confirmed state and control

`AirConditionerState` is the authoritative read model. `Optional` fields distinguish an
unreported value from a legitimate zero or false value. `valid`, `received_at`, and `revision`
describe the last confirmed `C0` snapshot.

Controls are deltas. Rapid calls to `control()` are coalesced and applied to a fresh validated
snapshot. If no fresh snapshot exists, the library queries one first; it never constructs a
full-state write from defaults. This prevents a mode or setpoint update from overwriting
unrelated state carried in the same `0x40` frame.

The classic path owns power, mode, setpoint, fan, swing, presets, temperature unit,
purifier/anion, auxiliary heat, and humidity target. Purifier writes stay on `0x40`: some
firmware advertises property `0x021E` while ignoring a B0 write to that property.

### C0 typed fields

| Byte/field | Typed state | Notes |
|---|---|---|
| 1 | power, appliance-error flag | Error code itself is byte 16 |
| 2 | mode, target temperature | Half-degree bit is retained |
| 3 | fan speed | 30/50 reports normalize to low/medium |
| 7 | vertical/horizontal swing | Four combined swing states |
| 8 | turbo, power-saving, smart-eye, follow-me | Smart-eye is `0x40`; it is not “independent auxiliary heat” |
| 9 | natural wind, dry flag, auxiliary heat, eco, purifier | Natural wind is `0x02` |
| 10 | sleep, turbo alternate bit, Fahrenheit, prevent-cold | Preset and raw flags are both retained |
| 11–12, 15 | indoor/outdoor temperatures and decimals | `0xFF` means unavailable |
| 13 | target-temperature extension, filter alerts | Fresh-filter timeout is `0x40` |
| 14 | PMV and display level | PMV is low nibble × 0.5 − 3.5 |
| 16 | numeric error code | Zero means no reported error |
| 19 | humidity target / smart-dry byte | Raw target is retained |
| 21 | frost protection | `0x80` |
| 22 | comfort mode | `0x01`; the previous byte-14 mapping was incorrect |
| 24–27 | fresh-filter total/used counters | Big-endian raw counters; units are not asserted |

### Model-qualified multi-head mode-conflict observation

A controlled 2026-10-27 test on a two-head MRCOOL system qualified byte 1 bit `0x80` for
that installation. With both heads calling for heat, the instrumented Loft head reported
`appliance_error=false`. Office was then held in heat while Loft was changed to cool with a
setpoint below room temperature. Within about 13 seconds Loft reported
`appliance_error=true`, its numeric `error_code` remained zero, and every other exposed C0
diagnostic remained unchanged. Returning both heads to heat/cool cleared the flag on the next
status cycle.

This is strong model-qualified evidence that `appliance_error && error_code == 0` represents
an indoor head denied by the shared outdoor unit's opposing ambient mode on this MRCOOL
system. The generic field remains named `appliance_error`: independent implementations call
the same bit `inError`, and other appliances or genuine faults may use it more broadly. A
consumer may expose a device-specific mode-conflict entity after qualifying the same behavior;
the library must not relabel the bit globally.

## B5 capabilities

B5 values are not generic booleans. Each known capability uses its own value set. The decoder
implements the current corroborated sets for:

- available auto/cool/dry/heat modes;
- vertical/horizontal swing;
- silent/low/medium/high/auto/custom fan support;
- eco, turbo-cool, turbo-heat, frost protection and anion;
- display control, sound, humidity control and auxiliary heat;
- energy statistics/settings/encoding and rate-select levels;
- per-mode cool/auto/heat setpoint ranges.

Every record, including zero-length and unknown records, remains available through
`Capabilities::records()` / `get()`. `reported(id)` means the device returned the record;
`has(id)` is retained as a raw “any non-zero byte” compatibility helper. New code should use a
typed accessor whenever one exists because non-zero is not a universal support rule.

Capability pages are parsed completely before state is mutated. A malformed record, count
mismatch, overrun or trailer longer than the known message-ID/pagination forms rejects the
whole page.

## C1 diagnostic groups

The captured short query is `41 21 01 (40 | group) 00 01 CRC`. It has no message ID and must
not be padded to the legacy status-query length. Group 4's exact capture
`41 21 01 44 00 01 09` is regression-tested.

| Group | Typed data |
|---:|---|
| 0 | Electrified, total-operating and current-operating hours |
| 1 | Compressor frequency/target/current/voltage; T1/T2/T3/T4 and discharge temperature |
| 2 | Indoor-fan target/actual RPM, condensate-pump bit, raw fault/protection bytes |
| 3 | Outdoor-fan RPM |
| 4 | Total/current energy and real-time power with selectable encoding |
| 5 | Indoor humidity, outdoor-fan RPM and defrost flag |
| 7 | Outdoor-unit input power |
| 11 | Horizontal and vertical louver angles |

Diagnostic support is not declared by B5. Consumers probe only configured groups; an
unanswered group gets one attempt per poll and is retired after three failures until reboot.
Group 2 keeps the complete fault/protection bytes even though the corroborated water-pump bit
is also decoded.

Group 4 formats are selected with `setPowerFormat()`: BCD, binary, mixed/base-100, binary
0.01 kWh, or BCD energy with binary power.

## B0/B1 properties

`PropertyId` contains the complete tag vocabulary currently documented by the comparison
implementation. Every valid response record is retained in `AirConditioner::getProperties()`,
including unknown IDs, unsuccessful status records and data without a typed mapping.

The typed safe subset currently includes:

| IDs | State |
|---|---|
| `0x0009`, `0x000A` | Vertical/horizontal louver angle |
| `0x0015` | Indoor humidity, excluding `0x00`/`0xFF` placeholders |
| `0x0017`, `0x0018`, `0x001A` | Display, breezeless, prompt tone |
| `0x0039`, `0x003D`, `0x003F` | Self-clean, five-byte filter status, error code |
| `0x0042`, `0x0043`, `0x0048` | Breeze-away, breeze control, rate select |
| `0x004B`, `0x0050`, `0x0059`, `0x0067` | Fresh air, water pump, cascade, flash |
| `0x00CD`, `0x00E3` | Outdoor silent (`3` is on), iECO |
| `0x021E`, `0x022C` | Anion/purifier state and sound |

Property polling is consumer-driven. The ESPHome adapter asks for one configured property at a
time, validates that the response ID matches the request, and retires only that ID after three
empty, failed or timed-out responses. This prevents one unsupported tag from suppressing every
other property in a batch. Error-code property `0x003F` is never queried automatically because
known B1 devices can stop answering broader status/capability queries when it is included.

## Validation

Run the native suite:

```sh
python test/run_protocol_tests.py
```

It covers captured valid/malformed C0 frames, both integrity schemes, extended C0 offsets,
all implemented C1 groups and encodings, exact C1 wire format, B0/B1 bounds/trailers/raw
retention, B5 multi-valued semantics/pagination, and confirmed-state control coalescing. The
same suite is also run under AddressSanitizer and UndefinedBehaviorSanitizer in CI and during
local release validation.

PlatformIO CI compiles the library for the ESP8266 `esp12e`, ESP32 Arduino and ESP32 ESP-IDF.
The companion ESPHome component separately compiles its full opt-in entity surface for a real
ESP-12E against the installed production ESPHome version and the latest tagged release.
