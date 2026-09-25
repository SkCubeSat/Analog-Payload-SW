import machine
import time

# --- Configuration ---
I2C_ID = 0
SDA_PIN = 0
SCL_PIN = 1
I2C_FREQ = 100000
SLAVE_ADDR = 0x13

i2c = machine.I2C(I2C_ID, sda=machine.Pin(SDA_PIN), scl=machine.Pin(SCL_PIN), freq=I2C_FREQ)

# Commands
I2C_CMD_RESET = 97
I2C_CMD_STOP = 98
I2C_CMD_START = 99
I2C_CMD_NORMAL = 100
I2C_CMD_PWRSAV = 101
I2C_CMD_PWRNOR = 102
I2C_CMD_SET_RTC = 103
I2C_CMD_GET_RTC = 104
I2C_CMD_PWR_STATUS = 105
I2C_CMD_CHECK_LATEST_TS = 106
I2C_CMD_SEND_DATA = 197
I2C_CMD_SEND_ERROR = 198

# Status bits from STM32 firmware
I2C_STATUS_BUSY = 0x01
I2C_STATUS_READY = 0x02
I2C_STATUS_ERROR = 0x04


def read_status_byte():
    return i2c.readfrom(SLAVE_ADDR, 1)[0]


def status_str(status):
    flags = []
    if status & I2C_STATUS_BUSY:
        flags.append("BUSY")
    if status & I2C_STATUS_READY:
        flags.append("READY")
    if status & I2C_STATUS_ERROR:
        flags.append("ERROR")
    return "|".join(flags) if flags else "NONE"


def wait_until_ready(timeout_ms, poll_ms=50):
    waited = 0
    last = 0
    while waited <= timeout_ms:
        try:
            last = read_status_byte()
        except OSError:
            pass

        ready = (last & I2C_STATUS_READY) != 0
        busy = (last & I2C_STATUS_BUSY) != 0
        if ready and not busy:
            return True, last, waited

        time.sleep_ms(poll_ms)
        waited += poll_ms

    return False, last, waited


def wait_for_command_done(timeout_ms, poll_ms=20, min_guard_ms=120):
    """Wait for a command to be actually processed.

    Why this exists:
    READY may already be high from a previous command when we issue a new one.
    We prefer to observe BUSY at least once, then READY again. If BUSY is too
    brief to catch, we still enforce a small guard time before accepting READY.
    """
    waited = 0
    last = 0
    saw_busy = False

    while waited <= timeout_ms:
        try:
            last = read_status_byte()
        except OSError:
            pass

        ready = (last & I2C_STATUS_READY) != 0
        busy = (last & I2C_STATUS_BUSY) != 0
        if busy:
            saw_busy = True

        if saw_busy and ready and not busy:
            return True, last, waited, True

        # Fallback path when busy pulse is too short to sample.
        if (not saw_busy) and ready and (not busy) and waited >= min_guard_ms:
            return True, last, waited, False

        time.sleep_ms(poll_ms)
        waited += poll_ms

    return False, last, waited, saw_busy


def write_cmd(command, payload=None):
    if payload is None:
        i2c.writeto(SLAVE_ADDR, bytes([command]))
    else:
        i2c.writeto(SLAVE_ADDR, bytes([command]) + payload)


def set_stm32_rtc_from_pico():
    tm = time.localtime()
    year, month, day, hour, minute, second, wday, _ = tm
    payload = bytearray([
        (year >> 8) & 0xFF,
        year & 0xFF,
        month,
        day,
        wday + 1,
        hour,
        minute,
        second,
    ])
    write_cmd(I2C_CMD_SET_RTC, payload)
    print("RTC set request sent: {:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}".format(
        year, month, day, hour, minute, second
    ))


def request_rtc(ready_timeout_ms=5000):
    last_err = None
    for attempt in range(1, 6):
        write_cmd(I2C_CMD_GET_RTC)

        ok, st, waited, saw_busy = wait_for_command_done(timeout_ms=ready_timeout_ms)
        if not ok:
            print("GET_RTC attempt {} timeout: status=0x{:02X} ({}) waited={} ms".format(
                attempt, st, status_str(st), waited
            ))
            time.sleep_ms(100)
            continue

        try:
            raw = i2c.readfrom(SLAVE_ADDR, 11)
        except OSError as e:
            last_err = e
            print("GET_RTC attempt {} read timeout after {} ms (saw_busy={})".format(
                attempt, waited, saw_busy
            ))
            time.sleep_ms(120)
            continue

        status = raw[0]
        payload_len = raw[1] | (raw[2] << 8)
        if payload_len != 8:
            print("GET_RTC attempt {} bad length {} status=0x{:02X} ({})".format(
                attempt, payload_len, status, status_str(status)
            ))
            time.sleep_ms(80)
            continue

        p = raw[3:]
        year = (p[0] << 8) | p[1]
        month = p[2]
        day = p[3]
        weekday = p[4]
        hour = p[5]
        minute = p[6]
        second = p[7]
        print("RTC: {:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d} wd={} status=0x{:02X} ({})".format(
            year, month, day, hour, minute, second, weekday, status, status_str(status)
        ))
        return raw

    if last_err is not None:
        print("GET_RTC failed after retries; last error: {}".format(last_err))
    else:
        print("GET_RTC failed after retries")
    return None


def decode_binary_ts(b12):
    ts = [b12[i] | (b12[i + 1] << 8) for i in range(0, 12, 2)]
    return "%04d-%02d-%02d %02d:%02d:%02d" % tuple(ts)


def send_start_and_wait_done(timeout_ms=90000):
    write_cmd(I2C_CMD_START)
    print("START sent. Waiting for routine completion...")
    ok, st, waited = wait_until_ready(timeout_ms=timeout_ms, poll_ms=200)
    if not ok:
        print("START still busy after {} ms, status=0x{:02X} ({})".format(
            waited, st, status_str(st)
        ))
        return False
    print("Routine complete after {} ms, status=0x{:02X} ({})".format(
        waited, st, status_str(st)
    ))
    return True


def request_data_matrix(rows=5, cols=10, offset=0, ready_timeout_ms=8000):
    data_bytes = rows * cols * 2

    # SD path often returns 100 data bytes + 12 timestamp + 3 header = 115 bytes.
    # No-SD path can be shorter in current firmware.
    total_candidates = [115, 103]

    for total_len in total_candidates:
        try:
            i2c.writeto(SLAVE_ADDR, bytes([I2C_CMD_SEND_DATA, offset]))
            ok, st, waited = wait_until_ready(timeout_ms=ready_timeout_ms)
            if not ok:
                print("SEND_DATA timeout waiting ready: status=0x{:02X} ({}) waited={} ms".format(
                    st, status_str(st), waited
                ))
                return None, None

            raw = i2c.readfrom(SLAVE_ADDR, total_len)
            status = raw[0]
            payload_len = raw[1] | (raw[2] << 8)
            payload = raw[3:3 + payload_len]

            if payload_len < data_bytes:
                print("SEND_DATA payload too short: {} bytes".format(payload_len))
                return None, None

            data_raw = payload[:data_bytes]
            vals = [data_raw[i] | (data_raw[i + 1] << 8) for i in range(0, len(data_raw), 2)]
            matrix = []
            for r in range(rows):
                start = r * cols
                matrix.append(vals[start:start + cols])

            tail = payload[data_bytes:data_bytes + 12]
            timestamp = decode_binary_ts(tail) if len(tail) == 12 else ""

            print("SEND_DATA ok: status=0x{:02X} ({}) payload_len={}".format(
                status, status_str(status), payload_len
            ))
            return matrix, timestamp

        except OSError as e:
            print("SEND_DATA read {} bytes failed: {}".format(total_len, e))

    print("SEND_DATA failed for all candidate lengths.")
    return None, None


def test_pwr_status():
    write_cmd(I2C_CMD_PWR_STATUS)
    ok, st, waited = wait_until_ready(timeout_ms=3000)
    if not ok:
        print("PWR_STATUS timeout waiting ready: status=0x{:02X} ({}) waited={} ms".format(
            st, status_str(st), waited
        ))
        return None

    reply = i2c.readfrom(SLAVE_ADDR, 4)
    flag = reply[3]
    print("Power status = {} {}".format(flag, "(SAVING)" if flag else "(NORMAL)"))
    return flag


def test_latest_ts():
    write_cmd(I2C_CMD_CHECK_LATEST_TS)
    ok, st, waited = wait_until_ready(timeout_ms=5000)
    if not ok:
        print("CHECK_LATEST_TS timeout waiting ready: status=0x{:02X} ({}) waited={} ms".format(
            st, status_str(st), waited
        ))
        return None

    raw = i2c.readfrom(SLAVE_ADDR, 15)
    if raw[0] & I2C_STATUS_ERROR:
        print("Latest TS: slave reported error")
        return None

    timestamp = decode_binary_ts(raw[3:15])
    print("Latest file TS:", timestamp)
    return timestamp


def main():
    print("Testing STM32 communication...")
    print("Initial status: 0x{:02X} ({})".format(read_status_byte(), status_str(read_status_byte())))

    # Long-running path: no fixed 30/60 second delay; wait on status instead.
    if not send_start_and_wait_done(timeout_ms=90000):
        return

    request_rtc()
    set_stm32_rtc_from_pico()
    request_rtc()

    matrix, ts = request_data_matrix(rows=5, cols=10, offset=0)
    if matrix is not None:
        print("5x10 matrix:")
        for row in matrix:
            print(row)
        print("Timestamp:", ts if ts else "<none>")

    print("\nPower status test:")
    test_pwr_status()

    print("\nLatest timestamp test:")
    test_latest_ts()


if __name__ == "__main__":
    main()