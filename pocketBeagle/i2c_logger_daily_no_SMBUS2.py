#!/usr/bin/env python3
# i2c_logger.py
# systemctl stop i2c-logger.service

import os, time, csv, datetime, struct, errno
from pathlib import Path
from typing import Union, List, Tuple
import fcntl
import ctypes

# ---------- CONFIG (env-overridable) ----------
I2C_BUS     = int(os.getenv("I2C_BUS", "1"))          # /dev/i2c-1
SLAVE_ADDR  = int(os.getenv("I2C_ADDR", "0x13"), 16)  # seen in i2cdetect -y -r 1
CMD_SEND    = int(os.getenv("CMD_SEND", "197"))       # 0xC5
CMD_START   = int(os.getenv("CMD_START", "99"))       # 0x63
ROWS        = int(os.getenv("ROWS", "5"))
COLS        = int(os.getenv("COLS", "10"))
INTERVAL_S  = int(os.getenv("INTERVAL_S", "10"))
LOG_DIR     = Path(os.getenv("LOG_DIR", "/opt/i2c_logger"))
TS_BYTES    = int(os.getenv("TS_BYTES", "106"))       # optional hint
# ------------------------------------------------

DATA_BYTES_EXPECTED = ROWS * COLS * 2  # uint16 little-endian

# ----- Linux I2C ioctl constants -----
I2C_SLAVE = 0x0703
I2C_RDWR  = 0x0707
I2C_M_RD  = 0x0001

# ----- ctypes structs for I2C_RDWR -----
class I2CMsg(ctypes.Structure):
    _fields_ = [
        ("addr", ctypes.c_uint16),
        ("flags", ctypes.c_uint16),
        ("len", ctypes.c_uint16),
        ("buf", ctypes.c_void_p),
    ]

class I2CRdwr(ctypes.Structure):
    _fields_ = [
        ("msgs", ctypes.POINTER(I2CMsg)),
        ("nmsgs", ctypes.c_uint32),
    ]


# ---------- CSV helpers ----------
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


# ---------- Low-level I2C helpers (no smbus2 needed) ----------
def i2c_write(fd: int, addr: int, data: Union[bytes, bytearray, list, tuple]):
    if not isinstance(data, (bytes, bytearray)):
        data = bytes(data)
    fcntl.ioctl(fd, I2C_SLAVE, addr)
    os.write(fd, data)

def _i2c_rdwr(fd: int, addr: int, wbytes: Union[bytes, bytearray, list, tuple], rlen: int) -> bytes:
    """
    Combined write+read (repeated-start) using Linux I2C_RDWR ioctl.
    Equivalent to smbus2: bus.i2c_rdwr(i2c_msg.write(...), i2c_msg.read(...))
    """
    if rlen < 0:
        raise ValueError("negative read length")

    if not isinstance(wbytes, (bytes, bytearray)):
        wbytes = bytes(wbytes)

    wbuf = bytearray(wbytes)
    rbuf = bytearray(rlen)

    # ctypes buffers backed by Python bytearrays
    wbuf_c = (ctypes.c_uint8 * len(wbuf)).from_buffer(wbuf) if len(wbuf) else None
    rbuf_c = (ctypes.c_uint8 * len(rbuf)).from_buffer(rbuf) if len(rbuf) else None

    msgs = (I2CMsg * 2)()

    # Write message
    msgs[0] = I2CMsg(
        addr=addr,
        flags=0,
        len=len(wbuf),
        buf=(ctypes.cast(wbuf_c, ctypes.c_void_p) if wbuf_c is not None else ctypes.c_void_p(0)),
    )

    # Read message
    msgs[1] = I2CMsg(
        addr=addr,
        flags=I2C_M_RD,
        len=len(rbuf),
        buf=(ctypes.cast(rbuf_c, ctypes.c_void_p) if rbuf_c is not None else ctypes.c_void_p(0)),
    )

    rdwr = I2CRdwr(msgs=msgs, nmsgs=2)
    fcntl.ioctl(fd, I2C_RDWR, rdwr)
    return bytes(rbuf)

def rdwr_with_retry(fd: int, addr: int, wbytes: Union[bytes, list], rlen: int, retries=5, base_delay=0.05) -> bytes:
    """
    Combined write+read with retry/backoff on EBUSY.
    """
    for attempt in range(retries):
        try:
            return _i2c_rdwr(fd, addr, wbytes, rlen)
        except OSError as e:
            if e.errno == errno.EBUSY:
                time.sleep(base_delay * (attempt + 1))
                continue
            raise
    raise OSError(errno.EBUSY, "I2C still busy after retries")


# ---------- Protocol helpers ----------
def read_status(fd: int) -> int:
    # Send CMD_SEND, read 1 byte STATUS
    b = rdwr_with_retry(fd, SLAVE_ADDR, [CMD_SEND], 1)
    return b[0]

def wait_ready(fd: int, timeout_s=3.0, poll_delay=0.05) -> int:
    """
    Poll STATUS until ready=1 and busy=0.
    STATUS bits: bit0=busy, bit1=ready, bit2=error
    """
    t0 = time.time()
    last = 0
    while True:
        st = read_status(fd)
        last = st
        busy  = (st & 0x01) != 0
        ready = (st & 0x02) != 0
        if ready and not busy:
            return st
        if time.time() - t0 > timeout_s:
            raise TimeoutError(f"Not ready (status=0x{last:02X}) after {timeout_s:.2f}s")
        time.sleep(poll_delay)

def read_header(fd: int):
    """
    Read 3-byte header: [STATUS, LEN_L, LEN_H]
    """
    hdr = rdwr_with_retry(fd, SLAVE_ADDR, [CMD_SEND], 3)
    status = hdr[0]
    length = hdr[1] | (hdr[2] << 8)
    return status, length

def write_header(fd: int):
    """
    Send CMD_START to trigger STM32 capture / prepare payload.
    """
    i2c_write(fd, SLAVE_ADDR, [CMD_START])

def read_payload(fd: int, length: int) -> bytes:
    """
    Read exactly `length` bytes of payload with repeated-start: CMD_SEND then read.
    """
    if length < 0:
        raise ValueError("negative length")
    if length == 0:
        return b""
    if length > 65535:
        raise ValueError(f"payload length unrealistic: {length}")
    return rdwr_with_retry(fd, SLAVE_ADDR, [CMD_SEND], length)


# ---------- Payload parsing ----------
def split_payload(payload: bytes) -> Tuple[List[int], str]:
    """
    Interpret payload as:
      - first ROWS*COLS*2 bytes -> little-endian uint16 values
      - remaining bytes -> ASCII timestamp
    """
    data_part = payload[:DATA_BYTES_EXPECTED]
    ts_part   = payload[DATA_BYTES_EXPECTED:]

    if len(data_part) < DATA_BYTES_EXPECTED:
        data_part = data_part + b"\x00" * (DATA_BYTES_EXPECTED - len(data_part))

    count_vals = DATA_BYTES_EXPECTED // 2
    vals = list(struct.unpack('<' + 'H' * count_vals, data_part))

    if ts_part:
        ts = ts_part.decode('ascii', errors='ignore').strip('\x00\r\n ')
    else:
        ts = ""

    if not ts and TS_BYTES and len(payload) >= DATA_BYTES_EXPECTED:
        tail = payload[DATA_BYTES_EXPECTED:]
        idx = tail.find(b'TS:')
        if idx != -1:
            ts = tail[idx:].decode('ascii', errors='ignore').strip('\x00\r\n ')

    return vals, ts


# ---------- High-level read ----------
def fetch_matrix_and_ts(fd: int) -> Tuple[List[List[int]], str]:
    wait_ready(fd)
    status, length = read_header(fd)

    if not ((status & 0x02) and not (status & 0x01)):
        time.sleep(0.05)
        wait_ready(fd)
        status, length = read_header(fd)

    payload = read_payload(fd, length)
    vals, ts = split_payload(payload)
    matrix = [vals[r*COLS:(r+1)*COLS] for r in range(ROWS)]
    return matrix, ts


# ---------- Main ----------
def main():
    write = 0
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    i2c_path = f"/dev/i2c-{I2C_BUS}"
    fd = os.open(i2c_path, os.O_RDWR)

    try:
        while True:
            now = datetime.datetime.now()
            log_path = current_log_path(now)
            ensure_header(log_path)

            try:
                if write == 0:
                    write_header(fd)
                    write = 1
                    print("writing data from sensors")
                else:
                    matrix, stm32_ts = fetch_matrix_and_ts(fd)
                    with log_path.open("a", newline="") as f:
                        w = csv.writer(f)
                        for row in matrix:
                            w.writerow([now.isoformat(timespec="seconds"), stm32_ts, *row])
                    print(f"[{now:%Y-%m-%d %H:%M:%S}] Wrote {ROWS}x{COLS}")
                    write = 0

            except Exception as e:
                with log_path.open("a", newline="") as f:
                    csv.writer(f).writerow([now.isoformat(timespec="seconds"), "ERR", str(e)])
                print(f"[{now:%Y-%m-%d %H:%M:%S}] ERROR: {e}")

            time.sleep(INTERVAL_S)

    finally:
        os.close(fd)

if __name__ == "__main__":
    main()