import machine
import time

# --- Configuration ---
# Define the I2C pins and bus (these are the defaults for many Pico boards, adjust as needed)
I2C_ID = 0  # I2C bus 0
SDA_PIN = 0
SCL_PIN = 1
I2C_FREQ = 400000  # 400 kHz fast mode (adjust if necessary)

# Instantiate the I2C peripheral
i2c = machine.I2C(I2C_ID, sda=machine.Pin(SDA_PIN), scl=machine.Pin(SCL_PIN), freq=I2C_FREQ)

# I2C slave (STM32) address (7-bit address; adjust to your device)
SLAVE_ADDR = 0x13

# Command definitions
I2C_CMD_RESET   = 97 # Reset the payload board 
I2C_CMD_STOP    = 98 # Stop all routines on the payload board 
I2C_CMD_START   = 99 # Forced start of single testing routine
I2C_CMD_NORMAL = 100 # read sensors, write to sd card
I2C_CMD_PWRSAV = 101 # Power saving mode, no routines till OBC says normal mode.
I2C_CMD_PWRNOR = 102 # Normal power mode
I2C_CMD_CHECK_LATEST_TS = 106 # latest S_*.CSV FAT timestamp
I2C_CMD_PWR_STATUS      = 105 # current power flag

I2C_CMD_GET_RTC    = 104
I2C_CMD_SEND_DATA  = 197
I2C_CMD_SEND_ERROR = 198

# Number of bytes we expect to read back.
READ_LENGTH = 106

# --- Function Definitions ---

def request_rtc():
    """
    Send GET_RTC command and decode the 8-byte RTC payload.
    Wire format: [STATUS][LEN_L][LEN_H][year_hi][year_lo][month][day][weekday][hour][min][sec]
    Returns a dict with the decoded fields, or None on error.
    """
    i2c.writeto(SLAVE_ADDR, bytes([I2C_CMD_GET_RTC]))
    time.sleep_ms(50)           # RTC read is instant; short delay is enough
    raw = i2c.readfrom(SLAVE_ADDR, 3 + 8)   # 3-byte header + 8-byte payload
    status  = raw[0]
    pay_len = raw[1] | (raw[2] << 8)
    if pay_len != 8:
        print("request_rtc: unexpected payload length", pay_len)
        return None
    p = raw[3:]
    year    = (p[0] << 8) | p[1]
    month   = p[2]
    day     = p[3]
    weekday = p[4]
    hour    = p[5]
    minute  = p[6]
    second  = p[7]
    result = {
        "status":  status,
        "year":    year,
        "month":   month,
        "day":     day,
        "weekday": weekday,
        "hour":    hour,
        "minute":  minute,
        "second":  second,
    }
    print("RTC: {:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d} (weekday={}) status=0x{:02X}".format(
          year, month, day, hour, minute, second, weekday, status))
    return result

def send_data(command):
    try:
        # Send the command (write transaction; R/W bit = write)
        i2c.writeto(SLAVE_ADDR, bytes([command]))
        print("Command 0x{:02X} sent.".format(command))
    except Exception as e:
        print("Error writing command: ", e)
        return None

# def request_data(command, read_length):
#     """
#     Sends a command to the slave device and then reads back a block of data.
#     The process:
#       1. Write the command.
#       2. Wait for a short delay to allow the slave to prepare data.
#       3. Read the specified number of bytes.
#     """
#     try:
#         # Send the command (write transaction; R/W bit = write)
#         i2c.writeto(SLAVE_ADDR, bytes([command]))
#         print("Command 0x{:02X} sent.".format(command))
#     except Exception as e:
#         print("Error writing command: ", e)
#         return None
# 
#     # Allow some time for the slave to process the command and load its buffer.
#     time.sleep(0.2)  # 100ms delay; adjust depending on your slave processing time.
# 
#     try:
#         # Read data from the slave.
#         data = i2c.readfrom(SLAVE_ADDR, read_length)
#         print("Received data:", data)
#         return data
#     except Exception as e:
#         print("Error reading data: ", e)
#         return None

def data_request(command, read_length):
    """
    Send a one-byte command, then read back exactly `read_length` bytes
    and unpack them as little‑endian uint16 values.
    """
    # 1) Write the command
    i2c.writeto(SLAVE_ADDR, bytes([command]))
    # 2) Give the STM32 time to prepare its buffer
    time.sleep_ms(50)
    # 3) Read the fixed-size buffer
    raw = i2c.readfrom(SLAVE_ADDR, read_length)
    # 4) Unpack into a list of 16-bit integers
    values = [raw[i] | (raw[i+1] << 8) for i in range(0, len(raw), 2)]
    print("Received %d values:" % len(values), values)
    return values

# def data_request_matrix(command, rows, cols):
#     """
#     Send `command`, read back rows*cols 16-bit values,
#     and return them as a `rows x cols` matrix.
#     """
#     byte_count = rows * cols * 2
#     # 1) send the command
#     i2c.writeto(SLAVE_ADDR, bytes([command]))
#     time.sleep_ms(50)  # give STM32 time to fill its buffer
#     
#     # 2) read exact number of bytes
#     raw = i2c.readfrom(SLAVE_ADDR, byte_count)
#     
#     # 3) unpack to list of uint16
#     vals = [raw[i] | (raw[i+1] << 8) for i in range(0, len(raw), 2)]
#     
#     # 4) reshape into matrix
#     matrix = []
#     for r in range(rows):
#         start = r * cols
#         matrix.append(vals[start:start + cols])
#     return matrix

def decode_binary_ts(b12):
    """Decode 12 bytes -> 'YYYY-MM-DD HH:MM:SS'.
    Layout: 6 little-endian uint16 = year (full), month, day, hour, min, sec.
    Same format on every path: SD SEND_DATA, no-SD SEND_DATA, CHECK_LATEST_TS.
    """
    ts = [b12[i] | (b12[i + 1] << 8) for i in range(0, 12, 2)]
    return "%04d-%02d-%02d %02d:%02d:%02d" % tuple(ts)


def data_request_matrix(command, rows, cols, offset=0):
    """
    Send `command` (+offset byte), then do ONE I2C read transaction.
    STM32 restarts TX at byte 0 for each read transaction, so splitting
    header/payload into two reads misaligns bytes.

    Layout expected (both SD and no-SD paths are now identical):
      [status][len_l][len_h][ rows*cols uint16 ][ 12-byte binary timestamp ]
    """
    data_bytes = rows * cols * 2
    ts_bytes = 12
    total_bytes = 3 + data_bytes + ts_bytes

    # 1) send command + which-file byte
    i2c.writeto(SLAVE_ADDR, bytes([command, offset]))
    time.sleep_ms(500)

    # 2) read full frame in one transaction
    raw = i2c.readfrom(SLAVE_ADDR, total_bytes)
    status = raw[0]
    payload_len = raw[1] | (raw[2] << 8)
    payload = raw[3:3 + payload_len]

    if payload_len < data_bytes:
        print("Payload too short:", payload_len, "expected at least", data_bytes)
        return None, None

    # 3) decode numeric matrix payload
    data_raw = payload[:data_bytes]
    vals = [data_raw[i] | (data_raw[i + 1] << 8) for i in range(0, len(data_raw), 2)]

    matrix = []
    for r in range(rows):
        start = r * cols
        matrix.append(vals[start:start + cols])

    # 4) decode 12-byte binary timestamp tail
    tail = payload[data_bytes:data_bytes + 12]
    timestamp = decode_binary_ts(tail) if len(tail) == 12 else ""

    # 5) display
    print("Status: 0x%02X, payload_len=%d" % (status, payload_len))
    print("Received data matrix (%dx%d):" % (rows, cols))
    for row in matrix:
        print("  ", row)
    if timestamp:
        print("Timestamp:", timestamp)
    else:
        print("Timestamp: <none>")

    return matrix, timestamp

def test_pwr_status():
    """
    Ask the board for its power flag and print it (0 = NORMAL, 1 = SAVING).
    Reply is [STATUS][LEN_L][LEN_H][flag], so the flag is the 4th byte.
    """
    try:
        i2c.writeto(SLAVE_ADDR, bytes([I2C_CMD_PWR_STATUS]))  # ask
        time.sleep_ms(50)
        reply = i2c.readfrom(SLAVE_ADDR, 4)                    # read answer
    except OSError as e:
        print("Power status: no reply (", e, ") - board flashed/wired?")
        return None
    flag = reply[3]
    print("Power status =", flag, "(SAVING)" if flag else "(NORMAL)")
    return flag


def test_latest_ts():
    """
    Send I2C_CMD_CHECK_LATEST_TS and print the latest file's timestamp.
    Reply layout: [STATUS][LEN_L][LEN_H][6 x uint16 LE: y,mo,d,h,mi,s]
    """
    i2c.writeto(SLAVE_ADDR, bytes([I2C_CMD_CHECK_LATEST_TS]))
    time.sleep_ms(500)  # SD mount + f_stat
    raw = i2c.readfrom(SLAVE_ADDR, 3 + 12)
    if raw[0] & 0x04:  # error bit -> no SD card / no S_*.CSV
        print("Latest TS: slave reported error (no SD card or no S_*.CSV)")
        return None
    timestamp = decode_binary_ts(raw[3:3 + 12])
    print("Latest file TS:", timestamp)
    return timestamp


def main():
    """
    Main function to test sending commands and receiving data.
    """
    print("Testing STM32 communication...")

    # # Request error logs
    # print("\nRequesting ERROR logs from STM32:")
    # error_logs = request_data(I2C_CMD_SEND_ERROR, READ_LENGTH)
    # # You might process or print error_logs further here.
    # time.sleep(1)

    # send_data(I2C_CMD_START)
    
    # time.sleep(1)

    # # Request SD card data
    # print("\nRequesting SD DATA from STM32:")
    # sd_data = data_request(I2C_CMD_SEND_DATA, READ_LENGTH)
    # # Additional processing of sd_data can be done here.
    
    # time.sleep(1)
    
    # Example: 5 rows of 10 fields → a 5×10 matrix
    
    # simulate error uncomment below
    send_data(I2C_CMD_START)
    
    matrix_5x10,ts = data_request_matrix(I2C_CMD_SEND_DATA, rows=5, cols=10, offset=0)
    print("5x10 matrix:")
    for row in matrix_5x10:
        print(row)
    # print("timestamp")
    # print(ts)

    # --- separate tests for the new commands ---
    print("\nPower status test:")
    test_pwr_status()

    print("\nLatest timestamp test:")
    test_latest_ts()

if __name__ == '__main__':
    main()