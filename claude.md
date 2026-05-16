# Spectrometer Project — Claude Code Context

## Hardware
- **MCU**: Raspberry Pi Pico (RP2040) via PlatformIO + EarlPhilhower Arduino core
- **CCD**: Hamamatsu S3903-1024Q (1024 pixel NMOS linear image sensor)
- **ADC**: ADS8866 (16-bit, 100kSPS, SPI-compatible, 3-wire CS mode)

## Pin Mapping
| Signal | GPIO |
|--------|------|
| φ1 (CCD clock 1) | 2 |
| φ2 (CCD clock 2) | 3 |
| φst (CCD start) | 4 |
| EOS (end of scan) | 5 |
| ADC MISO / DOUT | 8 |
| ADC CS / CONVST | 9 |
| ADC SCLK | 10 |
| Heater PWM | 12 |
| SD MISO | 16 |
| SD CS | 17 |
| SD SCLK | 18 |
| SD MOSI | 19 |
| I2C SDA | 20 |
| I2C SCL | 21 |

## I2C Devices
- **DS3231** RTC on Wire1 (pins 20/21)
- **TMP117** temperature sensor on Wire1 (pins 20/21)

## PlatformIO Config
```ini
[env:pico]
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
board = rpipico
framework = arduino
board_build.core = earlephilhower
board_build.f_cpu = 133000000L
monitor_speed = 115200
upload_protocol = picotool
board_build.arduino.earlephilhower.pio_files =
    src/ccd_driver.pio
```

## Source Files
- `src/ccd_driver.pio` — PIO programs for CCD timing + ADC CONVST
- `src/ccd.h` / `src/ccd.cpp` — CCD driver library
- `src/ads8866.h` / `src/ads8866.cpp` — ADC driver library
- `src/main.cpp` — main scan + stack loop
- `plot_ccd.py` — live Python plotter (macOS)

---

## CCD Driver (`ccd.h` / `ccd.cpp`)

### API
```cpp
void ccd_init();          // init PIO state machines + EOS interrupt
void ccd_start_scan();    // fire phist + phi1/phi2
bool ccd_wait_for_end();  // wait for EOS interrupt, returns false on timeout
void ccd_stop();          // stop phi SM, restore idle state
```

### Key Details
- Uses **pio0**, sm0 = phist, sm1 = phi1/phi2
- φ1 and φ2 are driven by the same SM using `set pins, 0b01` / `set pins, 0b10`
- φ2 starting HIGH is forced via `pio_sm_exec(pio, sm_phi, 0xe02a)` after init
- EOS is active-low, detected via `attachInterrupt(FALLING)`
- `clkdiv` in `phi_init()` controls exposure time: clkdiv=6 ≈ 1ms total scan
- Scan restart sequence: disable → restart → clear FIFOs → clear IRQ → jmp to start → put trigger → enable_sync

### EOS Timing
- EOS is a ~4µs negative pulse
- Must use interrupt (not polling) — too short to catch reliably by polling
- `eos_fired` is `volatile bool` set in ISR

---

## PIO Programs (`ccd_driver.pio`)

### `phist_pulse`
- Triggered by `pio_sm_put_blocking`
- Goes HIGH, fires `irq set 0` after short overlap delay
- Stays HIGH for 3× loop (32×32 cycles each) then goes LOW
- φ1/φ2 SM waits on `wait 1 irq 0` before starting

### `phi_square`
- Waits for IRQ 0 from phist
- Alternates `set pins, 0b01` (φ1 HIGH) and `set pins, 0b10` (φ2 HIGH)
- Each phase: nested loop, 32×32 = 1024 cycles × clkdiv × 8ns

### `convst_pulse`
- Used by ADC CS on pio0 sm2
- Triggered by `pio_sm_exec(pio0, 2, 0xe001)` / `pio_sm_exec(pio0, 2, 0xe000)`
- No FIFO — CS is controlled directly via exec injection

---

## ADC Driver (`ads8866.h` / `ads8866.cpp`)

### API
```cpp
void ads_init();             // init PIO sm2 for CS, GPIO for SCLK/MISO
uint16_t ads_read();         // trigger conversion, clock out 16 bits
float ads_to_volts(uint16_t raw);  // convert to volts (VREF=5V)
```

### Key Details
- CS (CONVST) driven by **pio0 sm2** via direct `pio_sm_exec` injection
- SCLK and MISO are plain GPIO using direct `sio_hw` register writes for speed
- Conversion sequence:
  1. CONVST HIGH (`0xe001`) → wait 9µs
  2. CONVST LOW (`0xe000`) → wait 4 nops (~32ns)
  3. Clock 16 bits MSB first, sample on SCLK rising edge
- Total read time: ~10µs
- VREF = 5V, AVDD = 3.3V, DVDD = 3.3V (split supply)
- 1 LSB = 5V / 65536 = 76.3µV

### ADS8866 3-Wire CS Mode
- DIN hardwired HIGH → CS mode selected
- CONVST rising edge: starts conversion, tristates DOUT
- CONVST falling edge: DOUT exits tristate, MSB valid after 12.3ns
- 16 SCLK pulses clock out D15..D0
- DOUT tristates after 16th SCLK falling edge

---

## Scan + Stack Loop (`main.cpp`)

```cpp
#define NUM_PIXELS  1024
#define STACK_COUNT 100
```

### Flow
1. Run 100 individual scans
2. Accept scans with ≥1000 pixels (exact 1024 not always achieved due to EOS timing)
3. Accumulate into `uint32_t stack_accum[]`
4. Average and output as `stacked:N pixel0,pixel1,...`

### Pixel Readout
- Poll φ1 rising edge → φ1 falling edge (φ2 rises) → call `ads_read()`
- ADC read happens during φ2 HIGH window
- Pixel voltage appears at φ2 rising edge per S3903 datasheet

### Serial Output Format
```
stacked:98 13311,13409,13439,...
```

---

## Python Plotter (`plot_ccd.py`)

### Usage
```bash
python plot_ccd.py --port /dev/cu.usbmodem101 --smooth 20
```

### Arguments
| Arg | Default | Description |
|-----|---------|-------------|
| `--port` | required | Serial port |
| `--baud` | 115200 | Baud rate |
| `--vref` | 5.0 | Reference voltage |
| `--smooth` | 10 | Moving average window |

### Features
- Live serial reading in background thread
- FuncAnimation for non-blocking plot updates (macosx backend)
- 3 plots: raw ADC counts, voltage, relative brightness (min→max normalised)
- Pixels 10–990 displayed (strips dummy/EOS pixels)
- Handles `stacked:N` prefix in data lines
- Auto-reconnects on serial disconnect

---

## Known Issues / Quirks

### PIO
- `pio_add_program` returns wrong offset if called multiple times — use `pio_add_program_at_offset` or load all programs together
- PIO0 has 32 instruction slots: phist(10) + phi_square(13) + convst(5) = 28 used, 4 free
- EarlPhilhower `SPI.setRX()` hangs on some pins — use pio0 sm2 with direct exec for CS instead
- `set x` / `set y` max value is 31 (5-bit) — use nested loops for longer delays
- PIO delay `[N]` max is 31 — use loop for longer

### Timing
- φ2 must be longer than ADC read time (~10µs)
- EOS fires synchronously with φ2 rising edge after last pixel
- φst must overlap φ2 by ≥200ns (datasheet requirement)
- φ1 and φ2 must never both be HIGH simultaneously

### Serial
- Close PlatformIO monitor before running Python plotter — they share the port
- Pico resets on serial connect — add `time.sleep(2)` after connecting
- 1024 pixels × ~5 chars = ~5KB per line — fits in serial buffer fine

### ADC
- DOUT had 10k pullup resistor — acceptable, chip can drive against it
- Cold solder joint on DOUT pin 7 was root cause of initial 0xFFFF readings
- AVDD=3.3V, DVDD=3.3V, REF=5V is valid per datasheet (REF independent of AVDD)

---

## TODO
- [ ] DS3231 RTC reading (I2C, pins 20/21, Wire1)
- [ ] TMP117 temperature reading (I2C, pins 20/21, Wire1)
- [ ] SD card logging (SPI1, pins 16-19)
- [ ] Heater PWM control (GPIO 12, P-controller, target 10-35°C)
- [ ] Wavelength calibration (pixel → nm mapping)