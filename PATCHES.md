# Qlima Control patches

Branch `qlima-control` of this fork is used by [Qlima Control](https://github.com/MRDonnii/qlima-control)
(ESPHome external component `midea`). It is based on upstream commit `eeea6c3`, the
commit ESPHome 2026.9 pins, plus:

1. **No dropped commands** – upstream PR #48: `control()` calls that arrive while a
   SET_STATUS request is in flight are merged and sent when it completes, instead of
   being discarded (issue #47).
2. **ECO in AUTO and HEAT** – upstream only sends ECO in COOL (issue #41). The status
   reply shows whether the unit accepted it.
3. **Group data queries** – `AirConditioner::queryGroup(group, onData, onError)` sends the
   engineering query `41 21 01 (0x40|group)` and hands the 0xC1 reply to `onData`.
   Byte offsets as in mill1000/midea-msmart (`Group1Response` …) and
   citi94/midea-homekit-dongle.
4. **Fault code and filter reminder** from the 0xC0 status frame (byte 16 and byte 13 bit 5):
   `getErrorCode()`, `getFilterAlert()`.
5. **Optional power query** – `setPowerQuery(false)` stops the GET_POWERUSAGE (group 4) request
   every 30 s. Units that never answer it (e.g. Qlima S 52xx) otherwise hold the bus for
   three 2-second retries each time, delaying commands.
6. **Raw appliance-error flag** from status byte 1 bit 7: `getApplianceError()` is
   retained independently of byte 16's numeric fault code. Some multi-zone status
   conditions may set the flag without publishing a conventional fault code.
