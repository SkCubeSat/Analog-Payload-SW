# send_start_command.py
#
# Sends the I2C_CMD_START command (99 / 0x63) to the STM32 payload board.
# This forces an immediate single measurement routine, equivalent to the
# board being triggered by its 12-hour timer.

import machine
import time

# I2C bus configuration
I2C_ID   = 0
SDA_PIN  = 0
SCL_PIN  = 1
I2C_FREQ = 400000  # 400 kHz fast mode

SLAVE_ADDR    = 0x13  # STM32 payload board 7-bit address
I2C_CMD_START = 99    # Forced start of single testing routine

i2c = machine.I2C(
    I2C_ID,
    sda=machine.Pin(SDA_PIN),
    scl=machine.Pin(SCL_PIN),
    freq=I2C_FREQ,
)


def send_start_command():
    """Send I2C_CMD_START and confirm on the serial console."""
    i2c.writeto(SLAVE_ADDR, bytes([I2C_CMD_START]))
    time.sleep_ms(50)
    print("Sent START command ({} / 0x{:02X}) to slave 0x{:02X}".format(
        I2C_CMD_START,
        I2C_CMD_START,
        SLAVE_ADDR,
    ))


send_start_command()
