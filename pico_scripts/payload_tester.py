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

I2C_CMD_SEND_DATA  = 197
I2C_CMD_SEND_ERROR = 198

# Number of bytes we expect to read back.
READ_LENGTH = 106

# --- Function Definitions ---

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

def data_request_matrix(command, rows, cols):
    """
    Send `command`, read back rows*cols 16-bit values plus a 24-byte ASCII timestamp,
    unpack into a matrix and print both the matrix and timestamp.
    """
    data_bytes = rows * cols * 2
    ts_bytes   = 24                # length of "TS:YYYY-MM-DD hh:mm:ss\r\n"
    total      = data_bytes + ts_bytes

    # 1) send the command
    i2c.writeto(SLAVE_ADDR, bytes([command]))
    time.sleep_ms(500)  # allow STM32 to prepare

    # 2) read exactly that many bytes
    raw = i2c.readfrom(SLAVE_ADDR, total)

    # 3) split into data vs timestamp
    data_raw = raw[:data_bytes]
    ts_raw   = raw[data_bytes:]

    # 4) unpack into list of uint16
    vals = [data_raw[i] | (data_raw[i+1] << 8)
            for i in range(0, len(data_raw), 2)]

    # 5) reshape into matrix
    matrix = []
    for r in range(rows):
        start = r * cols
        matrix.append(vals[start:start + cols])

    # 6) decode timestamp
    timestamp = ''.join(chr(b) for b in ts_raw)

    # 7) display
    print("Received data matrix (%dx%d):" % (rows, cols))
    for row in matrix:
        print("  ", row)
    print("Timestamp:", timestamp.strip())

    return matrix, timestamp

def main():
    """
    Main function to test sending commands and receiving data.
    """
    print("Testing STM32 communication...")

#     # Request error logs
#     print("\nRequesting ERROR logs from STM32:")
#     error_logs = request_data(I2C_CMD_SEND_ERROR, READ_LENGTH)
#     # You might process or print error_logs further here.
#     time.sleep(1)

    #send_data(I2C_CMD_START)
    
#     time.sleep(1)

    # Request SD card data
#     print("\nRequesting SD DATA from STM32:")
#     sd_data = data_request(I2C_CMD_SEND_DATA, READ_LENGTH)
#     # Additional processing of sd_data can be done here.
#     
#     time.sleep(1)
#     
    # Example: 5 rows of 10 fields → a 5×10 matrix
    
    #simulate error uncomment below
    send_data(I2C_CMD_START)
    
    matrix_5x10,ts = data_request_matrix(I2C_CMD_SEND_DATA, rows=5, cols=10)
    print("5×10 matrix:")
    for row in matrix_5x10:
        print(row)
    #print("timestamp")
    #print(ts)
    
    

    

    
    

if __name__ == '__main__':
    main()