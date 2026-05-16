# Spectrometer Mainboard Firmware

Firmware for the Raspberry Pi Pico-based spectrometer mainboard with temperature control and CCD linear sensor readout.

## Features

- **Temperature Control Loop**: Proportional (P) control of heater via PWM
- **CCD Scanning**: S3903-1024Q NMOS linear image sensor readout with SPI ADC interface
- **Data Logging**: CSV output to microSD card with timestamp and temperature
- **Real-Time Clock**: DS3231SN RTC for accurate timestamping
- **Temperature Monitoring**: TMP117 high-accuracy temperature sensor

## Hardware

### Pinout

| Component | Function | GPIO |
|-----------|----------|------|
| UART | TX/RX | 0, 1 |
| CCD | ψ1 | 2 |
| CCD | ψ2 | 3 |
| CCD | ψst (start) | 4 |
| CCD | EOS | 5 |
| ADC SPI | MISO | 8 |
| ADC SPI | CS | 9 |
| ADC SPI | SCLK | 10 |
| **Heater PWM** | **GPIO12** | **12** |
| **Temp Interrupt** | **GPIO13** | **13** |
| **SD Card SPI** | **MISO** | **16** |
| **SD Card SPI** | **CS** | **17** |
| **SD Card SPI** | **SCLK** | **18** |
| **SD Card SPI** | **MOSI** | **19** |
| **I2C** | **SDA** | **20** |
| **I2C** | **SCL** | **27** |

### Devices

1. **DS3231SN RTC** - I2C address 0x68
   - Provides accurate timestamps
   - Battery-backed operation
   
2. **TMP117 Temperature Sensor** - I2C address 0x48
   - High accuracy: ±0.1°C (-20 to +50°C)
   - Continuous conversion mode, 8-average
   
3. **S3903-1024Q CCD Linear Sensor**
   - 1024 pixels, 25 µm pitch
   - Current output read via external ADC over SPI
   - Requires ~10 kΩ pullup on EOS line to 3.3V
   
4. **MCP3201/MCP3204 ADC** (or equivalent SPI ADC)
   - Reads CCD video output
   - 12-bit resolution recommended

## Configuration

Edit [include/config.h](include/config.h) to customize:

### Control Parameters

- `SETPOINT_TEMP`: Target temperature (°C)
- `TEMP_KP`: Proportional gain for heater control
- `CCD_EXPOSURE_MS`: Integration time (ms)
- `CCD_INTERVAL_S`: Scan interval (seconds)

### Pin Configuration

All GPIO pins are defined in [include/config.h](include/config.h):

```cpp
#define I2C_SDA_PIN     20
#define I2C_SCL_PIN     27
#define HEATER_PWM_PIN  12
#define CCD_PSI1_PIN    2
// ... etc
```

## Building and Uploading

### Prerequisites

- PlatformIO CLI or VS Code extension
- Raspberry Pi Pico
- USB cable for upload and serial monitor

### Build

```bash
pio run -e pico
```

### Upload

```bash
pio run -e pico --target upload
```

### Monitor Serial Output

```bash
pio device monitor -e pico -b 115200
```

## Operation

### Startup Sequence

1. System initializes all peripherals
2. RTC provides current timestamp
3. TMP117 starts continuous temperature conversion
4. CCD is set to idle state
5. Log file is created on SD card
6. System enters dual control loop

### Control Loops (Non-blocking)

**Temperature Loop** (~every 500 ms):
- Reads current temperature from TMP117
- Calculates P control: `PWM = Kp × (Setpoint - Current)`
- Outputs PWM to heater driver on GPIO12
- Logs status every 2 seconds

**CCD Scanning Loop** (~every 10 seconds, configurable):
- Records RTC timestamp and current temperature
- Performs full CCD scan with specified exposure time
- Reads all 1024 pixel values via SPI ADC
- Writes scan data + metadata to SD card CSV

### Data Logging

Log file format (CSV):
```
timestamp,exposure_ms,pixel_0,pixel_1,...,pixel_1023,temperature_c
2024-12-15 14:30:45,10,256,258,260,...,251,25.34
2024-12-15 14:30:55,10,261,263,265,...,254,25.36
```

## Serial Output

```
=== Spectrometer Mainboard Startup ===
[I2C] Initialized on pins SDA=20, SCL=27
[SD] SD card initialized
[SD] Log file opened: scan_1234567.csv
[RTC] Initialized (DS3231SN)
[RTC] Current time: 2024-12-15 14:30:30
[TEMP] Initialized (TMP117)
[TEMP] Device ID: 0x117
[CCD] Initialized (S3903-1024Q)
[PWM] Heater control initialized on GPIO12
[GPIO] Temp interrupt input on GPIO13

=== System Configuration ===
Target temperature: 25.00 °C
Temperature control Kp: 5.0
CCD exposure time: 10 ms
CCD scan interval: 10 s

=== System Ready ===

[TEMP] 2024-12-15 14:30:32 | T=22.45°C | error=2.55 | PWM=12/255
[TEMP] 2024-12-15 14:30:34 | T=22.67°C | error=2.33 | PWM=11/255
[CCD] Starting scan at 2024-12-15 14:30:40...
[CCD] Scan completed in 145 ms | EOS: YES | Temp: 23.12°C
[SD] Data logged (1024 pixels)
```

## Troubleshooting

### I2C Issues
- Verify I2C pullup resistors (typically 4.7 kΩ) on SDA/SCL
- Check device addresses with: `pio device monitor` and monitor startup output
- Ensure Wire.setSDA() and Wire.setSCL() match hardware pins

### CCD Not Scanning
- Verify GPIO pins 2-5 are connected correctly
- Check EOS line has 10 kΩ pullup to 3.3V
- Verify ADC CS, SCLK, MISO pins are connected

### SD Card Not Working
- Verify SD card is FAT32 formatted
- Check GPIO 17 (CS) is properly connected
- Confirm GPIO 16-19 SPI connections

### Temperature Control Issues
- Increase `TEMP_KP` for faster response (but may oscillate)
- Decrease `TEMP_KP` for more stable but slower response
- Monitor `[TEMP]` serial output to see error and PWM

### High Power Consumption
- Increase `CCD_INTERVAL_S` to scan less frequently
- Reduce PWM duty cycle by lowering target temperature or `TEMP_KP`
- Check for excessive I2C or SPI bus activity

## Hardware Tips

### Heater Driver (GPIO12)
- PWM output is 0-255 (256 levels)
- Connect to MOSFET gate or PWM motor driver
- Add 100 Ω series resistor for stability
- Ensure heater power supply is properly filtered

### CCD Bias Network
- Video bias voltage: typically 2-2.5V
- Use external current integration amplifier for best linearity
- Match source impedance to ADC input requirements

### I2C Bus
- 400 kHz fast mode is default
- Place 0.1 µF bypass capacitors near each device
- For long runs, consider CAN bus or differential signals

## Development

### Source Files

- [src/main.cpp](src/main.cpp) - Main firmware logic
- [include/config.h](include/config.h) - Configurable parameters
- [include/DS3231.h](include/DS3231.h) - RTC driver
- [include/TMP117.h](include/TMP117.h) - Temperature sensor driver
- [include/S3903.h](include/S3903.h) - CCD sensor driver
- [platformio.ini](platformio.ini) - Build configuration

### Adding New Features

To add new control loops or features:

1. Create driver header file in `include/`
2. Instantiate object in `src/main.cpp`
3. Initialize in `setup()`
4. Call update function in `loop()`
5. Use non-blocking I/O to avoid stalling

## License

Development code for research use.

## References

- [DS3231 Datasheet](datasheets/DS3231_datasheet.pdf)
- [TMP117 Datasheet](datasheets/TMP117_datasheet.pdf)
- [S3903 Datasheet](datasheets/S3903_datasheet.pdf)
- [Raspberry Pi Pico Hardware](https://www.raspberrypi.com/products/raspberry-pi-pico/)
- [PlatformIO Documentation](https://docs.platformio.org/)
