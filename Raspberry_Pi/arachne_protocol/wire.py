"""Bounded COBS framing and CRC-16/CCITT-FALSE; see protocol/PROTOCOL.md."""
from dataclasses import dataclass
from enum import IntEnum
import struct

VERSION = 1
MAX_PAYLOAD = 256
HEADER = struct.Struct("<BBIQIH")
MAX_RAW = HEADER.size + MAX_PAYLOAD + 2
MAX_ENCODED = MAX_RAW + MAX_RAW // 254 + 1
MAX_WIRE = MAX_ENCODED + 1


class Message(IntEnum):
    HELLO = 1
    HEARTBEAT = 2
    GET_STATUS = 3
    ENABLE_REQUEST = 4
    DISABLE = 5
    SET_JOINT_TARGET = 6
    SET_MULTI_JOINT_TARGET = 7
    EMERGENCY_STOP = 8
    HELLO_ACK = 128
    ACK = 129
    NACK = 130
    STATUS = 131
    HEARTBEAT_REPLY = 132


class ProtocolError(ValueError):
    pass


@dataclass(frozen=True)
class Frame:
    message: Message
    request: int
    session: int = 0
    token: int = 0
    payload: bytes = b""
    version: int = VERSION


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xFFFF
    return crc


def cobs_encode(data: bytes) -> bytes:
    if len(data) > MAX_RAW:
        raise ProtocolError("raw frame too large")
    output = bytearray([0])
    code_at, code = 0, 1
    for byte in data:
        if byte == 0:
            output[code_at] = code
            code_at, code = len(output), 1
            output.append(0)
        else:
            output.append(byte)
            code += 1
            if code == 255:
                output[code_at] = code
                code_at, code = len(output), 1
                output.append(0)
    output[code_at] = code
    return bytes(output)


def cobs_decode(data: bytes) -> bytes:
    if len(data) > MAX_ENCODED:
        raise ProtocolError("encoded frame too large")
    output = bytearray()
    pos = 0
    while pos < len(data):
        code = data[pos]
        pos += 1
        if code == 0 or pos + code - 1 > len(data):
            raise ProtocolError("malformed COBS")
        block = data[pos:pos + code - 1]
        if 0 in block:
            raise ProtocolError("embedded delimiter")
        output.extend(block)
        pos += code - 1
        if code != 255 and pos < len(data):
            output.append(0)
        if len(output) > MAX_RAW:
            raise ProtocolError("decoded frame too large")
    return bytes(output)


def encode(frame: Frame) -> bytes:
    if frame.version != VERSION or len(frame.payload) > MAX_PAYLOAD:
        raise ProtocolError("unsupported version or oversized payload")
    try:
        message = Message(frame.message)
        body = HEADER.pack(frame.version, message, frame.request, frame.session,
                           frame.token, len(frame.payload)) + frame.payload
    except (ValueError, struct.error) as error:
        raise ProtocolError("invalid header") from error
    return cobs_encode(body + struct.pack("<H", crc16(body))) + b"\0"


def decode(data: bytes) -> Frame:
    if not data or data[-1] != 0:
        raise ProtocolError("truncated frame: missing delimiter")
    if len(data) > MAX_WIRE:
        raise ProtocolError("frame too large")
    raw = cobs_decode(data[:-1])
    if len(raw) < HEADER.size + 2:
        raise ProtocolError("truncated header")
    version, message, request, session, token, length = HEADER.unpack_from(raw)
    if length > MAX_PAYLOAD or len(raw) != HEADER.size + length + 2:
        raise ProtocolError("invalid payload length")
    if crc16(raw[:-2]) != struct.unpack_from("<H", raw, len(raw) - 2)[0]:
        raise ProtocolError("CRC mismatch")
    if version != VERSION:
        raise ProtocolError("unsupported version")
    try:
        kind = Message(message)
    except ValueError as error:
        raise ProtocolError("unknown message") from error
    return Frame(kind, request, session, token, raw[HEADER.size:-2], version)


class Parser:
    """Incremental bounded parser. Bad packets are counted, never returned."""
    def __init__(self):
        self.buffer = bytearray()
        self.dropping = False
        self.errors = 0

    def push(self, byte: int) -> Frame | None:
        if not 0 <= byte <= 255:
            raise ProtocolError("not a byte")
        if self.dropping:
            if byte == 0:
                self.dropping = False
            return None
        if byte == 0 and not self.buffer:
            return None
        if len(self.buffer) == MAX_ENCODED and byte != 0:
            self.buffer.clear()
            self.dropping = True
            self.errors += 1
            return None
        self.buffer.append(byte)
        if byte != 0:
            return None
        try:
            return decode(bytes(self.buffer))
        except ProtocolError:
            self.errors += 1
            return None
        finally:
            self.buffer.clear()

    def finish(self):
        partial = bool(self.buffer) or self.dropping
        self.buffer.clear()
        self.dropping = False
        if partial:
            self.errors += 1
            raise ProtocolError("truncated stream")
