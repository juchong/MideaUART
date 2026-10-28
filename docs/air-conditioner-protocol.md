# Air-conditioner protocol evidence ledger

This document records what the `0xAC` implementation knows, why it knows it, and what remains
raw. It is not a claim of complete Midea protocol coverage.

The principal comparison source is the maintained
[`midea-local` AC implementation](https://github.com/midea-lan/midea-local/blob/main/midealocal/devices/ac/message.py).
The older independent
[`midea-uart` C0 parser](https://github.com/reneklootwijk/midea-uart/blob/main/src/C0_Airco_parser.cpp)
is useful when checking lineage, but disagreements with it are not silently resolved in favor
of either implementation.

## Evidence policy

| Level | Meaning | Library treatment |
|---|---|---|
| Captured | Present in a saved frame from real hardware, with the behavior independently observed | Typed decoding is permitted for that model; generic promotion still needs corroboration |
| Corroborated | Matching byte layout and meaning in independent implementations | Typed generic decoding is permitted |
| Lineage | Present only in the inherited MideaUART implementation or one comparison source | Preserve raw; type only when the API makes the limited evidence explicit |
| Unknown | No defensible meaning | Preserve raw and do not label or control |

Bounds checks, checksums and synthetic test frames prove parser safety. They do not prove that
a byte has the semantic name assigned to it. The test suite uses those terms separately.

## Implemented protocol families

| Family | Implementation | Evidence boundary |
|---|---|---|
| `C0` status / `0x40` control | Core climate state plus selected diagnostics | Core climate fields have captured fixtures; extended fields are classified below |
| `B5` capabilities | Lossless records plus typed known values | Most typed values match maintained `midea-local`; inherited-only accessors are identified below |
| `C1` groups | Groups 0, 1, 2, 3, 4, 5, 7 and raw group 11 | Typed subset matches maintained `midea-local`, except Group 0 which is raw-only after live rejection; every accepted payload is retained raw |
| `B0` / `B1` properties | Lossless records, query/write builders and a corroborated typed subset | The fixed response byte is preserved as `reserved`; it has no inferred success meaning |
| `A0` / `A1` | Not decoded or emitted | Needs UART fixtures and model routing |
| `BB` | Not decoded or emitted | Model/subtype-specific; needs fixtures and routing |

## C0 status

The following fields are generic because their layout is captured, inherited from the working
MideaUART climate implementation, and corroborated by `midea-local`:

| Byte | Fields |
|---:|---|
| 1 | power; raw appliance-error bit `0x80` |
| 2 | mode and target temperature, including the half-degree bit |
| 3 | fan value |
| 7 | vertical and horizontal swing |
| 8 | turbo and power-saving; Smart Eye bit `0x40` |
| 9 | natural wind `0x02`, auxiliary heat `0x08`, eco `0x10`, purifier/anion `0x20` |
| 10 | sleep, alternate turbo bit, Fahrenheit and prevent-cold |
| 11, 12, 15 | indoor/outdoor temperature and decimal nibbles |
| 13 | extended setpoint, filter alert `0x20`, fresh-filter timeout `0x40` |
| 14 | PMV low nibble |
| 16 | numeric error code |
| 19 | raw target-humidity/smart-dry field |
| 21 | frost protection `0x80` |
| 22 | comfort mode `0x01` |
| 24–27 | raw fresh-filter counters; units are not asserted |

Three fields remain deliberately raw:

| Raw field | Reason |
|---|---|
| C0 byte 8 bit 7 | The older parser calls it `feelOwn`; the maintained implementation does not expose it as Follow Me. It is not named Follow Me here. |
| C0 byte 9 bit 2 | Maintained `midea-local` calls it `dry`; the older parser calls it `dryClean`. It is not presented as an active Dry-mode indicator. |
| C0 byte 14 bits 4–6 | Implementations disagree about display polarity. The three-bit field is retained, but `display_on` is not derived from C0. |

`StatusData` exposes these as `getByte8Bit7()`, `getByte9Bit2()` and
`getDisplayField()`. The complete validated C0 frame is available through
`AirConditioner::getStatusData()`.

### MRCOOL multi-head observation

On 2026-10-27, a bounded test on the Loft and Office MRCOOL heads observed the raw C0 byte 1
`0x80` bit assert on the head denied during an opposing heat/cool call while error byte 16
remained zero. That supports a model-specific *candidate* predicate of
`appliance_error && error_code == 0`; it does not change the generic meaning of the raw bit.

Normal operation later showed the same Office combination while both Home Assistant climate
entities were in Heat/Cool. Because a head in automatic mode can still internally choose an
ambient mode, that observation neither proves nor disproves a conflict. The library therefore
does not expose an “opposing mode disabled” state.

Error byte 16 also briefly reported decimal `25`. The independent older parser maps code 25
to heating/cold-wind protection. It is not a conflict code in this implementation.

## B5 capabilities

Every valid B5 record is retained, including unknown and zero-length records. Typed accessors
exist for the maintained-source-correlated value sets for:

- auto/cool/dry/heat mode combinations;
- vertical/horizontal swing combinations;
- silent/low/medium/high/auto/custom fan sets;
- eco, turbo, anion, display, sound and humidity support;
- energy statistics, settings and encoding;
- rate-select levels; and
- per-mode temperature ranges.

The inherited accessors for frost protection, filter reminder, auxiliary heat, active clean,
breeze features and unit switching remain lineage-level. Their raw B5 records are authoritative;
callers must not treat mere record presence or a nonzero byte as generic proof of support.

## C1 groups

The exact query body is `41 21 01 (40 | group)`, followed by sixteen zero bytes and the CRC
(21 bytes total). It contains no message ID. A seven-byte form from an external protocol capture
was rejected after repeated live Loft polls produced no C1 replies while C0 and B1 traffic
continued normally. The 20-byte request body is the form that produced the live Group 1, 2 and 3
qualification data below. Every supported group selector and all sixteen padding bytes are
regression-tested.

| Group | Typed fields | Evidence |
|---:|---|---|
| 0 | No typed fields | Raw preservation only; the former five-byte time interpretation was rejected by live MRCOOL output |
| 1 | Compressor frequency/target/current/voltage, T1/T2/T3/T4, discharge temperature | Corroborated |
| 2 | Indoor-fan target/actual RPM and condensate-pump bit | Corroborated |
| 3 | Outdoor-fan RPM | Corroborated |
| 4 | Total/current energy and real-time power with selectable encoding | Corroborated |
| 5 | Indoor humidity only | Corroborated |
| 7 | Outdoor-unit input power | Corroborated |
| 11 | No typed fields | Raw preservation only |

Previously assigned Group 2 fault/protection arrays, Group 5 outdoor-fan/defrost fields and
Group 11 louver angles were removed because no captured fixture or maintained corroborating
implementation established those meanings. `GroupData::raw_payload` retains every byte after
the four-byte C1 header for future qualification.

During a 2026-10-27 Loft MRCOOL qualification using the 20-byte request body, Groups 1, 2 and 3
returned plausible idle-state values:
241 V compressor supply, 23.0 C indoor coil, 15.5 C outdoor coil, 20 C discharge pipe, and zero
target/actual fan and compressor speeds. Group 0 also replied, but its former five-byte
day/hour/minute/second interpretation produced impossible values (including more than 879,000
operating hours), so those typed fields were removed and the payload remains raw. Groups 5 and
7 did not reply during the complete bounded sweep. Group 4 was deliberately not queried because
its device-specific energy encoding had not been established.

## B0/B1 properties

A response record is parsed as:

```
id_low id_high reserved length data...
```

The third byte is fixed/reserved in the maintained comparison implementation. Earlier code
called it `status` and treated bit `0x10` as failure; that inference was removed. Nonzero values
are preserved and do not cause records to be discarded.

The typed response subset is limited to meanings corroborated by maintained `midea-local`:

| IDs | Typed state |
|---|---|
| `0x0009`, `0x000A` | Vertical/horizontal louver angle |
| `0x0015` | Indoor humidity, excluding `0x00` and values over 100 |
| `0x0017`, `0x0018` | Display state and breezeless state |
| `0x0039`, `0x003D`, `0x003F` | Self-clean, five-byte filter status and error code |
| `0x0042` | Breeze-away/indirect-wind state (`2` is on) |
| `0x0048` | Rate-select value |
| `0x004B` | Fresh-air speed when the payload has at least two bytes |
| `0x00CD` | Outdoor silent (`3` is on) |
| `0x022C` | Sound state |

`0x001A` prompt tone is retained as a typed writer/reader because the maintained implementation
emits the same boolean encoding, but it is not part of that implementation's default query set.

The same Loft qualification received valid values only for display state (`0x0017`) and
horizontal louver angle (`0x000A`). The other configured property queries completed a full sweep
without a response and therefore remain available protocol vocabulary, not MRCOOL-validated
telemetry.

All other known tag IDs are vocabulary and raw records only. In particular, `0x0050` water
pump, `0x0059` wind-around, `0x0067` and `0x00E3` do not create typed state. The latter two use
neutral `UNKNOWN_*` enum names.

## Control behavior

Controls are deltas applied to the last validated C0 snapshot. If no validated snapshot exists,
one status query is made before the first full-state write. A validated snapshot does not become
artificially invalid after 15 seconds; that threshold was not part of the protocol and has been
removed.

Configured C1 and B1 polling intervals are scheduling choices, not claimed device delays. The
adapter no longer adds separate 15- or 20-second startup waits and no longer retires a configured
query after three failures until reboot.

## Validation and remaining limits

Run:

```sh
python test/run_protocol_tests.py
python test/run_protocol_tests.py --sanitize
```

The saved fixture set contains twelve upstream Carrier/SLWF-01 C0 frames. It validates core C0
decoding, integrity handling and rejection behavior. Extended C0, C1, B0/B1 and B5 tests use
synthetic frames and validate bounds, raw preservation, transformations and request wire format;
they are not hardware-semantic proof.

CI compiles the library for ESP8266 `esp12e`, ESP32 Arduino and ESP32 ESP-IDF. The companion
adapter compiles a full ESP-12E configuration against both the deployed ESPHome release and the
latest published ESPHome release.
