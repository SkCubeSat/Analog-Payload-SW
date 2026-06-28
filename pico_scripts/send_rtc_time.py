# send_rtc_time.py
#
# Reads the Pico's local clock and writes the current date/time to the STM32
# RTC via I2C.
#
# Wire format (9 bytes, single writeto transaction):
#   [CMD_SET_RTC][year_hi][year_lo][month][day][weekday][hour][minute][second]
# Weekday is 1-indexed (Monday = 1) to match the STM32 HAL convention.

import machine
import time

# I2C bus configuration
I2C_ID   = 0
SDA_PIN  = 0
SCL_PIN  = 1
I2C_FREQ = 100000  # 100 kHz — matches STM32 I2C clock speed setting

i2c = machine.I2C(I2C_ID, sda=machine.Pin(SDA_PIN), scl=machine.Pin(SCL_PIN), freq=I2C_FREQ)

SLAVE_ADDR  = 0x13  # STM32 payload board 7-bit address
CMD_SET_RTC = 103   # I2C_CMD_SET_RTC


def set_stm32_rtc():
    """Read the Pico's local time and send it to the STM32 RTC."""
    # time.localtime() -> (year, mon, day, hour, min, sec, weekday, yday)
    year, mon, day, hour, minute, sec, wday, _ = time.localtime()

    # Pack year as two bytes (big-endian), STM32 side reassembles it.
    # Weekday is stored 0-based by MicroPython; STM32 HAL expects 1-based.
    payload = bytearray([
        CMD_SET_RTC,
        (year >> 8) & 0xFF,
        year & 0xFF,
        mon,
        day,
        wday + 1,
        hour,
        minute,
        sec,
    ])

    i2c.writeto(SLAVE_ADDR, payload)
    print("RTC set to %04d-%02d-%02d %02d:%02d:%02d" % (year, mon, day, hour, minute, sec))


set_stm32_rtc()
