# Hardware Wiring

## Components

- **ESP32** dev board (controller/web server)
- **QR204** 58mm thermal receipt printer
- **Amoner 5V/3A** USB wall adapter
- **Split USB cable** — USB-A to bare wire with female dupont connectors
- **Breadboard** (temporary, standard rows/columns)
- **Dupont jumper wires** (male-to-female)

## Current Wiring (Breadboard)

Single USB cable from the Amoner adapter, split on the breadboard to power both
devices directly. The printer is **not** powered through the ESP32.

```
Amoner 5V/3A adapter
    │
    USB-A
    │
    Split USB cable (bare wire + female dupont)
    │
    ▼
┌─────────────────────────────────────────────┐
│  BREADBOARD                                 │
│                                             │
│  Row 1 (5V):   USB 5V wire                 │
│                 ├── jumper → Printer VCC     │
│                 └── jumper → ESP32 VIN       │
│                                             │
│  Row 2 (GND):  USB GND wire                │
│                 ├── jumper → Printer GND     │
│                 └── jumper → ESP32 GND       │
│                                             │
└─────────────────────────────────────────────┘

Separate signal wire:
  ESP32 GPIO17 (TX2) ────── QR204 RX
```

### Pin Mapping

| Signal | ESP32 Pin | QR204 Pin | Wire Color (suggested) |
|--------|-----------|-----------|------------------------|
| 5V     | VIN       | VCC (VH)  | Red                    |
| GND    | GND       | GND       | Black                  |
| TX→RX  | GPIO17    | RX        | Green                  |

### Notes

- Use the **VIN** pin on the ESP32, not the 3V3 pin. VIN accepts 5V and feeds the
  onboard voltage regulator.
- Any GND pin on the ESP32 works — they are all connected internally.
- The signal wire (GPIO17 → RX) carries data only, not power.

## Planned Wiring (Soldered Single Cable)

Replace the breadboard with inline solder splices to create a clean single-cable
solution, matching the reference setup.

```
Amoner 5V/3A adapter
    │
    USB-A
    │
    Split USB cable
    │
    5V wire ──── solder splice ──┬── to Printer VCC
    │                            └── to ESP32 VIN
    │
    GND wire ─── solder splice ──┬── to Printer GND
                                 └── to ESP32 GND

Separate signal wire:
    ESP32 GPIO17 (TX2) ────── QR204 RX
```

### What You Need to Solder

- Soldering iron + solder
- Heat shrink tubing (to insulate each splice)
- 2 short lengths of hookup wire (one for 5V branch, one for GND branch)
- Female dupont connectors or pin headers (to plug into ESP32/printer)
- (Optional) Inline connectors (JST-RCY or similar) for easy disconnect

### Steps

1. Strip a small section in the middle of the 5V wire
2. Solder a branch wire onto the exposed section
3. Slide heat shrink over the joint and shrink it
4. Repeat for the GND wire
5. Add dupont connectors to the branch wire ends
6. One pair goes to the printer (VCC + GND), the other to the ESP32 (VIN + GND)

## Power Requirements

| Requirement     | Value                                      |
|-----------------|--------------------------------------------|
| Voltage         | **5V DC** (USB)                            |
| Minimum current | **2.4A** (ESP32 ~500mA + printer ~1.5-2A)  |
| Recommended     | **3A** (your Amoner adapter meets this)    |
| Connector       | **USB-A** (to match the split cable)       |

The thermal printer draws up to 2A during print bursts. A weak adapter
(e.g. 1A phone charger) will cause voltage drops and brownout resets.

## Known Issue: Brownout Reset Loop

### Symptom

The "PRINTER ONLINE" startup message prints multiple times (e.g. 8x) when the
device is powered on.

### Cause

When both devices share a single 5V rail without buffering:

1. ESP32 boots and sends print commands to the printer
2. The printer's print head activates, drawing a 1.5-2A current burst
3. The voltage on the shared 5V rail drops below the ESP32's minimum
4. The ESP32 browns out and resets
5. It boots again and prints again — loop repeats until power stabilizes

### Fixes

**Software (current):** A 2-second delay before the first print command in
`firmware/src/main.cpp` lets the power rail stabilize after boot. This is a
workaround, not a proper fix.

**Hardware (recommended):** Add a **470-1000µF electrolytic capacitor** (rated
10V or higher) across the 5V rail. This absorbs current spikes from the printer
and prevents the voltage from dipping below the ESP32's brownout threshold.

How to add the capacitor:
- On the breadboard: place it across the 5V and GND rows
- On the soldered cable: solder it at the splice point
- **Polarity matters:** the longer leg is positive (+), the striped side is
  negative (-). Connect + to 5V and - to GND.

A 680µF/16V electrolytic capacitor is currently in use and prevents brownout
during print bursts. The 2-second delay in `firmware/src/main.cpp` is still
present as an additional safety margin during boot.
