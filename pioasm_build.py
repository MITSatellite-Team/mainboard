import subprocess
import os
import re
from pathlib import Path

Import("env")

PIOASM = str(Path.home() / ".platformio/packages/tool-pioasm-rp2040-earlephilhower/pioasm")

def build_pio(source, target, env):
    os.makedirs("lib/ccd", exist_ok=True)
    result = subprocess.run([
        PIOASM, "-v", "0",
        "lib/ccd/ccd_driver.pio",
        "lib/ccd/ccd_driver.pio.h"
    ], capture_output=True, text=True)
    if result.returncode != 0:
        print("pioasm error:", result.stderr)
        return
    # strip pio_version and used_gpio_ranges from generated header
    with open("lib/ccd/ccd_driver.pio.h", "r") as f:
        content = f.read()
    content = re.sub(r'\s*\.pio_version = [^,]+,', '', content)
    content = re.sub(r'#if PICO_PIO_VERSION > 0.*?#endif', '', content, flags=re.DOTALL)
    with open("lib/ccd/ccd_driver.pio.h", "w") as f:
        f.write(content)
    print("pioasm: ccd_driver.pio.h generated and patched")

env.AddPreAction("buildprog", build_pio)