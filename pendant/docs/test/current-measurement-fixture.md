# INA228 Current Measurement Fixture — Fixed Specification

Status: **FIXED TEST FIXTURE SPECIFICATION** (2026-10-14, measurement protocol finalized)

This document defines the bench measurement fixture and operating protocol used to characterize lifeOS Pendant power consumption from µA-class deep sleep through BLE, microphone, NAND, and full representative workloads.

This is **test equipment**, not part of the production pendant BOM.

The hardware architecture is fixed. Revisit it only when real measurements demonstrate a concrete limitation.

Related repository documents:

- [05. デバッグ・テストポイント](../spec/board.md)
- [19. 電源・エネルギー収支 再確認](../spec/power.md)

## 1. Measurement intent

The fixture must answer four different questions. Do not collapse them into one number.

1. **Deep-sleep floor** — measure with the 100 Ω high-sensitivity range.
2. **Real-firmware average current** — measure at `1V8_MAIN` with a low-burden shunt selected from actual peak current.
3. **Power actually drawn from the 120 F storage element** — prefer direct storage-side measurement instead of assuming TPS62840 efficiency.
4. **CR2032 emergency consumption** — measure separately because firmware policy changes in emergency mode.

Final outputs should support:

- normal CAP-mode average power,
- no-sun runtime from the normal upper capacitor voltage to the actual CAP→CR2032 switchover point,
- CR2032 emergency-mode current/runtime,
- event-energy decomposition for BLE, NAND, microphone, and other subsystems.

## 2. Fixed fixture hardware

Use an **INA228 breakout with the original on-board shunt removed**, plus replaceable external shunts.

Fixed decisions:

- **100 Ω** external shunt for forced deep-sleep characterization,
- removable **startup bypass jumper across the 100 Ω shunt**,
- replaceable low-ohmic shunt for representative/active operation,
- first low-ohmic candidates: **1 Ω, 0.5 Ω, 0.2 Ω**; use lower only if measured burden voltage requires it,
- INA228 `VS` from an external 3.3 V source,
- `VBUS` on the source side (`IN+` side),
- no automatic ranging,
- no MOSFET/relay shunt switching,
- no dedicated 10 Ω intermediate range,
- no large fixture-side reservoir capacitor added merely to survive the 100 Ω range.

## 3. Fixed wiring

High-side measurement:

```text
                     ┌──────────── VBUS
Source / bench PSU ──┴──────────── IN+
                                    │
                              external RSHUNT
                                    │
                                   IN-
                                    │
                                   DUT
                                    │
                                   GND

INA228 VS   ← external 3.3 V
INA228 GND  ← common GND
INA228 I2C  ← external logger MCU / host adapter
```

For the 100 Ω sleep fixture:

```text
                     IN+
                      │
                 ┌────┴────┐
                 │ 100 Ω   │
                 │         │
                 └─ jumper ┘  ← startup bypass
                      │
                     IN-
```

The jumper shorts the 100 Ω resistor during boot. Remove it only after the DUT reaches forced sleep. Because the resistor remains physically connected, removing the jumper transfers current into the resistor without opening the DUT current path.

### 3.1 VBUS placement

`VBUS` shall be connected to **source-side `IN+`**, not load-side `IN-`.

The INA228 bus-voltage input has finite input impedance. Putting `VBUS` after the shunt would add a µA-class current after the shunt and corrupt deep-sleep measurement.

With `VBUS` on the source side, INA228 `POWER` / `ENERGY` include shunt loss and are not the authoritative pendant load-energy metric. Use shunt current / `CHARGE` plus an independently validated rail voltage, or measure storage-side input power directly.

## 4. Breakout-board modification

Low-cost INA228 breakouts may differ from the Adafruit reference layout.

Mandatory steps:

1. **Identify and remove the original on-board shunt.**
   - Adafruit-5832-style boards use a 15 mΩ reference shunt; clones may differ.
   - Never leave the original shunt in parallel with the external shunt.
2. Verify continuity from terminals/pads to `IN+`, `IN-`, and `VBUS`.
3. Wire/configure `VBUS` to source-side `IN+`.
4. Power `VS` from an external 3.3 V measurement supply, not the DUT current path.
5. Any breakout LED or I²C pull-up must also be powered from the external measurement supply.

## 5. Device identity acceptance

Do not trust module silkscreen alone.

At first power-up read:

- `MANUFACTURER_ID` at `0x3E`: expected reset value `0x5449` (`"TI"`),
- `DEVICE_ID` at `0x3F`: bits `[15:4]` shall read `0x228`; low four bits are revision ID.

A module that fails this check is not accepted for calibrated measurements. This detects wrong-part shipment and obvious mismatch; it is not complete counterfeit authentication.

## 6. INA228 ranges

INA228 shunt ADC ranges:

- `ADCRANGE = 1`: ±40.96 mV, 78.125 nV/LSB,
- `ADCRANGE = 0`: ±163.84 mV, 312.5 nV/LSB.

Nominal examples:

| Shunt | ADC range | Full-scale current | Current per shunt-voltage LSB | Main role |
|---:|---:|---:|---:|---|
| 100 Ω | ±40.96 mV | ±409.6 µA | 0.78125 nA/LSB | forced deep-sleep floor |
| 100 Ω | ±163.84 mV | ±1.6384 mA | 3.125 nA/LSB | sleep/debug only |
| 1 Ω | ±40.96 mV | ±40.96 mA | 78.125 nA/LSB | light representative load |
| 1 Ω | ±163.84 mV | ±163.84 mA | 312.5 nA/LSB | wider 1 Ω range |
| 0.5 Ω | ±40.96 mV | ±81.92 mA | 156.25 nA/LSB | lower burden |
| 0.5 Ω | ±163.84 mV | ±327.68 mA | 625 nA/LSB | wide 0.5 Ω range |
| 0.2 Ω | ±40.96 mV | ±204.8 mA | 390.625 nA/LSB | low-burden representative run |
| 0.2 Ω | ±163.84 mV | ±819.2 mA | 1.5625 µA/LSB | only if wider range is required |

Use the **narrow ±40.96 mV range whenever the measured peak shunt voltage fits with margin**. Use ±163.84 mV only when needed for headroom.

These are ADC code scales, not guaranteed accuracy. Offset, gain error, noise, shunt tolerance, PCB leakage, contamination, wiring, and DUT repeatability remain relevant.

Whenever the shunt changes, update INA228 `SHUNT_CAL` / `CURRENT_LSB` correctly for that shunt and ADC range, or log raw `VSHUNT` and convert offline using the calibrated resistance.

## 7. Shunt selection for representative workload

Do **not** define 1 Ω as the battery-life range by default.

Select the low-ohmic shunt from observed peak current and allowable burden voltage:

```text
RSHUNT <= VDROP_BUDGET / IPEAK
```

Initial target:

```text
VDROP_BUDGET <= 10 mV at representative peak current
```

Also verify ADC full-scale headroom.

This is important because the pendant's 1.8 V rail has limited POR / minimum-voltage margin. The fixture must not change the behavior being measured.

Examples:

- 50 mA with 1 Ω → 50 mV added drop: too intrusive for a precision lifetime run.
- 50 mA with 0.2 Ω → 10 mV added drop: preferred starting point.
- if 0.2 Ω still violates the drop budget, use a lower shunt or move the measurement point.

Use an oscilloscope on the 1.8 V rail during representative bursts to verify that the fixture has not materially reduced the minimum rail voltage.

## 8. Forced deep-sleep floor measurement

The 100 Ω mode is **deep-sleep floor characterization only**, not the full-workload lifetime measurement.

Procedure:

1. Install 100 Ω.
2. Fit the startup bypass jumper.
3. Configure firmware to enter the target sleep state reproducibly and suppress intentional BLE, NAND, microphone, watchdog-test, or periodic work during the measurement window.
4. Power normally.
5. Wait for the intended sleep state.
6. Remove the bypass jumper **while powered**. Do not remove the shunt itself while powered.
7. Allow the reading to settle.
8. Record `VSHUNT` / `CURRENT`; for longer checks, also record `CHARGE`.
9. Refit the jumper or remove DUT power before permitting active operation.

Critical constraint: do not allow a normal active wake with 100 Ω in circuit.

- 1 mA → 0.1 V drop,
- 10 mA → 1 V drop,
- 50 mA → 5 V drop.

The purpose is to establish a trusted µA floor and cross-check the sleep intervals seen in the low-ohmic representative-workload test.

## 9. Representative-workload lifetime measurement

This is the **primary mode for real operating consumption**.

Measure the complete `1V8_MAIN` bridge defined in [05](../spec/board.md), not only one subsystem rail.

### 9.1 INA228 configuration

For lifetime current integration:

- continuous shunt-only: `ADC_CONFIG.MODE = 0xA`,
- initial `AVG = 1`,
- initial `VSHCT = 50 µs` or `84 µs`,
- low-burden shunt selected by §7,
- smallest ADC range that does not clip,
- `CHARGE` as the integrated-current observable.

Shunt-only mode avoids spending ADC cycles on VBUS/temperature when the quantity of interest is current/charge. Rail voltage is measured independently.

The 50–84 µs values are starting points, **not accuracy guarantees**. They improve temporal coverage of bursty loads but lie outside the datasheet condition for the specified ±1 µV maximum shunt offset. Therefore every lifetime configuration requires empirical zero-offset characterization (§11).

INA228 remains a delta-sigma converter with a digital filter. Do not claim oscilloscope-grade reconstruction of arbitrary sub-conversion-time RF/NAND pulses. The target is repeatable accumulated consumption.

### 9.2 Representative firmware workload

Run the real policy, including the relevant mix of:

- sleep / idle,
- microphone operation,
- RAM/NAND write scheduling,
- BLE advertising,
- BLE synchronization,
- periodic housekeeping,
- normal power gating.

Do not infer full-device lifetime from separate sleep and active spot readings alone.

### 9.3 Measurement duration

The design includes approximately **2-hour BLE burst synchronization**. A 5–10 minute run can omit a major event.

Use two levels:

**Fast engineering model**

- measure sleep/base current,
- measure one NAND write sequence,
- measure one BLE synchronization event,
- measure other important periodic events,
- combine by actual occurrence rate.

**Final representative run**

- run real firmware for at least one complete BLE-sync period,
- preferably multiple periods,
- use corrected accumulated charge over the full interval.

The event model and long run should agree within the chosen error budget. A large mismatch indicates a missing state or a measurement problem.

## 10. CAP mode and CR2032 mode are separate tests

Do not use one average-current value for both source modes.

Firmware behavior changes in CR2032 emergency mode; BLE synchronization and NAND work may be reduced/deferred.

Perform at least:

1. **CAP-mode representative workload** — normal policy.
2. **CR2032-mode representative workload** — actual emergency policy.

Record firmware revision and active source policy with every result.

## 11. Mandatory zero-offset correction for lifetime runs

A constant current offset does not average away; it integrates directly into `CHARGE`.

The INA228 ±1 µV maximum shunt-offset specification is given for `TCT > 280 µs`. Lifetime runs may use 50 or 84 µs, so actual system offset must be measured.

For every shunt + ADC range + `VSHCT` combination used for lifetime decisions:

1. establish the same common-mode voltage as the real run,
2. force `IN+` and `IN-` equipotential with no DUT current,
3. keep INA228 configuration identical to the real run,
4. record charge change long enough to determine a stable slope,
5. calculate:

```text
I_OFFSET = ΔQ_ZERO / Δt_ZERO
```

6. correct the run:

```text
I_AVG_CORR = (ΔQ_RUN / Δt_RUN) - I_OFFSET
```

Do not disconnect the DUT and leave `IN-` floating as a zero-current calibration.

Repeat the zero check when shunt, ADC range, `VSHCT`, fixture wiring, or relevant temperature changes.

This correction is mandatory for lifetime figures from the low-ohmic representative range.

## 12. From 1.8 V current to regulated-load power

For the representative rail run:

```text
P_1V8 = V_RAIL × I_AVG_CORR
```

Use measured rail voltage when accuracy matters; do not assume exactly 1.800 V if the real rail differs.

This is regulated-load power. It is not automatically equal to power drawn from the 120 F capacitor because TPS62840 efficiency and upstream current depend on input voltage and load.

## 13. Storage-side power: measure instead of assuming buck efficiency

For final no-sun lifetime estimation, do not treat one assumed TPS62840 efficiency (for example 90%) as authoritative.

Preferred procedure if the capacitor-side path is accessible:

1. replace the 120 F capacitor with a current-capable bench source at the same storage node/test point,
2. disable/control solar charging,
3. remove/isolate CR2032 or otherwise force/verify that the **CAP path remains selected**,
4. run the representative CAP-mode firmware workload,
5. measure average current drawn from the storage node at:
   - **3.70 V**,
   - **3.00 V**,
   - a **lower CAP-mode point immediately above the measured CAP→CR2032 switchover**, nominally near 2.50 V,
6. calculate:

```text
P_CAP_ELECTRONICS(V) = V × I_CAP_IN(V)
```

### 13.1 Do not blindly use exactly 2.50 V

The design's nominal CAP→CR2032 LOW threshold is 2.50 V, with real component/threshold tolerance. At exactly 2.50 V the mux may already have switched to CR2032 or may chatter near the transition.

Therefore:

- monitor `TP_VBAT_OK` / `TP_PWR_SOURCE` or equivalent source-state signal,
- determine the actual switchover voltage on the specimen,
- take the lowest CAP-mode power point **just above the actual switchover** while CAP selection is stable,
- characterize the source transition itself separately.

Do not label a measurement as CAP-mode input power unless the CAP path was confirmed active.

This direct input measurement includes the real TPS62840 conversion loss and circuit consumption seen from the storage node under the tested configuration.

The physical 120 F capacitor is absent when replaced by a bench source, so add capacitor leakage separately using measured leakage when available; until then use the conservative budget in [19](../spec/power.md).

If storage-side measurement is unavailable, perform at least one simultaneous input/output measurement to derive real converter-path efficiency for the actual fixture rather than using only the datasheet curve.

### 13.2 Runtime calculation with measured input power

Capacitor voltage falls during discharge, and input power may vary with voltage. Let the lower integration limit be the **measured stable CAP-mode switchover voltage** `V_SWITCH`, not blindly 2.50 V.

```text
dE = C × V × dV
```

```text
P_TOTAL(V) = P_CAP_ELECTRONICS(V) + V × I_CAP_LEAK(V)
```

Then estimate no-sun CAP runtime by:

```text
t = ∫[V_SWITCH→3.70] C × V / P_TOTAL(V) dV
```

Interpolate between measured storage-voltage points and evaluate numerically. The existing ~124 mWh constant-power calculation remains a useful first-order sanity check, but voltage-dependent measured input power is the final validation target.

## 14. Event / subsystem diagnosis

The prototype PCB retains removable 0 Ω current links on MAIN and selected subsystem rails as defined in [05](../spec/board.md).

Use subsystem measurements when whole-device consumption is too high or the event model does not close.

Typical breakdown targets:

- MCU/base system,
- microphone,
- NAND standby/program/erase,
- BLE advertise,
- BLE sync transfer,
- RTC / always-on domain,
- source transition.

Whole-device representative consumption is the acceptance quantity; subsystem readings are diagnostic quantities.

## 15. Calibration and acceptance

### 15.1 Minimum hardware acceptance

Before design decisions:

1. verify INA228 manufacturer/device ID,
2. measure external shunt resistance with the best available DMM,
3. verify near-zero differential reading at zero current,
4. apply at least one known current point and verify magnitude/sign,
5. verify real peak burden voltage and 1.8 V rail minimum with scope.

### 15.2 Higher-confidence calibration

Calibrate the **assembled fixture**, not only a loose resistor.

Useful 100 Ω points:

- ~10 µA,
- ~100 µA.

Generate a stable known current and verify with a trusted DMM/SMU in series. Fit offset/gain in software or post-processing if needed.

For low-ohmic workload ranges, the §11 zero-offset-rate correction is mandatory even if gain calibration is not performed.

## 16. Layout and handling rules

At µA-class currents, contamination and leakage matter.

- clean flux around `IN+`, `IN-`, shunt pads, and connectors,
- keep the measurement region dry,
- avoid touching inputs during final runs,
- use short, stable wiring,
- use proper headers/jumpers rather than loose wires,
- use Kelvin connection to low-ohmic shunts where practical,
- record shunt value, ADC range, `VSHCT`, averaging, zero-offset rate, firmware revision, workload mode, source mode, source voltage, and measurement duration with every result.

## 17. Explicitly rejected baseline features

Not part of the first fixture:

- automatic range switching,
- MOSFET / analog-switch shunt selection,
- diode-clamped auto-ranging,
- relay-based ranging,
- dedicated 10 Ω range,
- large fixture-side reservoir capacitor added only to survive the 100 Ω shunt,
- PPK2 or another dedicated profiler as a mandatory purchase.

Reason: each adds cost, leakage, switching uncertainty, parasitics, or implementation time. Deep-sleep floor, representative workload, storage-side power, and subsystem diagnosis are cleaner as separate tests.

## 18. Fixed design decision

> **Use a low-cost INA228 breakout with its original shunt removed. Use 100 Ω plus a startup bypass only for forced deep-sleep floor measurement. For real firmware/lifetime measurement, use the `1V8_MAIN` bridge with the lowest practical external shunt that keeps representative peak burden voltage within the accepted budget, and use the narrow INA228 shunt range whenever it does not clip. Run continuous shunt-only (`MODE = 0xA`) for charge integration, measure and subtract zero-current charge drift using the exact same shunt/range/timing, and validate final CAP-mode lifetime using measured storage-side input power at 3.70 V, 3.00 V, and the lowest stable CAP-mode point immediately above the measured CAP→CR2032 switchover. Measure CAP and CR2032 modes separately.**

Do not reopen this architecture based only on theoretical concerns. Revisit it when real measurements expose a concrete failure.

## 19. References

Primary external references:

- Texas Instruments, **INA228 85-V, 20-Bit, Ultra-Precise Power/Energy/Charge Monitor With I2C Interface**: https://www.ti.com/lit/ds/symlink/ina228.pdf
- Texas Instruments, **TPS62840 1.8-V to 6.5-V, 750-mA, 60-nA-IQ Step-Down Converter**: https://www.ti.com/lit/ds/symlink/tps62840.pdf
- Adafruit, **INA228 I2C Power Monitor — Pinouts**: https://learn.adafruit.com/adafruit-ina228-i2c-power-monitor/pinouts

Relevant INA228 points used here:

- ±40.96 mV / ±163.84 mV selectable shunt ranges,
- 78.125 nV / 312.5 nV shunt-voltage resolution,
- `MODE = 0xA` = continuous shunt-voltage-only conversion,
- `VSHCT` = 50, 84, 150, 280, 540, 1052, 2074, or 4120 µs,
- charge/energy accumulation requires continuous conversion mode,
- ±1 µV maximum shunt-offset specification is given for `TCT > 280 µs`, so short-latency configurations require empirical zero-offset characterization,
- `MANUFACTURER_ID` at `0x3E`, reset `0x5449`,
- `DEVICE_ID` at `0x3F`, `DIEID = 0x228`.

Relevant TPS62840 point:

- light-load efficiency depends on input voltage, output current, and mode; datasheet efficiency curves are design references, not a substitute for final storage-side in-system power measurement.