#!/usr/bin/env python3
# .service stored in debian@BeagleBone:/etc/systemd/system$ in pocketbeagle
# systemctl stop i2c-logger.service
#WILL have to instal SMBUS2 !!
import os, time, csv, datetime, struct
from pathlib import Path
from smbus2 import SMBus, i2c_msg
import errno
from typing import Union 

# ---------- CONFIG (env-overridable) ----------
I2C_BUS     = int(os.getenv("I2C_BUS", "1"))          # /dev/i2c-1
SLAVE_ADDR  = int(os.getenv("I2C_ADDR", "0x13"), 16)  # seen in i2cdetect -y -r 1
CMD_SEND    = int(os.getenv("CMD_SEND", "197"))       # 0xC5
CMD_START   = int("99")
ROWS        = int(os.getenv("ROWS", "5"))
COLS        = int(os.getenv("COLS", "10"))
INTERVAL_S  = int(os.getenv("INTERVAL_S", "10"))
LOG_DIR     = Path(os.getenv("LOG_DIR", "/opt/i2c_logger"))
# Optional expectation for timestamp bytes (used only for sanity checks / parsing help)
TS_BYTES    = int(os.getenv("TS_BYTES", "106"))
# ------------------------------------------------

DATA_BYTES_EXPECTED = ROWS * COLS * 2  # uint16 little-endian

def ensure_header(path: Path):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.stat().st_size == 0:
        with path.open("w", newline="") as f:
            w = csv.writer(f)
            header = ["local_iso", "stm32_ts"] + [f"v{i+1}" for i in range(ROWS * COLS)]
            w.writerow(header)

def current_log_path(now=None):
    now = now or datetime.datetime.now()
    return LOG_DIR / f"{now:%Y-%m-%d}.csv"

# ----- Low-level I2C helpers -----
def rdwr_with_retry(bus: SMBus, addr: int, wbytes: Union[bytes, list], rlen: int, retries=5, base_delay=0.05):
    """
    Combined write+read (repeated-start). Recreate i2c_msg each attempt.
    Retries/backoff on EBUSY.
    """
    for attempt in range(retries):
        try:
            w = i2c_msg.write(addr, wbytes)
            r = i2c_msg.read(addr, rlen)
            bus.i2c_rdwr(w, r)
            return bytes(r)
        except OSError as e:
            if e.errno == errno.EBUSY:
                time.sleep(base_delay * (attempt + 1))
                continue
            raise
    raise OSError(errno.EBUSY, "I2C still busy after retries")

def read_status(bus: SMBus) -> int:
    # Send CMD, read 1 byte STATUS in one combined transfer.
    b = rdwr_with_retry(bus, SLAVE_ADDR, [CMD_SEND], 1)
    return b[0]

def wait_ready(bus: SMBus, timeout_s=3.0, poll_delay=0.05):
    """
    Poll STATUS until ready=1 and busy=0.
    STATUS bits (per your code): bit0=busy, bit1=ready, bit2=error
    """
    t0 = time.time()
    last = None
    while True:
        st = read_status(bus)
        last = st
        busy  = (st & 0x01) != 0
        ready = (st & 0x02) != 0
        if ready and not busy:
            return st
        if time.time() - t0 > timeout_s:
            raise TimeoutError(f"Not ready (status=0x{last:02X}) after {timeout_s:.2f}s")
        time.sleep(poll_delay)

def read_header(bus: SMBus):
    """
    Read 3-byte header: [STATUS, LEN_L, LEN_H]
    """
    hdr = rdwr_with_retry(bus, SLAVE_ADDR, [CMD_SEND], 3)
    status = hdr[0]
    length = hdr[1] | (hdr[2] << 8)
    return status, length

def write_header(bus: SMBus):
    """
    Read 3-byte header: [STATUS, LEN_L, LEN_H]
    """
    i2c_msg.write(SLAVE_ADDR, [CMD_START])

def read_payload(bus: SMBus, length: int) -> bytes:
    """
    Read exactly `length` bytes of payload with a combined xfer (CMD then read).
    """
    if length < 0:
        raise ValueError("negative length")
    if length == 0:
        return b""
    # Guard against pathological values
    if length > 65535:
        raise ValueError(f"payload length unrealistic: {length}")
    return rdwr_with_retry(bus, SLAVE_ADDR, [CMD_SEND], length)

# ----- Payload parsing -----
TS_BYTES_BIN = 12  # 6 little-endian uint16: year(full), month, day, hour, minute, second

def split_payload(payload: bytes) -> tuple[list[int], str]:
    """
    Interpret payload as:
      - first ROWS*COLS*2 bytes -> little-endian uint16 values
      - next 12 bytes           -> 6 little-endian uint16 timestamp
    Same binary timestamp format on both SD and no-SD paths.
    """
    data_part = payload[:DATA_BYTES_EXPECTED]
    ts_part   = payload[DATA_BYTES_EXPECTED:DATA_BYTES_EXPECTED + TS_BYTES_BIN]

    # If the payload is shorter than expected, pad zeros to avoid struct errors.
    if len(data_part) < DATA_BYTES_EXPECTED:
        data_part = data_part + b"\x00" * (DATA_BYTES_EXPECTED - len(data_part))

    count_vals = DATA_BYTES_EXPECTED // 2
    vals = list(struct.unpack('<' + 'H' * count_vals, data_part))

    # Decode 12-byte binary timestamp (year is full, e.g. 2026)
    if len(ts_part) == TS_BYTES_BIN:
        y, mo, d, h, mi, s = struct.unpack('<HHHHHH', ts_part)
        ts = "%04d-%02d-%02d %02d:%02d:%02d" % (y, mo, d, h, mi, s)
    else:
        ts = ""

    return vals, ts

# ----- High-level read -----
def fetch_matrix_and_ts(bus: SMBus) -> tuple[list[list[int]], str]:
    """
    Poll ready, read header (status+length), then read payload.
    Parse into ROWS×COLS matrix and timestamp string.
    """
    wait_ready(bus)                       # ensure device is ready
    status, length = read_header(bus)     # get length
    if not ((status & 0x02) and not (status & 0x01)):
        # Not ready (race) -> small wait and retry once
        time.sleep(0.05)
        wait_ready(bus)
        status, length = read_header(bus)

    payload = read_payload(bus, length)
    vals, ts = split_payload(payload)

    # reshape to ROWS x COLS
    matrix = [vals[r*COLS:(r+1)*COLS] for r in range(ROWS)]
    return matrix, ts

# ----- Main -----

def main():
    read=0
    write=0
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    with SMBus(I2C_BUS, force=True) as bus:
        while True:
            now = datetime.datetime.now()
            log_path = current_log_path(now)
            ensure_header(log_path)

            try:
                if(write == 0):
                    write_header(bus)
                    write = 1
                    print("writing data from sensors")
                else:
                    matrix, stm32_ts = fetch_matrix_and_ts(bus)
                    with log_path.open("a", newline="") as f:
                        w = csv.writer(f)
                        # write one CSV row per matrix row
                        for row in matrix:
                            w.writerow([now.isoformat(timespec="seconds"), stm32_ts, *row])
                    print(f"[{now:%Y-%m-%d %H:%M:%S}] Wrote {ROWS}x{COLS}")
                    write = 0
            except Exception as e:
                # log error row so you retain timeline of failures
                with log_path.open("a", newline="") as f:
                    csv.writer(f).writerow([now.isoformat(timespec="seconds"), "ERR", str(e)])
                print(f"[{now:%Y-%m-%d %H:%M:%S}] ERROR: {e}")

            time.sleep(INTERVAL_S)

if __name__ == "__main__":
    main()
