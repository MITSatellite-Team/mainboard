import serial
import matplotlib.pyplot as plt
import numpy as np
import sys

PORT = '/dev/cu.usbmodem101'
BAUD = 115200
NUM_PIXELS = 1024
VREF = 5.0

fig, (ax, ax_info) = plt.subplots(1, 2, figsize=(14, 4),
                                   gridspec_kw={'width_ratios': [10, 1]})
ax_info.axis('off')
line, = ax.plot([], [], 'b-', linewidth=0.8)
avg_line = ax.axhline(y=0, color='r', linestyle='--', linewidth=1, label='mean')
ax.set_xlim(0, NUM_PIXELS)
ax.set_ylim(0.9290, 0.9854)
ax.set_xlabel('Pixel')
ax.set_ylabel('Voltage (V)')
ax.set_title('S3903 CCD Live Scan')
ax.grid(True, alpha=0.3)
ax.legend(loc='upper right')

info_text = ax_info.text(0.1, 0.5, '', transform=ax_info.transAxes,
                          fontsize=10, verticalalignment='center',
                          fontfamily='monospace')

def parse_data_line(raw):
    if not raw.startswith('data:'):
        return None
    try:
        values = [int(x) for x in raw[5:].split(',')]
        if len(values) == NUM_PIXELS:
            return values
    except ValueError:
        pass
    return None

def read_serial():
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
        print(f"Connected to {PORT}")
    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)

    plt.ion()
    plt.tight_layout()
    plt.show()

    while True:
        try:
            raw = ser.readline().decode('utf-8', errors='ignore').strip()
            if raw:
                print(raw)
                data = parse_data_line(raw)
                if data:
                    y = np.array(data, dtype=float)
                    y_volts = y / 65535.0 * VREF

                    mean_v = np.mean(y_volts)
                    min_v  = np.min(y_volts)
                    max_v  = np.max(y_volts)
                    std_v  = np.std(y_volts)

                    line.set_data(np.arange(NUM_PIXELS), y_volts)
                    avg_line.set_ydata([mean_v, mean_v])

                    info_text.set_text(
                        f"mean: {mean_v:.4f} V\n"
                        f"min:  {min_v:.4f} V\n"
                        f"max:  {max_v:.4f} V\n"
                        f"std:  {std_v:.4f} V\n"
                        f"range:{max_v-min_v:.4f} V"
                    )

                    fig.canvas.draw()
                    fig.canvas.flush_events()
        except KeyboardInterrupt:
            print("Stopped")
            ser.close()
            break
        except Exception as e:
            print(f"Error: {e}")

if __name__ == '__main__':
    if len(sys.argv) > 1:
        PORT = sys.argv[1]
    read_serial()