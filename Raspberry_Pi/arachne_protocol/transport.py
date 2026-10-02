from collections import deque
from typing import Protocol
from .wire import MAX_WIRE, ProtocolError


class Transport(Protocol):
    """Nonblocking byte transport; write enqueues all bytes or raises."""
    def write(self, data: bytes) -> None: ...
    def read(self, maximum: int) -> bytes: ...


class MemoryTransport:
    CAPACITY = 4 * MAX_WIRE

    def __init__(self):
        self.peer: MemoryTransport | None = None
        self.buffer: deque[int] = deque()
        self.closed = False

    @classmethod
    def pair(cls):
        left, right = cls(), cls()
        left.peer, right.peer = right, left
        return left, right

    def write(self, data: bytes):
        if self.closed or self.peer is None or self.peer.closed:
            raise ProtocolError("transport closed/unconnected")
        if len(data) > self.CAPACITY - len(self.peer.buffer):
            raise ProtocolError("transport capacity exceeded")
        self.peer.buffer.extend(data)

    def read(self, maximum: int) -> bytes:
        if self.closed:
            raise ProtocolError("transport closed")
        if maximum < 0:
            raise ValueError("negative read size")
        return bytes(self.buffer.popleft() for _ in range(min(maximum, len(self.buffer))))
