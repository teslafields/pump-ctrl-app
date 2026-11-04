# Complete System Wiring Schematic: Water Tank & Pump Controller

This document provides the complete wiring schematic, pin-by-pin hardware connections, and safety checklist for connecting the **ESP32-C3 (0.42" OLED)** with the **VL53L4CD** (distance), **INA219** (high-side current/voltage), **SHT30** (temp/humidity), and **XR60N10L / EKM034** (MOSFET pump driver).

---

## 1. Complete System Wiring Schematic

```text
====================================================================================
                           5V DC POWER SOURCE (>= 2A)
                             [ +5V ]         [ GND ]
                                │               │
        ┌───────────────────────┴───┐           │
        │                           │           │
        ▼                           ▼           ▼
┌───────────────┐           ┌──────────────┐  ┌─────────────┐
│   ESP32-C3    │           │    INA219    │  │ COMMON GND  │
│               │           │   [ VIN+ ]   │  │ BUS / RAIL  │
│  [ 5V Pin ]   │           │      │       │  └──────┬──────┘
│               │           │   (Shunt)    │         │
│  [ GND Pin ] ─┼───────────┼──────┼───────┼─────────┤
│               │           │      ▼       │         │
│  [ IO3 (PWM) ]├───┐       │   [ VIN- ]   │         │
│               │   │       └──────┬───────┘         │
│  [ 3.3V Out ] │   │              │ (Monitored +5V) │
└───┬───┬───┬───┘   │              │                 │
    │   │   │       │    ┌─────────┼─────────────────┼─────────┐
    │   │   │       │    │ EKM034 / XR60N10L DRIVER  │         │
    │   │   │       │    │         ▼                 │         │
    │   │   │       │    │  [ VIN / OUT+ ]           │         │
    │   │   │       │    │         │                 │         │
    │   │   │       │    │         ├──────────┐      │         │
    │   │   │       │    │         │          ▼      │         │
    │   │   │       │    │     ┌───┴───┐  ┌────────┐ │         │
    │   │   │       │    │ (K-)│   ▲   │  │ 5V DC  │ │         │
    │   │   │       │    │     │Flyback│  │ PUMP   │ │         │
    │   │   │       │    │     │ Diode │  │ (+)Red │ │         │
    │   │   │       │    │ (A+)│   │   │  └───┬────┘ │         │
    │   │   │       │    │     └───┬───┘   (Motor)   │         │
    │   │   │       │    │         │          │      │         │
    │   │   │       │    │         │  ┌───────┘      │         │
    │   │   │       │    │         │  ▼   ┌────────┐ │         │
    │   │   │       │    │         ├─────►│ 5V DC  │ │         │
    │   │   │       │    │         ▼      │ PUMP   │ │         │
    │   │   │       │    │  [ OUT- (D) ]◄─┤ (-)Blu │ │         │
    │   │   │       │    │         │      └────────┘ │         │
    │   │   │       │    │    [ MOSFET ]             │         │
    │   │   │       └───►│    [  Gate  ]             │         │
    │   │   │            │    [ Source ]             │         │
    │   │   │            │         │                 │         │
    │   │   │            │      [ GND ] ─────────────┼─────────┤
    │   │   │            └───────────────────────────┼─────────┘
    │   │   │                                        │
    │   │   │ (IO5: SDA / IO6: SCL)                  │
    ▼   ▼   ▼                                        ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│ 3.3V I2C SENSOR BUS                                                              │
│ • Shared Lines : VCC -> 3.3V Out | GND -> Common GND | SDA -> IO5 | SCL -> IO6   │
│ • INA219 (0x40): Logic side powered via 3.3V bus; High-side shunt on +5V rail    │
│ • VL53L4CD(0x29): XSHUT -> 3.3V (pull high for default operation) or IO4         │
│ • SHT30  (0x44): ADDR  -> GND (or leave floating for default 0x44)               │
└──────────────────────────────────────────────────────────────────────────────────┘
```

<div class="page-break"></div>

## 2. Detailed Pin-by-Pin Connections

### A. ESP32-C3 0.42" OLED Board Pin Connections
| Pin Label on ESP32 | Connected To | Wire Role / Notes |
| :--- | :--- | :--- |
| **5V** | 5V Power Supply (+) | Main 5V board power |
| **G (GND)** | Common Ground Bus | System Ground reference |
| **3V (3.3V)** | Sensor 3.3V Bus (VL53L4CD, INA219, SHT30) | Regulated 3.3V logic power |
| **IO5 (D5)** | I2C SDA (VL53L4CD, INA219, SHT30) | Shared I2C Data line |
| **IO6 (D6)** | I2C SCL (VL53L4CD, INA219, SHT30) | Shared I2C Clock line |
| **IO3 (D3)** | EKM034 / XR60N10L module **`IN`** | Pump Gate On/Off control (Active HIGH) |
| **IO4 (D4)** | *Optional:* VL53L4CD **`XSHUT`** | Hardware reset/enable (or tie XSHUT to 3.3V) |

---

### B. High-Power Circuit: INA219, Pump, and XR60N10L (EKM034)
The INA219 measures current on the **High Side** (+5V path) while the XR60N10L switches the **Low Side** (GND return path).

#### 1. INA219 Current Sensor Connections
| INA219 Pin | Connects To | Explanation |
| :--- | :--- | :--- |
| **VCC** | ESP32 **`3.3V`** | **CRITICAL:** Do NOT connect to 5V! Logic must be 3.3V. |
| **GND** | Common **`GND`** | Ground reference for I2C and internal ADC. |
| **SCL** | ESP32 **`IO6`** | I2C Clock |
| **SDA** | ESP32 **`IO5`** | I2C Data |
| **VIN+** (Screw Terminal or Pin) | **+5V Rail** (Power Supply) | Current enters shunt from the 5V supply |
| **VIN-** (Screw Terminal or Pin) | EKM034 **`VIN / OUT+`** | Current leaves shunt and feeds positive driver rail |

#### 2. XR60N10L / EKM034 MOSFET Board Connections
| Module Pin / Terminal | Connects To | Explanation |
| :--- | :--- | :--- |
| **IN** (Header Pin) | ESP32 **`IO3`** | Logic signal from MCU: `HIGH` (3.3V) = Pump ON, `LOW` = Pump OFF |
| **GND** (Header Pin) | Common **`GND`** | Logic ground reference for gate signal |
| **VL** (Header Pin) | ESP32 **`3.3V`** (Optional) | Powers onboard gate status LED (can be left open) |
| **VIN / OUT+** (Screw Terminal) | INA219 **`VIN-`** & Pump (+) Wire | Positive power to load and flyback diode cathode |
| **OUT-** (Screw Terminal) | **Pump (-) Wire** (Black/Blue) | Switched low side (MOSFET drain & diode anode) |
| **GND** (Screw Terminal) | Common **`GND`** | High-current power return back to 5V PSU |

> [!IMPORTANT]
> **Flyback Diode Integration:**
> The EKM034 board includes an onboard flyback diode between `OUT-` (anode) and `VIN / OUT+` (cathode). Connecting INA219 `VIN-` and the Pump (+) wire to `VIN / OUT+` clamps the diode across the pump motor, protecting against inductive turn-off spikes.

---

### C. Sensors Bus: VL53L4CD & SHT30

#### 1. VL53L4CD (ToF Distance Sensor)
| VL53L4CD Pin | Connects To | Notes |
| :--- | :--- | :--- |
| **VIN / VCC** | ESP32 **`3.3V`** | 3.3V Power |
| **GND** | Common **`GND`** | Ground |
| **SCL** | ESP32 **`IO6`** | I2C SCL (Address `0x29`) |
| **SDA** | ESP32 **`IO5`** | I2C SDA |
| **XSHUT** | ESP32 **`3.3V`** (or **`IO4`**) | Must be pulled HIGH to enable the sensor |
| **GPIO1 / INT**| *Not Connected (NC)* | Optional interrupt (leave floating) |

#### 2. SHT30 (Temperature & Humidity Sensor)
| SHT30 Pin | Connects To | Notes |
| :--- | :--- | :--- |
| **VIN / VCC** | ESP32 **`3.3V`** | 3.3V Power |
| **GND** | Common **`GND`** | Ground |
| **SCL** | ESP32 **`IO6`** | I2C SCL (Address `0x44`) |
| **SDA** | ESP32 **`IO5`** | I2C SDA |
| **ADDR** | Common **`GND`** (or open) | Address select: Low = `0x44`, High = `0x45` |
| **ALERT / ALR**| *Not Connected (NC)* | Hardware threshold alert (leave floating) |

---

## 3. Hardware Checklist Before Powering On
1. **Power Supply Capacity**: The 5V supply must supply at least **2.0A to 2.5A** to ensure the pump's inrush current (~1.5A) does not cause the ESP32-C3 to brown out.
2. **INA219 VCC = 3.3V**: Do not connect INA219 VCC to 5V; its I2C lines would pull up towards 5V and risk damaging the ESP32-C3 GPIOs.
3. **Common Ground**: Ensure the ESP32 GND, EKM034 GND, Power Supply (-), and all sensor GND pins are bonded to the same ground rail.
