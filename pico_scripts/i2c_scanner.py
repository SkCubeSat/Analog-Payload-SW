# i2c_scanner.py
#
# Scans the I2C bus and prints the hex address of every device found.
# Run this on the Pico to confirm the STM32 payload board is reachable at
# address 0x13 (19) before running any of the other test scripts.

import machine

# I2C bus configuration
I2C_ID   = 0
SDA_PIN  = machine.Pin(0)
SCL_PIN  = machine.Pin(1)
I2C_FREQ = 400000  # 400 kHz fast mode

i2c = machine.I2C(I2C_ID, sda=SDA_PIN, scl=SCL_PIN, freq=I2C_FREQ)

devices = i2c.scan()

if devices:
    print("Number of I2C devices found:", len(devices))
    for device in devices:
        print("  Device hex address:", hex(device))
else:
    print("No I2C devices found.")